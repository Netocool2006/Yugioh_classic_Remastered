# Yugioh Classic Remaster (rama classic-fondos): cola propia de Gemini para los FONDOS fotorrealistas, uno por personaje
# (47, sin repetir) y uno por tema para los rivales genericos (16). Solo texto (sin imagenes de entrada): el prompt de
# fondos_datos.py -> nano-banana\classic\fondos\<id>\resultado.png; al terminar cada uno, fondos_post.py <id> lo recorta,
# lo escala (Real-ESRGAN x4), le saca la profundidad (Depth Anything V2) y lo deja en classic-assets\fondos\ (uno por vez:
# el siguiente post-proceso espera al anterior).
# Usa herramientas/puente-gemini/cola_gemini.py (pausa de 20 s, limite y silencio con reintento cada 30 min, rechazos a
# rechazo.txt) con su propio estado: nano-banana\classic\fondos\estado.json y cola.log.
# NO correr en paralelo con otra cola en el mismo Chrome (cola_classic.py): pausar aquella antes.
# Uso: python -I cola_fondos.py --preparar                 (solo deja carpetas y prompts; no abre Gemini)
#      python -I cola_fondos.py [--solo pegasus,kaiba,yugi] [--max N] [--sin-temas]
#      python -I cola_fondos.py --campos [--preparar]   (rama classic-campos: solo los 25 de las cartas de campo)
import os, subprocess, sys
AQUI = os.path.dirname(os.path.abspath(__file__))
PUENTE = r"D:\Yugi-Reload\herramientas\puente-gemini"
sys.path.insert(0, PUENTE)
sys.path.insert(0, AQUI)
MAX = int(sys.argv[sys.argv.index("--max") + 1]) if "--max" in sys.argv else None
SOLO = sys.argv[sys.argv.index("--solo") + 1].split(",") if "--solo" in sys.argv else None
PREPARAR = "--preparar" in sys.argv
SIN_TEMAS = "--sin-temas" in sys.argv
CAMPOS = "--campos" in sys.argv
sys.argv = sys.argv[:1]
import cola_gemini as cg      # noqa: E402
import fondos_datos as fd     # noqa: E402

FONDOS = os.path.join(cg.NB, "classic", "fondos")
cg.TODO = FONDOS
cg.LOG = os.path.join(FONDOS, "cola.log")
cg.ESTADO = os.path.join(FONDOS, "estado.json")
os.makedirs(FONDOS, exist_ok=True)

# prioridad del usuario: Pegasus (castillo), Kaiba, Yugi, Joey, Dartz, Marik; luego el resto y los temas
PRIMERO = ["pegasus", "kaiba", "yugi", "joey", "dartz", "marik"]


def orden():
    pj = sorted(fd.PERSONAJES, key=lambda f: (PRIMERO.index(f["id"]) if f["id"] in PRIMERO else len(PRIMERO)))
    lista = pj + ([] if SIN_TEMAS else fd.TEMAS)
    if CAMPOS:
        lista = list(fd.CAMPOS)
    elif SOLO:
        lista += fd.CAMPOS
    if SOLO:
        lista = [f for f in lista if f["id"] in SOLO]
        lista.sort(key=lambda f: SOLO.index(f["id"]))
    return lista


def preparar(f):
    d = os.path.join(FONDOS, f["id"])
    os.makedirs(d, exist_ok=True)
    p = fd.prompt(f)
    open(os.path.join(d, "prompt.txt"), "w", encoding="utf-8").write(p)
    open(os.path.join(d, "prompt_neutro.txt"), "w", encoding="utf-8").write(p)   # ya sin nombres de la franquicia
    open(os.path.join(d, "lugar.txt"), "w", encoding="utf-8").write("%s\n%s\n%s\n" % (f["es"], f["en"], f["tema"]))
    return d


def trabajos():
    return [("fondo", preparar(f), None) for f in orden()]


def reintentar_rechazadas(pag):
    pass   # el prompt ya es neutro: un rechazo se revisa a mano (rechazo.txt)


_generar, _hechas, _post = cg.pg.generar, [0], [None]


def generar(pag, imagenes, prompt, salida, *a, **k):
    _generar(pag, imagenes, prompt, salida, *a, **k)
    id = os.path.basename(os.path.dirname(salida))
    if _post[0] is not None:
        _post[0].wait()   # un post-proceso (Real-ESRGAN + profundidad) a la vez
    log = open(os.path.join(os.path.dirname(salida), "post.log"), "w", encoding="utf-8")
    _post[0] = subprocess.Popen([sys.executable, "-I", os.path.join(AQUI, "fondos_post.py"), id], stdout=log, stderr=subprocess.STDOUT,
                                creationflags=0x08000000)
    _hechas[0] += 1
    if MAX and _hechas[0] >= MAX:
        _post[0].wait()
        cg.registrar("piloto: %d fondos hechos, la cola para (--max)" % MAX, evento=True)
        cg.mantener_despierto(False)
        cg.guardar_estado(estado="piloto terminado")
        raise SystemExit(0)


cg.trabajos = trabajos
cg.reintentar_rechazadas = reintentar_rechazadas
cg.pg.generar = generar

if __name__ == "__main__":
    if PREPARAR:
        n = len([preparar(f) for f in orden()])
        print("preparados %d fondos en %s" % (n, FONDOS))
    else:
        cg.main()
        if _post[0] is not None:
            _post[0].wait()
