// Practica 3 - Tarea 4. Paralelizacion del clasificador kNN con MPI.
//
// Tres estrategias (primer argumento):
//
//  datos        Paralelismo de DATOS. Se mantiene el orden secuencial
//               (para cada K, para cada fold) pero las muestras de
//               validacion de cada fold se reparten por bloques entre los P
//               procesos. Cada proceso cuenta sus aciertos y se suman con
//               MPI_Allreduce. La evaluacion final reparte el test igual y
//               suma aciertos con MPI_Reduce.
//
//  tareas       Paralelismo de TAREAS (grid search). Las nK*folds
//               validaciones (K, fold) son independientes: se reparten de
//               forma ciclica (tarea t -> proceso t % P). Cada proceso
//               calcula la accuracy de sus tareas, se combinan con
//               MPI_Reduce(SUM), el proceso 0 elige el mejor K y lo difunde
//               con MPI_Bcast. La evaluacion final se reparte por datos.
//
//  especulativo Paralelismo FUNCIONAL + ejecucion especulativa. Se divide
//               MPI_COMM_WORLD en dos grupos (MPI_Comm_split):
//                 - grupo AJUSTE (ranks 0..T-1): hace el fine-tuning con
//                   paralelismo de datos dentro del grupo.
//                 - grupo EVALUACION (ranks T..P-1): empieza YA la evaluacion
//                   final sobre test con el mejor K conocido (al principio
//                   minK). Cada CHECK muestras comprueba con MPI_Iprobe si el
//                   rank 0 ha enviado un K mejor (MPI_Send punto a punto). Si
//                   el K cambia, invalida lo hecho y reinicia.
//                 - al terminar el ajuste, el rank 0 envia un mensaje FINAL
//                   con el K optimo; la evaluacion garantiza que su resultado
//                   corresponde a ese K.
//
// Todos los procesos leen MNIST de su disco local (el dataset esta copiado
// en cada nodo): asi se evita enviar ~50 MB por la red a cada proceso.
//
// Uso:
//   mpirun -np P ./knn_mpi <datos|tareas|especulativo> <nCV> <nTrainFinal> <nTest> <minK> <maxK> <folds> [nEval] [dirDatos]
//   (nEval = procesos del grupo de evaluacion en modo especulativo; por defecto P/2)
#include "sample.h"
#include "mnist.h"

#include <mpi.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Clasificacion kNN: misma logica que knn.cc (distancia euclidea, ordenacion
// completa, histograma de los k primeros y etiqueta mas votada) pero con
// reserve() en los vectores (conclusion de la Tarea 0) y sin copiar el
// conjunto de entrenamiento al crear el clasificador.
// ---------------------------------------------------------------------------
struct Res { int label; float distance; bool operator<(const Res& o) const { return distance < o.distance; } };

static float euclidean(const TSample& a, const TSample& b) {
    float sum = 0.0f;
    const float* A = a.image.pixels.data();
    const float* B = b.image.pixels.data();
    const int n = a.image.rows * a.image.cols;
    for (int i = 0; i < n; i++) { float d = A[i] - B[i]; sum += d * d; }
    return std::sqrt(sum);
}

static int classify(const std::vector<TSample>& train, const TSample& s, int k, std::vector<Res>& results) {
    results.clear();
    results.reserve(train.size());
    for (size_t i = 0; i < train.size(); i++) results.push_back({train[i].label, euclidean(s, train[i])});
    std::sort(results.begin(), results.end());
    std::map<int, int> votes;
    for (int i = 0; i < k; i++) votes[results[i].label]++;
    int maxVotes = 0, maxLabel = 0;
    for (auto& v : votes) if (v.second > maxVotes) { maxVotes = v.second; maxLabel = v.first; }
    return maxLabel;
}

// Aciertos al clasificar val[ini, fin)
static long countMatches(const std::vector<TSample>& train, const std::vector<TSample>& val, size_t ini, size_t fin, int k) {
    std::vector<Res> buf;
    long m = 0;
    for (size_t i = ini; i < fin; i++) if (classify(train, val[i], k, buf) == val[i].label) m++;
    return m;
}

// Igual que CrossValidation::splitDataset pero con reserve()
static void splitFold(const std::vector<TSample>& data, unsigned fold, unsigned folds,
                      std::vector<TSample>& train, std::vector<TSample>& val) {
    size_t start = data.size() * fold / folds, end = data.size() * (fold + 1) / folds;
    train.clear(); val.clear();
    train.reserve(data.size() - (end - start)); val.reserve(end - start);
    for (size_t i = 0; i < data.size(); i++) (i < start || i >= end ? train : val).push_back(data[i]);
}

static inline size_t blockBegin(size_t n, int r, int P) { return n * r / P; }

// Temporizador de comunicaciones: acumula el tiempo que el proceso pasa dentro de llamadas MPI
static double gComm = 0.0;
#define COMM(call) do { double _t = MPI_Wtime(); call; gComm += MPI_Wtime() - _t; } while (0)

// Fine-tuning con paralelismo de datos dentro del comunicador 'comm'.
// onNewBest se llama (solo en rank 0 de comm) cada vez que se mejora el K.
template <class F>
static int tuningDatos(const std::vector<TSample>& cvData, unsigned minK, unsigned maxK, unsigned folds,
                       MPI_Comm comm, F onNewBest, bool verbose) {
    int r, P; MPI_Comm_rank(comm, &r); MPI_Comm_size(comm, &P);
    int bestK = minK; double bestAcc = 0.0;
    std::vector<TSample> train, val;
    // El split no depende de K: se precalculan los folds una sola vez
    std::vector<std::vector<TSample>> trains(folds), vals(folds);
    for (unsigned f = 0; f < folds; f++) splitFold(cvData, f, folds, trains[f], vals[f]);
    for (unsigned k = minK; k <= maxK; k++) {
        double acc = 0.0;
        for (unsigned f = 0; f < folds; f++) {
            const auto& V = vals[f];
            long local = countMatches(trains[f], V, blockBegin(V.size(), r, P), blockBegin(V.size(), r + 1, P), k);
            long total = 0;
            COMM(MPI_Allreduce(&local, &total, 1, MPI_LONG, MPI_SUM, comm));
            acc += (double)total / V.size();
        }
        acc /= folds;
        if (verbose && r == 0) std::printf("K: %u Accuracy: %.4f%%\n", k, acc * 100);
        if (acc > bestAcc) { bestAcc = acc; bestK = k; if (r == 0) onNewBest(bestK); }
    }
    return bestK;
}

// Fine-tuning con paralelismo de tareas: (K, fold) -> proceso (t % P)
static int tuningTareas(const std::vector<TSample>& cvData, unsigned minK, unsigned maxK, unsigned folds, MPI_Comm comm) {
    int r, P; MPI_Comm_rank(comm, &r); MPI_Comm_size(comm, &P);
    unsigned nK = maxK - minK + 1;
    std::vector<double> accLocal(nK, 0.0), accTotal(nK, 0.0);
    std::vector<TSample> train, val;
    int lastFold = -1;
    // Recorremos fold externo / K interno para reutilizar el split del fold
    for (unsigned f = 0; f < folds; f++) {
        for (unsigned ki = 0; ki < nK; ki++) {
            unsigned t = f * nK + ki;
            if ((int)(t % P) != r) continue;
            if ((int)f != lastFold) { splitFold(cvData, f, folds, train, val); lastFold = f; }
            accLocal[ki] += (double)countMatches(train, val, 0, val.size(), minK + ki) / val.size();
        }
    }
    COMM(MPI_Reduce(accLocal.data(), accTotal.data(), nK, MPI_DOUBLE, MPI_SUM, 0, comm));
    int bestK = minK;
    if (r == 0) {
        double bestAcc = 0.0;
        for (unsigned ki = 0; ki < nK; ki++) {
            double acc = accTotal[ki] / folds;
            std::printf("K: %u Accuracy: %.4f%%\n", minK + ki, acc * 100);
            if (acc > bestAcc) { bestAcc = acc; bestK = minK + ki; }
        }
    }
    COMM(MPI_Bcast(&bestK, 1, MPI_INT, 0, comm));
    return bestK;
}

enum { TAG_K = 100, TAG_RES = 200, MSG_NEWK = 1, MSG_FINAL = 2 };

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);
    int rank, P;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &P);

    if (argc < 8) {
        if (rank == 0) std::fprintf(stderr, "Uso: %s <datos|tareas|especulativo> <nCV> <nTrainFinal> <nTest> <minK> <maxK> <folds> [nEval] [dirDatos]\n", argv[0]);
        MPI_Finalize(); return 1;
    }
    std::string modo = argv[1];
    size_t nCV = std::atol(argv[2]), nTrainFinal = std::atol(argv[3]), nTest = std::atol(argv[4]);
    unsigned minK = std::atoi(argv[5]), maxK = std::atoi(argv[6]), folds = std::atoi(argv[7]);
    int nEval = (argc > 8) ? std::atoi(argv[8]) : P / 2;
    std::string dir = (argc > 9) ? std::string(argv[9]) + "/" : "";

    // ---- Carga local de datos (cada proceso desde su disco) ----
    double tl = MPI_Wtime();
    std::vector<TSample> train, test;
    bool ok = mnist::loadMnist((dir + "train/train-images-idx3-ubyte").c_str(), (dir + "train/train-labels-idx1-ubyte").c_str(), train) &&
              mnist::loadMnist((dir + "test/t10k-images-idx3-ubyte").c_str(), (dir + "test/t10k-labels-idx1-ubyte").c_str(), test);
    int okAll = 0, okLocal = ok ? 1 : 0;
    MPI_Allreduce(&okLocal, &okAll, 1, MPI_INT, MPI_MIN, MPI_COMM_WORLD);
    if (!okAll) {
        if (!ok) { char h[MPI_MAX_PROCESSOR_NAME]; int l; MPI_Get_processor_name(h, &l); std::fprintf(stderr, "[rank %d en %s] No se pudo leer MNIST\n", rank, h); }
        MPI_Finalize(); return 1;
    }
    nCV = std::min(nCV, train.size()); nTrainFinal = std::min(nTrainFinal, train.size()); nTest = std::min(nTest, test.size());
    std::vector<TSample> cvData(train.begin(), train.begin() + nCV);
    train.erase(train.begin() + nTrainFinal, train.end());
    test.erase(test.begin() + nTest, test.end());
    double tLoad = MPI_Wtime() - tl;

    MPI_Barrier(MPI_COMM_WORLD);
    double t0 = MPI_Wtime();
    double t1 = t0;               // fin del ajuste
    int bestK = minK; double testAcc = 0.0; int reinicios = 0; long desperdiciadas = 0;

    if (modo == "datos" || modo == "tareas") {
        if (modo == "datos") bestK = tuningDatos(cvData, minK, maxK, folds, MPI_COMM_WORLD, [](int) {}, true);
        else                 bestK = tuningTareas(cvData, minK, maxK, folds, MPI_COMM_WORLD);
        t1 = MPI_Wtime();
        long local = countMatches(train, test, blockBegin(nTest, rank, P), blockBegin(nTest, rank + 1, P), bestK);
        long total = 0;
        COMM(MPI_Reduce(&local, &total, 1, MPI_LONG, MPI_SUM, 0, MPI_COMM_WORLD));
        testAcc = (double)total / nTest;
    } else if (modo == "especulativo") {
        if (P < 2 || nEval < 1 || nEval >= P) {
            if (rank == 0) std::fprintf(stderr, "Modo especulativo: P>=2 y 1<=nEval<P\n");
            MPI_Finalize(); return 1;
        }
        const int T = P - nEval;           // procesos de ajuste: ranks 0..T-1
        const bool esAjuste = rank < T;
        MPI_Comm grupo;
        MPI_Comm_split(MPI_COMM_WORLD, esAjuste ? 0 : 1, rank, &grupo);
        int gr, gP; MPI_Comm_rank(grupo, &gr); MPI_Comm_size(grupo, &gP);

        if (esAjuste) {
            auto avisar = [&](int k) {      // rank 0 -> todos los evaluadores
                int msg[2] = {MSG_NEWK, k};
                for (int d = T; d < P; d++) COMM(MPI_Send(msg, 2, MPI_INT, d, TAG_K, MPI_COMM_WORLD));
            };
            bestK = tuningDatos(cvData, minK, maxK, folds, grupo, avisar, true);
            t1 = MPI_Wtime();
            if (rank == 0) {
                int msg[2] = {MSG_FINAL, bestK};
                for (int d = T; d < P; d++) COMM(MPI_Send(msg, 2, MPI_INT, d, TAG_K, MPI_COMM_WORLD));
                // Resultado de la evaluacion (lo envia el rank T, raiz del grupo de evaluacion)
                double res[4];
                COMM(MPI_Recv(res, 4, MPI_DOUBLE, T, TAG_RES, MPI_COMM_WORLD, MPI_STATUS_IGNORE));
                testAcc = res[0]; reinicios = (int)res[1]; desperdiciadas = (long)res[2];
                if ((int)res[3] != bestK) std::fprintf(stderr, "ERROR: evaluacion con K=%d != K optimo %d\n", (int)res[3], bestK);
            }
        } else {
            const int CHECK = 10;            // cada cuantas muestras se mira si hay mensajes
            size_t ini = blockBegin(nTest, gr, gP), fin = blockBegin(nTest, gr + 1, gP);
            int curK = minK;                 // especulacion inicial
            bool finalRecibido = false;
            long matches = 0;
            std::vector<Res> buf;
            // Procesa un mensaje; devuelve true si obliga a reiniciar
            auto procesar = [&](int* msg) {
                if (msg[0] == MSG_FINAL) finalRecibido = true;
                if (msg[1] != curK) { curK = msg[1]; return true; }
                return false;
            };
            while (true) {
                bool reiniciar = false;
                matches = 0;
                size_t i = ini;
                for (; i < fin && !reiniciar; i++) {
                    if (!finalRecibido && (i - ini) % CHECK == 0) {
                        int flag = 1;
                        while (flag && !finalRecibido) {
                            MPI_Status st;
                            COMM(MPI_Iprobe(0, TAG_K, MPI_COMM_WORLD, &flag, &st));
                            if (flag) {
                                int msg[2];
                                COMM(MPI_Recv(msg, 2, MPI_INT, 0, TAG_K, MPI_COMM_WORLD, MPI_STATUS_IGNORE));
                                reiniciar |= procesar(msg);
                            }
                        }
                        if (reiniciar) break;
                    }
                    if (classify(train, test[i], curK, buf) == test[i].label) matches++;
                }
                if (!reiniciar) {
                    // Terminado el bloque: esperar (bloqueante) hasta conocer el K definitivo
                    while (!finalRecibido) {
                        int msg[2];
                        COMM(MPI_Recv(msg, 2, MPI_INT, 0, TAG_K, MPI_COMM_WORLD, MPI_STATUS_IGNORE));
                        reiniciar |= procesar(msg);
                    }
                }
                if (!reiniciar) break;
                reinicios++; desperdiciadas += (long)(i - ini);
            }
            long total = 0, reinTot = 0, despTot = 0, rein = reinicios;
            COMM(MPI_Reduce(&matches, &total, 1, MPI_LONG, MPI_SUM, 0, grupo));
            COMM(MPI_Reduce(&rein, &reinTot, 1, MPI_LONG, MPI_MAX, 0, grupo));
            COMM(MPI_Reduce(&desperdiciadas, &despTot, 1, MPI_LONG, MPI_SUM, 0, grupo));
            if (gr == 0) {
                double res[4] = {(double)total / nTest, (double)reinTot, (double)despTot, (double)curK};
                COMM(MPI_Send(res, 4, MPI_DOUBLE, 0, TAG_RES, MPI_COMM_WORLD));
            }
            bestK = curK;
        }
        MPI_Comm_free(&grupo);
    } else {
        if (rank == 0) std::fprintf(stderr, "Modo desconocido: %s\n", modo.c_str());
        MPI_Finalize(); return 1;
    }

    double tEnd = MPI_Wtime();
    // El tiempo total es el del proceso que mas tarda
    double myTotal = tEnd - t0, maxTotal = 0, maxComm = 0, myTun = t1 - t0, maxTun = 0, maxLoad = 0;
    MPI_Reduce(&myTotal, &maxTotal, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&gComm, &maxComm, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&tLoad, &maxLoad, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&myTun, &maxTun, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        double tTest = maxTotal - maxTun;  // en especulativo: lo que la evaluacion tarda DESPUES del ajuste
        std::printf("\n=== kNN MPI | modo=%s | P=%d%s ===\n", modo.c_str(), P,
                    modo == "especulativo" ? (" (ajuste=" + std::to_string(P - nEval) + ", evaluacion=" + std::to_string(nEval) + ")").c_str() : "");
        std::printf("nCV=%zu nTrainFinal=%zu nTest=%zu K=[%u,%u] folds=%u\n", nCV, nTrainFinal, nTest, minK, maxK, folds);
        std::printf("Best k: %d | Test accuracy: %.4f\n", bestK, testAcc);
        if (modo == "especulativo") std::printf("Reinicios de la evaluacion: %d | muestras de test desechadas: %ld\n", reinicios, desperdiciadas);
        std::printf("Carga datos (max): %.3f s\n", maxLoad);
        std::printf("Ajuste (fine-tuning): %.4f s | Test tras ajuste: %.4f s\n", maxTun, tTest);
        std::printf("TOTAL (sin carga): %.4f s | Tiempo en MPI (max proceso): %.4f s (%.1f%%)\n",
                    maxTotal, maxComm, 100.0 * maxComm / maxTotal);
        // modo,P,nCV,nTrainFinal,nTest,folds,minK,maxK,bestK,testAcc,t_tuning,t_test,t_total,t_comm,reinicios
        std::printf("CSV,%s,%d,%zu,%zu,%zu,%u,%u,%u,%d,%.4f,%.4f,%.4f,%.4f,%.4f,%d\n", modo.c_str(), P, nCV, nTrainFinal,
                    nTest, folds, minK, maxK, bestK, testAcc, maxTun, tTest, maxTotal, maxComm, reinicios);
    }
    MPI_Finalize();
    return 0;
}
