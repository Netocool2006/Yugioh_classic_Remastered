"""Baja insumos CC0 de Poly Haven (HDRI, texturas PBR y modelos glTF) para los escenarios pre-renderizados.

Uso: python polyhaven.py <destino> hdri:<id>[@4k] tex:<id>[@2k] mod:<id>[@2k] ...
Destino tipico: D:/Docker/compartido/classic-assets/entornos_src. Deja:
  hdris/<id>_<res>.hdr, texturas/<id>/<id>_{diff,nor_gl,rough,disp,ao}_<res>.jpg|png, modelos/<id>/<id>_<res>.gltf (+ bin y texturas)
No vuelve a bajar lo que ya esta. Licencia: CC0 (https://polyhaven.com/license).
"""
import json
import os
import sys
import urllib.request

API = "https://api.polyhaven.com/files/"
UA = {"User-Agent": "YugiohClassicRemaster-escenarios/1.0"}


def bajar(url, ruta):
    if os.path.exists(ruta) and os.path.getsize(ruta) > 0:
        return
    os.makedirs(os.path.dirname(ruta), exist_ok=True)
    req = urllib.request.Request(url, headers=UA)
    with urllib.request.urlopen(req, timeout=300) as r, open(ruta + ".part", "wb") as f:
        while True:
            b = r.read(1 << 20)
            if not b:
                break
            f.write(b)
    os.replace(ruta + ".part", ruta)
    print("  ", os.path.basename(ruta), os.path.getsize(ruta) // 1024, "KB")


def archivos(ident):
    with urllib.request.urlopen(urllib.request.Request(API + ident, headers=UA), timeout=60) as r:
        return json.load(r)


def hdri(dest, ident, res="4k"):
    d = archivos(ident)
    bajar(d["hdri"][res]["hdr"]["url"], os.path.join(dest, "hdris", f"{ident}_{res}.hdr"))


def textura(dest, ident, res="2k"):
    d = archivos(ident)
    mapas = {"Diffuse": "diff", "nor_gl": "nor_gl", "Rough": "rough", "Displacement": "disp", "AO": "ao"}
    for k, nombre in mapas.items():
        if k not in d or res not in d[k]:
            continue
        fmt = "png" if k == "Displacement" and "png" in d[k][res] else "jpg"
        bajar(d[k][res][fmt]["url"], os.path.join(dest, "texturas", ident, f"{ident}_{nombre}_{res}.{fmt}"))


def modelo(dest, ident, res="2k"):
    d = archivos(ident)
    g = d["gltf"][res]["gltf"]
    base = os.path.join(dest, "modelos", ident)
    bajar(g["url"], os.path.join(base, f"{ident}_{res}.gltf"))
    for rel, info in g.get("include", {}).items():
        bajar(info["url"], os.path.join(base, rel))


def main():
    dest = sys.argv[1]
    for a in sys.argv[2:]:
        tipo, _, resto = a.partition(":")
        ident, _, res = resto.partition("@")
        print(tipo, ident)
        {"hdri": hdri, "tex": textura, "mod": modelo}[tipo](dest, ident, *( [res] if res else []))


if __name__ == "__main__":
    main()
