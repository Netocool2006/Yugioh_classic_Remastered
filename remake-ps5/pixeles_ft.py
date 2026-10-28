#!/usr/bin/env python3
# Rama classic-front-total: mira en las fotos de la vista previa del PC (captura_front_total.sh, front_total_pc.h) si
# cada zona de monstruo con una carta BOCA ARRIBA muestra la cara de la carta y no la casilla vacia (el recuadro
# oscuro con su borde). Por cada zona: el color medio y la variacion dentro del cuadrilatero de la carta (sin los
# bordes) comparados con los de las zonas vacias de la misma foto (la casilla). Escribe un TSV de casos
# (CU-FT-PIXEL-<config>-<foto>-<zona>) y una imagen marcada (<foto>_marcada.png) para revisar a ojo.
# Uso: python3 pixeles_ft.py <carpeta de la corrida> <nombre de la config> <salida.tsv>
import sys, os
import numpy as np
from PIL import Image, ImageDraw

def muestras(img, quad, margen=0.22, n=24):
    (x0, y0), (x1, y1), (x2, y2), (x3, y3) = quad
    h, w = img.shape[:2]
    px = []
    for i in range(n):
        for j in range(n):
            u = margen + (1 - 2 * margen) * i / (n - 1)
            v = margen + (1 - 2 * margen) * j / (n - 1)
            # bilineal: 0-1 abajo, 3-2 arriba
            xa, ya = x0 + (x1 - x0) * u, y0 + (y1 - y0) * u
            xb, yb = x3 + (x2 - x3) * u, y3 + (y2 - y3) * u
            x, y = xa + (xb - xa) * v, ya + (yb - ya) * v
            xi, yi = int(round(x)), int(round(y))
            if 0 <= xi < w and 0 <= yi < h:
                px.append(img[yi, xi, :3])
    return np.array(px, dtype=np.float32)

def stats(p):
    if len(p) == 0:
        return None
    lum = p @ np.array([0.299, 0.587, 0.114], dtype=np.float32)
    return {"media": p.mean(axis=0), "lum": float(lum.mean()), "var": float(lum.std()), "col": float(np.abs(p - p.mean(axis=0)).mean())}

def main():
    carpeta, config, salida = sys.argv[1], sys.argv[2], sys.argv[3]
    filas = []
    for foto in ("ft_1", "ft_2"):
        zon = os.path.join(carpeta, foto + "_zonas.txt")
        cid = "CU-FT-PIXEL-%s-%s" % (config, foto)
        if not os.path.exists(zon):
            filas.append((cid, "FALLA", "no hay zonas (%s): el juego no llego a la foto" % foto))
            continue
        lineas = [l.strip().split("|") for l in open(zon, encoding="utf-8")]
        cab = [c for c in lineas if c and c[0] == "#cuadro"]
        cuadro, W = (int(cab[0][1]), float(cab[0][2])) if cab else (0, 1920.0)
        # el cuadro grabado mas cercano (uno cada 15: c_0001 = el cuadro 15)
        k = max(1, int(round(cuadro / 15.0)))
        ruta = os.path.join(carpeta, "c_%04d.png" % k)
        if not os.path.exists(ruta):
            filas.append((cid, "FALLA", "no hay cuadro grabado %s para el cuadro %d" % (ruta, cuadro)))
            continue
        im = Image.open(ruta).convert("RGB")
        img = np.asarray(im)
        esc = im.size[0] / W
        zonas = []
        for c in lineas:
            if len(c) < 8 or c[0].startswith("#"):
                continue
            quad = [tuple(float(v) * esc for v in e.split(",")) for e in c[4:8]]
            fig = c[8] == "fig1" if len(c) > 8 else None
            con = c[9] == "con1" if len(c) > 9 else False
            zonas.append({"zona": c[0], "carta": c[1], "abajo": c[2] == "1", "def": c[3] == "1", "quad": quad, "fig": fig, "con": con})
        vacias = [stats(muestras(img, z["quad"])) for z in zonas if z["carta"] == "-"]
        vacias = [v for v in vacias if v]
        if not vacias:
            filas.append((cid, "FALLA", "sin zonas vacias para comparar"))
            continue
        ref = {"media": np.mean([v["media"] for v in vacias], axis=0), "var": max(v["var"] for v in vacias), "col": max(v["col"] for v in vacias)}
        dib = ImageDraw.Draw(im)
        for z in zonas:
            s = stats(muestras(img, z["quad"]))
            if z["carta"] == "-" or z["abajo"] or s is None:
                dib.polygon(z["quad"], outline=(90, 90, 90))
                continue
            dist = float(np.linalg.norm(s["media"] - ref["media"]))
            # la cara de una carta: color propio (lejos de la casilla) o mucha variacion (arte, texto, marco)
            cara = dist > 30.0 or s["var"] > ref["var"] * 1.8 + 6.0
            det = "%s en %s (%s): distancia a la casilla %.0f, variacion %.1f (casilla %.1f), luz %.0f" % (
                z["carta"], z["zona"], "defensa" if z["def"] else "ataque", dist, s["var"], ref["var"], s["lum"])
            filas.append(("%s-%s" % (cid, z["zona"].upper().replace("_", "-")), "OK" if cara else "FALLA", ("" if cara else "se ve la casilla vacia y no la carta: ") + det))
            dib.polygon(z["quad"], outline=(0, 255, 0) if cara else (255, 0, 0))
        # las figuras 3D: solo las cartas con figura boca arriba la muestran (y ninguna con la opcion apagada)
        apagadas = "apagadas" in config
        for z in zonas:
            if z["fig"] is None or z["carta"] == "-":
                continue
            quiere = z["con"] and not z["abajo"] and not apagadas
            filas.append(("CU-FT-FIGURA-%s-%s-%s" % (config, foto, z["zona"].upper().replace("_", "-")), "OK" if z["fig"] == quiere else "FALLA",
                          "" if z["fig"] == quiere else "%s en %s (%s): figura %s y deberia %s" % (z["carta"], z["zona"], "boca abajo" if z["abajo"] else "boca arriba",
                                                                                                    "visible" if z["fig"] else "ausente", "verse" if quiere else "no verse")))
        im.save(os.path.join(carpeta, foto + "_marcada.png"))
    fig = os.path.join(carpeta, "figuras.txt")
    if os.path.exists(fig):
        for l in open(fig, encoding="utf-8"):
            c = l.strip().split("|")
            if len(c) >= 3:
                ok = c[1] == "1" or ("apagadas" in config and c[0] == "VOLTEADA-CON-FIGURA")
                filas.append(("CU-FT-FIGURA-%s-%s" % (config, c[0]), "OK" if ok else "FALLA", "" if ok else c[2]))
    elif "figuras" in config:
        filas.append(("CU-FT-FIGURA-%s-BATALLA" % config, "FALLA", "no se llego a la batalla de las figuras (figuras.txt)"))
    with open(salida, "a", encoding="utf-8") as f:
        for i, e, d in filas:
            f.write("%s\t%s\t%s\n" % (i, e, d))
    for i, e, d in filas:
        print("%s %s %s" % (e, i, d))

main()
