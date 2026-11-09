# Variables comunes a todos los scripts (se carga con "source")
# RAIZ = carpeta del repositorio (debe ser la MISMA ruta en todos los PCs)
RAIZ="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
HOSTS="${HOSTS:-$RAIZ/hosts}"
MPI_EXTRA="${MPI_EXTRA:-}"
RES="${RES:-$RAIZ/resultados}"
mkdir -p "$RES"

# Comando mpirun. Si existe el fichero hosts se usa el cluster; si no, local.
#   MPI_EXTRA permite anadir opciones, p. ej.:
#   MPI_EXTRA="--oversubscribe"                         (si da error de slots)
#   MPI_EXTRA="--mca btl_tcp_if_include eno1 --mca oob_tcp_if_include eno1" (si se cuelga)
mpi_cmd() {
    local np="$1"; shift
    if [ -f "$HOSTS" ]; then
        mpirun -mca plm_rsh_no_tree_spawn 1 --hostfile "$HOSTS" --map-by node -np "$np" $MPI_EXTRA "$@"
    else
        mpirun -np "$np" $MPI_EXTRA "$@"
    fi
}
