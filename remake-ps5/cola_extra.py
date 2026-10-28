# Yugioh Classic Remaster (rama classic-torneo-extras): cola de Gemini para las ilustraciones de las CARTAS EXTRA
# (datos/cartas_extra.txt) con el MISMO proceso y prompt que cola_classic.py / cola_personajes.py (arte de las firma):
# la ventana de arte de YGOPRODeck (cards_cropped) -> nano-banana\classic\arte\<id>\1_ilustracion_original.png +
# prompt.txt (cola_gemini.PROMPT_ARTE con el nombre en espanol oficial) -> resultado.png; al terminar cada una,
# preparar_classic.py --solo <id> [<ui>] recompone sus caras ES/EN (y HD).
# Usa herramientas/puente-gemini/cola_gemini.py (pausa de 20 s, limite y silencio con reintento cada 30 min, rechazos
# por derechos a rechazo.txt) con su propio estado: nano-banana\classic\extra_estado.json y extra_cola.log.
# NO correr en paralelo con otra cola en el mismo Chrome (puerto 9333).
# Revisar cada resultado contra 1_ilustracion_original.png (Gemini inventa): si no se parece, borrar resultado.png y
# volver a correr la cola.
# Uso: python -I cola_extra.py [--ui <carpeta ui>] [--solo id1,id2] [--preparar]
import os, subprocess, sys
AQUI = os.path.dirname(os.path.abspath(__file__))
PUENTE = r"D:\Yugi-Reload\herramientas\puente-gemini"
sys.path.insert(0, PUENTE)
sys.path.insert(0, AQUI)
UI = sys.argv[sys.argv.index("--ui") + 1] if "--ui" in sys.argv else None
SOLO = set(sys.argv[sys.argv.index("--solo") + 1].split(",")) if "--solo" in sys.argv else None
PREPARAR = "--preparar" in sys.argv
sys.argv = sys.argv[:1] + ([UI] if UI else [])   # preparar_classic lee sys.argv al importarse
import cola_gemini as cg          # noqa: E402
import preparar_classic as pc     # noqa: E402
from PIL import Image             # noqa: E402

CLASSIC = os.path.join(cg.NB, "classic")
cg.TODO = CLASSIC
cg.LOG = os.path.join(CLASSIC, "extra_cola.log")
cg.ESTADO = os.path.join(CLASSIC, "extra_estado.json")


def extra():
    """(id, passcode, nombre ES) de las cartas extra."""
    import sqlite3
    es = {r[0]: r[1] for r in sqlite3.connect(os.path.join(pc.CACHE, "es", "cards.cdb")).execute("select id, name from texts")}
    res = [(pc.ID_VIEJO.get(c["name"], pc.slug(c["name"])), c["id"], es.get(c["id"], c["name"])) for c, _ in pc.cargar_extra()]
    return [r for r in res if SOLO is None or r[0] in SOLO]


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


def trabajos():
    return [("arte", preparar_arte(*r), None) for r in extra()]


def reintentar_rechazadas(pag):
    pass   # (con 10 cartas se revisan a mano: prompt_neutro.txt queda listo en cada carpeta)


_generar = cg.pg.generar


def generar(pag, imagenes, prompt, salida, *a, **k):
    _generar(pag, imagenes, prompt, salida, *a, **k)
    cmd = [sys.executable, "-I", os.path.join(AQUI, "preparar_classic.py")] + ([UI] if UI else []) + ["--solo", os.path.basename(os.path.dirname(salida))]
    subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, creationflags=0x08000000)


cg.trabajos = trabajos
cg.reintentar_rechazadas = reintentar_rechazadas
cg.pg.generar = generar

if __name__ == "__main__":
    if PREPARAR:
        for r in extra():
            print(preparar_arte(*r))
    else:
        cg.main()
