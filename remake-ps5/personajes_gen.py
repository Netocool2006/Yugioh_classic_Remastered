# Yugioh Classic Remaster (rama classic-personajes): los mazos de los PERSONAJES del anime (personajes_datos.py), de
# forma DETERMINISTA como rivales_gen.py (usa sus cartas, recetas, staples y tabla de niveles). Razonamiento en
# docs/personajes.md.
#
# Uso:
#   python -I personajes_gen.py --firma   -> datos/cartas_firma.txt: las cartas de los mazos que no estan entre las
#                                            1690 Goat y existen impresas (YGOPRODeck); las que no existen se avisan
#                                            (hay que sustituirlas en personajes_datos.py). Luego: preparar_classic.py
#   python -I personajes_gen.py           -> datos/personajes.txt y datos/personajes_rivales.txt
#
# personajes.txt (lo lee src/personajes.h):  id|nombre_es|nombre_en|saga|nivel|personalidad|entorno|lema_es|lema_en
# personajes_rivales.txt (formato de rivales.txt, lo lee ia.h ademas de rivales.txt; tema = "personaje:<saga>"):
#   R|id|nivel|personalidad|personaje:<saga>|nombre ES|nombre EN|lema ES|lema EN
#   V|id|variante|nombre ES|nombre EN       nombre de cada variante (la 0 es "la suya")
#   M|id|variante|ids   X|id|variante|ids   F|fusion|mat+mat   T|ritual|magia
import json, os, random, sys, urllib.parse, urllib.request

AQUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, AQUI)
UI = r"D:\Docker\compartido\classic-assets\ui"
FUENTE = os.path.join(os.path.dirname(UI), "fuente_classic")
GOAT = os.path.join(FUENTE, "goat.json")
import personajes_datos as pd   # noqa: E402

LIMITE_FIRMA = {"Slifer the Sky Dragon": 1, "Obelisk the Tormentor": 1, "The Winged Dragon of Ra": 1}


def partir(entrada):
    n, _, k = entrada.partition("*")
    return n, int(k) if k else 1


def bajar_json(url, destino):
    if not os.path.exists(destino):
        req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0 (personajes_gen)"})
        data = urllib.request.urlopen(req, timeout=60).read()
        open(destino, "wb").write(data)
    return json.load(open(destino, encoding="utf-8"))


def firma():
    import re
    goat = {c["name"].lower() for c in json.load(open(GOAT, encoding="utf-8"))["data"]}
    duennos = {}
    for p in pd.PERSONAJES:
        for v in p["variantes"]:
            for e in v[3]:
                n = partir(e)[0]
                if n.lower() not in goat:
                    duennos.setdefault(n, [])
                    if p["id"] not in duennos[n]:
                        duennos[n].append(p["id"])
    d = os.path.join(FUENTE, "firma")
    os.makedirs(d, exist_ok=True)
    lineas = ["# cartas_firma.txt - generado por personajes_gen.py --firma (no editar a mano): cartas de los personajes",
              "# que no estan entre las 1690 Goat. nombre TCG|limite|duennos (el jugador la desbloquea al vencer a uno)"]
    faltan = []
    for n in sorted(duennos):
        slug = re.sub(r"[^a-z0-9]+", "_", n.lower()).strip("_")
        try:
            c = bajar_json("https://db.ygoprodeck.com/api/v7/cardinfo.php?" + urllib.parse.urlencode({"name": n}),
                           os.path.join(d, slug + ".json"))["data"][0]
        except Exception as e:
            faltan.append("%s (%s): %s" % (n, ",".join(duennos[n]), e))
            continue
        lineas.append("%s|%d|%s" % (c["name"], LIMITE_FIRMA.get(c["name"], 3), ",".join(duennos[n])))
    open(os.path.join(AQUI, "datos", "cartas_firma.txt"), "w", encoding="utf-8", newline="\n").write("\n".join(lineas) + "\n")
    print("cartas firma: %d -> datos/cartas_firma.txt" % (len(lineas) - 2))
    for f in faltan:
        print("NO EXISTE IMPRESA (sustituir en personajes_datos.py):", f)


# ---------------------------------------------------------------------------------------------- mazos
def cargar_rg():
    sys.argv = [sys.argv[0], os.path.join(UI, "cartas_ui.txt"), os.path.join(UI, "cartas_classic.txt"), GOAT, os.devnull]
    import rivales_gen as rg
    return rg


def armar(rg, p, nv, avisos):
    C = rg.C
    nombre_v, _, tema, lista = p["variantes"][nv]
    nivel, pers = p["nivel"], p["pers"]
    rng = random.Random(rg.semilla("personaje", p["id"], nv))
    cap4, _prom, t56, t7, n_gm, n_gt = rg.NIVEL[nivel]
    main, extra, cnt = [], [], {}

    def add(i, maxrep=3):
        c = C.get(i)
        if not c or c["limite"] == 0 or c["clase"] == "fusion":
            return False
        if cnt.get(i, 0) >= min(maxrep, c["limite"]) or len(main) >= 60:
            return False
        cnt[i] = cnt.get(i, 0) + 1
        main.append(i)
        return True

    def mons():
        return [i for i in main if rg.monstruo(C[i])]

    # 1) el nucleo: las cartas de la serie
    fusiones = []
    listadas = {rg.idd(partir(e)[0]) for e in lista}
    for e in lista:
        n, k = partir(e)
        i = rg.idd(n)
        if not i:
            avisos.append("%s v%d: no esta en cartas_ui.txt: %s" % (p["id"], nv, n))
            continue
        c = C[i]
        if c["clase"] == "fusion":
            fusiones.append(i)
            continue
        if i in rg.NOMI_MAZO and c["subtipo"] != "toon":   # (rama classic-efectos: las del motor si entran)
            avisos.append("%s v%d: no se puede invocar de forma normal (fuera): %s" % (p["id"], nv, n))
            continue
        for _ in range(k):
            add(i)
        if c["clase"] == "ritual" and i in rg.RITUAL and rg.RITUAL[i] not in listadas:   # (su magia de ritual)
            for _ in range(k):
                add(rg.RITUAL[i])
    for f in fusiones:   # fusiones del nucleo: con sus materiales y Polymerization
        if f not in rg.FUSION:
            avisos.append("%s v%d: fusion sin receta (fuera): %s" % (p["id"], nv, f))
            continue
        for x in rg.FUSION[f]:
            if x not in cnt and not (x in rg.NOMI or C[x]["limite"] == 0):
                add(x, 2)
                add(x, 2)
        if all(x in cnt for x in rg.FUSION[f]):
            extra.append(f)
        else:
            avisos.append("%s v%d: faltan materiales de %s" % (p["id"], nv, f))
    if extra and rg.POLY not in cnt:
        add(rg.POLY)
        add(rg.POLY)
    # 2) relleno del tema (cartas de la epoca; nunca cartas firma de otro)
    pool = rg.filtro(tema)
    if len([c for c in pool if c["nivel"] <= 4]) < 10:
        attrs = {c["attr"] for c in pool} or {"TIERRA"}
        pool += [c for c in rg.MAIN_MON if c["attr"] in attrs and c["nivel"] <= 4 and c not in pool]

    def fuerza(c):
        return max(c["atk"], c["defn"] * (0.95 if pers == "defensivo" else 0.75)) + 0.3 * c["atk"]

    def ruido():
        return rng.uniform(0, 300 + 40 * nv)

    grandes = [c for c in pool if c["nivel"] >= 5] or [c for c in rg.MAIN_MON if c["nivel"] >= 5]
    for lo, hi, n in ((5, 6, t56), (7, 12, t7)):
        ya = sum(1 for i in mons() if lo <= C[i]["nivel"] <= hi)
        cand = sorted([c for c in grandes if lo <= c["nivel"] <= hi], key=lambda c: -(fuerza(c) + ruido()))
        for c in cand[:max(0, n - ya)]:
            add(c["id"], 1 if nivel <= 2 else 2)
    nm = rng.randint(18, 21) + (2 if pers in ("defensivo", "agresivo", "temerario") else 0)
    bajos = sorted([c for c in pool if c["nivel"] <= 4 and c["atk"] <= cap4], key=lambda c: -(fuerza(c) + ruido()))
    for j, c in enumerate(bajos):
        if len(mons()) >= nm:
            break
        add(c["id"], 2)
        if j < 4 and rng.random() < 0.6:
            add(c["id"], 2)
    relleno = sorted([c for c in rg.MAIN_MON if c["nivel"] <= 4 and c["atk"] <= cap4], key=lambda c: -(fuerza(c) + ruido()))
    for c in relleno:
        if len(mons()) >= nm:
            break
        add(c["id"], 2)
    for f, m in sorted(rg.FUSION.items()):   # fusiones "de paso"
        if f not in extra and C[f]["limite"] > 0 and all(x in cnt for x in m) and len(extra) < 15:
            extra.append(f)
    if extra and rg.POLY not in cnt:
        add(rg.POLY)
    # 3) magias y trampas: campo/equipos del tema, de la epoca segun el nivel y basicas
    mags = list(rg.CAMPO.get(tema[5:].split(",")[0], [])) if tema.startswith("raza:") else []
    if pers in ("farolero", "tramposo"):
        mags += rg.VOLTEO_EXTRA
    gm, gt = list(rg.GOAT_MAG), list(rg.GOAT_TRAP)
    rng.shuffle(gm)
    rng.shuffle(gt)
    mags += gm[:n_gm]
    basicas = list(rg.BASICAS_MAG) + list(rg.BASICAS_TRAP)
    rng.shuffle(basicas)
    total = max(40, min(60, len(main) + 4))
    for i in mags:
        if len(main) >= total - max(4, n_gt):
            break
        add(i, 2)
    for i in gt[:n_gt] + basicas:
        if len(main) >= total:
            break
        add(i, 2)
    while len(main) < 40:
        add(rng.choice(relleno)["id"], 2)
    main.sort(key=lambda i: (not rg.monstruo(C[i]), C[i]["clase"], i))
    return main, sorted(set(extra))


def mazos():
    rg = cargar_rg()
    C = rg.C
    if not any(c.get("firma") for c in C.values()):
        # (una cola_classic.py con el preparar_classic.py viejo reescribe cartas_ui/cartas_classic sin las firma)
        sys.exit("ERROR: cartas_classic.txt no trae las cartas firma: correr antes python -I preparar_classic.py")
    sagas = [s[0] for s in pd.SAGAS]
    pj = sorted(pd.PERSONAJES, key=lambda p: (sagas.index(p["saga"]), p["nivel"]))
    out = ["# personajes_rivales.txt - generado por remake-ps5/personajes_gen.py (no editar a mano). Ver docs/personajes.md",
           "# formato de rivales.txt + V|id|variante|nombre ES|nombre EN ; tema = personaje:<saga>"]
    tabla = ["# personajes.txt - generado por remake-ps5/personajes_gen.py. id|nombre_es|nombre_en|saga|nivel|personalidad|entorno|lema_es|lema_en",
             "# sagas (orden del modo historia): " + " ".join(sagas)]
    avisos, resumen, fus, rit = [], [], set(), set()
    for p in pj:
        tabla.append("|".join(str(x) for x in (p["id"], p["es"], p["en"], p["saga"], p["nivel"], p["pers"], p["id"],
                                               p["lema_es"], p["lema_en"])))
        out.append("R|%s|%d|%s|personaje:%s|%s|%s|%s|%s" % (p["id"], p["nivel"], p["pers"], p["saga"], p["es"], p["en"],
                                                            p["lema_es"], p["lema_en"]))
        for nv, v in enumerate(p["variantes"]):
            main, extra = armar(rg, p, nv, avisos)
            rg.validar(main, extra)
            out.append("V|%s|%d|%s|%s" % (p["id"], nv, v[0], v[1]))
            out.append("M|%s|%d|%s" % (p["id"], nv, " ".join(main)))
            out.append("X|%s|%d|%s" % (p["id"], nv, " ".join(extra)))
            fus |= set(extra)
            rit |= {i for i in main if C[i]["clase"] == "ritual"}
            ms = [i for i in main if rg.monstruo(C[i])]
            nf = sum(1 for i in set(main) | set(extra) if C[i].get("firma"))
            resumen.append("# %-17s n%d v%d %-10s %2d cartas, %2d monstruos (ATK medio %4d), %2d magias, %2d trampas, %d fusiones, %2d firma"
                           % (p["id"], p["nivel"], nv, p["pers"], len(main), len(ms), sum(C[i]["atk"] for i in ms) / max(1, len(ms)),
                              sum(1 for i in main if C[i]["clase"] == "magia"), sum(1 for i in main if C[i]["clase"] == "trampa"),
                              len(extra), nf))
    for f in sorted(fus):
        out.append("F|%s|%s" % (f, "+".join(rg.FUSION[f])))
    for x in sorted(rit):
        if x in rg.RITUAL:
            out.append("T|%s|%s" % (x, rg.RITUAL[x]))
    out += resumen
    os.makedirs(os.path.join(AQUI, "datos"), exist_ok=True)
    open(os.path.join(AQUI, "datos", "personajes_rivales.txt"), "w", encoding="utf-8", newline="\n").write("\n".join(out) + "\n")
    open(os.path.join(AQUI, "datos", "personajes.txt"), "w", encoding="utf-8", newline="\n").write("\n".join(tabla) + "\n")
    print("personajes: %d, mazos: %d -> datos/personajes.txt, datos/personajes_rivales.txt" % (len(pj), sum(len(p["variantes"]) for p in pj)))
    for a in avisos:
        print("aviso:", a)
    for r in resumen:
        print(r)


if __name__ == "__main__":
    if "--firma" in sys.argv:
        firma()
    else:
        mazos()
