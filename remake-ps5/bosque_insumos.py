# Yugi Reload - carta de campo Bosque: descarga los insumos CC0 del bosque realista y los prepara para el juego.
# Fuentes (todas CC0, ver third_party/LICENCIAS_bosque.md): Poly Haven (texturas de corteza y suelo; modelos de
# helecho, rocas con musgo, tocon y tronco caido) y ambientCG (hojas sueltas para armar los ramilletes).
# Nada de esto va al repo: las descargas quedan en D:\Yugi-Reload\terceros\bosque\descargas y lo preparado en
# <assets>/bosque (lo copia sincronizar.sh al titulo).
# Uso (Windows, Python 3 con Pillow y numpy):
#   python -I bosque_insumos.py [--descargas DIR] [--assets DIR] [--blender EXE]
#   1) descarga lo que falte; 2) arma las capas de textura (png 1024x1024: color+alfa o normal+rugosidad);
#   3) con Blender (bosque_mallas_blender.py) reduce los modelos de Poly Haven y los guarda como .malla
import argparse
import json
import math
import os
import random
import subprocess
import sys
import urllib.request
import zipfile

import numpy as np
from PIL import Image

AQUI = os.path.dirname(os.path.abspath(__file__))
TAM = 1024

# texturas de Poly Haven (1k): corteza, suelo
PH_TEXTURAS = ["bark_brown_02", "pine_bark", "forrest_ground_01", "forest_leaves_02"]
# modelos de Poly Haven (gltf 1k, con sus texturas)
PH_MODELOS = ["fern_02", "rock_moss_set_01", "tree_stump_01", "dead_tree_trunk"]
# atlas de hojas de ambientCG (1K-PNG)
ACG = ["LeafSet024", "LeafSet019", "LeafSet017", "Moss002"]


def bajar(url, destino):
    if os.path.exists(destino) and os.path.getsize(destino) > 0:
        return destino
    os.makedirs(os.path.dirname(destino), exist_ok=True)
    print("bajando", url)
    pedido = urllib.request.Request(url, headers={"User-Agent": "YugiReload-bosque/1.0"})
    with urllib.request.urlopen(pedido) as r, open(destino + ".part", "wb") as f:
        f.write(r.read())
    os.replace(destino + ".part", destino)
    return destino


def json_url(url):
    pedido = urllib.request.Request(url, headers={"User-Agent": "YugiReload-bosque/1.0"})
    with urllib.request.urlopen(pedido) as r:
        return json.loads(r.read().decode("utf-8"))


def bajar_polyhaven(desc):
    for nombre in sorted(set(PH_TEXTURAS + PH_MODELOS)):
        carpeta = os.path.join(desc, "polyhaven", nombre)
        if os.path.exists(os.path.join(carpeta, "listo")):
            continue
        archivos = json_url("https://api.polyhaven.com/files/" + nombre)
        for mapa in ("Diffuse", "nor_gl", "Rough", "AO", "Alpha"):
            if mapa in archivos and "1k" in archivos[mapa]:
                formato = "png" if mapa == "Alpha" else "jpg"
                info = archivos[mapa]["1k"][formato]
                bajar(info["url"], os.path.join(carpeta, nombre + "_" + mapa + "." + formato))
        if nombre in PH_MODELOS:
            g = archivos["gltf"]["1k"]["gltf"]
            bajar(g["url"], os.path.join(carpeta, "gltf", os.path.basename(g["url"])))
            for ruta, info in g["include"].items():
                bajar(info["url"], os.path.join(carpeta, "gltf", ruta))
        open(os.path.join(carpeta, "listo"), "w").close()


def bajar_ambientcg(desc):
    for nombre in ACG:
        carpeta = os.path.join(desc, "acg", nombre)
        if os.path.isdir(carpeta) and any(f.endswith("_Color.png") for f in os.listdir(carpeta)):
            continue
        z = bajar("https://ambientcg.com/get?file=%s_1K-PNG.zip" % nombre, os.path.join(desc, "acg", nombre + ".zip"))
        with zipfile.ZipFile(z) as zz:
            for m in zz.namelist():
                if m.endswith(".png") and any(k in m for k in ("_Color", "_Opacity", "_NormalGL", "_Roughness", "_AmbientOcclusion")):
                    zz.extract(m, carpeta)


# ------------------------------------------------------------------------------------------------ imagenes
def leer(ruta, modo="RGB", tam=TAM):
    im = Image.open(ruta).convert(modo)
    if tam and im.size != (tam, tam):
        im = im.resize((tam, tam), Image.LANCZOS)
    return np.asarray(im).astype(np.float32) / 255.0


def guardar(rgba, ruta):
    a = np.clip(rgba * 255.0 + 0.5, 0, 255).astype(np.uint8)
    Image.fromarray(a, "RGBA").save(ruta, optimize=True)
    print("  ", os.path.basename(ruta))


def ph(desc, nombre, mapa):
    for ext in ("jpg", "png"):
        r = os.path.join(desc, "polyhaven", nombre, nombre + "_" + mapa + "." + ext)
        if os.path.exists(r):
            return r
    return None


def capa_material(desc, nombre, salida, prefijo, ao=True):
    """color (rgb) + oclusion (a), y normal GL (rgb) + rugosidad (a)"""
    color = leer(ph(desc, nombre, "Diffuse"))
    a = leer(ph(desc, nombre, "AO"), "L") if ao and ph(desc, nombre, "AO") else np.ones((TAM, TAM), np.float32)
    guardar(np.dstack([color, a]), os.path.join(salida, prefijo + "_color.png"))
    n = leer(ph(desc, nombre, "nor_gl"))
    r = leer(ph(desc, nombre, "Rough"), "L") if ph(desc, nombre, "Rough") else np.full((TAM, TAM), 0.8, np.float32)
    guardar(np.dstack([n, r]), os.path.join(salida, prefijo + "_normal.png"))


def capa_acg(desc, nombre, salida, prefijo):
    """material de ambientCG: color (+ oclusion si hay) y normal GL + rugosidad"""
    carpeta = os.path.join(desc, "acg", nombre)
    def busca(clave):
        for raiz, _, archivos in os.walk(carpeta):
            for f in archivos:
                if clave in f:
                    return os.path.join(raiz, f)
        return None
    color = leer(busca("_Color"))
    ao = busca("_AmbientOcclusion")
    a = leer(ao, "L") if ao else np.ones((TAM, TAM), np.float32)
    guardar(np.dstack([color, a]), os.path.join(salida, prefijo + "_color.png"))
    r = busca("_Roughness")
    guardar(np.dstack([leer(busca("_NormalGL")), leer(r, "L") if r else np.full((TAM, TAM), 0.8, np.float32)]),
            os.path.join(salida, prefijo + "_normal.png"))


def hojas_sueltas(desc, nombre):
    """recorta cada hoja del atlas de ambientCG (color, alfa, normal) por sus manchas de alfa"""
    carpeta = os.path.join(desc, "acg", nombre)
    base = None
    for raiz, _, archivos in os.walk(carpeta):
        for f in archivos:
            if f.endswith("_Color.png"):
                base = os.path.join(raiz, f[: -len("_Color.png")])
    color = leer(base + "_Color.png", "RGB", None)
    alfa = leer(base + "_Opacity.png", "L", None)
    normal = leer(base + "_NormalGL.png", "RGB", None)
    h, w = alfa.shape
    marca = alfa > 0.5
    visto = np.zeros_like(marca)
    hojas = []
    # componentes conexas a lo bruto (por filas de cajas): basta con buscar cajas de alfa separadas
    cols = marca.any(axis=0)
    filas = marca.any(axis=1)
    def tramos(v):
        t, dentro, ini = [], False, 0
        for i, x in enumerate(v):
            if x and not dentro:
                dentro, ini = True, i
            elif not x and dentro:
                dentro = False
                t.append((ini, i))
        if dentro:
            t.append((ini, len(v)))
        return t
    for (y0, y1) in tramos(filas):
        sub = marca[y0:y1]
        for (x0, x1) in tramos(sub.any(axis=0)):
            caja = marca[y0:y1, x0:x1]
            ys = np.where(caja.any(axis=1))[0]
            if len(ys) == 0 or (x1 - x0) * (ys[-1] - ys[0]) < 400:
                continue
            yy0, yy1 = y0 + ys[0], y0 + ys[-1] + 1
            hojas.append((color[yy0:yy1, x0:x1], alfa[yy0:yy1, x0:x1], normal[yy0:yy1, x0:x1]))
    return hojas


def pegar(lienzo_c, lienzo_a, lienzo_n, hoja, cx, cy, ang, esc, tinte=(1.0, 1.0, 1.0)):
    """pega una hoja girada (ang, rad; 0 = la hoja apunta hacia arriba en la imagen) con su normal girada"""
    c, a, n = hoja
    h, w = a.shape
    # las hojas del atlas apuntan hacia arriba; se escalan y giran con Pillow
    tw, th = max(2, int(w * esc)), max(2, int(h * esc))
    grados = math.degrees(ang)
    def girar(arr, modo):
        im = Image.fromarray(np.clip(arr * 255 + 0.5, 0, 255).astype(np.uint8), modo).resize((tw, th), Image.LANCZOS)
        return np.asarray(im.rotate(grados, resample=Image.BICUBIC, expand=True)).astype(np.float32) / 255.0
    cg, ag, ng = girar(c, "RGB"), girar(a, "L"), girar(n, "RGB")
    # la normal tangente tambien gira (xy)
    nx, ny = ng[..., 0] * 2 - 1, ng[..., 1] * 2 - 1
    co, si = math.cos(ang), math.sin(ang)
    # en la imagen y crece hacia abajo; la normal GL tiene y hacia arriba: girar en sentido antihorario = +ang
    rx, ry = co * nx - si * ny, si * nx + co * ny
    ng = np.dstack([rx * 0.5 + 0.5, ry * 0.5 + 0.5, ng[..., 2]])
    H, W = lienzo_a.shape
    gh, gw = ag.shape
    x0, y0 = int(cx - gw / 2), int(cy - gh / 2)
    xa, ya, xb, yb = max(0, x0), max(0, y0), min(W, x0 + gw), min(H, y0 + gh)
    if xa >= xb or ya >= yb:
        return
    sa = ag[ya - y0:yb - y0, xa - x0:xb - x0][..., None]
    lienzo_c[ya:yb, xa:xb] = lienzo_c[ya:yb, xa:xb] * (1 - sa) + cg[ya - y0:yb - y0, xa - x0:xb - x0] * np.array(tinte) * sa
    lienzo_n[ya:yb, xa:xb] = lienzo_n[ya:yb, xa:xb] * (1 - sa) + ng[ya - y0:yb - y0, xa - x0:xb - x0] * sa
    lienzo_a[ya:yb, xa:xb] = np.maximum(lienzo_a[ya:yb, xa:xb], sa[..., 0])


def linea(lienzo_c, lienzo_a, p0, p1, r0, r1, color):
    """tallo: segmento grueso pintado (de r0 a r1 px)"""
    H, W = lienzo_a.shape
    n = int(max(abs(p1[0] - p0[0]), abs(p1[1] - p0[1]))) + 1
    for i in range(n):
        t = i / max(1, n - 1)
        x, y = p0[0] + (p1[0] - p0[0]) * t, p0[1] + (p1[1] - p0[1]) * t
        r = r0 + (r1 - r0) * t
        xa, xb, ya, yb = int(x - r - 1), int(x + r + 2), int(y - r - 1), int(y + r + 2)
        xa, ya, xb, yb = max(0, xa), max(0, ya), min(W, xb), min(H, yb)
        yy, xx = np.mgrid[ya:yb, xa:xb]
        m = np.clip(r + 0.5 - np.hypot(xx - x, yy - y), 0, 1)
        sombra = 0.75 + 0.25 * np.clip((xx - x) / max(r, 1), -1, 1)
        lienzo_c[ya:yb, xa:xb] = lienzo_c[ya:yb, xa:xb] * (1 - m[..., None]) + (np.array(color) * sombra[..., None]) * m[..., None]
        lienzo_a[ya:yb, xa:xb] = np.maximum(lienzo_a[ya:yb, xa:xb], m)


def lienzos(tam):
    c = np.zeros((tam, tam, 3), np.float32)
    a = np.zeros((tam, tam), np.float32)
    n = np.zeros((tam, tam, 3), np.float32)
    n[...] = (0.5, 0.5, 1.0)
    return c, a, n


def sangrar(c, a, n, pasadas=24):
    """extiende el color bajo el alfa 0 (para que los mipmaps no se oscurezcan en los bordes de las hojas)"""
    c, n = c.copy(), n.copy()
    lleno = a > 0.02
    for _ in range(pasadas):
        if lleno.all():
            break
        suma_c = np.zeros_like(c)
        suma_n = np.zeros_like(n)
        cuenta = np.zeros(a.shape, np.float32)
        for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            m = np.roll(lleno, (dy, dx), (0, 1))
            suma_c += np.roll(c, (dy, dx), (0, 1)) * m[..., None]
            suma_n += np.roll(n, (dy, dx), (0, 1)) * m[..., None]
            cuenta += m
        nuevo = (~lleno) & (cuenta > 0)
        c[nuevo] = suma_c[nuevo] / cuenta[nuevo][..., None]
        n[nuevo] = suma_n[nuevo] / cuenta[nuevo][..., None]
        lleno = lleno | nuevo
    c[~lleno] = c[lleno].mean(axis=0)
    n[~lleno] = (0.5, 0.5, 1.0)
    return c, n


def borde_suave(a, margen=10):
    """el alfa se apaga en el borde de cada variante (sin cortes rectos al recortar)"""
    h, w = a.shape
    y = np.minimum(np.arange(h), h - 1 - np.arange(h))[:, None]
    x = np.minimum(np.arange(w), w - 1 - np.arange(w))[None, :]
    return a * np.clip(np.minimum(x, y) / margen, 0, 1)


def curva(p0, c, p1, n):
    pts = []
    for i in range(n):
        t = i / (n - 1)
        pts.append(((1 - t) ** 2 * p0[0] + 2 * (1 - t) * t * c[0] + t * t * p1[0],
                    (1 - t) ** 2 * p0[1] + 2 * (1 - t) * t * c[1] + t * t * p1[1]))
    return pts


def ramillete_hoja_ancha(hojas, semilla, tam, tintes, escala=1.0):
    """un ramillete de hojas anchas (2x2 variantes en la textura): una rama con 4-6 ramitas laterales y 30-40
    hojas alternas, que deja huecos (la silueta se lee como hojas y no como una mancha)"""
    rnd = random.Random(semilla)
    C, A, N = lienzos(tam)
    medio = tam // 2
    k = medio / 512
    marron = (0.21, 0.16, 0.10)
    for q in range(4):
        ox, oy = (q % 2) * medio, (q // 2) * medio
        sub_c, sub_a, sub_n = lienzos(medio)
        base = (medio * 0.5 + rnd.uniform(-15, 15) * k, medio * 0.99)
        punta = (medio * 0.5 + rnd.uniform(-40, 40) * k, medio * 0.27)
        ctrl = (medio * 0.5 + rnd.uniform(-70, 70) * k, medio * 0.55)
        tallo = curva(base, ctrl, punta, 14)
        for i in range(13):
            linea(sub_c, sub_a, tallo[i], tallo[i + 1], (5.0 * (1 - i / 13) + 1.5) * k, (5.0 * (1 - (i + 1) / 13) + 1.5) * k, marron)
        ramitas = []   # (puntos de la ramita, t a lo largo del tallo)
        nr = rnd.randint(4, 6)
        for r in range(nr):
            t = 0.22 + 0.68 * (r + rnd.uniform(0.0, 0.6)) / nr
            p0 = tallo[min(13, int(t * 13))]
            lado = -1 if r % 2 else 1
            largo = medio * rnd.uniform(0.18, 0.27) * (1.0 - 0.45 * t)
            ang = lado * rnd.uniform(0.75, 1.15)
            p1 = (p0[0] - math.sin(ang) * largo, p0[1] - math.cos(ang) * largo * 0.8 - largo * 0.15)
            c = ((p0[0] + p1[0]) / 2, (p0[1] + p1[1]) / 2 + largo * 0.12)
            pts = curva(p0, c, p1, 8)
            for i in range(7):
                linea(sub_c, sub_a, pts[i], pts[i + 1], 2.4 * k * (1 - i / 9), 2.4 * k * (1 - (i + 1) / 9), marron)
            ramitas.append((pts, ang))
        ramitas.append((tallo[4:], 0.0))
        piezas = []
        for pts, ang0 in ramitas:
            n = len(pts)
            nh = rnd.randint(6, 9)
            for j in range(nh):
                u = 0.2 + 0.8 * j / max(1, nh - 1)
                x, y = pts[min(n - 1, int(u * (n - 1)))]
                lado = -1 if j % 2 else 1
                ang = ang0 + lado * rnd.uniform(0.6, 1.1) * (1.0 - 0.5 * u) + rnd.uniform(-0.15, 0.15)
                piezas.append((rnd.random(), x, y, ang, u))
            piezas.append((rnd.random(), pts[-1][0], pts[-1][1], ang0 + rnd.uniform(-0.2, 0.2), 1.0))
        piezas.sort()
        for _, x, y, ang, u in piezas:
            hoja = hojas[rnd.randrange(len(hojas))]
            esc = rnd.uniform(0.27, 0.36) * k * escala * (1.0 - 0.2 * u)
            hh = hoja[1].shape[0] * esc
            tinte = np.array(tintes[rnd.randrange(len(tintes))]) * rnd.uniform(0.8, 1.08)
            pegar(sub_c, sub_a, sub_n, hoja, x - math.sin(ang) * hh * 0.5, y - math.cos(ang) * hh * 0.5, ang, esc, tuple(tinte))
        C[oy:oy + medio, ox:ox + medio] = sub_c
        A[oy:oy + medio, ox:ox + medio] = borde_suave(sub_a)
        N[oy:oy + medio, ox:ox + medio] = sub_n
    return C, A, N


def ramillete_conifera(ramitas, semilla, tam):
    """ramas de abeto (2x2 variantes): tallo central y ramitas en espiga, mas cortas hacia la punta"""
    rnd = random.Random(semilla)
    C, A, N = lienzos(tam)
    medio = tam // 2
    k = medio / 512
    for q in range(4):
        ox, oy = (q % 2) * medio, (q // 2) * medio
        sub_c, sub_a, sub_n = lienzos(medio)
        base = (medio * 0.5, medio * 0.99)
        punta = (medio * 0.5 + rnd.uniform(-20, 20) * k, medio * 0.2)
        tallo = curva(base, (medio * 0.5 + rnd.uniform(-40, 40) * k, medio * 0.55), punta, 12)
        marron = (0.20, 0.14, 0.09)
        for i in range(11):
            linea(sub_c, sub_a, tallo[i], tallo[i + 1], 4.5 * k * (1 - i / 12) + 1.0, 4.5 * k * (1 - (i + 1) / 12) + 1.0, marron)
        piezas = []
        n = 14
        for j in range(n):
            t = 0.08 + 0.9 * j / (n - 1)
            x, y = tallo[min(11, int(t * 11))]
            lado = -1 if j % 2 else 1
            ang = lado * rnd.uniform(0.85, 1.2) * (1.0 - 0.35 * t)
            piezas.append((rnd.random(), x, y, ang, t))
        piezas.append((2.0, punta[0], punta[1], rnd.uniform(-0.1, 0.1), 1.0))
        piezas.sort()
        for _, x, y, ang, t in piezas:
            r = ramitas[rnd.randrange(len(ramitas))]
            esc = (0.24 + 0.16 * (1 - t)) * k * rnd.uniform(0.85, 1.1)
            hh = r[1].shape[0] * esc
            v = rnd.uniform(0.82, 1.08)
            pegar(sub_c, sub_a, sub_n, r, x - math.sin(ang) * hh * 0.45, y - math.cos(ang) * hh * 0.45, ang, esc, (v * 0.95, v, v * 0.92))
        C[oy:oy + medio, ox:ox + medio] = sub_c
        A[oy:oy + medio, ox:ox + medio] = borde_suave(sub_a)
        N[oy:oy + medio, ox:ox + medio] = sub_n
    return C, A, N


def ramitas_de_abeto(hojas):
    """las ramitas del atlas LeafSet019 vienen acostadas (apuntan a la derecha): se paran apuntando arriba"""
    paradas = []
    for c, a, n in hojas:
        c2 = np.rot90(c, 1).copy()
        a2 = np.rot90(a, 1).copy()
        n2 = np.rot90(n, 1).copy()
        nx, ny = n2[..., 0] * 2 - 1, n2[..., 1] * 2 - 1
        n2 = np.dstack([(-ny) * 0.5 + 0.5, nx * 0.5 + 0.5, n2[..., 2]])
        paradas.append((c2, a2, n2))
    return paradas


def preparar_capas(desc, salida):
    os.makedirs(salida, exist_ok=True)
    print("capas de textura en", salida)
    capa_material(desc, "bark_brown_02", salida, "corteza")
    capa_material(desc, "pine_bark", salida, "corteza_pino")
    capa_material(desc, "forrest_ground_01", salida, "suelo")
    capa_material(desc, "forest_leaves_02", salida, "suelo2")
    capa_acg(desc, "Moss002", salida, "musgo")
    capa_material(desc, "rock_moss_set_01", salida, "roca")
    capa_material(desc, "tree_stump_01", salida, "tocon")
    capa_material(desc, "dead_tree_trunk", salida, "tronco")
    # helecho: color + alfa del modelo de Poly Haven
    color = leer(ph(desc, "fern_02", "Diffuse"))
    alfa = leer(ph(desc, "fern_02", "Alpha"), "L")
    n = leer(ph(desc, "fern_02", "nor_gl"))
    c2, n2 = sangrar(color, alfa, n)
    guardar(np.dstack([c2, alfa]), os.path.join(salida, "helecho_color.png"))
    r = leer(ph(desc, "fern_02", "Rough"), "L")
    guardar(np.dstack([n2, r]), os.path.join(salida, "helecho_normal.png"))
    # ramilletes de hojas anchas (haya, LeafSet024) y de abeto (LeafSet019)
    hojas = hojas_sueltas(desc, "LeafSet024")
    print("  hojas sueltas LeafSet024:", len(hojas))
    verdes = [(1.0, 1.0, 1.0), (0.92, 1.02, 0.85), (1.05, 1.03, 0.9), (0.85, 0.95, 0.8)]
    c, a, n = ramillete_hoja_ancha(hojas, 7, TAM, verdes)
    c, n = sangrar(c, a, n)
    guardar(np.dstack([c, a]), os.path.join(salida, "hojas_color.png"))
    guardar(np.dstack([n, np.full(a.shape, 0.55, np.float32)]), os.path.join(salida, "hojas_normal.png"))
    ramitas = ramitas_de_abeto(hojas_sueltas(desc, "LeafSet019"))
    print("  ramitas LeafSet019:", len(ramitas))
    c, a, n = ramillete_conifera(ramitas, 11, TAM)
    c, n = sangrar(c, a, n)
    guardar(np.dstack([c, a]), os.path.join(salida, "abeto_color.png"))
    guardar(np.dstack([n, np.full(a.shape, 0.6, np.float32)]), os.path.join(salida, "abeto_normal.png"))
    # hojarasca de otono (LeafSet030) y hiedra (LeafSet017) para el sotobosque: un ramillete mas
    hiedra = hojas_sueltas(desc, "LeafSet017")
    print("  hojas LeafSet017:", len(hiedra))
    c, a, n = ramillete_hoja_ancha(hiedra, 23, TAM, [(1.0, 1.0, 1.0), (0.9, 1.0, 0.85)])
    c, n = sangrar(c, a, n)
    guardar(np.dstack([c, a]), os.path.join(salida, "arbusto_color.png"))
    guardar(np.dstack([n, np.full(a.shape, 0.5, np.float32)]), os.path.join(salida, "arbusto_normal.png"))


def preparar_mallas(desc, salida, blender):
    script = os.path.join(AQUI, "bosque_mallas_blender.py")
    for nombre, tris in (("fern_02", 1400), ("rock_moss_set_01", 2400), ("tree_stump_01", 1600), ("dead_tree_trunk", 2400)):
        gltf = [f for f in os.listdir(os.path.join(desc, "polyhaven", nombre, "gltf")) if f.endswith(".gltf")][0]
        destino = os.path.join(salida, nombre + ".malla")
        subprocess.run([blender, "-b", "--factory-startup", "-noaudio", "--python", script, "--",
                        os.path.join(desc, "polyhaven", nombre, "gltf", gltf), destino, str(tris)], check=True)


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--descargas", default=r"D:\Yugi-Reload\terceros\bosque\descargas")
    p.add_argument("--assets", default=r"D:\Docker\compartido\classic-assets")
    p.add_argument("--blender", default=r"D:\tools\blender-5.2.2\blender.exe")
    p.add_argument("--solo", choices=["bajar", "capas", "mallas"], default=None)
    a = p.parse_args()
    salida = os.path.join(a.assets, "bosque")
    if a.solo in (None, "bajar"):
        bajar_polyhaven(a.descargas)
        bajar_ambientcg(a.descargas)
    if a.solo in (None, "capas"):
        preparar_capas(a.descargas, salida)
    if a.solo in (None, "mallas"):
        preparar_mallas(a.descargas, salida, a.blender)


if __name__ == "__main__":
    main()
