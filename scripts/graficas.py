#!/usr/bin/env python3
"""Genera graficas (graficas/*.png) y tablas markdown (resultados/tablas.md)
a partir de los CSV de resultados/. Requiere matplotlib:
    pip install --user matplotlib     (o: python3 -m venv venv && venv/bin/pip install matplotlib)
Uso: python3 scripts/graficas.py
"""
import csv
import os
import re
from collections import defaultdict

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RES = os.environ.get("RES", os.path.join(RAIZ, "resultados"))
GRA = os.environ.get("GRA", os.path.join(RAIZ, "graficas"))
os.makedirs(GRA, exist_ok=True)
tablas = []


def leer(nombre):
    p = os.path.join(RES, nombre)
    if not os.path.exists(p):
        print(f"(no existe {nombre}, se omite)")
        return []
    with open(p) as f:
        return list(csv.DictReader(f))


def media(filas, clave_grupo, campo):
    """Media de 'campo' agrupando por la tupla clave_grupo(fila) (por si hay repeticiones)."""
    acc = defaultdict(list)
    for r in filas:
        acc[clave_grupo(r)].append(float(r[campo]))
    return {k: sum(v) / len(v) for k, v in acc.items()}


def guardar(fig, nombre):
    fig.tight_layout()
    fig.savefig(os.path.join(GRA, nombre), dpi=130)
    plt.close(fig)
    print("->", os.path.join("graficas", nombre))


# ----------------------------------------------------------------- Tarea 0
t0 = leer("tarea0.csv")
if t0:
    fig, ax = plt.subplots(1, 2, figsize=(10, 4))
    for i, tipo in enumerate(["vector", "matriz"]):
        f = sorted([r for r in t0 if r["tipo"] == tipo], key=lambda r: int(r["size"]))
        if not f:
            continue
        x = [int(r["size"]) for r in f]
        ax[i].plot(x, [float(r["tA"]) for r in f], "o-", label="A (push_back)" if tipo == "vector" else "A (por columnas)")
        ax[i].plot(x, [float(r["tB"]) for r in f], "s-", label="B (prealocado)" if tipo == "vector" else "B (por filas)")
        ax[i].set_xscale("log"); ax[i].set_yscale("log")
        ax[i].set_xlabel("SIZE"); ax[i].set_ylabel("tiempo (s)"); ax[i].set_title(tipo); ax[i].legend(); ax[i].grid(alpha=.3)
        tablas.append(f"\n### Tarea 0 - {tipo}\n\n| SIZE | t A (s) | t B (s) | Ganancia A/B |\n|---|---|---|---|\n" +
                      "\n".join(f"| {r['size']} | {float(r['tA']):.4f} | {float(r['tB']):.4f} | {float(r['gain']):.2f} |" for r in f))
    guardar(fig, "tarea0.png")

# ----------------------------------------------------------------- gprof
fila_flat = re.compile(r"^\s*([\d.]+)\s+([\d.]+)\s+([\d.]+)\s+(\d+)?\s*([\d.]+)?\s*([\d.]+)?\s+(.+)$")
for fn in sorted(os.listdir(RES)) if os.path.isdir(RES) else []:
    if not (fn.startswith("gprof_") and fn.endswith(".txt")):
        continue
    funcs = []
    with open(os.path.join(RES, fn)) as f:
        en_flat = False
        for line in f:
            if line.strip().startswith("%") and "cumulative" in line:
                en_flat = True
                continue
            if en_flat:
                if not line.strip() or "Call graph" in line or line.startswith("\f"):
                    if funcs:
                        break
                    continue
                if line.strip().startswith("time") or line.strip().startswith("Flat"):
                    continue
                m = fila_flat.match(line)
                if m:
                    funcs.append((m.group(7).strip(), float(m.group(1)), m.group(4) or "-"))
    if not funcs:
        continue
    top = funcs[:8][::-1]
    fig, ax = plt.subplots(figsize=(9, 0.5 * len(top) + 1.5))
    nombres = [(n[:55] + "…") if len(n) > 56 else n for n, _, _ in top]
    ax.barh(nombres, [p for _, p, _ in top], color="#4472c4")
    for i, (_, p, c) in enumerate(top):
        ax.text(p, i, f" {p:.1f}%  ({c} llamadas)", va="center", fontsize=8)
    ax.set_xlabel("% tiempo de CPU (self)"); ax.set_xlim(0, 115)
    ax.set_title(fn.replace(".txt", "").replace("_", " "))
    guardar(fig, fn.replace(".txt", ".png"))
    tablas.append(f"\n### {fn}\n\n| Funcion | % tiempo | llamadas |\n|---|---|---|\n" +
                  "\n".join(f"| `{n}` | {p:.2f} | {c} |" for n, p, c in funcs[:8]))


# ----------------------------------------------------------------- Tareas 3 y 4
def analizar(filas, nombre, col_talla, col_modo, etiqueta_talla, extra_comm):
    if not filas:
        return
    seq = media([r for r in filas if r[col_modo] == "secuencial"], lambda r: int(r[col_talla]), "t_total")
    par = [r for r in filas if r[col_modo] != "secuencial"]
    T = media(par, lambda r: (r[col_modo], int(r[col_talla]), int(r["P"])), "t_total")
    C = media(par, lambda r: (r[col_modo], int(r[col_talla]), int(r["P"])), extra_comm)
    modos = sorted({k[0] for k in T})
    tallas = sorted({k[1] for k in T})
    Ps = sorted({k[2] for k in T})

    # Tabla
    t = [f"\n### {nombre}: tiempos, speed-up y eficiencia\n",
         f"| {etiqueta_talla} | Estrategia | P | T (s) | Speed-up | Eficiencia | % tiempo MPI |", "|---|---|---|---|---|---|---|"]
    for n in tallas:
        if n in seq:
            t.append(f"| {n} | secuencial | 1 | {seq[n]:.3f} | 1.00 | 1.00 | 0 |")
        for m in modos:
            for p in Ps:
                if (m, n, p) in T and n in seq:
                    s = seq[n] / T[(m, n, p)]
                    t.append(f"| {n} | {m} | {p} | {T[(m, n, p)]:.3f} | {s:.2f} | {s / p:.2f} | {100 * C[(m, n, p)] / T[(m, n, p)]:.1f} |")
    tablas.append("\n".join(t))

    estilos = ["o-", "s--", "^-.", "d:"]
    for tipo in ["speedup", "eficiencia", "tiempo"]:
        fig, axs = plt.subplots(1, len(modos), figsize=(5 * len(modos), 4), squeeze=False)
        for ax, m in zip(axs[0], modos):
            for i, n in enumerate(tallas):
                xs = [p for p in Ps if (m, n, p) in T and n in seq]
                if not xs:
                    continue
                if tipo == "speedup":
                    ys = [seq[n] / T[(m, n, p)] for p in xs]
                elif tipo == "eficiencia":
                    ys = [seq[n] / T[(m, n, p)] / p for p in xs]
                else:
                    ys = [T[(m, n, p)] for p in xs]
                ax.plot(xs, ys, estilos[i % 4], label=f"{etiqueta_talla}={n}")
            if tipo == "speedup":
                ax.plot(Ps, Ps, "k:", alpha=.5, label="ideal (S=P)")
            if tipo == "eficiencia":
                ax.axhline(1, color="k", ls=":", alpha=.5)
            if tipo == "tiempo":
                ax.set_yscale("log")
            ax.set_title(f"{m}"); ax.set_xlabel("procesos MPI (P)")
            ax.set_ylabel({"speedup": "speed-up", "eficiencia": "eficiencia (S/P)", "tiempo": "tiempo (s)"}[tipo])
            ax.grid(alpha=.3); ax.legend(fontsize=8)
        fig.suptitle(f"{nombre} - {tipo}")
        guardar(fig, f"{nombre.lower().replace(' ', '')}_{tipo}.png")

    # Computo vs comunicacion (talla mayor)
    n = tallas[-1]
    fig, axs = plt.subplots(1, len(modos), figsize=(5 * len(modos), 4), squeeze=False)
    for ax, m in zip(axs[0], modos):
        xs = [p for p in Ps if (m, n, p) in T]
        com = [C[(m, n, p)] for p in xs]
        cal = [max(T[(m, n, p)] - C[(m, n, p)], 0) for p in xs]
        ax.bar([str(p) for p in xs], cal, label="computo")
        ax.bar([str(p) for p in xs], com, bottom=cal, label="comunicacion + sincronizacion")
        ax.set_title(f"{m} ({etiqueta_talla}={n})"); ax.set_xlabel("P"); ax.set_ylabel("tiempo (s)"); ax.legend(fontsize=8)
    guardar(fig, f"{nombre.lower().replace(' ', '')}_comunicacion.png")

    # 3D: speed-up frente a P y talla (primera estrategia "buena")
    try:
        from mpl_toolkits.mplot3d import Axes3D  # noqa: F401
        m = modos[-1] if nombre == "Tarea 3" else modos[0]
        fig = plt.figure(figsize=(6, 5)); ax = fig.add_subplot(111, projection="3d")
        for n in tallas:
            xs = [p for p in Ps if (m, n, p) in T and n in seq]
            ax.plot(xs, [n] * len(xs), [seq[n] / T[(m, n, p)] for p in xs], "o-")
        ax.set_xlabel("P"); ax.set_ylabel(etiqueta_talla); ax.set_zlabel("speed-up"); ax.set_title(f"{nombre} ({m})")
        guardar(fig, f"{nombre.lower().replace(' ', '')}_3d.png")
    except Exception as e:  # pragma: no cover
        print("3D omitido:", e)


t3 = leer("tarea3.csv")
for r in t3:
    r["t_comm"] = str(float(r["t_bcast"]) + float(r["t_reduce"]))
analizar(t3, "Tarea 3", "n", "estrategia", "n", "t_comm")
if t3:  # desequilibrio de carga bloque vs ciclico
    fig, ax = plt.subplots(figsize=(6, 4))
    for e, est in [("bloque", "o-"), ("ciclico", "s-")]:
        f = defaultdict(list)
        for r in t3:
            if r["estrategia"] == e and float(r["t_comp_min"]) > 0:
                f[int(r["P"])].append(float(r["t_comp_max"]) / float(r["t_comp_min"]))
        if f:
            xs = sorted(f)
            ax.plot(xs, [sum(f[p]) / len(f[p]) for p in xs], est, label=e)
    ax.set_xlabel("P"); ax.set_ylabel("t_computo max / min"); ax.set_title("Tarea 3 - desequilibrio de carga"); ax.grid(alpha=.3); ax.legend()
    guardar(fig, "tarea3_desequilibrio.png")

t4 = leer("tarea4.csv")
analizar(t4, "Tarea 4", "nCV", "modo", "nCV", "t_comm")
esp = [r for r in t4 if r["modo"] == "especulativo"]
if esp:
    tablas.append("\n### Tarea 4 - ejecucion especulativa\n\n| nCV | P | best K | reinicios | t ajuste (s) | t test tras ajuste (s) | T total (s) |\n|---|---|---|---|---|---|---|\n" +
                  "\n".join(f"| {r['nCV']} | {r['P']} | {r['bestK']} | {r['reinicios']} | {float(r['t_tuning']):.3f} | {float(r['t_test']):.3f} | {float(r['t_total']):.3f} |" for r in esp))

with open(os.path.join(RES, "tablas.md"), "w") as f:
    f.write("# Tablas generadas automaticamente (copiad las que necesitéis a MEMORIA.md)\n")
    f.write("\n".join(tablas) + "\n")
print("-> resultados/tablas.md")
