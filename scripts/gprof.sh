#!/usr/bin/env bash
# Tarea 1 - Perfilado con gprof (-O0 y -O2) del codigo de la P2 y del kNN.
# Deja los informes en resultados/gprof_*.txt
# Uso: ./scripts/gprof.sh [N_lidar] [nCV_knn]
set -euo pipefail
source "$(dirname -- "$0")/comun.sh"
N=${1:-50000}
NCV=${2:-2000}

cd "$RAIZ/tarea3"
make -s lidar_prof_O0 lidar_prof_O2
for opt in O0 O2; do
    rm -f gmon.out
    ./lidar_prof_$opt "$N" 1.5 15.0 42 > /dev/null
    gprof -b -p -q ./lidar_prof_$opt gmon.out > "$RES/gprof_lidar_$opt.txt"
    echo "-> resultados/gprof_lidar_$opt.txt"
done

cd "$RAIZ/tarea4/knn2"
make -s prof
cd build
for opt in O0 O2; do
    rm -f gmon.out
    ./knn_prof_$opt "$NCV" 10000 500 1 10 5 > /dev/null
    gprof -b -p -q ./knn_prof_$opt gmon.out > "$RES/gprof_knn_$opt.txt"
    echo "-> resultados/gprof_knn_$opt.txt"
done
echo; echo "=== Resumen (5 funciones con mas tiempo) ==="
for f in "$RES"/gprof_*.txt; do echo "--- $(basename "$f")"; sed -n '1,10p' "$f"; done
