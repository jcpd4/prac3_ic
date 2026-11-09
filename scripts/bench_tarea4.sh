#!/usr/bin/env bash
# Tarea 4 - Mide el kNN secuencial y MPI (datos, tareas, especulativo).
# Resultado: resultados/tarea4.csv (+ log completo en resultados/tarea4.log)
# Uso: ./scripts/bench_tarea4.sh
#      NCVS="2000 4000" PROCS="2 3 6" ./scripts/bench_tarea4.sh
set -uo pipefail
source "$(dirname -- "$0")/comun.sh"
NCVS=${NCVS:-"2000 4000 8000"}     # talla del problema de ajuste (muestras de validacion cruzada)
NTRAIN=${NTRAIN:-20000}            # muestras de entrenamiento para la evaluacion final
NTEST=${NTEST:-1000}               # muestras de test
MINK=${MINK:-1}; MAXK=${MAXK:-10}; FOLDS=${FOLDS:-5}
PROCS=${PROCS:-"2 3 4 6 8 12"}
MODOS=${MODOS:-"datos tareas especulativo"}
CSV="$RES/tarea4.csv"; LOG="$RES/tarea4.log"
[ -f "$CSV" ] || echo "modo,P,nCV,nTrainFinal,nTest,folds,minK,maxK,bestK,testAcc,t_tuning,t_test,t_total,t_comm,reinicios" > "$CSV"
cd "$RAIZ/tarea4/knn2" && make -s all
cd build
for ncv in $NCVS; do
  echo ">>> secuencial nCV=$ncv" | tee -a "$LOG"
  ./knn_seq "$ncv" "$NTRAIN" "$NTEST" "$MINK" "$MAXK" "$FOLDS" | grep -E '^(K:|Best|Test|CSV|Carga)' | tee -a "$LOG" | grep '^CSV' | cut -d, -f2- >> "$CSV"
  for p in $PROCS; do
    for m in $MODOS; do
      [ "$m" = especulativo ] && [ "$p" -lt 2 ] && continue
      echo ">>> MPI $m P=$p nCV=$ncv" | tee -a "$LOG"
      mpi_cmd "$p" ./knn_mpi "$m" "$ncv" "$NTRAIN" "$NTEST" "$MINK" "$MAXK" "$FOLDS" | tee -a "$LOG" | grep '^CSV' | cut -d, -f2- >> "$CSV"
    done
  done
done
echo "Hecho -> $CSV"; column -s, -t < "$CSV" | tail -60
