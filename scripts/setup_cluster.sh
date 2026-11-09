#!/usr/bin/env bash
# Tarea 2 - Prepara el cluster desde el PC MAESTRO (el primero del fichero hosts):
#   1) crea la clave SSH (si no existe) y la copia al resto de PCs
#   2) copia el repositorio (codigo + MNIST) a la MISMA ruta en todos los PCs
#   3) compila en cada PC
# Uso: ./scripts/setup_cluster.sh       (con el fichero hosts ya creado en la raiz)
set -uo pipefail
source "$(dirname -- "$0")/comun.sh"
[ -f "$HOSTS" ] || { echo "Falta $HOSTS (copiad tarea2/hosts.example y poned las IPs)"; exit 1; }
chmod 600 "$HOSTS"
[ -f ~/.ssh/id_rsa ] || ssh-keygen -t rsa -N "" -f ~/.ssh/id_rsa
MI_IP=$(hostname -I 2>/dev/null | awk '{print $1}')
for h in $(grep -v '^\s*#' "$HOSTS" | awk 'NF{print $1}'); do
    echo "==================== $h ===================="
    if [ "$h" = "$MI_IP" ] || [ "$h" = "$(hostname)" ] || [ "$h" = "localhost" ]; then
        echo "(maestro, se compila en local)"; (cd "$RAIZ" && make -s -C tarea2 && make -s -C tarea3 all && make -s -C tarea4/knn2 all); continue
    fi
    # Equivale a copiar id_rsa.pub como ~/.ssh/authorized_keys en el otro PC
    ssh-copy-id -o StrictHostKeyChecking=no "$h" || { echo "!! No se pudo copiar la clave a $h"; continue; }
    ssh "$h" "mkdir -p '$RAIZ'"
    rsync -az --exclude '.git' "$RAIZ/" "$h:$RAIZ/" || scp -rq "$RAIZ/." "$h:$RAIZ/"
    ssh "$h" "cd '$RAIZ' && make -s -B -C tarea2 && make -s -B -C tarea3 all && make -s -B -C tarea4/knn2 all && chmod -R o+x tarea2 tarea3 tarea4/knn2/build && echo COMPILADO OK en \$(hostname)"
done
