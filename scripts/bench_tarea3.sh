#!/usr/bin/env bash
# Tarea 3 - Mide la P2 secuencial y MPI (bloque y ciclico) para varias tallas y P.
# Resultado: resultados/tarea3.csv (+ log completo en resultados/tarea3.log)
# Uso: ./scripts/bench_tarea3.sh            (tallas y P por defecto)
#      TALLAS="50000 100000" PROCS="2 3 6" ./scripts/bench_tarea3.sh
set -uo pipefail
source "$(dirname -- "$0")/comun.sh"
TALLAS=${TALLAS:-"50000 100000 200000"}
PROCS=${PROCS:-"2 3 4 6 8 12"}
REPS=${REPS:-1}
CSV="$RES/tarea3.csv"; LOG="$RES/tarea3.log"
[ -f "$CSV" ] || echo "estrategia,P,n,t_total,t_bcast,t_comp_max,t_comp_min,t_reduce,dmin" > "$CSV"
cd "$RAIZ/tarea3" && make -s all
for n in $TALLAS; do
  for r in $(seq "$REPS"); do
    echo ">>> secuencial n=$n" | tee -a "$LOG"
    ./lidar_seq "$n" 1.5 15.0 42 | tee -a "$LOG" | grep '^CSV' | cut -d, -f2- >> "$CSV"
    for p in $PROCS; do
      for e in bloque ciclico; do
        echo ">>> MPI $e P=$p n=$n" | tee -a "$LOG"
        mpi_cmd "$p" ./lidar_mpi "$n" 1.5 15.0 42 "$e" | tee -a "$LOG" | grep '^CSV' | cut -d, -f2- >> "$CSV"
      done
    done
  done
done
echo "Hecho -> $CSV"; column -s, -t < "$CSV" | tail -40
