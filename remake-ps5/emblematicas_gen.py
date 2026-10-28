#!/usr/bin/env python3
"""Cartas emblematicas de Yugioh Classic Remaster (rama classic-figuras).

Lee la lista del usuario (remake/diseno/emblematicas.json, campo "emblematicas": nombres TCG en ingles) y las cartas
firma de los personajes (datos/cartas_firma.txt), las busca por nombre en cartas_ui_en.txt (id|nombre|clase|...) y
escribe:
  datos/emblematicas.txt  id de cada emblematica que esta en el juego (y de donde sale)
  datos/figuras.txt       id slug: las emblematicas de monstruo que ya tienen figura 3D (models/yugi/<slug>.glb)
  docs/figuras_pendientes.md  las emblematicas de monstruo SIN figura (para producirlas con TRELLIS en Colab)

Uso: python emblematicas_gen.py [--json ...] [--ui .../cartas_ui_en.txt] [--modelos .../models/yugi]
"""
import argparse
import json
import os
import re

AQUI = os.path.dirname(os.path.abspath(__file__))
MONSTRUO = {"normal", "efecto", "fusion", "ritual"}
# nombres que el juego escribe distinto que la lista del usuario (TCG)
ALIAS = {
    "red-eyes b. dragon": "red_eyes",
    "blue-eyes white dragon": "blue_eyes",
}


def clave(nombre):
    return re.sub(r"[^a-z0-9]+", " ", nombre.lower()).strip()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--json", default="D:/Yugi-Reload/remake/diseno/emblematicas.json")
    ap.add_argument("--firma", default=os.path.join(AQUI, "datos", "cartas_firma.txt"))
    ap.add_argument("--ui", default="D:/Docker/compartido/classic-assets/ui/cartas_ui_en.txt")
    ap.add_argument("--modelos", default="D:/Docker/compartido/classic-assets/models/yugi")
    args = ap.parse_args()

    cartas = {}   # clave del nombre -> (id, nombre, clase)
    porId = {}
    for linea in open(args.ui, encoding="utf-8"):
        p = linea.rstrip("\n").split("|")
        if len(p) < 3 or linea.startswith("#"):
            continue
        cartas.setdefault(clave(p[1]), (p[0], p[1], p[2]))
        porId[p[0]] = (p[0], p[1], p[2])

    nombres = [(n, "emblematica") for n in json.load(open(args.json, encoding="utf-8"))["emblematicas"]]
    if os.path.exists(args.firma):
        for linea in open(args.firma, encoding="utf-8"):
            if linea.strip() and not linea.startswith("#"):
                nombres.append((linea.split("|")[0].strip(), "firma"))

    vistos, filas, faltan = set(), [], []
    for nombre, origen in nombres:
        c = cartas.get(clave(nombre))
        if not c and nombre.lower() in ALIAS:
            c = porId.get(ALIAS[nombre.lower()])
        if not c:
            faltan.append((nombre, origen))
            continue
        if c[0] in vistos:
            continue
        vistos.add(c[0])
        filas.append((c[0], c[1], c[2], origen))

    os.makedirs(os.path.join(AQUI, "datos"), exist_ok=True)
    with open(os.path.join(AQUI, "datos", "emblematicas.txt"), "w", encoding="utf-8", newline="\n") as f:
        f.write("# emblematicas.txt - generado por emblematicas_gen.py (no editar a mano): id de carta del juego de cada\n")
        f.write("# carta emblematica (remake/diseno/emblematicas.json) y de cada carta firma de los personajes (cartas_firma.txt)\n")
        for i, n, cl, o in filas:
            f.write(f"{i}|{n}|{cl}|{o}\n")

    # las figuras: el slug del modelo es el id de la carta (models/yugi/<id>.glb y, si tiene esqueleto, <id>_anim.glb)
    con, sin = [], []
    for i, n, cl, o in filas:
        if cl not in MONSTRUO:
            continue
        if os.path.exists(os.path.join(args.modelos, i + ".glb")):
            con.append((i, n))
        else:
            sin.append((i, n, cl, o))
    with open(os.path.join(AQUI, "datos", "figuras.txt"), "w", encoding="utf-8", newline="\n") as f:
        f.write("# figuras.txt - generado por emblematicas_gen.py: id de carta -> slug del modelo 3D (models/yugi/<slug>.glb,\n")
        f.write("# <slug>_anim.glb si tiene esqueleto). Solo las emblematicas: las demas cartas siguen planas\n")
        for i, n in con:
            f.write(f"{i} {i}\n")

    os.makedirs(os.path.join(AQUI, "docs"), exist_ok=True)
    with open(os.path.join(AQUI, "docs", "figuras_pendientes.md"), "w", encoding="utf-8", newline="\n") as f:
        f.write("# Figuras 3D pendientes (cartas emblematicas de monstruo sin modelo)\n\n")
        f.write("Generado por `emblematicas_gen.py`. Estas cartas emblematicas (o firma de un personaje) son monstruos y todavia\n")
        f.write("se ven planas en el tablero: hace falta su figura (`models/yugi/<id>.glb`, y `<id>_anim.glb` con clips\n")
        f.write("reposo, ataque, golpe, defensa, muerte si lleva esqueleto). Ciclo: arte original -> Gemini (1 foto) ->\n")
        f.write("TRELLIS en Colab -> revision en PC. Al copiarla a `classic-assets/models/yugi/` y volver a correr el script,\n")
        f.write("la carta pasa sola a `datos/figuras.txt`.\n\n")
        f.write(f"Con figura ({len(con)}): " + ", ".join(f"`{i}`" for i, _ in con) + "\n\n")
        for titulo, origen in (("Emblematicas", "emblematica"), ("Cartas firma de los personajes", "firma")):
            grupo = [x for x in sin if x[3] == origen]
            f.write(f"## {titulo} sin figura ({len(grupo)})\n\n| id (slug) | carta | clase |\n|---|---|---|\n")
            for i, n, cl, o in grupo:
                f.write(f"| `{i}` | {n} | {cl} |\n")
            f.write("\n")
        if faltan:
            f.write(f"## No estan en el juego ({len(faltan)})\n\nNo aparecen en cartas_ui_en.txt: " + ", ".join(n for n, _ in faltan) + "\n")
    print(f"emblematicas: {len(filas)} en el juego ({len(faltan)} no estan); monstruos con figura {len(con)}, sin figura {len(sin)}")
    for i, n in con:
        print("  figura:", i, "-", n)


if __name__ == "__main__":
    main()
