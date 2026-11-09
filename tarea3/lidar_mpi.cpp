// Practica 3 - Tarea 3. Version MPI del codigo de la Practica 2 (LIDAR).
//
// Segun gprof, la funcion critica es distancia_minima_entre_puntos (O(n^2),
// >99% del tiempo). Es la que se paraleliza:
//   - El proceso 0 genera la nube (misma semilla -> mismos datos que la
//     version secuencial) y la difunde con MPI_Bcast.
//   - Cada proceso recorre solo SUS filas i del doble bucle (paralelismo de
//     datos) y calcula un minimo parcial.
//   - MPI_Reduce(MPI_MIN) junta los minimos parciales en el proceso 0.
//   - Las tareas A (centroide+bbox, O(n)) y B (ordenacion, O(n log n)) son
//     despreciables y las hace el proceso 0 (no compensa comunicarlas).
//
// Dos estrategias de reparto de las filas i (el trabajo de la fila i es
// n-1-i comparaciones, es decir, la carga es TRIANGULAR):
//   bloque : el proceso p recibe un bloque contiguo de filas -> desequilibrado
//            (el proceso 0 hace muchas mas comparaciones que el ultimo).
//   ciclico: la fila i va al proceso i % P -> carga equilibrada.
//
// Uso: mpirun -np P ./lidar_mpi <n_puntos> <dispersion> <distancia> [semilla] [bloque|ciclico]
#include <mpi.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>
#include <random>
#include <limits>

struct Point3D { double x, y, z; };
struct BoundingBox { double min_x, max_x, min_y, max_y, min_z, max_z; };

static std::vector<Point3D> generar_obstaculo(int n, double dispersion, double distancia, unsigned seed) {
    std::mt19937 rng(seed);
    std::normal_distribution<double> lateral(0.0, dispersion);
    std::normal_distribution<double> altura(1.0, dispersion * 0.5);
    std::normal_distribution<double> profundidad(distancia, dispersion * 0.3);
    std::vector<Point3D> nube;
    nube.reserve(n);
    for (int i = 0; i < n; ++i) nube.push_back({lateral(rng), altura(rng), profundidad(rng)});
    return nube;
}

static void centroide_y_bbox(const std::vector<Point3D>& nube, Point3D& c, BoundingBox& b) {
    double sx = 0, sy = 0, sz = 0;
    b.min_x = b.min_y = b.min_z = std::numeric_limits<double>::max();
    b.max_x = b.max_y = b.max_z = std::numeric_limits<double>::lowest();
    for (const auto& p : nube) {
        sx += p.x; sy += p.y; sz += p.z;
        b.min_x = std::min(b.min_x, p.x); b.max_x = std::max(b.max_x, p.x);
        b.min_y = std::min(b.min_y, p.y); b.max_y = std::max(b.max_y, p.y);
        b.min_z = std::min(b.min_z, p.z); b.max_z = std::max(b.max_z, p.z);
    }
    size_t n = nube.size();
    c = {sx / n, sy / n, sz / n};
}

// Minimo (al cuadrado) de las filas que le tocan a este proceso.
static double min_parcial(const std::vector<Point3D>& nube, int rank, int P, bool ciclico, long long& comparaciones) {
    double mejor = std::numeric_limits<double>::max();
    const size_t n = nube.size();
    size_t ini, fin, paso;
    if (ciclico) { ini = rank; fin = n; paso = P; }
    else { ini = n * rank / P; fin = n * (rank + 1) / P; paso = 1; }
    comparaciones = 0;
    for (size_t i = ini; i < fin; i += paso) {
        const Point3D pi = nube[i];
        for (size_t j = i + 1; j < n; ++j) {
            double dx = pi.x - nube[j].x, dy = pi.y - nube[j].y, dz = pi.z - nube[j].z;
            double d2 = dx * dx + dy * dy + dz * dz;
            if (d2 < mejor) mejor = d2;
        }
        comparaciones += (long long)(n - 1 - i);
    }
    return mejor;
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);
    int rank, P;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &P);

    if (argc < 4) {
        if (rank == 0) std::fprintf(stderr, "Uso: %s <n_puntos> <dispersion> <distancia_m> [semilla] [bloque|ciclico]\n", argv[0]);
        MPI_Finalize();
        return 1;
    }
    int n = std::atoi(argv[1]);
    double dispersion = std::atof(argv[2]);
    double distancia = std::atof(argv[3]);
    unsigned seed = (argc > 4) ? (unsigned)std::atoi(argv[4]) : 42u;
    bool ciclico = (argc > 5) ? (std::strcmp(argv[5], "bloque") != 0) : true;
    const double UMBRAL_SOLIDO = 0.05;

    MPI_Barrier(MPI_COMM_WORLD);

    // 1) Generacion (solo proceso 0, no se cuenta en el tiempo, igual que en la P2)
    std::vector<Point3D> nube(n);
    if (rank == 0) nube = generar_obstaculo(n, dispersion, distancia, seed);
    MPI_Barrier(MPI_COMM_WORLD);
    double t0 = MPI_Wtime();

    // 2) Difusion de la nube a todos los procesos (comunicacion colectiva)
    MPI_Bcast(nube.data(), n * 3, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    double t1 = MPI_Wtime();

    // 3) Tareas A y B en el proceso 0 (baratas)
    Point3D centroide{}; BoundingBox caja{}; double mas_cercano = 0;
    if (rank == 0) {
        centroide_y_bbox(nube, centroide, caja);
        std::vector<Point3D> copia = nube;
        std::sort(copia.begin(), copia.end(), [](const Point3D& a, const Point3D& b) { return a.z < b.z; });
        mas_cercano = copia.front().z;
    }
    double t2 = MPI_Wtime();

    // 4) Tarea C repartida
    long long comps = 0;
    double local = min_parcial(nube, rank, P, ciclico, comps);
    double t3 = MPI_Wtime();

    // 5) Reduccion del minimo global
    double global = 0;
    MPI_Reduce(&local, &global, 1, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);
    double t4 = MPI_Wtime();

    // Tiempos por proceso -> el tiempo de computo real es el del proceso mas lento
    double t_comp = t3 - t2, t_comp_max = 0, t_comp_min = 0;
    MPI_Reduce(&t_comp, &t_comp_max, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&t_comp, &t_comp_min, 1, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        double d_min = std::sqrt(global);
        double total = t4 - t0;
        double t_bcast = t1 - t0;
        double t_ab = t2 - t1;
        double t_reduce = t4 - t3; // incluye la espera a los procesos mas lentos
        std::printf("=== Deteccion de obstaculo LIDAR (MPI, %d procesos, reparto %s) ===\n", P, ciclico ? "ciclico" : "bloque");
        std::printf("Puntos analizados: %d\n", n);
        std::printf("Centroide: (%.2f, %.2f, %.2f) m\n", centroide.x, centroide.y, centroide.z);
        std::printf("Bounding box: ancho=%.2f m, alto=%.2f m, profundidad=%.2f m\n",
                    caja.max_x - caja.min_x, caja.max_y - caja.min_y, caja.max_z - caja.min_z);
        std::printf("Punto mas cercano al parachoques: %.2f m\n", mas_cercano);
        std::printf("Distancia minima entre puntos: %.4f m -> objeto %s\n",
                    d_min, (d_min < UMBRAL_SOLIDO ? "SOLIDO (frenar)" : "DIFUSO (posible ruido/lluvia)"));
        std::printf("\n--- Tiempos (MPI_Wtime) ---\n");
        std::printf("Bcast nube:              %.4f s\n", t_bcast);
        std::printf("Tareas A+B (rank 0):     %.4f s\n", t_ab);
        std::printf("Par mas cercano (rank0): %.4f s  (min %.4f / max %.4f entre procesos)\n", t_comp, t_comp_min, t_comp_max);
        std::printf("Reduce + espera:         %.4f s\n", t_reduce);
        std::printf("TOTAL (sin generacion):  %.4f s\n", total);
        std::printf("Desequilibrio (max/min computo): %.2f\n", t_comp_min > 0 ? t_comp_max / t_comp_min : 0.0);
        // Linea para copiar al CSV: estrategia,P,n,total,bcast,comp_max,comp_min,reduce,dmin
        std::printf("CSV,%s,%d,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n", ciclico ? "ciclico" : "bloque", P, n,
                    total, t_bcast, t_comp_max, t_comp_min, t_reduce, d_min);
    }
    MPI_Finalize();
    return 0;
}
