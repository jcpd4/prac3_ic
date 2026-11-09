#!/usr/bin/env bash
# Caracterizacion de la arquitectura de un nodo -> resultados/sistema_<host>.txt
source "$(dirname -- "$0")/comun.sh"
OUT="$RES/sistema_$(hostname).txt"
{
  echo "### hostname: $(hostname)   IP: $(hostname -I 2>/dev/null)"
  echo "### lscpu";               lscpu 2>/dev/null || sysctl -a 2>/dev/null | grep -E 'machdep.cpu.brand|hw.ncpu|hw.physicalcpu|cachesize|memsize'
  echo; echo "### caches (getconf)"; getconf -a 2>/dev/null | grep -i CACHE
  echo; echo "### memoria";        free -h 2>/dev/null
  echo; echo "### SO";             uname -a; cat /etc/os-release 2>/dev/null | head -2
  echo; echo "### red";            ip -br addr 2>/dev/null; ethtool $(ip route 2>/dev/null | awk '/default/{print $5; exit}') 2>/dev/null | grep -i speed
  echo; echo "### MPI";            mpirun --version 2>/dev/null | head -1
  echo; echo "### compilador";     g++ --version | head -1
} > "$OUT" 2>&1
echo "-> $OUT"; head -30 "$OUT"
