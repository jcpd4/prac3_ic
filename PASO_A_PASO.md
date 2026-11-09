# Práctica 3 (MPI) — Paso a paso para hacerla en clase (1 h 30 min)

> **Idea general:** todo el código ya está hecho y probado. En clase solo hay que
> **montar el clúster (≥3 PCs), lanzar los scripts y guardar los resultados**.
> Los scripts escriben solos los CSV en `resultados/`; con ellos se generan las
> gráficas y tablas que van a la memoria.
>
> Cada vez que veas **📌**, ese dato/captura va a la sección indicada de
> [`MEMORIA.md`](MEMORIA.md) (en la memoria esos huecos también están marcados con 📌).

---

## 0. ANTES de clase (5 min, en casa)

1. **Token de GitHub** para poder clonar/subir desde el PC del laboratorio (el repo es privado):
   GitHub → *Settings* → *Developer settings* → *Personal access tokens* → *Tokens (classic)* →
   *Generate new token* → marca `repo` → cópialo en el móvil/notas.
   (Al hacer `git clone`/`git push`, usuario = `jcpd4`, contraseña = **el token**.)
2. Plan B si el token falla: descargar el repo como **ZIP** desde la web de GitHub y, al final,
   subir la carpeta `resultados/` con *Add file → Upload files* (o llevártela en un USB/correo).

---

## Resumen del tiempo en clase

| Min. | Qué | Script |
|---|---|---|
| 0–15 | Clonar, fichero `hosts`, claves SSH, copiar y compilar en los 3+ PCs | `setup_cluster.sh` |
| 15–20 | **Tarea 2**: hola mundo MPI → **enseñárselo al profesor** | `mpirun ... hello_mpi` |
| 20–25 | Info del sistema + **Tarea 0** | `info_sistema.sh`, `bench_tarea0.sh` |
| 25–30 | **Tarea 1**: gprof | `gprof.sh` |
| 30–45 | **Tarea 3**: medir P2 secuencial vs MPI | `bench_tarea3.sh` |
| 45–75 | **Tarea 4**: medir kNN secuencial vs MPI (3 estrategias) | `bench_tarea4.sh` |
| 75–90 | Subir resultados a GitHub (y gráficas si hay matplotlib) | `git push` |

Si vais justos de tiempo: **lo imprescindible es tener los CSV de `resultados/` subidos**. Las gráficas y la memoria se terminan en casa.

---

## 1. Montar el clúster (0–15 min)

Elegid **un PC MAESTRO** (desde el que lanzaréis todo). Todos los PCs con **el mismo usuario**.

### 1.1 En el MAESTRO: clonar el repo en `$HOME`

```bash
cd ~
git clone https://github.com/jcpd4/prac3_ic.git
cd ~/prac3_ic
```

### 1.2 Comprobar que hay MPI y compiladores (en el maestro)

```bash
mpirun --version && mpic++ --version && gprof --version | head -1
```

Si falta MPI y tenéis sudo: `sudo apt install -y openmpi-bin libopenmpi-dev`.

### 1.3 Averiguar la IP y los núcleos de cada PC

En **cada** PC (los de vuestros compañeros/los de al lado):

```bash
hostname -I      # IP (la primera)
nproc            # nº de hilos;   lscpu | grep -E "^Core|^Socket"  -> núcleos físicos
```

### 1.4 Crear el fichero `hosts` (en el maestro)

```bash
cp tarea2/hosts.example hosts
nano hosts
```

Una línea por PC, **el maestro el primero**. `slots` = nº de **núcleos físicos** de ese PC:

```
192.168.X.A slots=4
192.168.X.B slots=4
192.168.X.C slots=4
```

📌 Apuntad las IPs/nombres y núcleos → **MEMORIA §1.1 (tabla de nodos)**.

### 1.5 Descargar MNIST en el maestro (≈1 min)

El dataset **no** está en GitHub (pesa 55 MB):

```bash
cd ~/prac3_ic/tarea4/knn2 && ./get_dataset.sh && cd ~/prac3_ic
ls tarea4/knn2/build/train tarea4/knn2/build/test     # deben salir 2 ficheros en cada una
```

### 1.6 Claves SSH + copiar el repo (con MNIST) + compilar en todos los PCs (automático)

```bash
./scripts/setup_cluster.sh
```

Esto hace lo del tutorial del enunciado: `ssh-keygen -t rsa`, copia `id_rsa.pub` como
`~/.ssh/authorized_keys` en los demás (`ssh-copy-id`, pedirá la contraseña **una vez por PC**),
copia `~/prac3_ic` a **la misma ruta** en todos y compila allí. Debe acabar con
`COMPILADO OK en <pc>` por cada PC.

Comprobad que entra sin contraseña: `ssh 192.168.X.B hostname`.

> Si el `/home` es compartido por red (mismo home en todos los PCs) no pasa nada: la copia es inofensiva.
> Si no hay `rsync`, el script usa `scp`. Si cambiáis algo del código después, volved a lanzarlo.

---

## 2. Tarea 2 — Hola mundo MPI (15–20 min) ⭐ entregable presencial

```bash
cd ~/prac3_ic/tarea2
make
mpirun -mca plm_rsh_no_tree_spawn 1 --hostfile ../hosts --map-by node -np 6 ./hello_mpi
```

Debe salir un `Hello from process i out of 6 on <nombre-PC>` con **nombres de PC distintos**
y la línea `¡Hola! desde el procesador 2`.

- **Llamad al profesor y enseñádselo.**
- 📌 **Captura de pantalla** → guardadla como `graficas/tarea2_hello.png` y pegad la salida en **MEMORIA §4.3**.
- Guardar la salida en texto:

```bash
mpirun -mca plm_rsh_no_tree_spawn 1 --hostfile ../hosts --map-by node -np 6 ./hello_mpi | tee ../resultados/tarea2_hello.txt
cd ~/prac3_ic
```

### Si algo falla
| Síntoma | Solución |
|---|---|
| `There are not enough slots available` | `export MPI_EXTRA="--oversubscribe"` (y en mpirun a mano añadid `--oversubscribe`) |
| Se queda colgado sin salida | Hay varias interfaces de red. Mirad la buena con `ip -br addr` (p. ej. `eno1`) y: `export MPI_EXTRA="--mca btl_tcp_if_include eno1 --mca oob_tcp_if_include eno1"` |
| `Permission denied (publickey)` | Repetid `ssh-copy-id <IP>` |
| `orted: command not found` | En ese PC no está Open MPI en el PATH: probad `mpirun --prefix /usr ...` |
| `No se pudo leer MNIST` en un rank | Falta el dataset en ese PC → paso 1.6 |

`MPI_EXTRA` lo usan **todos los scripts** de benchmark, así que basta con exportarlo una vez.

---

## 3. Información del sistema + Tarea 0 (20–25 min)

```bash
cd ~/prac3_ic
./scripts/info_sistema.sh          # -> resultados/sistema_<pc>.txt
./scripts/bench_tarea0.sh          # -> resultados/tarea0.csv
```

📌 `sistema_<pc>.txt` → **MEMORIA §1.1** (CPU, núcleos, cachés L1/L2/L3, RAM, red).
📌 `tarea0.csv` → **MEMORIA §2** (tabla de tiempos; las respuestas teóricas ya están escritas).
Si queréis, lanzad también `info_sistema.sh` en otro PC por ssh: `ssh <IP> ~/prac3_ic/scripts/info_sistema.sh`.

---

## 4. Tarea 1 — gprof (25–30 min)

```bash
./scripts/gprof.sh                 # -> resultados/gprof_{lidar,knn}_{O0,O2}.txt
```

Compila con `-pg` (-O0 y -O2), ejecuta y genera los informes. Al final imprime el resumen.
📌 Apuntad las **3 funciones con más % de tiempo y nº de llamadas** de cada informe → **MEMORIA §3.2 y §3.3**
(las gráficas visuales las genera `graficas.py` a partir de estos .txt).

---

## 5. Tarea 3 — P2 (LIDAR) secuencial vs MPI (30–45 min)

Probad primero que funciona (unos segundos):

```bash
cd ~/prac3_ic/tarea3
./lidar_seq 50000 1.5 15.0 42
mpirun -mca plm_rsh_no_tree_spawn 1 --hostfile ../hosts --map-by node -np 3 ./lidar_mpi 50000 1.5 15.0 42 ciclico
cd ~/prac3_ic
```

La distancia mínima de ambos **debe coincidir** (comprobación de que el paralelo es correcto).

Lanzad el benchmark completo. Ajustad `PROCS` al total de núcleos del clúster
(p. ej. 3 PCs × 4 núcleos = 12):

```bash
PROCS="2 3 4 6 8 12" ./scripts/bench_tarea3.sh      # -> resultados/tarea3.csv
```

- Tallas por defecto: `n = 50000 100000 200000`. Si el secuencial de 200000 tarda > 60 s, quitadlo:
  `TALLAS="50000 100000 150000" PROCS="..." ./scripts/bench_tarea3.sh`
- Si os sobra tiempo al final: `REPS=3` para repetir y promediar.

📌 Todo lo que hay que meter en la memoria sale de `resultados/tarea3.csv` (tablas y gráficas automáticas) → **MEMORIA §5.4**.

---

## 6. Tarea 4 — kNN secuencial vs MPI (45–75 min)

Prueba rápida (≈10 s):

```bash
cd ~/prac3_ic/tarea4/knn2/build
./knn_seq 1000 5000 200 1 10 5 | tail -3
mpirun -mca plm_rsh_no_tree_spawn 1 --hostfile ../../../hosts --map-by node -np 3 ./knn_mpi datos 1000 5000 200 1 10 5 | tail -6
mpirun -mca plm_rsh_no_tree_spawn 1 --hostfile ../../../hosts --map-by node -np 4 ./knn_mpi especulativo 1000 5000 200 1 10 5 | tail -6
cd ~/prac3_ic
```

`Best k` y `Test accuracy` **deben coincidir** entre secuencial y MPI.

Benchmark completo:

```bash
PROCS="2 3 4 6 8 12" ./scripts/bench_tarea4.sh      # -> resultados/tarea4.csv
```

- Parámetros por defecto: `nCV = 2000 4000 8000` (talla del ajuste), `NTRAIN=20000`, `NTEST=1000`, K∈[1,10], 5 folds.
- **Mirad cuánto tarda el secuencial de nCV=2000.** El de 4000 tarda ≈4× y el de 8000 ≈16×.
  Si el de 2000 tarda más de ~15 s, cortad la ejecución (Ctrl+C) y usad: `NCVS="1000 2000 4000"`.
- Mientras corre: **no uséis los PCs del clúster** (falsea los tiempos).
- Para el análisis de la especulativa con otro reparto ajuste/evaluación (opcional):
  `cd tarea4/knn2/build && mpirun ... -np 6 ./knn_mpi especulativo 4000 20000 1000 1 10 5 2` (último número = procesos de evaluación).

📌 `resultados/tarea4.csv` → **MEMORIA §6.5 y §6.7**.

---

## 7. Subir resultados (75–90 min)

```bash
cd ~/prac3_ic
python3 scripts/graficas.py   # si da "No module named matplotlib": pip3 install --user matplotlib  (o hacedlo en casa)
git add resultados graficas
git commit -m "Resultados de clase"
git push                       # usuario jcpd4 + token
```

Si `git push` no va: comprimid `resultados/` (`zip -r res.zip resultados`) y subidlo/enviádoslo.

---

## 8. En casa: terminar la memoria

1. `git pull` en tu Mac.
2. `python3 -m venv venv && venv/bin/pip install matplotlib && venv/bin/python scripts/graficas.py`
   → genera `graficas/*.png` y `resultados/tablas.md`.
3. Abre [`MEMORIA.md`](MEMORIA.md) y rellena **cada 📌**:
   - copia las tablas de `resultados/tablas.md` en su sección,
   - las gráficas ya están enlazadas (`graficas/…png`); solo comprueba que salen,
   - escribe los números concretos en los huecos `⬜` de las respuestas (las respuestas ya
     están redactadas; solo hay que confirmar que los datos dicen lo mismo y retocar si no).
4. Exportar a PDF (VS Code + extensión *Markdown PDF*, o `pandoc MEMORIA.md -o memoria.pdf`).
   Los diagramas `mermaid` se ven en GitHub/VS Code; para el PDF, si no salen, haz captura desde GitHub.
5. Entregar en UACloud: **PDF de la memoria + código** (`tarea3/lidar_mpi.cpp`, `tarea4/knn2/knn_mpi.cc`, `main_seq.cc`, Makefiles y scripts). Fecha: antes de la sesión de la semana del 16 de noviembre.

---

## Mapa del repositorio

```
prac3_ic/
├── PASO_A_PASO.md          ← esta guía
├── MEMORIA.md              ← la memoria (rellenar 📌)
├── hosts                   ← lo creáis en clase (no se sube)
├── tarea0/  vector.cpp matriz.cpp            (códigos del enunciado, tamaño por argumento)
├── tarea2/  hello_mpi.c  hosts.example
├── tarea3/  lidar_seq.cpp (P2 original)  lidar_mpi.cpp (versión MPI)
├── tarea4/knn2/  código del profesor + main_seq.cc (secuencial parametrizado)
│                 + knn_mpi.cc (MPI: datos | tareas | especulativo) + Makefile
├── scripts/  setup_cluster.sh info_sistema.sh bench_tarea0.sh gprof.sh
│             bench_tarea3.sh bench_tarea4.sh graficas.py
├── resultados/   ← aquí se guardan SOLOS los CSV, logs y gprof
└── graficas/     ← PNG generados por graficas.py (+ captura de la tarea 2)
```

### Uso de los ejecutables (por si queréis lanzar algo a mano)

```bash
./lidar_seq <n> <dispersion> <distancia> [semilla]
mpirun ... ./lidar_mpi <n> <dispersion> <distancia> [semilla] [bloque|ciclico]
./knn_seq <nCV> <nTrainFinal> <nTest> <minK> <maxK> <folds>
mpirun ... ./knn_mpi <datos|tareas|especulativo> <nCV> <nTrainFinal> <nTest> <minK> <maxK> <folds> [nEval]
```

Todos imprimen al final una línea `CSV,...` que es la que recogen los scripts.
