# Yugioh Classic Remaster (rama classic-personajes): recorta los retratos de los personajes y los deja para el juego.
#   entrada: nano-banana\classic\personajes\<id>\resultado.png (Gemini, fondo de croma verde o magenta: fondo.txt)
#            o, si aun no hay, la foto original (D:\Yugi-Reload\Personajes) como PROVISIONAL
#   recorte: isnet-anime (herramientas\modelos, onnxruntime de herramientas\pylib) combinado con la llave de croma, y
#            "despill" del color de la llave en los bordes
#   salida:  D:\Docker\compartido\classic-assets\personajes\retrato_<id>.png (RGBA, 1400 px de alto, recortado al
#            personaje) y origen_<id>.txt (gemini|provisional). No va al repo.
# Uso: python -I retratos_post.py [--solo id1,id2] [--todo]   (sin --todo solo rehace los que cambiaron)
import os, sys
sys.path.insert(0, r"D:\Yugi-Reload\herramientas\pylib")
AQUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, AQUI)
import numpy as np                 # noqa: E402
import onnxruntime as ort          # noqa: E402
from PIL import Image, ImageFilter  # noqa: E402
import personajes_datos as pd      # noqa: E402

PJ = r"D:\Yugi-Reload\nano-banana\classic\personajes"
FOTOS = r"D:\Yugi-Reload\Personajes"
SALIDA = r"D:\Docker\compartido\classic-assets\personajes"
MODELO = r"D:\Yugi-Reload\herramientas\modelos\isnet-anime.onnx"
ALTO = 1400
SOLO = set(sys.argv[sys.argv.index("--solo") + 1].split(",")) if "--solo" in sys.argv else None
TODO = "--todo" in sys.argv


def mascara(sesion, img):
    """isnet-anime: entrada 1024x1024, media (0.485,0.456,0.406), desviacion 1 (como poc-godot/preparar_cartas.py)."""
    x = np.asarray(img.convert("RGB").resize((1024, 1024), Image.LANCZOS)).astype(np.float32) / 255.0
    x = (x - np.array([0.485, 0.456, 0.406], np.float32)) / np.array([1.0, 1.0, 1.0], np.float32)
    y = sesion.run(None, {sesion.get_inputs()[0].name: x.transpose(2, 0, 1)[None]})[0][0, 0]
    y = (y - y.min()) / max(1e-6, y.max() - y.min())
    return np.asarray(Image.fromarray((y * 255).astype(np.uint8)).resize(img.size, Image.LANCZOS)).astype(np.float32) / 255.0


def croma(rgb, llave):
    """alfa por distancia al color de la llave (0 = fondo) y el rgb sin el tinte de la llave en los bordes."""
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    if llave == "verde":
        exceso = g - np.maximum(r, b)
    else:   # magenta
        exceso = np.minimum(r, b) - g
    alfa = 1.0 - np.clip((exceso - 0.18) / 0.30, 0.0, 1.0)
    limpio = rgb.copy()
    if llave == "verde":
        limpio[..., 1] = np.minimum(g, np.maximum(r, b) + 0.04)
    else:
        tope = g + 0.04
        limpio[..., 0] = np.minimum(r, np.maximum(tope, b))
        limpio[..., 2] = np.minimum(b, tope)
    return alfa, limpio


def procesar(sesion, p):
    gem = os.path.join(PJ, p["id"], "resultado.png")
    fuente, origen = (gem, "gemini") if os.path.exists(gem) else (os.path.join(FOTOS, p["foto"]), "provisional")
    firma = "%s|%s|%d" % (origen, fuente, os.path.getmtime(fuente))
    p_or = os.path.join(SALIDA, "origen_%s.txt" % p["id"])
    destino = os.path.join(SALIDA, "retrato_%s.png" % p["id"])
    if not TODO and os.path.exists(destino) and os.path.exists(p_or) and open(p_or, encoding="utf-8").read() == firma:
        return None
    img = Image.open(fuente).convert("RGB")
    rgb = np.asarray(img).astype(np.float32) / 255.0
    a = mascara(sesion, img)
    if origen == "gemini":
        llave = open(os.path.join(PJ, p["id"], "fondo.txt"), encoding="utf-8").read().strip() \
            if os.path.exists(os.path.join(PJ, p["id"], "fondo.txt")) else "verde"
        ac, rgb_l = croma(rgb, llave)
        # la llave manda donde es clara (fondo liso); isnet suaviza el borde y salva el pelo del color parecido
        a = np.where(ac < 0.5, np.minimum(a, ac), np.maximum(a, ac * (a > 0.15)))
        mezcla = np.clip((1.0 - a) * 2.0, 0, 1)[..., None]   # despill sobre todo en los bordes
        rgb = rgb * (1 - mezcla) + rgb_l * mezcla
    a = np.clip((a - 0.08) / 0.84, 0, 1)
    out = Image.fromarray(np.dstack([(rgb * 255).astype(np.uint8), (a * 255).astype(np.uint8)]), "RGBA")
    caja = Image.fromarray((a > 0.1).astype(np.uint8) * 255).getbbox()
    if caja:
        m = 6
        caja = (max(0, caja[0] - m), max(0, caja[1] - m), min(out.width, caja[2] + m), min(out.height, caja[3] + m))
        out = out.crop(caja)
    f = ALTO / out.height
    out = out.resize((max(1, round(out.width * f)), ALTO), Image.LANCZOS)
    out.save(destino, optimize=True)
    open(p_or, "w", encoding="utf-8").write(firma)
    return origen


def main():
    os.makedirs(SALIDA, exist_ok=True)
    sesion = ort.InferenceSession(MODELO, providers=["CPUExecutionProvider"])
    hechos = {}
    for p in pd.PERSONAJES:
        if SOLO and p["id"] not in SOLO:
            continue
        r = procesar(sesion, p)
        if r:
            hechos[r] = hechos.get(r, 0) + 1
    print("retratos en %s: %s" % (SALIDA, hechos or "sin cambios"))


if __name__ == "__main__":
    main()
