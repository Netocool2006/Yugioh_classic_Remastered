# Yugi Reload - arma el flipbook de un efecto del Niagara Examples Pack a partir de los cuadros crudos que graba
# hornear_niagara.py (dentro de Unreal). Python del sistema con numpy + PIL.
#   python -I componer_flipbook.py <crudo/efecto> <Saved/Horneado> <assets/fx del juego>
#   python -I componer_flipbook.py --texturas <crudo/_texturas> <Saved/Horneado> <assets/fx>
# Salida por efecto: <efecto>.png (atlas RGBA: RGB premultiplicado lineal codificado sRGB, A lineal; se
# muestrea como R8G8B8A8_SRGB y se mezcla con ONE, ONE_MINUS_SRC_ALPHA) y <efecto>.json (cuadros, fps, loop,
# origen...). En Saved/Horneado ademas una vista previa sobre gris para revisar a ojo.
import json, os, sys, shutil
import numpy as np
from PIL import Image


def srgb(x):
    x = np.clip(x, 0.0, 1.0)
    return np.where(x <= 0.0031308, 12.92 * x, 1.055 * np.power(x, 1 / 2.4) - 0.055)


def reducir(img, k):
    if k == 1:
        return img
    h, w, c = img.shape
    return img.reshape(h // k, k, w // k, k, c).mean(axis=(1, 3))


def cuadro(carpeta, i, res):
    neg = np.fromfile(os.path.join(carpeta, "%03d_n.f32" % i), dtype=np.float32).reshape(res, res, 4)[..., :3]
    bla = np.fromfile(os.path.join(carpeta, "%03d_b.f32" % i), dtype=np.float32).reshape(res, res, 4)[..., :3]
    neg = np.nan_to_num(np.maximum(neg, 0.0))
    bla = np.nan_to_num(np.maximum(bla, 0.0))
    # W: el blanco del fondo, de los bordes donde el efecto no pone nada
    borde = np.concatenate([bla[0], bla[-1], bla[:, 0], bla[:, -1]])
    bneg = np.concatenate([neg[0], neg[-1], neg[:, 0], neg[:, -1]])
    libres = borde[bneg.sum(1) < 1e-3]
    w = float(np.median(libres[:, 0])) if len(libres) > 8 else float(np.median(borde[:, 0]))
    w = max(w, 1e-6)
    a = np.clip(1.0 - (bla - neg).mean(axis=2, keepdims=True) / w, 0.0, 1.0)
    c = neg / w
    return np.concatenate([c, a], axis=2)


def componer(carpeta, salida, juego):
    m = json.load(open(os.path.join(carpeta, "meta.json"), encoding="utf-8"))
    res, lado, g, n = m["res_captura"], m["lado"], m["grilla"], m["cuadros"]
    k = max(1, res // lado)
    cuadros = [reducir(cuadro(carpeta, i, res), k) for i in range(m["cuadros_grabados"])]
    if m["loop"] and len(cuadros) > n:
        # loop sin salto: la cola grabada despues del final se funde sobre los primeros cuadros
        extra = len(cuadros) - n
        for i in range(extra):
            t = (i + 0.5) / extra
            cuadros[i] = cuadros[n + i] * (1 - t) + cuadros[i] * t
    cuadros = cuadros[:n]
    pila = np.stack(cuadros)
    # brillo: lo emisivo (fuego, rayos) pasa de 1; se comprime con 1 - e^-x despues de normalizar el pico
    lum = pila[..., :3].max(axis=3)
    vis = lum[pila[..., 3] > 0.02] if (pila[..., 3] > 0.02).any() else lum.ravel()
    pico = float(np.percentile(lum[lum > 1e-4], 99.5)) if (lum > 1e-4).any() else 1.0
    # el pico (lo mas brillante del efecto, percentil 99.5) queda cerca de 1: aprovecha los 8 bits; el juego puede
    # volver al brillo de Unreal dividiendo por "ganancia" (va en el json)
    # ... pero lo que tapa (humo, polvo: alfa alto) no puede quedar mas claro que su alfa (seria luz): su "albedo"
    # (color / alfa) se limita a ~0.85
    ganancia = 1.0 / max(pico, 1e-4)
    tapa = pila[..., 3] > 0.3
    if tapa.sum() > 50:
        albedo = float(np.percentile(lum[tapa] / pila[..., 3][tapa], 98))
        ganancia = min(ganancia, 0.85 / max(albedo, 1e-4))
    ganancia = float(np.clip(ganancia, 0.5, 8.0))
    c = pila[..., :3] * ganancia
    c = np.where(c > 0.6, 0.6 + 0.4 * (1 - np.exp(-(c - 0.6) / 0.4)), c)   # rodilla suave arriba de 0.6
    a = pila[..., 3:4]
    atlas = np.zeros((g * lado, g * lado, 4), np.float32)
    for i in range(n):
        y, x = divmod(i, g)
        atlas[y * lado:(y + 1) * lado, x * lado:(x + 1) * lado, :3] = c[i]
        atlas[y * lado:(y + 1) * lado, x * lado:(x + 1) * lado, 3:] = a[i]
    rgba = np.concatenate([srgb(atlas[..., :3]), np.clip(atlas[..., 3:], 0, 1)], axis=2)
    img = Image.fromarray((rgba * 255 + 0.5).astype(np.uint8), "RGBA")
    nombre = m["efecto"]
    os.makedirs(salida, exist_ok=True)
    img.save(os.path.join(salida, nombre + ".png"), optimize=True)
    # vista previa sobre gris oscuro (premultiplicado: c + (1-a) fondo)
    fondo = 0.06
    prev = atlas[..., :3] + (1 - atlas[..., 3:]) * fondo
    Image.fromarray((srgb(prev) * 255 + 0.5).astype(np.uint8), "RGB").save(os.path.join(salida, nombre + "_vista.jpg"), quality=85)
    # caja util de todo el flipbook (para recortar el cuadrado en el juego si sobra)
    cob = (a[..., 0] > 0.01) | (c.max(axis=3) > 0.01)
    ys, xs = np.nonzero(cob.any(axis=0))
    caja = [float(xs.min()) / lado, float(ys.min()) / lado, float(xs.max() + 1) / lado, float(ys.max() + 1) / lado] if len(xs) else [0, 0, 1, 1]
    aditivo = float((c.max(axis=3) > a[..., 0] + 0.05).mean() / max(cob.mean(), 1e-6))
    info = {k2: m[k2] for k2 in ("efecto", "ruta", "uso", "tipo", "vista", "loop", "duracion", "tamano_cm", "origen")}
    info.update({"atlas": nombre + ".png", "columnas": g, "filas": g, "cuadros": n, "fps": round(m["fps"], 3),
                 "lado_cuadro": lado, "ancho_atlas": g * lado, "ganancia": round(ganancia, 4), "caja": caja,
                 "parte_aditiva": round(aditivo, 3),
                 "formato": "RGBA8 sRGB, RGB premultiplicado (lineal), A lineal; mezcla ONE, ONE_MINUS_SRC_ALPHA",
                 "fuente": "Niagara Examples Pack (Epic Games, Fab) horneado con herramientas/unreal/hornear_niagara.py"})
    json.dump(info, open(os.path.join(salida, nombre + ".json"), "w", encoding="utf-8"), indent=1, ensure_ascii=False)
    if juego:
        os.makedirs(juego, exist_ok=True)
        for ext in (".png", ".json"):
            shutil.copy2(os.path.join(salida, nombre + ext), os.path.join(juego, nombre + ext))
    # los crudos ya no hacen falta (pesan ~1 MB por cuadro)
    if os.environ.get("YUGI_FX_GUARDAR_CRUDO") != "1":
        for f in os.listdir(carpeta):
            if f.endswith(".f32"):
                os.remove(os.path.join(carpeta, f))
    print("%s: %dx%d cuadros de %d px, %.1f fps, ganancia %.3f, pico %.2f" % (nombre, g, g, lado, m["fps"], ganancia, pico))


# OpenEXR no viene con python: pip install --target D:/tools/pylib_exr OpenEXR
EXR = os.environ.get("YUGI_PYLIB_EXR", "D:/tools/pylib_exr")


def leer_exr(ruta):
    """RGBA float32 de un .EXR (export_assets de Unreal los saca asi). OpenEXR va aparte (ver EXR)."""
    if EXR not in sys.path:
        sys.path.append(EXR)
    import OpenEXR
    f = OpenEXR.File(ruta)
    ch = f.parts[0].channels
    a = ch["RGBA"].pixels if "RGBA" in ch else np.stack([ch[k].pixels for k in "RGBA"], axis=-1)
    return np.nan_to_num(a.astype(np.float32))


def guardar_pack(nombre, rgba_lineal, salida, juego, fps, uso):
    """atlas 8x8 (premultiplicado lineal) de un sistema que el pack ya trae horneado -> png sRGB + json como los demas"""
    lado_atlas = 2048
    a = rgba_lineal
    k = a.shape[0] // lado_atlas
    if k > 1:
        a = reducir(a, k)
    g, n, lado = 8, 64, lado_atlas // 8
    rgba = np.concatenate([srgb(a[..., :3]), np.clip(a[..., 3:], 0, 1)], axis=2)
    for d in (salida, juego):
        if not d:
            continue
        os.makedirs(d, exist_ok=True)
        Image.fromarray((rgba * 255 + 0.5).astype(np.uint8), "RGBA").save(os.path.join(d, nombre + ".png"), optimize=True)
    cob = (a[..., 3] > 0.01).reshape(g, lado, g, lado).any(axis=(0, 2))
    ys, xs = np.nonzero(cob)
    caja = [float(xs.min()) / lado, float(ys.min()) / lado, float(xs.max() + 1) / lado, float(ys.max() + 1) / lado]
    info = {"efecto": nombre, "ruta": "/Game/NiagaraExamples/Utilities/SpriteGeneration (BakedAtlas del pack)", "uso": uso,
            "tipo": "loop", "vista": "lado", "loop": True, "duracion": n / fps, "tamano_cm": 100.0, "origen": [0.5, 0.5],
            "atlas": nombre + ".png", "columnas": g, "filas": g, "cuadros": n, "fps": fps, "lado_cuadro": lado,
            "ancho_atlas": lado_atlas, "ganancia": 1.0, "caja": caja,
            "formato": "RGBA8 sRGB, RGB premultiplicado (lineal), A lineal; mezcla ONE, ONE_MINUS_SRC_ALPHA",
            "fuente": "Niagara Examples Pack (Epic Games, Fab): atlas ya horneado del pack, compuesto con componer_flipbook.py"}
    for d in (salida, juego):
        if d:
            json.dump(info, open(os.path.join(d, nombre + ".json"), "w", encoding="utf-8"), indent=1, ensure_ascii=False)
    print("%s: %dx%d cuadros de %d px (del pack)" % (nombre, g, g, lado))


def texturas(carpeta, salida, juego):
    """Los atlas que el pack ya trae horneados (SpriteGeneration: ExplosionRoil y SmokePuffLight, 8x8 en loop):
    se combinan como su material (el emisivo del ExplosionRoil es el fuego; el resto, humo iluminado)."""
    exr = {os.path.splitext(f)[0]: os.path.join(r, f) for r, _, fs in os.walk(carpeta) for f in fs if f.lower().endswith(".exr")}
    if "NS_SmokePuffLight_BakedAtlas_Smoke_2048_RGB" in exr:
        a = leer_exr(exr["NS_SmokePuffLight_BakedAtlas_Smoke_2048_RGB"])
        al = np.clip(a[..., 3:4], 0, 1)
        c = np.clip(a[..., :3] * 2.2, 0, 1) * al          # humo gris iluminado (el horneado esta oscuro: ~0.37)
        guardar_pack("NS_SmokePuffLight", np.concatenate([c, al], axis=2), salida, juego, 16.0, "bocanada de humo (humo())")
    if "NS_ExplosionRoil_BakedAtlas_Alpha" in exr and "NS_ExplosionRoil_BakedAtlas_Emissive" in exr:
        a = leer_exr(exr["NS_ExplosionRoil_BakedAtlas_Alpha"])
        e = leer_exr(exr["NS_ExplosionRoil_BakedAtlas_Emissive"])[..., 0:1]
        al = np.clip(a[..., 3:4], 0, 1)
        humo = np.clip(a[..., :3], 0, 1) * 0.4 * al
        q = np.clip(e / 0.6, 0, 1)
        fuego = q ** 1.2 * np.array([2.6, 1.1, 0.3], np.float32) + q ** 3 * np.array([1.2, 1.1, 0.8], np.float32)
        c = humo * (1 - np.clip(q * 1.5, 0, 1)) + fuego * al
        guardar_pack("NS_ExplosionRoil", np.concatenate([np.clip(c, 0, 1), al], axis=2), salida, juego, 16.0, "bola de fuego que rueda")


if __name__ == "__main__":
    if sys.argv[1] == "--texturas":
        texturas(sys.argv[2], sys.argv[3], sys.argv[4] if len(sys.argv) > 4 else "")
    else:
        componer(sys.argv[1], sys.argv[2], sys.argv[3] if len(sys.argv) > 3 else "")
