#!/usr/bin/env bash
# Tarea 0 - Ejecuta vector y matriz con varios tamanos -> resultados/tarea0.csv
set -uo pipefail
source "$(dirname -- "$0")/comun.sh"
cd "$RAIZ/tarea0" && make -s
CSV="$RES/tarea0.csv"
echo "tipo,size,tA,tB,gain" > "$CSV"
for s in 1000 10000 100000 1000000 10000000 100000000; do ./vector $s | tee /dev/stderr | grep '^CSV' | cut -d, -f2- >> "$CSV"; done
for s in 100 250 500 1000 2000 4000 8000 12000; do ./matriz $s | tee /dev/stderr | grep '^CSV' | cut -d, -f2- >> "$CSV"; done
echo "-> $CSV"; column -s, -t < "$CSV"
