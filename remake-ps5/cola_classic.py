# Yugioh Classic Remaster: remasteriza con Gemini las ilustraciones de las cartas Goat que no tienen arte de Gemini
# (las de Joey ya lo tienen en nano-banana/arte y todo/arte). MISMO proceso que la cola de las 771 de Joey:
# herramientas/puente-gemini/cola_gemini.py (pausa de 20 s, limite y silencio de Google con reintento cada 30 min,
# rechazos por "derechos de terceros" a rechazo.txt y pasada final con prompt neutro), con su PROMPT_ARTE.
#   entrada: la ventana de arte de YGOPRODeck (cards_cropped, 624x624) -> 1_ilustracion_original.png
#   salida:  D:\Yugi-Reload\nano-banana\classic\arte\<id>\resultado.png   (cola propia: estado.json y cola.log en
#            nano-banana\classic; no toca el estado de la cola vieja)
# Al terminar cada imagen recompone su cara (preparar_classic.py --solo <id>, que solo rehace lo que cambio).
# Requisito: "Abrir Chrome del puente.bat" abierto y con sesion en Gemini (puerto 9333); si no, la cola espera.
# Uso: python -I cola_classic.py [--max N]   (--max: para tras N imagenes, para el piloto)
import os, subprocess, sys
AQUI = os.path.dirname(os.path.abspath(__file__))
PUENTE = r"D:\Yugi-Reload\herramientas\puente-gemini"
sys.path.insert(0, PUENTE)
sys.path.insert(0, AQUI)
MAX = int(sys.argv[sys.argv.index("--max") + 1]) if "--max" in sys.argv else None
sys.argv = sys.argv[:1]   # preparar_classic lee sys.argv al importarse
import cola_gemini as cg        # noqa: E402
import preparar_classic as pc   # noqa: E402
from PIL import Image           # noqa: E402

CLASSIC = os.path.join(cg.NB, "classic")
cg.TODO = CLASSIC
cg.LOG = os.path.join(CLASSIC, "cola.log")
cg.ESTADO = os.path.join(CLASSIC, "estado.json")
os.makedirs(os.path.join(CLASSIC, "arte"), exist_ok=True)


def preparar(d, passcode, nombre):
    os.makedirs(d, exist_ok=True)
    crop = os.path.join(pc.CACHE, "cropped", "%d.jpg" % passcode)
    if not os.path.exists(crop):
        pc.bajar("https://images.ygoprodeck.com/images/cards_cropped/%d.jpg" % passcode, crop)
    Image.open(crop).convert("RGB").save(os.path.join(d, "1_ilustracion_original.png"))
    open(os.path.join(d, "prompt.txt"), "w", encoding="utf-8").write(cg.PROMPT_ARTE.format(nombre=nombre))


def trabajos():
    cartas, es = pc.cargar()
    gem = pc.artes_gemini()
    lista = []
    for c in cartas:
        if pc.slug(c["name"]) in gem or pc.NOMBRE_JOEY.get(c["name"]) in gem:
            continue
        cid = pc.ID_VIEJO.get(c["name"], pc.slug(c["name"]))
        nombre = es.get(c["id"], (c["name"],))[0]
        lista.append(("arte", os.path.join(CLASSIC, "arte", cid),
                      lambda d, p=c["id"], n=nombre: preparar(d, p, n)))
    return lista


_generar, _hechas = cg.pg.generar, [0]


def generar(pag, imagenes, prompt, salida, *a, **k):
    _generar(pag, imagenes, prompt, salida, *a, **k)
    cid = os.path.basename(os.path.dirname(salida))
    subprocess.Popen([sys.executable, "-I", os.path.join(AQUI, "preparar_classic.py"), "--solo", cid],
                     stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, creationflags=0x08000000)
    _hechas[0] += 1
    if MAX and _hechas[0] >= MAX:
        cg.registrar("piloto: %d imagenes hechas, la cola para (--max)" % MAX, evento=True)
        cg.mantener_despierto(False)
        cg.guardar_estado(estado="piloto terminado")
        raise SystemExit(0)


cg.trabajos = trabajos
cg.pg.generar = generar

if __name__ == "__main__":
    cg.main()
