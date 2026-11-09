// Tarea 0 - Problema de los vectores dinamicos (codigo del enunciado, SIZE configurable)
// Uso: ./vector [SIZE]
#include <iostream>
#include <vector>
#include <ctime>
#include <cstdlib>
using namespace std;
int main(int argc, char** argv)
{
    long SIZE = (argc > 1) ? atol(argv[1]) : 1000000;
    // A: push_back sobre vector vacio
    clock_t begin = clock();
    std::vector<int> a;
    for (long i = 0; i < SIZE; i++) a.push_back(i);
    double aTime = double(clock() - begin) / CLOCKS_PER_SEC;
    cout << "A (push_back)   Elapsed time: " << aTime << "s" << endl;
    // B: vector reservado al tamano final
    begin = clock();
    std::vector<int> b(SIZE);
    for (long i = 0; i < SIZE; i++) b[i] = i;
    double bTime = double(clock() - begin) / CLOCKS_PER_SEC;
    cout << "B (prealocado)  Elapsed time: " << bTime << "s" << endl;
    cout << "Gain: " << aTime / bTime << endl;
    cout << "CSV,vector," << SIZE << "," << aTime << "," << bTime << "," << aTime / bTime << endl;
    return 0;
}
