// Tarea 2 - "Hola mundo" MPI (codigo del enunciado)
#define _GNU_SOURCE
#include <stdio.h>
#include "mpi.h"
#include <unistd.h>
#include <stdlib.h>
#include <sched.h>

int main(int argc, char *argv[]) {
    int numprocs, rank, namelen;
    char processor_name[MPI_MAX_PROCESSOR_NAME];

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Get_processor_name(processor_name, &namelen);
#ifdef __linux__
    int cpu = sched_getcpu();
#else
    int cpu = -1; /* sched_getcpu no existe en macOS */
#endif
    printf(" PID: %d Hello from process %d out of %d on %s processor %d \n",
           getppid(), rank, numprocs, processor_name, cpu);
    if (rank == 2) {
        printf("¡Hola! desde el procesador %d\n", rank);
    }
    MPI_Finalize();
    return 0;
}
