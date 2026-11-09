# Tarea 3 - Practica 2 secuencial vs MPI
CXX    = g++
MPICXX = mpic++
CXXFLAGS = -O2 -std=c++17 -Wall

all: lidar_seq lidar_mpi

lidar_seq: lidar_seq.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

lidar_mpi: lidar_mpi.cpp
	$(MPICXX) $(CXXFLAGS) -o $@ $<

# Versiones para gprof (Tarea 1): -O0 y -O2 con -pg
lidar_prof_O0: lidar_seq.cpp
	$(CXX) -O0 -g -pg -std=c++17 -fno-inline -o $@ $<
lidar_prof_O2: lidar_seq.cpp
	$(CXX) -O2 -g -pg -std=c++17 -fno-inline -o $@ $<

clean:
	rm -f lidar_seq lidar_mpi lidar_prof_O0 lidar_prof_O2 gmon.out
