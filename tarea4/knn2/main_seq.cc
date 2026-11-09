// Practica 3 - Tarea 4. Version SECUENCIAL de referencia PARAMETRIZADA.
//
// Es exactamente el main.cc original (usa las mismas clases KNN y
// CrossValidation sin tocar) pero con el tamano del problema por linea de
// comandos, para poder medir tallas pequenas en el tiempo de clase.
// El main.cc original equivale a: ./knn_seq 20000 60000 10000 1 10 5
//
// Uso: ./knn_seq <nCV> <nTrainFinal> <nTest> <minK> <maxK> <folds> [dirDatos]
#include "sample.h"
#include "knn.h"
#include "mnist.h"
#include "crossValidation.h"
#include "timer.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace knn;
using namespace mnist;
using namespace crossValidation;

static int fineTuning(unsigned minK, unsigned maxK, const std::vector<TSample>& dataset, unsigned folds) {
    CrossValidation cv = CrossValidation::NFold(dataset, folds);
    unsigned bestK = minK;
    double bestAccuracy = 0.0;
    for (unsigned k = minK; k <= maxK; k++) {
        double start = elapsedTime();
        double accuracy = cv.validate(k);
        std::cout << "K: " << k << " Accuracy: " << accuracy * 100 << "% completed in " << elapsedTime() - start << " seconds" << std::endl;
        if (accuracy > bestAccuracy) { bestAccuracy = accuracy; bestK = k; }
    }
    return bestK;
}

int main(int argc, char** argv) {
    if (argc < 7) {
        std::fprintf(stderr, "Uso: %s <nCV> <nTrainFinal> <nTest> <minK> <maxK> <folds> [dirDatos]\n", argv[0]);
        return 1;
    }
    size_t nCV = std::atol(argv[1]), nTrainFinal = std::atol(argv[2]), nTest = std::atol(argv[3]);
    unsigned minK = std::atoi(argv[4]), maxK = std::atoi(argv[5]), folds = std::atoi(argv[6]);
    std::string dir = (argc > 7) ? std::string(argv[7]) + "/" : "";

    std::vector<TSample> train, test;
    double tl = elapsedTime();
    if (!loadMnist((dir + "train/train-images-idx3-ubyte").c_str(), (dir + "train/train-labels-idx1-ubyte").c_str(), train) ||
        !loadMnist((dir + "test/t10k-images-idx3-ubyte").c_str(), (dir + "test/t10k-labels-idx1-ubyte").c_str(), test)) {
        std::cerr << "No se pudo leer MNIST (ejecutad desde build/ o pasad dirDatos)" << std::endl;
        return 1;
    }
    double tLoad = elapsedTime() - tl;
    nCV = std::min(nCV, train.size()); nTrainFinal = std::min(nTrainFinal, train.size()); nTest = std::min(nTest, test.size());

    double t0 = elapsedTime();
    int k = fineTuning(minK, maxK, std::vector<TSample>(train.begin(), train.begin() + nCV), folds);
    double t1 = elapsedTime();
    std::cout << "Best k: " << k << " found in " << t1 - t0 << " seconds" << std::endl;

    std::vector<int> labels;
    KNN knnFinal(std::vector<TSample>(train.begin(), train.begin() + nTrainFinal), k);
    double accuracy = knnFinal.classifyAndEvaluate(std::vector<TSample>(test.begin(), test.begin() + nTest), labels);
    double t2 = elapsedTime();
    std::cout << "Test accuracy: " << accuracy << " completed in " << t2 - t1 << " seconds" << std::endl;
    std::cout << "Carga datos: " << tLoad << " s | TOTAL (sin carga): " << t2 - t0 << " s" << std::endl;
    // modo,P,nCV,nTrainFinal,nTest,folds,minK,maxK,bestK,testAcc,t_tuning,t_test,t_total,t_comm,reinicios
    std::printf("CSV,secuencial,1,%zu,%zu,%zu,%u,%u,%u,%d,%.4f,%.4f,%.4f,%.4f,0,0\n",
                nCV, nTrainFinal, nTest, folds, minK, maxK, k, accuracy, t1 - t0, t2 - t1, t2 - t0);
    return 0;
}
