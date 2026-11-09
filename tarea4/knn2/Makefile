# Tarea 4 - kNN. Compila todo dentro de build/ (donde get_dataset.sh deja MNIST)
#   make            -> build/cv (original), build/knn_seq, build/knn_mpi
#   make prof       -> build/cv_prof_O0 y build/cv_prof_O2 (para gprof)
CXX    = g++
MPICXX = mpic++
CXXFLAGS = -O2 -std=c++17 -I.
LIB = crossValidation.cc mnist.cc knn.cc sample.cc

all: build/cv build/knn_seq build/knn_mpi

build:
	mkdir -p build/train build/test

build/cv: main.cc $(LIB) | build
	$(CXX) $(CXXFLAGS) -o $@ main.cc $(LIB)

build/knn_seq: main_seq.cc $(LIB) | build
	$(CXX) $(CXXFLAGS) -o $@ main_seq.cc $(LIB)

build/knn_mpi: knn_mpi.cc mnist.cc sample.cc | build
	$(MPICXX) $(CXXFLAGS) -o $@ knn_mpi.cc mnist.cc sample.cc

# gprof: se perfila knn_seq (mismo codigo que cv pero con talla configurable)
prof: build/knn_prof_O0 build/knn_prof_O2
build/knn_prof_O0: main_seq.cc $(LIB) | build
	$(CXX) -O0 -g -pg -fno-inline -std=c++17 -I. -o $@ main_seq.cc $(LIB)
build/knn_prof_O2: main_seq.cc $(LIB) | build
	$(CXX) -O2 -g -pg -fno-inline -std=c++17 -I. -o $@ main_seq.cc $(LIB)

build/tests: tests.cc $(LIB) | build
	$(CXX) $(CXXFLAGS) -o $@ tests.cc $(LIB)
test: build/tests
	./build/tests && echo "TESTS OK"

clean:
	rm -f build/cv build/knn_seq build/knn_mpi build/knn_prof_O0 build/knn_prof_O2 build/tests build/gmon.out
.PHONY: all prof test clean
