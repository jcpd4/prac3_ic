// Tarea 0 - Recorrido de matrices (codigo del enunciado, SIZE configurable)
// Se usa memoria dinamica (vector) porque con int a[SIZE][SIZE] en la pila
// los tamanos grandes (>~1400) provocan "Segmentation fault" (stack overflow).
// Uso: ./matriz [SIZE]
#include <iostream>
#include <vector>
#include <ctime>
#include <cstdlib>
using namespace std;
int main(int argc, char** argv)
{
    int SIZE = (argc > 1) ? atoi(argv[1]) : 1000;
    std::vector<int> a((size_t)SIZE * SIZE), b((size_t)SIZE * SIZE);
    // A: recorrido por COLUMNAS (a[j][i])
    clock_t begin = clock();
    for (int i = 0; i < SIZE; i++)
        for (int j = 0; j < SIZE; j++)
            a[(size_t)j * SIZE + i] = i + j;
    double aTime = double(clock() - begin) / CLOCKS_PER_SEC;
    cout << "A (por columnas) Elapsed time: " << aTime << "s" << endl;
    // B: recorrido por FILAS (b[i][j])
    begin = clock();
    for (int i = 0; i < SIZE; i++)
        for (int j = 0; j < SIZE; j++)
            b[(size_t)i * SIZE + j] = i + j;
    double bTime = double(clock() - begin) / CLOCKS_PER_SEC;
    cout << "B (por filas)    Elapsed time: " << bTime << "s" << endl;
    cout << "Gain: " << aTime / bTime << endl;
    cout << "CSV,matriz," << SIZE << "," << aTime << "," << bTime << "," << aTime / bTime << endl;
    // Evita que el compilador elimine los bucles
    volatile int sink = a[SIZE] + b[SIZE]; (void)sink;
    return 0;
}
