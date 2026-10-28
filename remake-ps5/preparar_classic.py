# Yugioh Classic Remaster: datos y caras de las 1690 cartas del Formato Goat (abril 2005), en el mismo estilo que las
# caras del juego (componer_cartas.componer: marco remasterizado + ilustracion + nombre/texto en espanol con codigo).
#   - lista Goat: JSON de YGOPRODeck (cardinfo.php?format=goat) en <cache>/goat.json (se baja si falta)
#   - espanol: base oficial de ygopro (mycard/ygopro-database locales/es-ES/cards.cdb) por passcode; si no esta, el
#     texto en espanol del juego de Joey (card_namespa.bin/card_descspa.bin); si tampoco, <cache>/traducciones.json
#     (traduccion propia con la terminologia oficial: se lista al final)
#   - ilustracion: la remasterizada por Gemini si existe (nano-banana/arte/<slug>, todo/arte/<bmp>, classic/arte/<id>);
#     si no, la ventana de arte de YGOPRODeck (cards_cropped, 624x624: ya supera la ventana de 564 px de la cara de
#     744 px, no hace falta el x4) como PROVISIONAL (<cache>/cartas/<id>/origen.txt = "provisional")
#   - cara_<id>.png (360 px) y mini_<id>.png (180 px) como preparar_ui.py, y en ingles cara_en_<id>/mini_en_<id>;
#     solo se rehacen las que cambian (firma_es/en.txt: ilustracion + textos) o faltan. Las 30 de la demo conservan
#     su id y su orden (cache/demo.txt) pero tambien llevan el espanol oficial
# Escribe en <ui>: cartas_ui.txt (formato de preparar_ui.py; ATK/DEF "?" = -1), cartas_ui_en.txt (igual, textos,
#   atributo y raza en ingles oficial de YGOPRODeck; la clase sigue con las claves del juego) y cartas_classic.txt
#   id|passcode|limite|subtipo   limite 0/1/2/3 (ban_goat; sin entrada = 3)
#   subtipo: normal/efecto/fusion/ritual/volteo/espiritu/union/toon (monstruos), magia_normal/campo/equipo/continua/
#            rapida/ritual, trampa_normal/continua/contraataque
# CARTAS FIRMA (rama classic-personajes): las de los personajes del anime que no estan entre las 1690 Goat. Lista en
#   datos/cartas_firma.txt (nombre TCG|limite|duennos|nota; la genera personajes_gen.py --firma). Mismo proceso que las
#   1690: datos EN de YGOPRODeck (cardinfo.php?name=, cache <cache>/firma/<slug>.json, todas juntas en <cache>/firma.json
#   para rivales_gen.py y personajes_gen.py), espanol de cards.cdb es-ES por passcode, ilustracion Gemini
#   (nano-banana/classic/arte/<id>, cola_personajes.py) o provisional. En cartas_classic.txt llevan dos campos mas:
#   id|passcode|limite|subtipo|firma|duenno,duenno  (el constructor decide si el jugador la tiene: se desbloquea al
#   vencer a su duenno). Dioses Egipcios: limite 1; sin motor de efectos son monstruos con su ATK/DEF; los de ATK/DEF
#   "?" (Slifer y Ra) llevan un valor jugable (VALOR_FIJO, ver docs/personajes.md).
# CARTAS EXTRA (rama classic-efectos): cartas fuera de las 1690 Goat y sin duenno en el anime (no se desbloquean: estan
#   en el constructor como las Goat). Lista en datos/cartas_extra.txt (nombre TCG|limite|nota). Mismo proceso que las
#   firma (YGOPRODeck por nombre, cache <cache>/extra/<slug>.json; espanol de cards.cdb; ilustracion Gemini o
#   provisional); en cartas_classic.txt van como las Goat (id|passcode|limite|subtipo).
# Se puede correr las veces que haga falta (p. ej. tras cada imagen de Gemini): solo recompone lo que cambio.
# Uso: python -I preparar_classic.py [<ui>] [--solo id1,id2,...] [--todo]
import csv, json, os, re, sqlite3, sys, time, urllib.request
from PIL import Image

AQUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(AQUI, "..", "remake", "diseno"))
import componer_cartas as cc   # noqa: E402

BASE = r"D:\Yugi-Reload"
NB = os.path.join(BASE, "nano-banana")
BIN = os.path.join(BASE, "datadat-extraido", "bin#")
LIST_CARD = os.path.join(BASE, "datadat-extraido", "card", "list_card.txt")
CARTAS_CSV = os.path.join(BASE, "re-output", "cards.csv")
args = [a for i, a in enumerate(sys.argv[1:], 1) if not a.startswith("--") and sys.argv[i - 1] not in ("--solo", "--firma")]
UI = args[0] if args else r"D:\Docker\compartido\classic-assets\ui"
CACHE = os.path.join(os.path.dirname(UI), "fuente_classic")
GOAT_URL = "https://db.ygoprodeck.com/api/v7/cardinfo.php?format=goat&misc=yes"
CDB_URL = "https://github.com/mycard/ygopro-database/raw/master/locales/es-ES/cards.cdb"
TODO = "--todo" in sys.argv
F_FIRMA = sys.argv[sys.argv.index("--firma") + 1] if "--firma" in sys.argv else os.path.join(AQUI, "datos", "cartas_firma.txt")
F_EXTRA = os.path.join(AQUI, "datos", "cartas_extra.txt")
VALOR_FIJO = {"Slifer the Sky Dragon": (3000, 3000), "The Winged Dragon of Ra": (4000, 4000)}   # ATK/DEF "?" -> jugable
SOLO = set(sys.argv[sys.argv.index("--solo") + 1].split(",")) if "--solo" in sys.argv else None
NOMBRE_JOEY = {"Red-Eyes Black Dragon": "red_eyes_b_dragon"}   # nombre TCG -> slug del nombre en el juego de Joey
NOMBRE_PROPIO = {21593977: "Makyura el Destructor"}
HD_W, HD_H = 1116, 1617   # cara HD: 1.5 x 744x1078 (los marcos de Gemini miden ~1600 de ancho)
ID_VIEJO = {"Red-Eyes Black Dragon": "red_eyes", "Blue-Eyes White Dragon": "blue_eyes", "Gaia The Fierce Knight": "gaia"}

TIPOS = {"Dragon": "Dragón", "Warrior": "Guerrero", "Spellcaster": "Lanzador de Conjuros", "Fiend": "Demonio",
         "Winged Beast": "Bestia Alada", "Machine": "Máquina", "Rock": "Roca", "Fish": "Pez", "Beast": "Bestia",
         "Sea Serpent": "Serpiente Marina", "Zombie": "Zombi", "Aqua": "Aqua", "Pyro": "Piro", "Thunder": "Trueno",
         "Insect": "Insecto", "Plant": "Planta", "Reptile": "Reptil", "Dinosaur": "Dinosaurio",
         "Beast-Warrior": "Guerrero-Bestia", "Fairy": "Hada", "Divine-Beast": "Bestia Divina"}
ATRIBUTOS = {"DARK": "OSCURIDAD", "LIGHT": "LUZ", "EARTH": "TIERRA", "WATER": "AGUA", "FIRE": "FUEGO", "WIND": "VIENTO", "DIVINE": "DIVINIDAD"}
LIMITE = {"Forbidden": 0, "Limited": 1, "Semi-Limited": 2}
SUB_MAGIA = {"Normal": ("normal", ""), "Field": ("campo", "de Campo"), "Equip": ("equipo", "de Equipo"),
             "Continuous": ("continua", "Continua"), "Quick-Play": ("rapida", "Rápida"), "Ritual": ("ritual", "de Ritual")}
SUB_TRAMPA = {"Normal": ("normal", ""), "Continuous": ("continua", "Continua"), "Counter": ("contraataque", "de Contraataque")}
SUB_MONSTRUO = {"Flip Effect Monster": ("volteo", "Volteo"), "Spirit Monster": ("espiritu", "Espíritu"),
                "Union Effect Monster": ("union", "Unión"), "Toon Monster": ("toon", "Toon"),
                "Gemini Monster": ("geminis", "Géminis")}

SUB_EN = {"volteo": "Flip", "geminis": "Gemini", "espiritu": "Spirit", "union": "Union", "toon": "Toon", "magia_campo": "Field",
          "magia_equipo": "Equip", "magia_continua": "Continuous", "magia_rapida": "Quick-Play", "magia_ritual": "Ritual",
          "trampa_continua": "Continuous", "trampa_contraataque": "Counter"}
ES = {"ritual": " / Ritual", "fusion": " / Fusión", "efecto": " / Efecto",
      "etiqueta": lambda clase, sub: ("[ CARTA %s %s ]" % ("MÁGICA" if clase == "magia" else "DE TRAMPA", sub)).replace("  ", " ")}
EN = {"ritual": " / Ritual", "fusion": " / Fusion", "efecto": " / Effect",
      "etiqueta": lambda clase, sub: ("[ %s %s CARD ]" % (sub.upper(), "SPELL" if clase == "magia" else "TRAP")).replace("[  ", "[ ")}


def slug(s):
    return re.sub(r"[^a-z0-9]+", "_", s.lower()).strip("_")


def bajar(url, destino):
    req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0 (preparar_classic)"})
    data = urllib.request.urlopen(req, timeout=60).read()
    open(destino + ".tmp", "wb").write(data)
    os.replace(destino + ".tmp", destino)


def espanol_joey():
    """slug del nombre ingles -> (nombre, descripcion) en espanol del juego de Joey (como preparar_figuras.py)."""
    nom, ind, desc = (open(os.path.join(BIN, n), "rb").read() for n in ("card_namespa.bin", "card_indxspa.bin", "card_descspa.bin"))
    res = {}
    for r in csv.DictReader(open(CARTAS_CSV, encoding="utf-8")):
        i = int(r["idx"])
        n = nom[i * 64:(i + 1) * 64].split(b"\0")[0].decode("latin-1").strip()
        o = int.from_bytes(ind[i * 4:i * 4 + 4], "little")
        res.setdefault(slug(r["nombre"]), (n, desc[o:desc.index(b"\0", o)].decode("latin-1").strip()))
    return res


def artes_gemini():
    """slug del nombre ingles -> resultado.png de Gemini de las cartas de Joey (nano-banana/arte y todo/arte/<bmp>)."""
    res, nombre = {}, None
    for l in open(LIST_CARD, encoding="latin-1").read().splitlines():
        l = l.strip()
        if l.startswith("//") and not re.match(r"//\s*\d{4}:", l) and "Generated" not in l:
            nombre = l[2:].strip()
        elif l.lower().endswith(".bmp") and nombre:
            p = os.path.join(NB, "todo", "arte", os.path.splitext(l.lower())[0], "resultado.png")
            if not l.lower().startswith("token_") and os.path.exists(p):
                res.setdefault(slug(nombre), p)
            nombre = None
    for d in os.listdir(os.path.join(NB, "arte")):
        p = os.path.join(NB, "arte", d, "resultado.png")
        if os.path.exists(p):
            res[d] = p
    return res


def marcos_hd():
    """Marcos a HD_W x HD_H recortados de los resultados de Gemini (~1600x2430, como preparar_marcos.py, que los dejo
    a 744: mismo recorte, solo que sin reducir tanto) y el de ritual: el normal con el tono girado a azul (el metal gris,
    poco saturado, no cambia)."""
    import preparar_marcos as pm   # remake/diseno (scipy de herramientas/pylib)
    res = {}
    for k, lote in (("normal", "base1"), ("efecto", "base1"), ("fusion", "base1b"), ("magia", "base1b"), ("trampa", "base1b")):
        p = os.path.join(CACHE, "marco_hd_%s.png" % k)
        if not os.path.exists(p):
            pm.recortar_carta(os.path.join(NB, lote, "marco_" + k, "resultado.png")).resize((HD_W, HD_H), Image.LANCZOS).save(p)
        res[k] = Image.open(p).convert("RGB")
    p = os.path.join(CACHE, "marco_hd_ritual.png")
    if not os.path.exists(p):
        h, sat, v = res["normal"].convert("HSV").split()
        h = h.point(lambda x: (x + 125) % 256)   # ~40 grados (amarillo) -> ~215 (azul)
        Image.merge("HSV", (h, sat, v)).convert("RGB").save(p)
    res["ritual"] = Image.open(p).convert("RGB")
    return res


def cargar():
    os.makedirs(CACHE, exist_ok=True)
    goat = os.path.join(CACHE, "goat.json")
    if not os.path.exists(goat):
        bajar(GOAT_URL, goat)
    cdb = os.path.join(CACHE, "es", "cards.cdb")
    if not os.path.exists(cdb):
        os.makedirs(os.path.dirname(cdb), exist_ok=True)
        bajar(CDB_URL, cdb)
    es = {r[0]: (r[1], r[2]) for r in sqlite3.connect(cdb).execute("select id, name, desc from texts")}
    return json.load(open(goat, encoding="utf-8"))["data"], es


def cargar_firma():
    """Las cartas firma de datos/cartas_firma.txt con sus datos de YGOPRODeck: lista de (carta, limite, duennos)."""
    if not os.path.exists(F_FIRMA):
        return []
    d = os.path.join(CACHE, "firma")
    os.makedirs(d, exist_ok=True)
    res = []
    for l in open(F_FIRMA, encoding="utf-8").read().splitlines():
        if not l.strip() or l.startswith("#"):
            continue
        p = l.split("|")
        nombre, limite, duennos = p[0], int(p[1]) if len(p) > 1 and p[1] else 3, p[2] if len(p) > 2 else ""
        cache = os.path.join(d, slug(nombre) + ".json")
        if not os.path.exists(cache):
            import urllib.parse
            bajar("https://db.ygoprodeck.com/api/v7/cardinfo.php?" + urllib.parse.urlencode({"name": nombre}), cache)
            time.sleep(0.1)
        c = json.load(open(cache, encoding="utf-8"))["data"][0]
        if c["name"] in VALOR_FIJO:
            c["atk"], c["def"] = VALOR_FIJO[c["name"]]
        res.append((c, limite, duennos))
    json.dump([c for c, _, _ in res], open(os.path.join(CACHE, "firma.json"), "w", encoding="utf-8"), ensure_ascii=False)
    return res


def cargar_extra():
    """Las cartas extra de datos/cartas_extra.txt con sus datos de YGOPRODeck: lista de (carta, limite)."""
    if not os.path.exists(F_EXTRA):
        return []
    d = os.path.join(CACHE, "extra")
    os.makedirs(d, exist_ok=True)
    res = []
    for l in open(F_EXTRA, encoding="utf-8").read().splitlines():
        if not l.strip() or l.startswith("#"):
            continue
        p = l.split("|")
        nombre, limite = p[0], int(p[1]) if len(p) > 1 and p[1] else 3
        cache = os.path.join(d, slug(nombre) + ".json")
        if not os.path.exists(cache):
            import urllib.parse
            bajar("https://db.ygoprodeck.com/api/v7/cardinfo.php?" + urllib.parse.urlencode({"name": nombre}), cache)
            time.sleep(0.1)
        res.append((json.load(open(cache, encoding="utf-8"))["data"][0], limite))
    json.dump([c for c, _ in res], open(os.path.join(CACHE, "extra.json"), "w", encoding="utf-8"), ensure_ascii=False)   # (rivales_gen.py)
    return res


def main():
    cartas, es = cargar()
    lista_firma = cargar_firma()
    firma_de = {c["id"]: (limite, duennos) for c, limite, duennos in lista_firma}
    goat_ids = {c["id"] for c in cartas}
    cartas = cartas + [c for c, _, _ in lista_firma if c["id"] not in goat_ids]
    lista_extra = cargar_extra()   # (cartas extra: sin duenno, como las Goat)
    extra_lim = {c["id"]: limite for c, limite in lista_extra}
    ya = {c["id"] for c in cartas}
    cartas = cartas + [c for c, _ in lista_extra if c["id"] not in ya]
    joey = espanol_joey()
    gem = artes_gemini()
    trad_p = os.path.join(CACHE, "traducciones.json")
    trad = json.load(open(trad_p, encoding="utf-8")) if os.path.exists(trad_p) else {}
    viejas = {}
    p_ui = os.path.join(UI, "cartas_ui.txt")
    if os.path.exists(p_ui):
        for l in open(p_ui, encoding="utf-8").read().splitlines():
            if l.strip():
                viejas[l.split("|")[0]] = l
    # las caras de la demo (las ~30 primeras) mandan: si cartas_classic.txt aun no existe, son las de cartas_ui.txt
    p_cl = os.path.join(UI, "cartas_classic.txt")
    p_demo = os.path.join(CACHE, "demo.txt")
    if not os.path.exists(p_demo):
        open(p_demo, "w", encoding="utf-8").write("\n".join(viejas) + "\n")
    demo = [l for l in open(p_demo, encoding="utf-8").read().split() if l]
    os.makedirs(os.path.join(CACHE, "cropped"), exist_ok=True)
    lineas_en = {}
    lineas, clasicas, fuentes, sin_es, propias, faltan_arte = {}, [], {"oficial": 0, "joey": 0, "propia": 0}, [], [], []
    marcos = None
    hechas = provisionales = 0
    for c in cartas:
        cid = ID_VIEJO.get(c["name"], slug(c["name"]))
        t = c["type"]
        if t == "Spell Card":
            clase, (sub, sub_txt) = "magia", SUB_MAGIA[c["race"]]
        elif t == "Trap Card":
            clase, (sub, sub_txt) = "trampa", SUB_TRAMPA[c["race"]]
        else:
            clase = "fusion" if "Fusion" in t else "ritual" if "Ritual" in t else "normal" if t == "Normal Monster" else "efecto"
            sub, sub_txt = SUB_MONSTRUO.get(t, (clase, ""))
        if clase in ("magia", "trampa"):
            sub = clase + "_" + sub
        if c["id"] in firma_de and c["id"] not in goat_ids:   # carta firma de un personaje
            limite = firma_de[c["id"]][0]
            clasicas.append("%s|%d|%d|%s|firma|%s" % (cid, c["id"], limite, sub, firma_de[c["id"]][1]))
        else:
            limite = extra_lim[c["id"]] if c["id"] in extra_lim else LIMITE.get(c.get("banlist_info", {}).get("ban_goat"), 3)
            clasicas.append("%s|%d|%d|%s" % (cid, c["id"], limite, sub))
        # espanol
        if c["id"] in NOMBRE_PROPIO:   # la base oficial lo deja en ingles: traduccion propia del nombre
            nombre, desc = NOMBRE_PROPIO[c["id"]], es[c["id"]][1].replace(c["name"], NOMBRE_PROPIO[c["id"]])
            fuentes["propia"] += 1; propias.append(c["name"])
        elif c["id"] in es:
            nombre, desc = es[c["id"]]; fuentes["oficial"] += 1
        elif slug(c["name"]) in joey:
            nombre, desc = joey[slug(c["name"])]; fuentes["joey"] += 1
        elif str(c["id"]) in trad:
            nombre, desc = trad[str(c["id"])]["nombre"], trad[str(c["id"])]["texto"]; fuentes["propia"] += 1
            propias.append(c["name"])
        else:
            nombre, desc = c["name"], c["desc"]; sin_es.append(c["name"])
        desc = " ".join(desc.split()).replace("|", "/")
        monstruo = clase not in ("magia", "trampa")
        nivel, atk, df = (c.get("level", 0), c.get("atk", 0), c.get("def", 0)) if monstruo else (0, 0, 0)
        atributo = ATRIBUTOS.get(c.get("attribute"), "") if monstruo else "MAGIA" if clase == "magia" else "TRAMPA"
        raza = TIPOS.get(c["race"], c["race"]) if monstruo else "Carta Mágica" if clase == "magia" else "Carta de Trampa"
        lineas_en[cid] = "|".join(str(v) for v in (
            cid, c["name"], clase, c.get("attribute") if monstruo else "SPELL" if clase == "magia" else "TRAP",
            c["race"] if monstruo else "Spell Card" if clase == "magia" else "Trap Card", nivel, atk, df,
            " ".join(c["desc"].split()).replace("|", "/")))
        es_demo = False   # las 30 de la demo tambien con el espanol oficial (decision del usuario); solo se conserva su orden
        lineas[cid] = viejas[cid] if es_demo else             "|".join(str(v) for v in (cid, nombre, clase, atributo, raza, nivel, atk, df, desc))
        if SOLO is not None and cid not in SOLO:
            continue
        # ilustracion: Gemini (Joey o cola classic) o provisional (cards_cropped de YGOPRODeck)
        dcarta = os.path.join(CACHE, "cartas", cid)
        os.makedirs(dcarta, exist_ok=True)
        g = gem.get(slug(c["name"])) or gem.get(NOMBRE_JOEY.get(c["name"], ""))
        gc = os.path.join(NB, "classic", "arte", cid, "resultado.png")
        if os.path.exists(gc):
            g = gc
        if g:
            origen = "gemini|%s|%d" % (g, os.path.getmtime(g))
        else:
            crop = os.path.join(CACHE, "cropped", "%d.jpg" % c["id"])
            if not os.path.exists(crop):
                try:
                    bajar("https://images.ygoprodeck.com/images/cards_cropped/%d.jpg" % c["id"], crop)
                    time.sleep(0.1)
                except Exception as e:
                    print("sin arte de YGOPRODeck:", cid, e)
                    faltan_arte.append(cid)
                    continue
            origen = "provisional"
            provisionales += 1
        p_or = os.path.join(dcarta, "origen.txt")
        viejo = open(p_or, encoding="utf-8").read() if os.path.exists(p_or) else ""
        if viejo != origen or not os.path.exists(os.path.join(dcarta, "arte.png")):
            arte = Image.open(g if g else crop).convert("RGB")
            if max(arte.size) > 1024:   # la ventana mide 564 px: 1024 sobra y la composicion va mas rapida
                arte = arte.resize((1024, round(1024 * arte.height / arte.width)), Image.LANCZOS)
            arte.save(os.path.join(dcarta, "arte.png"))
            open(p_or, "w", encoding="utf-8").write(origen)
        if marcos is None:
            cc.W, cc.H = HD_W, HD_H   # componer() escala sus medidas (px()) al ancho: texto y marco nativos en HD
            marcos = marcos_hd()
            cc.CARTAS = os.path.join(CACHE, "cartas")   # componer() lee <CARTAS>/<id>/arte.png
        efecto = clase != "normal" and "Effect" in t or clase == "efecto"
        versiones = [] if es_demo else [("", nombre, desc, TIPOS.get(c["race"], c["race"]), sub_txt, ES)]
        versiones.append(("en_", c["name"], " ".join(c["desc"].split()), c["race"], SUB_EN.get(sub, ""), EN))
        for pref, nom_v, desc_v, raza_v, sub_v, txt in versiones:
            firma = "|".join((origen, nom_v, desc_v, raza_v, sub_v))
            p_f = os.path.join(dcarta, "firma_%s.txt" % (pref or "es_"))
            if not TODO and os.path.exists(p_f) and open(p_f, encoding="utf-8").read() == firma and all(
                    os.path.exists(os.path.join(UI, f % (pref, cid))) for f in ("mini_%s%s.png", "cara_hd_%s%s.jpg")):
                continue
            d = {"id": cid, "nombre": nom_v, "atk": atk, "def": df, "nivel": nivel, "tipo": raza_v, "tipo_literal": True,
                 "atributo": c.get("attribute") or "-", "efecto": efecto, "clase": clase, "descripcion": desc_v}
            if monstruo:
                partes = ([txt["ritual"]] if clase == "ritual" else []) + ([" / " + sub_v] if sub_v else []) +                          ([txt["fusion"]] if clase == "fusion" else []) + ([txt["efecto"]] if efecto and clase != "fusion" else [])
                d["extra_tipo"] = "".join(partes)
            else:
                d["etiqueta"] = txt["etiqueta"](clase, sub_v)
            cara = cc.componer(d, marcos[clase])
            cara.save(os.path.join(UI, f"cara_hd_{pref}{cid}.jpg"), quality=92, optimize=True)   # panel de detalle / 4K
            for nom, ancho in (("cara", 360), ("mini", 180)):   # como preparar_ui.py (ahora reducidas desde la HD)
                alto = round(cara.height * ancho / cara.width)
                cara.resize((ancho, alto), Image.LANCZOS).convert("RGBA").save(
                    os.path.join(UI, f"{nom}_{pref}{cid}.png"), optimize=True)
            open(p_f, "w", encoding="utf-8").write(firma)
            hechas += 1
            if hechas % 50 == 0:
                print("caras compuestas:", hechas, flush=True)
    orden = [i for i in demo if i in lineas] + [i for i in lineas if i not in demo]
    open(p_ui, "w", encoding="utf-8", newline="\n").write("\n".join(lineas[i] for i in orden) + "\n")
    open(p_cl, "w", encoding="utf-8", newline="\n").write("\n".join(clasicas) + "\n")
    open(os.path.join(UI, "cartas_ui_en.txt"), "w", encoding="utf-8", newline="\n").write(
        "\n".join(lineas_en[i] for i in orden) + "\n")
    gemini = sum(1 for c in cartas if gem.get(slug(c["name"])) or gem.get(NOMBRE_JOEY.get(c["name"], "")) or
                 os.path.exists(os.path.join(NB, "classic", "arte", ID_VIEJO.get(c["name"], slug(c["name"])), "resultado.png")))
    print("%d cartas | espanol: %s | arte Gemini: %d | caras compuestas ahora: %d (provisionales en esta pasada: %d)"
          % (len(lineas), fuentes, gemini, hechas, provisionales))
    if propias:
        print("traduccion propia:", ", ".join(propias))
    if sin_es:
        print("SIN espanol:", ", ".join(sin_es))
    if faltan_arte:
        print("sin arte:", ", ".join(faltan_arte))


if __name__ == "__main__":
    main()
