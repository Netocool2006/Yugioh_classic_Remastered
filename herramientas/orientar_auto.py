# ORIENTACION AUTOMATICA: todo monstruo debe mirar a la pantalla (frente del modelo = glTF +Z, hacia la camara).
# Regla del usuario (2026-10-17): "no quiero revisar figura por figura" (Bladefly salio de espaldas).
# La figura de Gemini (nano-banana/todo/figura/<slug>/resultado.png) siempre muestra la CARA del monstruo (de frente o
# de tres cuartos). Se renderiza el modelo girado 0, 45, ... 315 grados con la camara del juego
# (blender/vistas_giro.py, colores planos de su textura) y se compara cada vista con la figura con DINOv2 (rasgos por
# zonas de 16x16: la misma parte del cuerpo en el mismo lugar), y tambien con la figura ESPEJADA: si la figura esta
# girada d grados hacia un lado, la espejada lo esta hacia el otro (los monstruos son simetricos), asi el frente es el
# promedio de los dos angulos que mejor coinciden. Se gira 180 grados (de espaldas -> de frente) cuando:
#   a) las vistas de atras (135..225) se parecen a la figura claramente mas que las de adelante (315..45), con la
#      figura y con la espejada; o
#   b) la figura esta bien girada (sus dos angulos difieren 60..150 grados) y el promedio cae de espaldas (+-30).
# Si no, se deja como esta (la gran mayoria ya mira de frente: un giro dudoso de 90 seria peor).
# El giro se aplica SIN PERDIDA al .glb: un nodo raiz nuevo "giro_frente_<g>" con la rotacion alrededor del eje
# vertical (el motor y vkglTF aplican las matrices de los nodos); un modelo que ya tiene ese nodo no se vuelve a mirar.
# Requiere torch + transformers; el modelo DINOv2 (85 MB) se guarda en D:\Yugi-Reload\cache_hf.
# Uso:
#   python -I orientar_auto.py <modelo.glb> [...] [--aplicar] [--vistas <carpeta con <slug>/giro_XXX.png>]
#     imprime "GIRO <slug> <grados> ..." por modelo y, con --aplicar, gira el archivo
#   como modulo: from orientar_auto import orientar   (devuelve los grados aplicados)
import json, math, os, shutil, struct, subprocess, sys, tempfile
import numpy as np
from PIL import Image, ImageFilter

DATOS = r"D:\Yugi-Reload"
os.environ.setdefault("HF_HOME", os.path.join(DATOS, "cache_hf"))
BLENDER = r"D:\tools\blender-5.2.2\blender.exe"
AQUI = os.path.dirname(os.path.abspath(__file__))
FIGURAS = os.path.join(DATOS, "nano-banana", "todo", "figura")
VISTAS = 8
_MODELO = None
MEDIA, DESV = np.array([0.485, 0.456, 0.406]), np.array([0.229, 0.224, 0.225])


def _dino():
    global _MODELO
    if _MODELO is None:
        from transformers import AutoModel
        _MODELO = AutoModel.from_pretrained("facebook/dinov2-small").eval()
    return _MODELO


def _preparar(rgb, mask, n=224):
    """recorta a la figura, la centra en un cuadrado sobre gris y la lleva a n x n; la mascara a 16 x 16"""
    ys, xs = np.nonzero(mask)
    y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
    L = int(max(y1 - y0, x1 - x0) * 1.05)
    cy, cx = (y0 + y1) // 2, (x0 + x1) // 2
    caja = (cx - L // 2, cy - L // 2, cx - L // 2 + L, cy - L // 2 + L)
    im = Image.fromarray(np.dstack([rgb, mask * 255]).astype(np.uint8), "RGBA").crop(caja)
    fondo = Image.new("RGBA", (L, L), (128, 128, 128, 255))
    fondo.paste(im, (0, 0), im)
    x = (np.asarray(fondo.convert("RGB").resize((n, n), Image.BICUBIC)) / 255.0 - MEDIA) / DESV
    m = np.asarray(Image.fromarray((mask * 255).astype(np.uint8)).crop(caja).resize((16, 16))) > 64
    return x.transpose(2, 0, 1)[None].astype(np.float32), m


def _rasgos(x):
    import torch
    with torch.no_grad():
        o = _dino()(pixel_values=torch.tensor(x)).last_hidden_state[0, 1:].reshape(16, 16, -1)
    return torch.nn.functional.normalize(o, dim=-1).numpy()


def _figura(ruta):
    im = Image.open(ruta).convert("RGB")
    im.thumbnail((448, 448))
    a = np.asarray(im.filter(ImageFilter.MedianFilter(3))).astype(float)
    fondo = np.median(np.concatenate([a[0], a[-1], a[:, 0], a[:, -1]]), 0)
    return _preparar(a.astype(np.uint8), np.sqrt(((a - fondo) ** 2).sum(-1)) > 38)


def _vista(ruta):
    a = np.asarray(Image.open(ruta).convert("RGBA"))
    return _preparar(a[..., :3], a[..., 3] > 128)


def _pico(r, permitido=None):
    r = np.array(r, dtype=float)
    if permitido is not None:
        r[~permitido] = -1.0
    i = int(np.argmax(r))
    a, b, c = r[i - 1], r[i], r[(i + 1) % len(r)]
    d = 0.5 * (a - c) / (a - 2 * b + c) if (a - 2 * b + c) < 0 and min(a, c) >= 0 else 0.0
    return ((i + d) * 360.0 / len(r)) % 360.0


def _dif(a, b):
    return abs((a - b + 180.0) % 360.0 - 180.0)


def puntajes(vistas, figura):
    """parecido (DINOv2 por zonas) de la figura y de la figura espejada con cada vista girada"""
    xf, mf = _figura(figura)
    pf = _rasgos(xf)
    pm, mm = _rasgos(xf[..., ::-1].copy()), mf[:, ::-1]
    r1, r2 = [], []
    for g in range(0, 360, 360 // VISTAS):
        xv, mv = _vista(os.path.join(vistas, "giro_%03d.png" % g))
        pv = _rasgos(xv)
        for r, p, m in ((r1, pf, mf), (r2, pm, mm)):
            ambos = m & mv
            r.append(float((p * pv).sum(-1)[ambos].mean()) if ambos.any() else 0.0)
    return np.array(r1), np.array(r2)


def elegir(r1, r2):
    """(grados, motivo): 180 si el modelo esta de espaldas, si no 0"""
    n = len(r1)
    adelante, atras = [n - 1, 0, 1], [n // 2 - 1, n // 2, n // 2 + 1]
    d1 = r1[atras].max() - r1[adelante].max()
    d2 = r2[atras].max() - r2[adelante].max()
    a1 = _pico(r1)
    a2 = _pico(r2, np.array([_dif(k * 360.0 / n, a1) <= 100 for k in range(n)]))
    fr = math.degrees(math.atan2(math.sin(math.radians(a1)) + math.sin(math.radians(a2)),
                                 math.cos(math.radians(a1)) + math.cos(math.radians(a2)))) % 360
    motivo = "a1=%.0f a2=%.0f frente=%.0f atras-adelante=%.2f/%.2f" % (a1, a2, fr, d1, d2)
    if d1 >= 0.06 and d2 >= 0.0:
        return 180, "de espaldas (vistas de atras) " + motivo
    if 60 <= _dif(a1, a2) <= 150 and _dif(fr, 180) <= 30:
        return 180, "de espaldas (figura girada) " + motivo
    return 0, motivo


def renderizar(glb, carpeta):
    r = subprocess.run([BLENDER, "--background", "--python", os.path.join(AQUI, "blender", "vistas_giro.py"), "--",
                        glb, carpeta, str(VISTAS), "192"], capture_output=True, text=True)
    if r.returncode != 0 or not os.path.exists(os.path.join(carpeta, "giro_000.png")):
        raise RuntimeError("vistas_giro.py fallo: " + r.stdout[-500:] + r.stderr[-500:])


def _leer_glb(ruta):
    datos = open(ruta, "rb").read()
    largo = struct.unpack_from("<I", datos, 12)[0]
    return json.loads(datos[20:20 + largo]), datos[20 + largo:]


def ya_girado(glb):
    g, _ = _leer_glb(glb)
    return next((n["name"] for n in g.get("nodes", []) if str(n.get("name", "")).startswith("giro_frente_")), None)


def girar_glb(entrada, salida, grados):
    """gira el modelo alrededor del eje vertical (glTF +Y) con un nodo raiz nuevo: sin reexportar nada"""
    g, resto = _leer_glb(entrada)
    th = math.radians(grados) / 2
    escena = g["scenes"][g.get("scene", 0)]
    g["nodes"].append({"name": "giro_frente_%d" % round(grados), "rotation": [0.0, math.sin(th), 0.0, math.cos(th)],
                       "children": escena["nodes"]})
    escena["nodes"] = [len(g["nodes"]) - 1]
    js = json.dumps(g, separators=(",", ":")).encode()
    js += b" " * ((4 - len(js) % 4) % 4)
    cuerpo = struct.pack("<II", len(js), 0x4E4F534A) + js + resto
    tmp = salida + ".tmp"
    open(tmp, "wb").write(struct.pack("<III", 0x46546C67, 2, 12 + len(cuerpo)) + cuerpo)
    os.replace(tmp, salida)


def decidir(glb, slug=None, vistas=None):
    """(grados, motivo) para un modelo; 0 si no tiene figura de Gemini o ya fue girado"""
    slug = slug or os.path.splitext(os.path.basename(glb))[0]
    hecho = ya_girado(glb)
    if hecho:
        return 0, "ya orientado (%s)" % hecho
    figura = os.path.join(FIGURAS, slug, "resultado.png")
    if not os.path.exists(figura):
        return 0, "sin figura de Gemini"
    tmp = None
    if not vistas or not os.path.exists(os.path.join(vistas, "giro_000.png")):
        tmp = vistas = tempfile.mkdtemp(prefix="vistas_")
        renderizar(glb, vistas)
    try:
        return elegir(*puntajes(vistas, figura))
    finally:
        if tmp:
            shutil.rmtree(tmp, ignore_errors=True)


def orientar(glb, slug=None, aplicar=True, vistas=None, copias=()):
    """decide y (si aplicar) gira el .glb y sus copias identicas; devuelve (grados, motivo)"""
    g, motivo = decidir(glb, slug, vistas)
    if g and aplicar:
        for c in copias:
            if os.path.exists(c) and open(c, "rb").read() == open(glb, "rb").read():
                girar_glb(c, c, g)
        girar_glb(glb, glb, g)
    return g, motivo


MODELOS = os.path.join(DATOS, "remake-ps5", "assets", "models", "yugi")
COPIAS = [r"D:\Docker\compartido\yugi-src\assets\models\yugi"]   # la copia que compila el contenedor
NO_SON = {"tablero", "esfera", "baculo", "espada", "rayo"}


def copias_de(slug):
    """las otras copias del mismo modelo (si son identicas se giran igual)"""
    return [os.path.join(c, slug + ".glb") for c in COPIAS] + \
        [os.path.join(DATOS, "nano-banana", "modelos3d", slug, "modelo_final.glb")]


def main():
    # --todos: todos los monstruos instalados (remake-ps5/assets/models/yugi) y sus copias identicas
    aplicar = "--aplicar" in sys.argv
    vistas = sys.argv[sys.argv.index("--vistas") + 1] if "--vistas" in sys.argv else None
    glbs = [a for a in sys.argv[1:] if a.lower().endswith(".glb")]
    if "--todos" in sys.argv:
        glbs = [os.path.join(MODELOS, f) for f in sorted(os.listdir(MODELOS))
                if f.endswith(".glb") and f[:-4] not in NO_SON and not f[:-4].endswith("_anim")]
    for glb in glbs:
        slug = os.path.splitext(os.path.basename(glb))[0]
        v = os.path.join(vistas, slug) if vistas else None
        try:
            g, motivo = orientar(glb, slug, aplicar, v, copias_de(slug))
            print("GIRO", slug, g, motivo, "(aplicado)" if g and aplicar else "")
        except Exception as ex:
            print("GIRO", slug, "FALLO", ex)


if __name__ == "__main__":
    main()
