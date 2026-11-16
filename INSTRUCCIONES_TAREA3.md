# Tarea 3: qué hacer en el ordenador del aula

> **Resumen:** el código ya está hecho y probado (en un Mac, un solo PC). Solo falta **medirlo en el
> clúster del aula (mínimo 3 PCs)** y subir los resultados. Son unos 20-25 minutos.

## Qué hay ya hecho

| Fichero | Qué es |
|---|---|
| `tarea3/lidar_seq.cpp` | Nuestra práctica 2 (LIDAR) tal cual: versión **secuencial** |
| `tarea3/lidar_mpi.cpp` | La misma práctica 2 **paralelizada con MPI** |
| `scripts/bench_tarea3.sh` | Script que ejecuta todas las pruebas y guarda los tiempos en un CSV |
| `scripts/graficas.py` | Genera las gráficas y tablas a partir del CSV |
| `MEMORIA.md` §5 | Sección de la memoria de la Tarea 3, ya redactada con los datos del Mac |
| `resultados/prueba_local_mac/` | Resultados de la prueba en el Mac (**no tocar**, solo de referencia) |

**Qué hace la versión MPI, en una frase:** el proceso 0 genera la nube de puntos y se la envía a
todos (`MPI_Bcast`). Cada proceso busca el par de puntos más cercano solo en *sus* filas, y al final
`MPI_Reduce(MPI_MIN)` junta los mínimos en el proceso 0. Hay dos formas de repartir las filas:
`bloque` (trozos seguidos, queda desequilibrado) y `ciclico` (filas intercaladas, equilibrado).

---

## Paso 1. Descargar el repositorio (en el PC "maestro")

Elige un PC como **maestro** (el que lanza todo). Abre una terminal:

```bash
cd ~
git clone https://github.com/jcpd4/prac3_ic.git
cd ~/prac3_ic
```

(Si ya lo tenías clonado: `cd ~/prac3_ic && git pull`.)

Comprueba que hay MPI: `mpirun --version` y `mpic++ --version`. Si no aparece, avisa al profesor
(o, si tienes `sudo`: `sudo apt install openmpi-bin libopenmpi-dev`).

## Paso 2. Prueba rápida en un solo PC (2 min)

Antes de montar el clúster, comprueba que compila y que funciona:

```bash
cd ~/prac3_ic/tarea3
make
./lidar_seq 30000 1.5 15.0 42
mpirun -np 4 ./lidar_mpi 30000 1.5 15.0 42 ciclico
```

En las dos salidas tiene que aparecer **la misma** `Distancia minima entre puntos`. Si es así, todo está bien.
Vuelve a la raíz: `cd ~/prac3_ic`

## Paso 3. Montar el clúster (≥ 3 PCs, unos 10 min)

Todos los PCs tienen que tener la sesión abierta **con el mismo usuario**.

1. En **cada** PC, apunta su IP y su número de núcleos:
   ```bash
   hostname -I     # la primera IP
   nproc           # número de núcleos
   ```
2. En el **maestro**, crea el fichero `hosts` (una línea por PC, **el maestro el primero**):
   ```bash
   cp tarea2/hosts.example hosts
   nano hosts
   ```
   Que quede así (con vuestras IPs y núcleos):
   ```
   192.168.X.A slots=4
   192.168.X.B slots=4
   192.168.X.C slots=4
   ```
   Guarda con `Ctrl+O`, `Enter` y sal con `Ctrl+X`.
3. Copia el código a los demás PCs y compílalo en todos (pide la contraseña **una vez por PC**):
   ```bash
   ./scripts/setup_cluster.sh
   ```
   Tiene que acabar con `COMPILADO OK en <pc>` para cada PC.
4. Comprueba que funciona entre PCs (Tarea 2, el "hola mundo"):
   ```bash
   mpirun -mca plm_rsh_no_tree_spawn 1 --hostfile hosts --map-by node -np 6 tarea2/hello_mpi
   ```
   Tienen que salir mensajes de **varios PCs distintos**.

## Paso 4. Ejecutar las pruebas de la Tarea 3 (10-15 min)

```bash
cd ~/prac3_ic
TALLAS="50000 100000 200000" PROCS="2 3 4 6 8 12" REPS=3 ./scripts/bench_tarea3.sh
```

- Usa el fichero `hosts` automáticamente, así que los procesos se reparten entre los PCs.
- `PROCS` no debe pasar del **total de núcleos** de todos los PCs juntos (suma de los `slots`).
  Si tenéis, por ejemplo, 3 PCs × 4 núcleos = 12, `"2 3 4 6 8 12"` está bien.
- **¿Vais justos de tiempo?** Usad `REPS=1` y/o quitad `200000` de las tallas.
- Al acabar enseña una tabla y deja los datos en `resultados/tarea3.csv`.

## Paso 5. Gráficas (opcional en clase, se puede hacer en casa)

```bash
pip install --user matplotlib     # solo si no está instalado
python3 scripts/graficas.py
```

Genera `graficas/tarea3_*.png` (sustituyen a las del Mac) y `resultados/tablas.md`.

## Paso 6. Subir los resultados a GitHub

```bash
cd ~/prac3_ic
git add resultados graficas
git commit -m "Resultados Tarea 3 en el cluster del aula"
git push
```

Al hacer `git push` pide **usuario** (`jcpd4`) y **contraseña**. La contraseña **NO** es la de
GitHub: es un **token**, y te lo tiene que pasar el dueño del repositorio.

**Si no funciona el push (plan B):**
```bash
cd ~/prac3_ic && zip -r ~/resultados_tarea3.zip resultados graficas
```
y manda `~/resultados_tarea3.zip` por correo o llévatelo en un USB.

---

## Si algo falla

| Síntoma | Solución |
|---|---|
| `There are not enough slots available` | `export MPI_EXTRA="--oversubscribe"` y repite el comando |
| Se queda colgado sin hacer nada | Hay varias tarjetas de red. Mira cuál es la buena con `ip -br addr` (p. ej. `eno1`) y luego: `export MPI_EXTRA="--mca btl_tcp_if_include eno1 --mca oob_tcp_if_include eno1"` |
| `Permission denied (publickey)` | `ssh-copy-id <IP del PC que falla>` |
| `orted: command not found` | Ese PC no tiene MPI en el PATH: `export MPI_EXTRA="--prefix /usr"` |
| `lidar_mpi: No such file` en otro PC | Repite `./scripts/setup_cluster.sh` |
| Cambiaste algo del código | Repite `./scripts/setup_cluster.sh` para copiarlo a todos los PCs |

## Qué apuntar para la memoria

- **IP, modelo de CPU y núcleos** de cada PC (`lscpu | head -20`) → MEMORIA §1.1.
- Una **captura** del hola mundo ejecutándose en varios PCs (Tarea 2).
- Con los datos nuevos, en `MEMORIA.md` §5.5 y §5.6 hay que actualizar las cifras (ahora son las del Mac).
  Las explicaciones siguen valiendo; lo único nuevo es que en el clúster el `MPI_Bcast` va por la red,
  así que el "% tiempo MPI" será algo mayor para tallas pequeñas.
