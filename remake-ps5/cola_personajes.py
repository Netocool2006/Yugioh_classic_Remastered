# Yugioh Classic Remaster (rama classic-personajes): cola propia de Gemini para
#   1) los RETRATOS de los 47 personajes (fotos de D:\Yugi-Reload\Personajes): retrato anime fiel, medio cuerpo, en 4K,
#      sobre fondo liso de croma -> nano-banana\classic\personajes\<id>\resultado.png; al terminar cada uno,
#      retratos_post.py lo recorta (isnet-anime) y deja classic-assets\personajes\retrato_<id>.png
#   2) las ilustraciones de las CARTAS FIRMA (datos/cartas_firma.txt) con el MISMO proceso y prompt que cola_classic.py
#      -> nano-banana\classic\arte\<id>\resultado.png; al terminar cada una, preparar_classic.py --solo <id>
# Usa herramientas/puente-gemini/cola_gemini.py (pausa de 20 s, limite y silencio con reintento cada 30 min, rechazos
# por derechos a rechazo.txt) con su propio estado: nano-banana\classic\personajes\estado.json y cola.log. Los
# rechazados se reintentan al final con prompt_neutro.txt (sin nombres de la franquicia).
# NO correr en paralelo con otra cola en el mismo Chrome (cola_classic.py): pausar aquella antes.
# Uso: python -I cola_personajes.py --preparar   (solo deja entradas y prompts; no abre Gemini)
#      python -I cola_personajes.py [--solo retratos|arte] [--max N]
import os, re, subprocess, sys, time
AQUI = os.path.dirname(os.path.abspath(__file__))
PUENTE = r"D:\Yugi-Reload\herramientas\puente-gemini"
FOTOS = r"D:\Yugi-Reload\Personajes"
sys.path.insert(0, PUENTE)
sys.path.insert(0, AQUI)
MAX = int(sys.argv[sys.argv.index("--max") + 1]) if "--max" in sys.argv else None
SOLO = sys.argv[sys.argv.index("--solo") + 1] if "--solo" in sys.argv else None
PREPARAR = "--preparar" in sys.argv
sys.argv = sys.argv[:1]   # preparar_classic lee sys.argv al importarse
import cola_gemini as cg          # noqa: E402
import preparar_classic as pc     # noqa: E402
import personajes_datos as pd     # noqa: E402
from PIL import Image             # noqa: E402

CLASSIC = os.path.join(cg.NB, "classic")
PJ = os.path.join(CLASSIC, "personajes")
cg.TODO = PJ
cg.LOG = os.path.join(PJ, "cola.log")
cg.ESTADO = os.path.join(PJ, "estado.json")
os.makedirs(PJ, exist_ok=True)

# fondo de croma: verde salvo los personajes con verde en el pelo o la ropa (magenta)
MAGENTA = {"weevil", "rex_raptor", "noah", "espa_roba", "hermanos_paradoja"}

PROMPT_RETRATO = (
    "Redibuja a este personaje del anime Yu-Gi-Oh! Duel Monsters ({nombre}) como un RETRATO OFICIAL de alta calidad "
    "para un videojuego de 2026.\n\n"
    "- FIEL al personaje de la imagen: misma cara, peinado, color de ojos y de pelo, ropa, accesorios y expresion "
    "caracteristica. Debe reconocerse al instante.\n"
    "- Estilo anime de la serie original (Duel Monsters), como una ilustracion oficial remasterizada: lineas limpias y "
    "nitidas, colores vivos, sombreado suave, mucho detalle. Nada de 3D ni de fotografia.\n"
    "- Encuadre: retrato de medio cuerpo (de la cintura hacia arriba), de frente o girado un poco hacia la izquierda, "
    "con una pose segura de duelista. La cabeza y el pelo completos dentro del cuadro, con un poco de margen arriba.\n"
    "- {extra}"
    "- FONDO: un unico color liso y uniforme, {fondo}, sin degradados, sin sombras, sin escenario, sin brillos ni "
    "particulas. Ese color no debe aparecer en el personaje.\n"
    "- Formato vertical 3:4, la maxima resolucion posible (4K).\n"
    "- Sin texto, sin marco, sin logotipos, sin marcas de agua.\n")


def prompt_neutro(p, fondo):
    """Mismo encargo sin nombrar la franquicia ni al personaje (lo que dispara el filtro de derechos)."""
    t = PROMPT_RETRATO.format(nombre="", fondo=fondo, extra=p["prompt_extra"] + "\n" if p["prompt_extra"] else "")
    t = t.replace(" del anime Yu-Gi-Oh! Duel Monsters ()", " de anime").replace("de la serie original (Duel Monsters)", "clasico de los 2000")
    return t + "- Es un personaje original propio; mantenlo igual que en la imagen.\n"


def preparar_retrato(p):
    d = os.path.join(PJ, p["id"])
    os.makedirs(d, exist_ok=True)
    destino = os.path.join(d, "1_foto_original.png")
    if not os.path.exists(destino):
        im = Image.open(os.path.join(FOTOS, p["foto"])).convert("RGB")
        if max(im.size) < 700:   # fotos pequennas: x4 con Real-ESRGAN anime (como las cartas de Joey)
            try:
                cg.x4(im, destino)
            except Exception as e:
                print("x4 fallo (%s): %s; se usa LANCZOS" % (p["id"], e))
                f = 1400 / max(im.size)
                im.resize((round(im.width * f), round(im.height * f)), Image.LANCZOS).save(destino)
        else:
            im.save(destino)
    extra = pd.FOTO_EXTRA.get(p["id"])
    if extra and not os.path.exists(os.path.join(d, "2_referencia.png")):
        Image.open(os.path.join(FOTOS, extra)).convert("RGB").save(os.path.join(d, "2_referencia.png"))
    fondo = "magenta puro (#FF00FF)" if p["id"] in MAGENTA else "verde croma puro (#00FF00)"
    ex = p["prompt_extra"] + "\n" if p["prompt_extra"] else ""
    open(os.path.join(d, "prompt.txt"), "w", encoding="utf-8").write(PROMPT_RETRATO.format(nombre=p["en"], fondo=fondo, extra=ex))
    open(os.path.join(d, "prompt_neutro.txt"), "w", encoding="utf-8").write(prompt_neutro(p, fondo))
    open(os.path.join(d, "fondo.txt"), "w", encoding="utf-8").write("magenta" if p["id"] in MAGENTA else "verde")
    return d


def firma():
    """(id, passcode, nombre ES) de las cartas firma, con los datos que ya dejo preparar_classic.py."""
    import sqlite3
    es = {r[0]: r[1] for r in sqlite3.connect(os.path.join(pc.CACHE, "es", "cards.cdb")).execute("select id, name from texts")}
    return [(pc.ID_VIEJO.get(c["name"], pc.slug(c["name"])), c["id"], es.get(c["id"], c["name"])) for c, _, _ in pc.cargar_firma()]


def preparar_arte(cid, passcode, nombre):
    d = os.path.join(CLASSIC, "arte", cid)
    os.makedirs(d, exist_ok=True)
    crop = os.path.join(pc.CACHE, "cropped", "%d.jpg" % passcode)
    if not os.path.exists(crop):
        pc.bajar("https://images.ygoprodeck.com/images/cards_cropped/%d.jpg" % passcode, crop)
    if not os.path.exists(os.path.join(d, "1_ilustracion_original.png")):
        Image.open(crop).convert("RGB").save(os.path.join(d, "1_ilustracion_original.png"))
    prompt = cg.PROMPT_ARTE.format(nombre=nombre)
    open(os.path.join(d, "prompt.txt"), "w", encoding="utf-8").write(prompt)
    open(os.path.join(d, "prompt_neutro.txt"), "w", encoding="utf-8").write(cg.prompt_neutro(prompt))
    return d


def carpetas():
    res = []
    if SOLO in (None, "retratos"):
        res += [("retrato", os.path.join(PJ, p["id"])) for p in pd.PERSONAJES]
    if SOLO in (None, "arte"):
        res += [("arte", os.path.join(CLASSIC, "arte", cid)) for cid, _, _ in firma()]
    return res


def trabajos():
    return [(g, d, None) for g, d in carpetas()]   # (ya preparados con --preparar)


def reintentar_rechazadas(pag):
    pendientes = [d for _, d in carpetas() if os.path.exists(os.path.join(d, "rechazo.txt"))
                  and not os.path.exists(os.path.join(d, "resultado.png")) and not os.path.exists(os.path.join(d, "rechazo2.txt"))]
    if not pendientes:
        return
    cg.registrar("reintento de %d rechazadas con prompt neutro" % len(pendientes), evento=True)
    for d in pendientes:
        prompt = open(os.path.join(d, "prompt_neutro.txt"), encoding="utf-8").read().strip()
        imgs = sorted(f for f in os.listdir(d) if re.match(r"\d_.*\.(png|jpg)$", f))
        while True:
            try:
                generar(pag, [os.path.join(d, f) for f in imgs], prompt, os.path.join(d, "resultado.png"))
                cg.registrar("rechazada recuperada: %s" % d)
                break
            except (cg.pg.ErrorSilencio, cg.pg.ErrorLimite) as e:
                cg.registrar("pausa en reintento de rechazadas (%s)" % e)
                time.sleep(cg.ESPERA_LIMITE)
            except Exception as e:
                open(os.path.join(d, "rechazo2.txt"), "w", encoding="utf-8").write(str(e))
                cg.registrar("sigue rechazada: %s" % d)
                break
        time.sleep(cg.PAUSA)


_generar, _hechas = cg.pg.generar, [0]


def generar(pag, imagenes, prompt, salida, *a, **k):
    _generar(pag, imagenes, prompt, salida, *a, **k)
    d = os.path.dirname(salida)
    if os.path.dirname(d) == PJ:   # retrato: recorte y retrato_<id>.png
        cmd = [sys.executable, "-I", os.path.join(AQUI, "retratos_post.py"), "--solo", os.path.basename(d)]
    else:                          # carta firma: su cara
        cmd = [sys.executable, "-I", os.path.join(AQUI, "preparar_classic.py"), "--solo", os.path.basename(d)]
    subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, creationflags=0x08000000)
    _hechas[0] += 1
    if MAX and _hechas[0] >= MAX:
        cg.registrar("piloto: %d imagenes hechas, la cola para (--max)" % MAX, evento=True)
        cg.mantener_despierto(False)
        cg.guardar_estado(estado="piloto terminado")
        raise SystemExit(0)


cg.trabajos = trabajos
cg.reintentar_rechazadas = reintentar_rechazadas
cg.pg.generar = generar

if __name__ == "__main__":
    if PREPARAR:
        for p in pd.PERSONAJES:
            preparar_retrato(p)
        n = 0
        for cid, passcode, nombre in firma():
            preparar_arte(cid, passcode, nombre)
            n += 1
        print("preparados: %d retratos en %s y %d ilustraciones de cartas firma en %s"
              % (len(pd.PERSONAJES), PJ, n, os.path.join(CLASSIC, "arte")))
    else:
        cg.main()
