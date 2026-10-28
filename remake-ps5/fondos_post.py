# Yugioh Classic Remaster (rama classic-fondos): post-proceso de un fondo (la imagen de Gemini) para el juego.
#   entrada: <nano-banana>\classic\fondos\<id>\resultado.png (o --entrada <imagen>)
#   salida : <classic-assets>\fondos\<id>.jpg        color sRGB (q92), ancho <= 8192
#            <classic-assets>\fondos\<id>_prof.png   distancia desde la camara de juego, 16 bits: log2(m) / 12; 65535 = cielo
#            <classic-assets>\fondos\<id>.txt        "plano <ancho> <abajo>", "origen", "cielo", "niebla", "tinte" (src/entornos.h)
# Pasos: 1) aspecto: si es mas alta que 21:9 se recorta ARRIBA (el cielo que no se ve); si es mas ancha que 3.2:1, los
#        costados. 2) escalado x4 con Real-ESRGAN (realesrgan-x4plus, el modelo de fotos) si mide menos de 6000 px de
#        ancho; luego LANCZOS a <= 8192. 3) profundidad con Depth Anything V2 (ONNX, CPU) -> distancia en metros
#        (DMIN tras la mesa .. DMAX al fondo; el cielo, sin distancia). 4) colores del cielo, del horizonte y de la luz.
# Uso: python -I fondos_post.py <id> [--entrada imagen] [--sin-escalar]
#      python -I fondos_post.py --prueba   (dos fondos de prueba desde fotos CC0 de Poly Haven: prueba_plaza, prueba_valle)
import math, os, subprocess, sys
sys.path.insert(0, r"D:\Yugi-Reload\herramientas\pylib")
import numpy as np
from PIL import Image
Image.MAX_IMAGE_PIXELS = None

NB = r"D:\Yugi-Reload\nano-banana\classic\fondos"
ASSETS = r"D:\Docker\compartido\classic-assets"
DESTINO = os.path.join(ASSETS, "fondos")
ESRGAN = r"D:\Yugi-Reload\herramientas\esrgan\realesrgan-ncnn-vulkan.exe"
MODELO_PROF = r"D:\Yugi-Reload\herramientas\modelos\depth-anything-v2-small.onnx"
ANCHO_GRADOS = 110.0     # acimut que cubre la imagen (la camara de juego ve unos -53..+38 grados)
ABAJO_GRADOS = -28.0     # elevacion del borde de abajo: la mesa tapa hasta -18 (el 25% de abajo); se ve hasta el 85%
OJO = (0.0, 6.4 * 1.4 * 1.08, 12.8 * 1.4 * 1.08)   # la camara de juego (yugireload.cpp: OJO)
DMIN, DMAX = 45.0, 4000.0
ASPECTO_MIN, ASPECTO_MAX = 21 / 9, 3.2
ANCHO_MAX = 8192


def aspecto(im):
    a = im.width / im.height
    if a < ASPECTO_MIN - 0.02:   # mas alta: se quita cielo de arriba
        h = round(im.width / ASPECTO_MIN)
        return im.crop((0, im.height - h, im.width, im.height))
    if a > ASPECTO_MAX:
        w = round(im.height * ASPECTO_MAX)
        x = (im.width - w) // 2
        return im.crop((x, 0, x + w, im.height))
    return im


def escalar(im, tmp):
    if im.width < 6000:
        ent, sal = tmp + "_ent.png", tmp + "_x4.png"
        if im.width > ANCHO_MAX // 4:   # x4 da justo el ancho maximo (Real-ESRGAN tarda segun la entrada)
            im = im.resize((ANCHO_MAX // 4, round(im.height * ANCHO_MAX / 4 / im.width)), Image.LANCZOS)
        im.save(ent)
        r = subprocess.run([ESRGAN, "-i", ent, "-o", sal, "-n", "realesrgan-x4plus", "-s", "4", "-t", "200"],
                           capture_output=True, text=True, cwd=os.path.dirname(ESRGAN))
        os.remove(ent)
        if r.returncode != 0 or not os.path.exists(sal):
            print("Real-ESRGAN fallo (%s); se escala con LANCZOS" % r.stderr[-200:])
        else:
            im = Image.open(sal).convert("RGB")
            im.load()
            os.remove(sal)
    if im.width > ANCHO_MAX:
        im = im.resize((ANCHO_MAX, round(im.height * ANCHO_MAX / im.width)), Image.LANCZOS)
    elif im.width < 4096:
        im = im.resize((4096, round(im.height * 4096 / im.width)), Image.LANCZOS)
    return im


def profundidad(im):
    """Depth Anything V2 (small): profundidad inversa relativa (1 = lo mas cerca), al tamano de entrada pedido."""
    import onnxruntime as ort
    s = ort.InferenceSession(MODELO_PROF, providers=["CPUExecutionProvider"])
    h = 518
    w = int(round(h * im.width / im.height / 14)) * 14
    x = np.asarray(im.resize((w, h), Image.BICUBIC)).astype(np.float32) / 255.0
    x = (x - np.array([0.485, 0.456, 0.406], np.float32)) / np.array([0.229, 0.224, 0.225], np.float32)
    y = s.run(None, {s.get_inputs()[0].name: x.transpose(2, 0, 1)[None]})[0][0]
    lo, hi = np.percentile(y, 1), np.percentile(y, 99.5)
    return np.clip((y - lo) / max(1e-6, hi - lo), 0.0, 1.0)


def desenfocar(a, sigma):
    r = int(3 * sigma)
    k = np.exp(-0.5 * (np.arange(-r, r + 1) / sigma) ** 2)
    k /= k.sum()
    p = np.pad(a, r, mode="edge")
    p = sum(k[i] * p[:, i:i + a.shape[1]] for i in range(2 * r + 1))
    return sum(k[i] * p[i:i + a.shape[0], :] for i in range(2 * r + 1))


def distancias(n, ancho):
    """inversa relativa -> metros (log-lineal entre DMIN y DMAX) -> png16; el cielo (casi 0, en la mitad de arriba) sin distancia."""
    h, w = n.shape
    alto = round(ancho * h / w)
    im = Image.fromarray((n * 65535).astype(np.uint16)).resize((ancho, alto), Image.BICUBIC)
    n = np.asarray(im).astype(np.float32) / 65535.0
    n = desenfocar(n, 1.5)
    d = DMIN * (DMAX / DMIN) ** (1.0 - n)
    v = np.clip(np.log2(d) / 12.0, 0.0, 0.9999)
    filas = np.arange(alto)[:, None] / alto
    cielo = (n < 0.03) & (filas < 0.65)
    q = np.where(cielo, 65535, np.round(v * 65535)).astype(np.uint16)
    return Image.fromarray(q), float(cielo.mean())


def colores(im):
    a = np.asarray(im.resize((512, round(512 * im.height / im.width)), Image.BILINEAR)).astype(np.float32) / 255.0
    h = a.shape[0]
    cielo = a[: max(2, h // 16)].reshape(-1, 3).mean(0)
    horiz = round(h * 0.45)   # la banda media (horizonte o pie del sujeto)
    niebla = a[max(0, horiz - h // 12): horiz + h // 12].reshape(-1, 3).mean(0)
    lin = a.reshape(-1, 3) ** 2.2
    lum = lin @ np.array([0.2126, 0.7152, 0.0722], np.float32)
    claros = lin[lum >= np.percentile(lum, 95)].mean(0)
    tinte = claros / max(1e-4, claros.max())
    return cielo, niebla, tinte


def procesar(id, entrada, sin_escalar=False):
    os.makedirs(DESTINO, exist_ok=True)
    im = aspecto(Image.open(entrada).convert("RGB"))
    print("%s: %dx%d (aspecto %.2f)" % (id, im.width, im.height, im.width / im.height))
    im = im if sin_escalar else escalar(im, os.path.join(DESTINO, "_" + id))
    if im.width > ANCHO_MAX:
        im = im.resize((ANCHO_MAX, round(im.height * ANCHO_MAX / im.width)), Image.LANCZOS)
    n = profundidad(im)
    prof, cielo_frac = distancias(n, 2048)
    cielo, niebla, tinte = colores(im)
    im.save(os.path.join(DESTINO, id + ".jpg"), quality=92, subsampling=0)
    prof.save(os.path.join(DESTINO, id + "_prof.png"))
    with open(os.path.join(DESTINO, id + ".txt"), "w", encoding="utf-8") as f:
        f.write("# fondos_post.py: %s (%dx%d)\n" % (os.path.basename(os.path.dirname(entrada)) + "/" + os.path.basename(entrada), im.width, im.height))
        f.write("plano %.2f %.2f\n" % (ANCHO_GRADOS, ABAJO_GRADOS))
        f.write("origen %.4f %.4f %.4f\n" % OJO)
        f.write("cielo %.3f %.3f %.3f\n" % tuple(cielo))
        f.write("niebla %.3f %.3f %.3f\n" % tuple(niebla))
        f.write("tinte %.3f %.3f %.3f\n" % tuple(tinte))
    print("%s listo: %dx%d, cielo %.0f%% de la imagen -> %s" % (id, im.width, im.height, cielo_frac * 100, DESTINO))


# ------------------------------------------------------------------ fondos de prueba (fotos CC0 de Poly Haven)
def recorte_cilindrico(equi, ancho_px, centro_grados):
    """De un panorama equirectangular, el recorte con la MISMA proyeccion que usa el juego (cilindro, v lineal en tan)."""
    W, H = equi.size
    A = ASPECTO_MIN
    alto_px = round(ancho_px / A)
    span = math.radians(ANCHO_GRADOS)
    t_abajo = math.tan(math.radians(ABAJO_GRADOS))
    t_arriba = t_abajo + span / A
    xs = (np.arange(ancho_px) + 0.5) / ancho_px
    ys = (np.arange(alto_px) + 0.5) / alto_px
    az = (xs - 0.5) * span + math.radians(centro_grados)
    el = np.arctan(t_arriba - ys * (t_arriba - t_abajo))
    u = ((az / (2 * math.pi) + 0.5) % 1.0) * (W - 1)
    v = (0.5 - el / math.pi) * (H - 1)
    a = np.asarray(equi).astype(np.float32)
    uu, vv = np.meshgrid(u, v)
    x0, y0 = np.floor(uu).astype(int), np.floor(vv).astype(int)
    fx, fy = (uu - x0)[..., None], (vv - y0)[..., None]
    x1, y1 = np.minimum(x0 + 1, W - 1), np.minimum(y0 + 1, H - 1)
    c = (a[y0, x0] * (1 - fx) * (1 - fy) + a[y0, x1] * fx * (1 - fy) + a[y1, x0] * (1 - fx) * fy + a[y1, x1] * fx * fy)
    return Image.fromarray(np.clip(c, 0, 255).astype(np.uint8))


def prueba():
    src = os.path.join(ASSETS, "fondos_src", "polyhaven")
    for id, archivo, centro in (("prueba_plaza", "piazza_san_marco.jpg", 4.0), ("prueba_valle", "neurathen_rock_castle.jpg", -34.0)):
        equi = Image.open(os.path.join(src, archivo)).convert("RGB")
        rec = recorte_cilindrico(equi, 2600, centro)
        del equi
        ruta = os.path.join(src, id + "_recorte.png")
        rec.save(ruta)
        procesar(id, ruta)


if __name__ == "__main__":
    if "--prueba" in sys.argv:
        prueba()
    else:
        id = sys.argv[1]
        ent = sys.argv[sys.argv.index("--entrada") + 1] if "--entrada" in sys.argv else os.path.join(NB, id, "resultado.png")
        procesar(id, ent, "--sin-escalar" in sys.argv)
