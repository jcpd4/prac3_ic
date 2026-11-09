#!/usr/bin/env bash
# =============================================================================
#  Practica 3 IC - TODO EN UNO
#
#  Uso (en el PC maestro):
#     git clone https://github.com/jcpd4/prac3_ic.git ~/prac3_ic
#     cd ~/prac3_ic && ./hazlo_todo.sh
#
#  Solo pregunta: las IPs de los otros PCs y la contrasena de cada PC (una vez).
#  Hace: comprobaciones, fichero hosts, claves SSH, MNIST, copia+compilacion en
#  todos los PCs, hola mundo (Tarea 2), info del sistema, Tarea 0, gprof (Tarea 1),
#  calibra las tallas segun el tiempo disponible, Tarea 3, Tarea 4, graficas y
#  sube los resultados a GitHub (o los deja en un zip).
#
#  Si se corta (Ctrl+C, error...), volved a lanzarlo: SALTA los pasos ya hechos.
#  Para repetir un paso: borrad su marca en resultados/.hecho_<paso>
#  Variables opcionales:  MINUTOS=35 (tiempo total para las Tareas 3+4)
#                         MPI_EXTRA="--oversubscribe" (opciones extra para mpirun)
# =============================================================================
set -uo pipefail
RAIZ="$(cd -- "$(dirname -- "$0")" && pwd)"
cd "$RAIZ"
export HOSTS="$RAIZ/hosts"
export RES="$RAIZ/resultados"
mkdir -p "$RES" "$RAIZ/graficas"
MINUTOS=${MINUTOS:-35}
export MPI_EXTRA="${MPI_EXTRA:-}"
LOGT="$RES/hazlo_todo.log"
T_INI=$(date +%s)

verde() { printf "\n\033[1;32m==> %s\033[0m   (%s)\n" "$*" "$(date +%H:%M:%S)" | tee -a "$LOGT"; }
aviso() { printf "\033[1;33m[!] %s\033[0m\n" "$*" | tee -a "$LOGT"; }
error() { printf "\033[1;31m[X] %s\033[0m\n" "$*" | tee -a "$LOGT"; }
hecho() { [ -f "$RES/.hecho_$1" ]; }
marcar() { touch "$RES/.hecho_$1"; }
mi_ip() { hostname -I 2>/dev/null | awk '{print $1}' || true; }
nucleos_fisicos() {   # nucleos fisicos de ESTE pc
    local c; c=$(lscpu -p=core,socket 2>/dev/null | grep -v '^#' | sort -u | wc -l)
    [ "${c:-0}" -gt 0 ] 2>/dev/null && echo "$c" || nproc 2>/dev/null || sysctl -n hw.physicalcpu
}
# Comando mpirun con las opciones del cluster
MPIRUN() { mpirun -mca plm_rsh_no_tree_spawn 1 --hostfile "$HOSTS" --map-by node ${MPI_EXTRA:-} "$@"; }

# ---------------------------------------------------------------- 1. herramientas
verde "1/12 Comprobando herramientas"
FALTAN=""
for c in mpirun mpic++ mpicc g++ make; do command -v $c >/dev/null || FALTAN="$FALTAN $c"; done
if [ -n "$FALTAN" ]; then
    aviso "Faltan:$FALTAN. Intento instalar (pedira la contrasena de sudo)..."
    sudo apt-get install -y openmpi-bin libopenmpi-dev g++ make rsync || { error "No se pudo instalar MPI. Pedid ayuda al profesor."; exit 1; }
fi
command -v gprof >/dev/null || aviso "No hay gprof: se saltara la Tarea 1 (sudo apt install binutils)"
mpirun --version | head -1

# ---------------------------------------------------------------- 2. fichero hosts
verde "2/12 Fichero hosts"
if [ ! -s "$HOSTS" ]; then
    MIIP=$(mi_ip)
    echo "IP de ESTE pc (maestro): ${MIIP:-desconocida}"
    echo "En cada uno de los OTROS pcs ejecutad:  hostname -I   (la primera IP)"
    read -r -p "Escribe las IPs de los OTROS pcs separadas por espacios: " OTRAS
    [ -z "${MIIP}" ] && read -r -p "No detecto tu IP; escribela: " MIIP
    echo "${MIIP} slots=$(nucleos_fisicos)" > "$HOSTS.tmp"
    for ip in $OTRAS; do echo "$ip slots=?" >> "$HOSTS.tmp"; done
    mv "$HOSTS.tmp" "$HOSTS"
fi
chmod 600 "$HOSTS"
NODOS=$(grep -v '^\s*#' "$HOSTS" | awk 'NF{print $1}')
MIIP=$(head -1 "$HOSTS" | awk '{print $1}')

# ---------------------------------------------------------------- 3. SSH
verde "3/12 Claves SSH (pedira la contrasena de cada PC UNA vez)"
[ -f ~/.ssh/id_rsa ] || ssh-keygen -t rsa -N "" -f ~/.ssh/id_rsa -q
for h in $NODOS; do
    [ "$h" = "$MIIP" ] && continue
    if ssh -o BatchMode=yes -o ConnectTimeout=5 -o StrictHostKeyChecking=no "$h" true 2>/dev/null; then
        echo "$h: ya entra sin contrasena"
    else
        ssh-copy-id -o StrictHostKeyChecking=no "$h" || { error "No se pudo copiar la clave a $h (¿IP bien? ¿mismo usuario?)"; exit 1; }
    fi
done
# Rellenar los slots de los nodos remotos con sus nucleos fisicos
for h in $NODOS; do
    if grep -q "^$h slots=?" "$HOSTS"; then
        n=$(ssh "$h" "lscpu -p=core,socket 2>/dev/null | grep -v '^#' | sort -u | wc -l" 2>/dev/null)
        [ "${n:-0}" -gt 0 ] 2>/dev/null || n=$(ssh "$h" nproc)
        sed -i "s/^$h slots=?/$h slots=$n/" "$HOSTS"
    fi
done
echo "--- hosts ---"; cat "$HOSTS"
TOTAL=$(awk -F'slots=' 'NF>1{s+=$2} END{print s+0}' "$HOSTS")
NN=$(echo "$NODOS" | wc -w)
echo "Cluster: $NN PCs, $TOTAL nucleos en total"
[ "$NN" -lt 3 ] && aviso "El enunciado pide minimo 3 PCs (teneis $NN)."

# Lista de P a medir: 2,3,4,6,8,12,16,24,32... <= TOTAL, y siempre TOTAL
PROCS=""
for p in 2 3 4 6 8 12 16 24 32 48 64; do [ "$p" -le "$TOTAL" ] && PROCS="$PROCS $p"; done
echo " $PROCS " | grep -q " $TOTAL " || PROCS="$PROCS $TOTAL"
export PROCS="${PROCS# }"
echo "Se mediran P = $PROCS"

# ---------------------------------------------------------------- 4. MNIST
verde "4/12 Dataset MNIST"
if [ -s tarea4/knn2/build/train/train-images-idx3-ubyte ] && [ -s tarea4/knn2/build/test/t10k-labels-idx1-ubyte ]; then
    echo "Ya descargado"
else
    (cd tarea4/knn2 && ./get_dataset.sh) || { error "No se pudo descargar MNIST (¿hay internet?). Copiadlo a tarea4/knn2/build/{train,test}/"; exit 1; }
fi

# ---------------------------------------------------------------- 5. copiar y compilar
if ! hecho setup; then
    verde "5/12 Copiando y compilando en todos los PCs"
    ./scripts/setup_cluster.sh 2>&1 | tee -a "$LOGT"
    marcar setup
else verde "5/12 (ya hecho) copia y compilacion"; fi
make -s -C tarea2 >/dev/null 2>&1

# ---------------------------------------------------------------- 6. hola mundo + autodeteccion de problemas de red
verde "6/12 Tarea 2: hola mundo MPI en el cluster"
NP_HELLO=$(( TOTAL < 6 ? TOTAL : 6 )); [ "$NP_HELLO" -lt "$NN" ] && NP_HELLO=$NN
probar_hello() { (cd tarea2 && timeout 60 mpirun -mca plm_rsh_no_tree_spawn 1 --hostfile "$HOSTS" --map-by node ${MPI_EXTRA:-} -np "$NP_HELLO" ./hello_mpi) > "$RES/tarea2_hello.txt" 2>&1; }
IFACE=$(ip route 2>/dev/null | awk '/default/{print $5; exit}')
if ! probar_hello; then
    aviso "Fallo el primer intento. Pruebo opciones alternativas..."
    cat "$RES/tarea2_hello.txt" | tail -5
    for extra in "--oversubscribe" "--mca btl_tcp_if_include $IFACE --mca oob_tcp_if_include $IFACE" \
                 "--oversubscribe --mca btl_tcp_if_include $IFACE --mca oob_tcp_if_include $IFACE"; do
        export MPI_EXTRA="${MPI_EXTRA:-} $extra"
        echo "Probando MPI_EXTRA=$MPI_EXTRA"
        probar_hello && break
        export MPI_EXTRA=""
    done
fi
cat "$RES/tarea2_hello.txt"
if grep -q "Hello from process" "$RES/tarea2_hello.txt"; then
    DIST=$(grep -o "on [^ ]* processor" "$RES/tarea2_hello.txt" | sort -u | wc -l)
    echo "Procesos repartidos en $DIST PCs distintos"
    echo "MPI_EXTRA=\"${MPI_EXTRA:-}\"" > "$RES/mpi_extra.txt"
else
    error "MPI no funciona en el cluster. Mirad la tabla 'Si algo falla' de PASO_A_PASO.md"
    error "Sigo SOLO en este PC para no perder la sesion (los datos valdran, pero no seran multicomputador)."
    rm -f "$HOSTS.cluster"; mv "$HOSTS" "$HOSTS.cluster"
    MIS=$(nucleos_fisicos); echo "localhost slots=$MIS" > "$HOSTS"; TOTAL=$MIS
    PROCS=""; for p in 2 3 4 6 8 12 16; do [ "$p" -le "$TOTAL" ] && PROCS="$PROCS $p"; done; export PROCS="${PROCS# }"
fi
printf "\a"
echo
echo "############################################################################"
echo "#  ENSENAD AHORA ESTA SALIDA AL PROFESOR y haced una CAPTURA DE PANTALLA    #"
echo "#  (guardadla luego como graficas/tarea2_hello.png).                         #"
echo "#  Pulsad ENTER para seguir (sigue solo en 3 minutos).                       #"
echo "############################################################################"
read -r -t 180 _ || true

# ---------------------------------------------------------------- 7. info del sistema
if ! hecho info; then
    verde "7/12 Informacion del sistema de cada PC"
    for h in $NODOS; do
        if [ "$h" = "$MIIP" ] || [ "$h" = localhost ]; then ./scripts/info_sistema.sh >/dev/null
        else ssh "$h" "cd '$RAIZ' && ./scripts/info_sistema.sh >/dev/null && cat resultados/sistema_\$(hostname).txt" > "$RES/sistema_$h.txt" 2>/dev/null || true; fi
    done
    ls "$RES"/sistema_*; marcar info
else verde "7/12 (ya hecho) info del sistema"; fi

# ---------------------------------------------------------------- 8. Tarea 0
if ! hecho t0; then verde "8/12 Tarea 0"; ./scripts/bench_tarea0.sh 2>/dev/null | tail -16; marcar t0
else verde "8/12 (ya hecho) Tarea 0"; fi

# ---------------------------------------------------------------- 9. Tarea 1 gprof
if ! hecho t1 && command -v gprof >/dev/null; then verde "9/12 Tarea 1: gprof"; ./scripts/gprof.sh 2>&1 | tail -45; marcar t1
else verde "9/12 (ya hecho o sin gprof) Tarea 1"; fi

# ---------------------------------------------------------------- 10. calibracion
verde "10/12 Calibrando tallas para que las Tareas 3 y 4 quepan en ~$MINUTOS min"
make -s -C tarea3 all >/dev/null; make -s -C tarea4/knn2 all >/dev/null
SUMINV=$(echo "$PROCS" | awk '{s=0; for(i=1;i<=NF;i++) s+=1/$i; print s}')
# LIDAR: coste ~ n^2
TL=$( (cd tarea3 && ./lidar_seq 30000 1.5 15.0 42) | awk -F, '/^CSV/{print $5}')
# kNN: coste del ajuste ~ nCV^2, test ~ NTRAIN*NTEST (20000*1000)
TK=$( (cd tarea4/knn2/build && ./knn_seq 1000 20000 1000 1 10 5) | awk -F, '/^CSV/{print $12" "$13}')
TK_TUN=$(echo "$TK" | awk '{print $1}'); TK_TEST=$(echo "$TK" | awk '{print $2}')
echo "Referencias: lidar_seq(30000)=${TL}s  knn ajuste(nCV=1000)=${TK_TUN}s  test(20000x1000)=${TK_TEST}s"
PRES3=$(awk -v m="$MINUTOS" 'BEGIN{print m*60*0.25}')   # 25% del tiempo a Tarea 3
PRES4=$(awk -v m="$MINUTOS" 'BEGIN{print m*60*0.75}')   # 75% a Tarea 4
# coste de una bateria = sum_tallas seq(n) * (1 + 1.25 * nEstrategias * sum(1/P))
elegir() {  # $1=tiempo_ref $2=talla_ref $3=presupuesto $4=nEstrategias $5=coste_fijo_por_talla ; candidatos en stdin
    awk -v t="$1" -v r="$2" -v pres="$3" -v ne="$4" -v fijo="$5" -v si="$SUMINV" '
      { c=0; for(i=1;i<=NF;i++){ s=t*($i/r)^2 + fijo; c+= s*(1+1.25*ne*si) } if (c<=pres) best=$0 }
      END{ print best }'
}
TALLAS=$(printf "20000 40000 60000\n30000 60000 100000\n50000 100000 150000\n50000 100000 200000\n100000 200000 300000\n" | elegir "$TL" 30000 "$PRES3" 2 0)
NCVS=$(printf "500 1000 2000\n1000 2000 3000\n1000 2000 4000\n2000 4000 6000\n2000 4000 8000\n4000 8000 12000\n4000 8000 16000\n" | elegir "$TK_TUN" 1000 "$PRES4" 3 "$TK_TEST")
[ -z "$TALLAS" ] && TALLAS="20000 40000 60000"
[ -z "$NCVS" ] && NCVS="500 1000 2000"
export TALLAS NCVS
echo "Tarea 3 -> n = $TALLAS   |   Tarea 4 -> nCV = $NCVS (NTRAIN=20000, NTEST=1000)   |   P = $PROCS"
echo "TALLAS=\"$TALLAS\" NCVS=\"$NCVS\" PROCS=\"$PROCS\"" > "$RES/parametros.txt"

# ---------------------------------------------------------------- 11. Tareas 3 y 4
if ! hecho t3; then verde "11/12 Tarea 3 (no useis los PCs del cluster mientras mide)"
    ./scripts/bench_tarea3.sh 2>&1 | tail -30; marcar t3
else verde "11/12 (ya hecha) Tarea 3"; fi
if ! hecho t4; then verde "11/12 Tarea 4 (no useis los PCs del cluster mientras mide)"
    ./scripts/bench_tarea4.sh 2>&1 | tail -40; marcar t4
else verde "11/12 (ya hecha) Tarea 4"; fi

# ---------------------------------------------------------------- 12. graficas y subida
verde "12/12 Graficas y subida de resultados"
PY=python3
if ! $PY -c "import matplotlib" 2>/dev/null; then
    $PY -m pip install --user -q matplotlib 2>/dev/null || $PY -m pip install --user -q --break-system-packages matplotlib 2>/dev/null || \
    { $PY -m venv "$HOME/.venv_prac3" 2>/dev/null && "$HOME/.venv_prac3/bin/pip" install -q matplotlib 2>/dev/null && PY="$HOME/.venv_prac3/bin/python"; }
fi
$PY scripts/graficas.py 2>&1 | tail -5 || aviso "No se pudieron generar las graficas aqui: hacedlo en casa (python3 scripts/graficas.py)"
cp "$HOSTS" "$RES/hosts_usado.txt" 2>/dev/null

(cd "$RAIZ" && zip -qr "$HOME/resultados_prac3.zip" resultados graficas) 2>/dev/null && echo "Copia de seguridad: ~/resultados_prac3.zip"
git add resultados graficas >/dev/null 2>&1
git -c user.name="prac3" -c user.email="prac3@localhost" commit -qm "Resultados de clase $(date +%F)" >/dev/null 2>&1 || true
echo "Subiendo a GitHub (usuario: jcpd4, contrasena: EL TOKEN)..."
if git push; then verde "Resultados subidos a GitHub"
else aviso "No se pudo hacer push. Subid ~/resultados_prac3.zip a GitHub (Add file > Upload files) o llevaoslo en USB/correo."; fi

MIN=$(( ($(date +%s) - T_INI) / 60 ))
verde "TERMINADO en $MIN min. Falta solo: captura del hola mundo -> graficas/tarea2_hello.png y rellenar MEMORIA.md en casa."
