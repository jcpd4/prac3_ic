# Se compila con -O0 para que el compilador no elimine/optimice los bucles
# y se vea el efecto real de memoria/cache.
CXX = g++
CXXFLAGS = -O0 -std=c++17
all: vector matriz
vector: vector.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<
matriz: matriz.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<
clean:
	rm -f vector matriz
