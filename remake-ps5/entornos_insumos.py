# Yugioh Classic Remaster - entornos por personaje rival (src/entornos.h): descarga las texturas CC0 de Poly Haven y
# las prepara como capas del arreglo de texturas del juego (png 1024x1024: color + oclusion, normal GL + rugosidad).
# Nada de esto va al repo: las descargas quedan en D:\Yugi-Reload\terceros\entornos\descargas y lo preparado en
# <assets>/entornos (lo copia sincronizar.sh al titulo). Licencias: third_party/LICENCIAS_entornos.md.
# Uso (Windows, Python 3 con Pillow y numpy):
#   python -I entornos_insumos.py [--descargas DIR] [--assets DIR]
import argparse
import json
import os
import sys
import urllib.request

import numpy as np
from PIL import Image

TAM = 1024

# capa del juego -> textura de Poly Haven (el ORDEN es el de entornos::Capa en src/entornos.h)
MATERIALES = [
    ("castillo", "castle_brick_07"),          # muros del castillo de Pegasus, laberinto
    ("adoquin", "cobblestone_floor_04"),      # patio del castillo, plazas
    ("arenisca", "sandstone_blocks_08"),      # templo, columnas, piramides
    ("arena", "aerial_sand"),                 # desierto, playa
    ("metal", "metal_plate"),                 # cubierta del dirigible, estadio, torre
    ("marmol", "marble_01"),                  # museo, Atlantida
    ("ladrillo", "red_brick_03"),             # edificios de Ciudad Domino, tienda
    ("hormigon", "concrete_floor_02"),        # azoteas, calles, estadio
    ("roca", "rock_face_03"),                 # acantilados, cuevas
    ("madera", "wood_floor_deck"),            # tienda, muelle
    ("cesped", "aerial_grass_rock"),          # pradera de la isla
    ("corteza", "bark_brown_02"),             # troncos
    ("hojas", "forest_leaves_02"),            # copas (bultos)
    ("baldosa", "granite_tile"),              # suelo del museo y la tienda
    ("losa", "large_sandstone_blocks"),       # suelo del templo
]


def bajar(url, destino):
    if os.path.exists(destino) and os.path.getsize(destino) > 0:
        return destino
    os.makedirs(os.path.dirname(destino), exist_ok=True)
    print("bajando", url)
    pedido = urllib.request.Request(url, headers={"User-Agent": "YugiReload-entornos/1.0"})
    with urllib.request.urlopen(pedido) as r, open(destino + ".part", "wb") as f:
        f.write(r.read())
    os.replace(destino + ".part", destino)
    return destino


def json_url(url):
    pedido = urllib.request.Request(url, headers={"User-Agent": "YugiReload-entornos/1.0"})
    with urllib.request.urlopen(pedido) as r:
        return json.loads(r.read().decode("utf-8"))


def bajar_polyhaven(desc):
    for _, nombre in MATERIALES:
        carpeta = os.path.join(desc, "polyhaven", nombre)
        if os.path.exists(os.path.join(carpeta, "listo")):
            continue
        archivos = json_url("https://api.polyhaven.com/files/" + nombre)
        for mapa in ("Diffuse", "nor_gl", "Rough", "AO"):
            if mapa in archivos and "1k" in archivos[mapa]:
                info = archivos[mapa]["1k"]["jpg"]
                bajar(info["url"], os.path.join(carpeta, nombre + "_" + mapa + ".jpg"))
        open(os.path.join(carpeta, "listo"), "w").close()


def leer(ruta, modo="RGB"):
    im = Image.open(ruta).convert(modo)
    if im.size != (TAM, TAM):
        im = im.resize((TAM, TAM), Image.LANCZOS)
    return np.asarray(im).astype(np.float32) / 255.0


def guardar(rgba, ruta):
    a = np.clip(rgba * 255.0 + 0.5, 0, 255).astype(np.uint8)
    Image.fromarray(a, "RGBA").save(ruta, optimize=True)
    print("  ", os.path.basename(ruta))


def preparar(desc, salida):
    os.makedirs(salida, exist_ok=True)
    for prefijo, nombre in MATERIALES:
        base = os.path.join(desc, "polyhaven", nombre, nombre + "_")
        color = leer(base + "Diffuse.jpg")
        ao = leer(base + "AO.jpg", "L") if os.path.exists(base + "AO.jpg") else np.ones((TAM, TAM), np.float32)
        guardar(np.dstack([color, ao]), os.path.join(salida, prefijo + "_color.png"))
        n = leer(base + "nor_gl.jpg")
        r = leer(base + "Rough.jpg", "L") if os.path.exists(base + "Rough.jpg") else np.full((TAM, TAM), 0.8, np.float32)
        guardar(np.dstack([n, r]), os.path.join(salida, prefijo + "_normal.png"))


def main():
    aqui = os.path.dirname(os.path.abspath(__file__))
    ap = argparse.ArgumentParser()
    ap.add_argument("--descargas", default=r"D:\Yugi-Reload\terceros\entornos\descargas")
    ap.add_argument("--assets", default=r"D:\Docker\compartido\classic-assets")
    a = ap.parse_args()
    bajar_polyhaven(a.descargas)
    preparar(a.descargas, os.path.join(a.assets, "entornos"))
    print("listo:", len(MATERIALES), "materiales en", os.path.join(a.assets, "entornos"), "(", aqui, ")")


if __name__ == "__main__":
    sys.exit(main())
