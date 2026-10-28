# Yugioh Classic Remaster (rama classic-ia): genera los 40 rivales y sus 120 mazos (3 variantes cada uno) con las
# 1690 cartas del Formato Goat, de forma DETERMINISTA (semillas CRC32 como deck_gen.py de Yugi Reload): el mismo
# archivo de entrada da siempre el mismo rivales.txt. El razonamiento esta en docs/rivales.md.
#
# Uso:  python -I rivales_gen.py [cartas_ui.txt] [cartas_classic.txt] [goat.json] [salida]
#   cartas_ui.txt       id|nombre|clase|ATRIBUTO|raza|nivel|atk|def|texto        (rama classic-datos)
#   cartas_classic.txt  id|passcode|limite|subtipo                                (limite 0/1/2/3 de la lista Goat)
#   goat.json           la lista Goat de YGOPRODeck (nombre y texto en ingles: materiales de fusion, rituales y los
#                       monstruos que no se pueden invocar de forma normal)
# Por defecto: D:\Docker\compartido\classic-assets\ui\ y datos/rivales.txt junto a este script.
#
# Formato de salida (lo lee src/ia.h; lineas con # son comentarios):
#   R|clave|nivel|personalidad|tema|nombre ES|nombre EN|lema ES|lema EN
#   M|clave|variante|id id id ...          mazo principal (40-60, max 3 copias, respeta prohibidas/limitadas)
#   X|clave|variante|id id ...             mazo de fusion (0-15)
#   F|fusion|material+material[+...]       receta de fusion (Polymerization + materiales de la mano o el campo)
#   T|ritual|magia_ritual                  receta de ritual (la magia + sacrificios cuyo nivel sume >= el del ritual)
#   N|id id ...                            no se pueden invocar de forma normal (la IA no los juega; los Toon si con
#                                          Toon World boca arriba)
import json, os, random, re, sys, zlib

AQUI = os.path.dirname(os.path.abspath(__file__))
UI = r"D:\Docker\compartido\classic-assets\ui"
ARG = sys.argv[1:]
F_UI = ARG[0] if len(ARG) > 0 else os.path.join(UI, "cartas_ui.txt")
F_CL = ARG[1] if len(ARG) > 1 else os.path.join(UI, "cartas_classic.txt")
F_GOAT = ARG[2] if len(ARG) > 2 else os.path.join(UI, "goat.json")
SALIDA = ARG[3] if len(ARG) > 3 else os.path.join(AQUI, "datos", "rivales.txt")


def semilla(*partes):
    return zlib.crc32(repr(partes).encode("utf-8"))


def slug(n):
    return re.sub(r"[^a-z0-9]+", "_", n.lower()).strip("_")


def num(s):
    try:
        return max(0, int(s))
    except ValueError:
        return 0


# ---------------------------------------------------------------------------------------------- cartas
C = {}
for linea in open(F_UI, encoding="utf-8"):
    p = linea.rstrip("\n").split("|")
    if len(p) < 9:
        continue
    C[p[0]] = dict(id=p[0], nombre=p[1], clase=p[2], attr=p[3], raza=p[4], nivel=num(p[5]), atk=num(p[6]),
                   defn=num(p[7]), texto=p[8], limite=3, subtipo=p[2], en="", desc="")
for linea in open(F_CL, encoding="utf-8"):
    p = linea.rstrip("\n").split("|")
    if len(p) >= 4 and p[0] in C:
        C[p[0]]["limite"] = int(p[2])
        C[p[0]]["subtipo"] = p[3]
        C[p[0]]["firma"] = len(p) > 4 and p[4] == "firma"   # cartas firma de los personajes: fuera de los genericos
# los ids son el slug del nombre en ingles, salvo estos dos (los de la demo original)
EXCEPCION = {"blue_eyes_white_dragon": "blue_eyes", "gaia_the_fierce_knight": "gaia", "red_eyes_black_dragon": "red_eyes"}
POR_EN = {}
# firma.json (junto a goat.json, lo deja preparar_classic.py): las cartas firma de los personajes (rama
# classic-personajes), para sus recetas de fusion/ritual
F_FIRMA = os.path.join(os.path.dirname(F_GOAT), "firma.json")
# extra.json (preparar_classic.py): las cartas extra (datos/cartas_extra.txt), para sus recetas. No entran en los mazos
# generados (asi los mazos de los rivales no cambian al anadirlas)
F_EXTRA = os.path.join(os.path.dirname(F_GOAT), "extra.json")
EXTRA = set()
if os.path.exists(os.path.join(AQUI, "datos", "cartas_extra.txt")):
    for l in open(os.path.join(AQUI, "datos", "cartas_extra.txt"), encoding="utf-8").read().splitlines():
        if l.strip() and not l.startswith("#"):
            EXTRA.add(re.sub(r"[^a-z0-9]+", "_", l.split("|")[0].lower()).strip("_"))
if os.path.exists(F_GOAT):
    for g in json.load(open(F_GOAT, encoding="utf-8"))["data"] + (
            json.load(open(F_FIRMA, encoding="utf-8")) if os.path.exists(F_FIRMA) else []) + (
            json.load(open(F_EXTRA, encoding="utf-8")) if os.path.exists(F_EXTRA) else []):
        i = slug(g["name"])
        i = EXCEPCION.get(i, i)
        if i in C:
            C[i]["en"] = g["name"]
            C[i]["desc"] = g.get("desc", "")
            POR_EN[g["name"].lower()] = i
else:
    print("AVISO: sin goat.json no hay recetas de fusion/ritual", file=sys.stderr)


# nombres viejos de los textos (YGOPRODeck) -> la carta con su nombre actual: Amphibious Bugroth pide "Defender of the Sea"
NOMBRE_VIEJO = {"defender of the sea": "sentinel_of_the_seas"}


def idd(nombre_en):
    i = POR_EN.get(nombre_en.lower()) or NOMBRE_VIEJO.get(nombre_en.lower()) or slug(nombre_en)
    i = EXCEPCION.get(i, i)
    return i if i in C else None


def monstruo(c):
    return c["clase"] in ("normal", "efecto", "ritual", "fusion")


# no se pueden invocar de forma normal (sin motor de efectos no hay manera de sacarlos): fuera de los mazos, menos los
# Toon (la IA los saca con Toon World boca arriba, regla basica de la epoca)
NOMI_RE = re.compile(r"cannot be normal summoned|must (first )?be special summoned|can only be special summoned|"
                     r"must first be special", re.I)
NOMI = sorted(i for i, c in C.items() if c["clase"] in ("normal", "efecto") and NOMI_RE.search(c["desc"]))
# rama classic-efectos: las que el motor de efectos (src/ia.h) ya sabe invocar de forma especial desde la mano. Siguen
# en N (no se invocan de forma normal) pero pueden ir en los mazos de los personajes (listas a mano). Los mazos
# genericos de este script no cambian (siguen sin ellas: regenerarlos baraja todos los mazos de los rivales)
ESPECIALES_MOTOR = {"black_luster_soldier_envoy_of_the_beginning", "chaos_emperor_dragon_envoy_of_the_end", "larvae_moth",
                    "great_moth", "perfectly_ultimate_great_moth"}
NOMI_MAZO = [i for i in NOMI if i not in ESPECIALES_MOTOR]

# recetas de fusion: los nombres entre comillas de la primera linea del texto ingles
FUSION = {}
for i, c in C.items():
    if c["clase"] != "fusion" or not c["desc"]:
        continue
    mats = [idd(n) for n in re.findall(r'"([^"]+)"', c["desc"].split("\n")[0])]
    if len(mats) >= 2 and all(mats):
        FUSION[i] = mats
# recetas de ritual: la magia dice a que monstruo invoca
RITUAL = {}
for i, c in C.items():
    if c["subtipo"] == "magia_ritual":
        m = re.search(r'Ritual Summon "([^"]+)"', c["desc"])
        r = idd(m.group(1)) if m else None
        if r and C[r]["clase"] == "ritual":
            RITUAL[r] = i
for i, c in C.items():   # genericas: Contrato con el Abismo (OSCURIDAD) y Canto de la Tierra (TIERRA)
    if c["clase"] == "ritual" and i not in RITUAL:
        if c["attr"] == "OSCURIDAD" and "contract_with_the_abyss" in C:
            RITUAL[i] = "contract_with_the_abyss"
        elif c["attr"] == "TIERRA" and "earth_chant" in C:
            RITUAL[i] = "earth_chant"

JUGABLE = [c for c in C.values() if c["limite"] > 0 and not c.get("firma") and c["id"] not in EXTRA]
MAIN_MON = [c for c in JUGABLE if c["clase"] in ("normal", "efecto") and c["id"] not in NOMI]
TOONS = [c for c in JUGABLE if c["subtipo"] == "toon" and c["clase"] in ("normal", "efecto")]


def ids(*nombres):
    r = []
    for n in nombres:
        i = idd(n)
        if i and C[i]["limite"] > 0 and not C[i].get("firma"):
            r.append(i)
    return r


# ---------------------------------------------------------------------------------------------- staples (por nivel)
# Sin motor de efectos estas cartas son, por ahora, farol boca abajo; cuando lleguen los efectos ya estaran en su sitio
BASICAS_MAG = ids("Dian Keto the Cure Master", "Red Medicine", "Mooyan Curry", "Sparks", "Hinotama", "Fissure",
                  "Swords of Revealing Light", "Monster Reborn", "Dark Hole", "Change of Heart", "Stop Defense")
BASICAS_TRAP = ids("Trap Hole", "Reinforcements", "Castle Walls", "Waboku", "Fake Trap", "Jar of Greed",
                   "Enchanted Javelin", "Dust Tornado", "Ultimate Offering", "House of Adhesive Tape", "Eatgaboon")
GOAT_MAG = ids("Pot of Greed", "Graceful Charity", "Heavy Storm", "Mystical Space Typhoon", "Scapegoat",
               "Book of Moon", "Nobleman of Crossout", "Smashing Ground", "Snatch Steal", "Premature Burial",
               "Delinquent Duo", "Confiscation", "Brain Control", "Swords of Revealing Light", "Upstart Goblin",
               "Enemy Controller", "Shrink", "Reinforcement of the Army", "Creature Swap", "Dark Hole", "Raigeki",
               "Monster Reborn", "Change of Heart", "Harpie's Feather Duster", "Painful Choice")
GOAT_TRAP = ids("Mirror Force", "Torrential Tribute", "Call of the Haunted", "Ring of Destruction", "Sakuretsu Armor",
                "Bottomless Trap Hole", "Magic Cylinder", "Dust Tornado", "Solemn Judgment", "Magic Jammer",
                "Trap Dustshoot", "Ceasefire", "Waboku", "Scapegoat", "Return from the Different Dimension",
                "Sakuretsu Armor", "Widespread Ruin", "Compulsory Evacuation Device")
QUEMA = ids("Ookazi", "Tremendous Fire", "Hinotama", "Final Flame", "Sparks", "Just Desserts", "Ceasefire",
            "Secret Barrel", "Chain Energy", "Wave-Motion Cannon", "Meteor of Destruction", "Poison of the Old Man",
            "Lava Golem", "Magic Cylinder", "Chain Destruction", "Level Limit - Area B", "Gravity Bind",
            "Messenger of Peace", "Nightmare Wheel", "Spirit Message \"I\"")
VOLTEO_EXTRA = ids("Book of Moon", "Book of Taiyou", "Stop Defense", "Light of Intervention", "Swords of Revealing Light",
                   "Waboku", "Magical Hats", "Scapegoat")
# magia de campo y equipos por raza (los de la epoca)
CAMPO = {
    "Dragón": ids("Mountain", "Dragon Treasure", "Stamping Destruction", "Dragon's Gunfire"),
    "Bestia Alada": ids("Mountain", "Rising Air Current", "Follow Wind"),
    "Trueno": ids("Mountain", "Umi", "Electro-Whip"),
    "Insecto": ids("Forest", "Insect Armor with Laser Cannon", "Insect Barrier", "Insect Imitation"),
    "Planta": ids("Forest", "Vile Germs"),
    "Bestia": ids("Forest", "Beast Fangs", "Bark of Dark Ruler"),
    "Guerrero-Bestia": ids("Sogen", "Forest", "Beast Fangs"),
    "Guerrero": ids("Sogen", "Legendary Sword", "Reinforcement of the Army", "Fusion Sword Murasame Blade"),
    "Zombi": ids("Wasteland", "Violet Crystal", "Call of the Mummy", "Book of Life", "Call of the Haunted"),
    "Roca": ids("Wasteland", "Gaia Power", "Rock Bombardment"),
    "Dinosaurio": ids("Wasteland", "Raise Body Heat", "Big Evolution Pill"),
    "Lanzador de Conjuros": ids("Yami", "Book of Secret Arts", "Magical Labyrinth", "Mystical Moon", "Dark Magic Curtain"),
    "Demonio": ids("Yami", "Dark Energy", "Pandemonium", "Mystic Plasma Zone"),
    "Hada": ids("Sanctuary in the Sky", "Luminous Spark", "Elf's Light", "Soul of the Pure"),
    "Máquina": ids("Machine Conversion Factory", "Limiter Removal", "Machine Duplication", "Gaia Power"),
    "Aqua": ids("Umi", "A Legendary Ocean", "Power of Kaishin", "Steel Shell"),
    "Pez": ids("Umi", "A Legendary Ocean", "Kaiser Colosseum"),
    "Serpiente Marina": ids("Umi", "A Legendary Ocean", "Power of Kaishin"),
    "Piro": ids("Molten Destruction", "Salamandra", "Burning Land", "Tremendous Fire"),
    "Reptil": ids("Wasteland", "Snake Fang"),
}
POLY = idd("Polymerization")

# ---------------------------------------------------------------------------------------------- dificultad
# por nivel (1..8): techo de ATK de los nivel 1-4, techo del ATK medio del mazo (1467 = el maximo de los mazos CPU
# originales, nivel 5), cuantos de tributo (5-6 / 7+), y cuantas cartas "de la epoca" fuertes entran
NIVEL = {
    #   cap4  prom   t56 t7  goatMag goatTrap
    1: (1400, 1150,  1,  0,  0,      0),
    2: (1500, 1250,  2,  0,  1,      1),
    3: (1600, 1330,  2,  1,  2,      2),
    4: (1700, 1400,  3,  1,  3,      3),
    5: (1800, 1467,  3,  1,  4,      4),
    6: (1900, 1540,  3,  2,  6,      5),
    7: (2000, 1620,  4,  2,  8,      6),
    8: (2100, 1700,  4,  3,  10,     7),
}

# ---------------------------------------------------------------------------------------------- rivales
# (clave, nivel, personalidad, tema, filtro de monstruos, nombre ES, nombre EN, lema ES, lema EN)
# temas: raza:<raza>[,<raza>]  attr:<ATRIBUTO>  sub:<subtipo>  fusion  ritual  quema  toon  goat
RIVALES = [
    # nivel 1: novatos (mazos flojos, cometen errores)
    ("mina", 1, "paciente", "raza:Insecto", "Mina la Entomóloga", "Mina the Entomologist", "Los bichos también muerden.", "Bugs bite too."),
    ("ramon", 1, "defensivo", "raza:Planta,Planta", "Ramón el Jardinero", "Ramón the Gardener", "Todo lo que crece, resiste.", "Whatever grows, endures."),
    ("pesca", 1, "equilibrado", "raza:Pez,Aqua", "Pesca la Marinera", "Pesca the Sailor", "Mar en calma, red llena.", "Calm sea, full net."),
    ("bruno", 1, "agresivo", "raza:Bestia", "Bruno el Domador", "Bruno the Tamer", "¡Mis bestias tienen hambre!", "My beasts are hungry!"),
    ("petra", 1, "defensivo", "raza:Roca", "Petra la Cantera", "Petra the Quarry", "Nadie atraviesa la piedra.", "No one gets through stone."),
    # nivel 2
    ("celeste", 2, "agresivo", "raza:Bestia Alada", "Celeste del Viento", "Celeste of the Wind", "Desde arriba se ve todo.", "From above you see it all."),
    ("sierpe", 2, "temerario", "raza:Reptil,Serpiente Marina", "Sierpe la Escamosa", "Sierpe the Scaled", "Un mordisco basta.", "One bite is enough."),
    ("rex", 2, "agresivo", "raza:Dinosaurio,Reptil", "Rex el Paleontólogo", "Rex the Paleontologist", "Hace millones de años que gano.", "I've been winning for millions of years."),
    ("aurora", 2, "defensivo", "raza:Hada", "Aurora la Celestial", "Aurora the Celestial", "La luz siempre protege.", "Light always protects."),
    ("voltio", 2, "farolero", "raza:Trueno", "Voltio el Chispas", "Voltio the Spark", "Cuidado, que da calambre...", "Careful, it shocks..."),
    # nivel 3
    ("rodrigo", 3, "equilibrado", "raza:Guerrero", "Rodrigo el Caballero", "Rodrigo the Knight", "Espada en mano, sin miedo.", "Sword in hand, no fear."),
    ("osario", 3, "temerario", "raza:Zombi", "Osario el Sepulturero", "Osario the Gravedigger", "Lo que cae vuelve a levantarse.", "What falls, rises again."),
    ("brasa", 3, "agresivo", "raza:Piro", "Brasa la Incendiaria", "Brasa the Arsonist", "Todo arde.", "Everything burns."),
    ("garra", 3, "equilibrado", "raza:Guerrero-Bestia", "Garra el Mercenario", "Garra the Mercenary", "Lucho por quien pague.", "I fight for whoever pays."),
    ("tuerca", 3, "paciente", "sub:union", "Tuerca la Ensambladora", "Tuerca the Assembler", "Pieza a pieza.", "Piece by piece."),
    # nivel 4
    ("bitio", 4, "calculador", "raza:Máquina", "Bitio el Ingeniero", "Bitio the Engineer", "Todo es cuestión de números.", "It's all about numbers."),
    ("merlina", 4, "paciente", "raza:Lanzador de Conjuros", "Merlina la Hechicera", "Merlina the Sorceress", "La magia no tiene prisa.", "Magic is never in a hurry."),
    ("belcebu", 4, "farolero", "raza:Demonio", "Belcebú el Embaucador", "Belcebú the Trickster", "¿Te atreves a atacar?", "Do you dare attack?"),
    ("marea", 4, "defensivo", "raza:Aqua,Pez,Serpiente Marina", "Marea la Abisal", "Marea of the Abyss", "Las olas siempre vuelven.", "The waves always return."),
    ("espectro", 4, "farolero", "sub:espiritu", "Espectro el Errante", "Espectro the Wanderer", "Vengo y voy como el viento.", "I come and go like the wind."),
    # nivel 5
    ("mascara", 5, "farolero", "sub:volteo", "Máscara la Ilusionista", "Máscara the Illusionist", "Nada es lo que parece.", "Nothing is what it seems."),
    ("tinta", 5, "temerario", "toon", "Tinta la Dibujante", "Tinta the Cartoonist", "¡Esto es de risa!", "This is hilarious!"),
    ("vendaval", 5, "agresivo", "attr:VIENTO", "Vendaval el Piloto", "Vendaval the Pilot", "Más rápido que tú.", "Faster than you."),
    ("volcan", 5, "agresivo", "attr:FUEGO", "Volcán la Forjadora", "Volcán the Forger", "Forjada en lava.", "Forged in lava."),
    ("terron", 5, "defensivo", "attr:TIERRA", "Terrón el Montañés", "Terrón the Mountaineer", "La montaña no se mueve.", "The mountain does not move."),
    # nivel 6
    ("ceniza", 6, "paciente", "quema", "Ceniza la Pirotécnica", "Ceniza the Pyrotechnician", "Poco a poco te quemo.", "Little by little, you burn."),
    ("lumen", 6, "calculador", "attr:LUZ", "Lumen el Paladín", "Lumen the Paladin", "La luz no falla.", "The light never fails."),
    ("alquimia", 6, "equilibrado", "fusion", "Alquimia la Fusionista", "Alquimia the Fusionist", "Dos mejor que uno.", "Two are better than one."),
    ("oficiante", 6, "paciente", "ritual", "Oficiante el Ritualista", "Oficiante the Ritualist", "Todo sacrificio tiene premio.", "Every sacrifice has its reward."),
    ("draco", 6, "agresivo", "raza:Dragón", "Draco el Domador de Dragones", "Draco the Dragon Tamer", "Mis dragones no perdonan.", "My dragons show no mercy."),
    # nivel 7
    ("noche", 7, "farolero", "attr:OSCURIDAD", "Noche la Sombría", "Noche the Shadowed", "En la oscuridad no ves mis cartas.", "In the dark you can't see my cards."),
    ("abisal", 7, "calculador", "attr:AGUA", "Abisal el Almirante", "Abisal the Admiral", "La marea está de mi lado.", "The tide is on my side."),
    ("acero", 7, "agresivo", "raza:Guerrero,Guerrero-Bestia", "Acero el Campeón", "Acero the Champion", "Invicto en cien duelos.", "Undefeated in a hundred duels."),
    ("arcano", 7, "calculador", "raza:Lanzador de Conjuros,Hada", "Arcano el Archimago", "Arcano the Archmage", "Ya vi tu jugada.", "I already saw your move."),
    ("titan", 7, "temerario", "raza:Máquina,Trueno", "Titán el Coloso", "Titán the Colossus", "Aplastar es mi estrategia.", "Crushing is my strategy."),
    # nivel 8: maestros
    ("leyenda", 8, "agresivo", "raza:Dragón", "Leyenda la Reina Dragón", "Leyenda the Dragon Queen", "Arrodíllate ante mis dragones.", "Kneel before my dragons."),
    ("caos", 8, "calculador", "attr:LUZ,OSCURIDAD", "Caos el Equilibrista", "Caos the Balancer", "Luz y sombra, a mi servicio.", "Light and shadow serve me."),
    ("cabra", 8, "paciente", "goat", "Cabra la Estratega", "Cabra the Strategist", "El duelo se gana en el turno diez.", "The duel is won on turn ten."),
    ("quimera", 8, "equilibrado", "fusion", "Quimera el Gran Alquimista", "Quimera the Grand Alchemist", "Mis fusiones no tienen limite.", "My fusions know no limit."),
    ("averno", 8, "temerario", "raza:Demonio,Zombi", "Averno el Señor del Inframundo", "Averno the Underlord", "Tu alma ya es mía.", "Your soul is already mine."),
]


def filtro(tema):
    """monstruos del tema (del mazo principal, jugables)"""
    if tema.startswith("raza:"):
        razas = set(tema[5:].split(","))
        return [c for c in MAIN_MON if c["raza"] in razas]
    if tema.startswith("attr:"):
        at = set(tema[5:].split(","))
        return [c for c in MAIN_MON if c["attr"] in at]
    if tema.startswith("sub:"):
        sub = tema[4:]
        base = [c for c in MAIN_MON if c["subtipo"] == sub]
        razas = {c["raza"] for c in base}
        return base + [c for c in MAIN_MON if c["raza"] in razas and c not in base and c["nivel"] <= 4]
    if tema == "toon":
        return TOONS + [c for c in MAIN_MON if c["raza"] in ("Lanzador de Conjuros", "Demonio") and c["nivel"] <= 4]
    if tema == "quema":
        return [c for c in MAIN_MON if c["defn"] >= 1300 and c["nivel"] <= 4] + [C[i] for i in QUEMA if C[i] in MAIN_MON]
    return list(MAIN_MON)   # fusion, ritual, goat: de todo


def armar(r, variante):
    clave, nivel, pers, tema = r[0], r[1], r[2], r[3]
    rng = random.Random(semilla(clave, nivel, tema, variante))
    cap4, prom_max, t56, t7, n_gm, n_gt = NIVEL[nivel]
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
        return [i for i in main if monstruo(C[i])]

    def ruido():
        return rng.uniform(0, 300 + 40 * variante)

    pool = filtro(tema)
    if len([c for c in pool if c["nivel"] <= 4]) < 10:   # temas cortos: se completan con monstruos del mismo atributo
        attrs = {c["attr"] for c in pool} or {"TIERRA"}
        pool += [c for c in MAIN_MON if c["attr"] in attrs and c["nivel"] <= 4 and c not in pool]

    def fuerza(c):
        k = 0.95 if pers == "defensivo" or tema == "quema" else 0.75
        return max(c["atk"], c["defn"] * k) + 0.3 * c["atk"]

    # especiales: fusion / ritual / toon
    if tema == "fusion":
        tope_mat = 4 if nivel < 8 else 6   # materiales que se pueden invocar sin (o con un) sacrificio
        recetas = [(f, m) for f, m in FUSION.items() if C[f]["limite"] > 0 and not C[f].get("firma") and all(
            C[x]["clase"] in ("normal", "efecto") and x not in NOMI and C[x]["limite"] > 0 and C[x]["nivel"] <= tope_mat
            for x in m) and sum(1 for x in m if C[x]["nivel"] >= 5) <= 1]
        recetas.sort(key=lambda fm: -(C[fm[0]]["atk"] + rng.uniform(0, 900)))
        tope = 2600 if nivel < 8 else 9999
        for f, m in recetas:
            if len(extra) >= 4 + nivel // 2:
                break
            if C[f]["atk"] > tope or len(main) + len(m) * 2 > 26:
                continue
            extra.append(f)
            for x in m:
                add(x, 2 if nivel < 8 else 3)
                add(x, 2 if nivel < 8 else 3)
        for _ in range(3):
            add(POLY)
    if tema == "ritual":
        rituales = [x for x in RITUAL if C[x]["limite"] > 0 and C[RITUAL[x]]["limite"] > 0 and not C[x].get("firma") and x not in EXTRA
                    and not C[RITUAL[x]].get("firma")]
        rng.shuffle(rituales)
        for x in rituales[:2 if nivel < 8 else 3]:
            add(x, 2)
            add(x, 2)
            add(RITUAL[x], 2)
            add(RITUAL[x], 2)
    if tema == "toon":
        add("toon_world", 3)
        add("toon_world", 3)
        for c in TOONS:
            if c["nivel"] <= 6:
                add(c["id"], 2)
                add(c["id"], 2)

    # tributos (5-6 y 7+): los iconos del tema primero
    grandes = [c for c in pool if c["nivel"] >= 5] or [c for c in MAIN_MON if c["nivel"] >= 5]
    for lo, hi, n in ((5, 6, t56), (7, 12, t7)):
        cand = sorted([c for c in grandes if lo <= c["nivel"] <= hi], key=lambda c: -(fuerza(c) + ruido()))
        if nivel <= 3:   # los novatos no llevan los mas fuertes
            cand = [c for c in cand if c["atk"] <= 2000 + 150 * nivel] or cand
        for c in cand[:n]:
            add(c["id"], 1 if nivel <= 2 else 2)
    # nucleo nivel 1-4: los mejores del tema bajo el techo del nivel, 1-3 copias
    nm = rng.randint(19, 22) + (2 if pers in ("defensivo", "agresivo") else 0) - (2 if tema in ("quema", "goat") else 0)
    bajos = sorted([c for c in pool if c["nivel"] <= 4 and c["atk"] <= cap4], key=lambda c: -(fuerza(c) + ruido()))
    for j, c in enumerate(bajos):
        if len(mons()) >= nm:
            break
        add(c["id"])
        if j < 5 and rng.random() < 0.7:
            add(c["id"])
        if j < 2 and rng.random() < 0.5:
            add(c["id"])
    relleno = sorted([c for c in MAIN_MON if c["nivel"] <= 4 and c["atk"] <= cap4], key=lambda c: -(fuerza(c) + ruido()))
    for c in relleno:
        if len(mons()) >= nm:
            break
        add(c["id"], 2)

    # fusiones "de paso": si el mazo trae todos los materiales de una receta, entra la fusion y Polymerization
    for f, m in sorted(FUSION.items()):
        if f not in extra and C[f]["limite"] > 0 and not C[f].get("firma") and all(x in cnt for x in m) and len(extra) < 15:
            extra.append(f)
    if extra and POLY not in cnt:
        add(POLY)
        add(POLY)

    # magias y trampas: del tema (campo/equipos), de la epoca segun el nivel y basicas
    mags = list(CAMPO.get(tema[5:].split(",")[0], [])) if tema.startswith("raza:") else []
    if tema == "quema":
        q = list(QUEMA)
        rng.shuffle(q)
        mags += q * 2
    if tema == "sub:volteo" or pers == "farolero":
        mags += VOLTEO_EXTRA
    gm, gt = list(GOAT_MAG), list(GOAT_TRAP)
    rng.shuffle(gm)
    rng.shuffle(gt)
    if tema == "goat":
        n_gm, n_gt = 12, 9
    mags += gm[:n_gm]
    basicas = list(BASICAS_MAG) + list(BASICAS_TRAP)
    rng.shuffle(basicas)
    trampas = gt[:n_gt]
    total = rng.choice((40, 40, 40, 41, 42)) if nivel < 6 else 40
    for i in mags:
        if len(main) >= total - max(4, n_gt):
            break
        add(i, 2 if tema != "quema" else 3)
    for i in trampas + basicas:
        if len(main) >= total:
            break
        add(i, 2)
    while len(main) < 40:   # por si algo no alcanzo
        add(rng.choice(relleno)["id"], 2)

    # techo del ATK medio (como reequilibrar_atk.py): cambia los nivel 1-4 mas fuertes por otros medios
    medios = [c for c in pool if c["nivel"] <= 4 and c["atk"] <= prom_max] or [c for c in MAIN_MON if c["atk"] <= prom_max]
    for _ in range(80):
        ms = mons()
        prom = sum(C[i]["atk"] for i in ms) / max(1, len(ms))
        if prom <= prom_max + 40:
            break
        b = [i for i in ms if C[i]["nivel"] <= 4 and C[i]["clase"] != "ritual" and not any(i in FUSION[f] for f in extra)]
        if not b:
            break
        peor = max(b, key=lambda i: C[i]["atk"])
        main.remove(peor)
        cnt[peor] -= 1
        if not cnt[peor]:
            del cnt[peor]
        rng.shuffle(medios)
        for c in medios:
            if add(c["id"]):
                break
    # fusiones sin sus materiales (por el reequilibrio): fuera
    extra = [f for f in extra if all(x in cnt for x in FUSION[f])][:15]
    if not extra:
        while POLY in cnt and tema != "fusion":
            main.remove(POLY)
            cnt[POLY] -= 1
            if not cnt[POLY]:
                del cnt[POLY]
            add(rng.choice(relleno)["id"], 2)
    main.sort(key=lambda i: (not monstruo(C[i]), C[i]["clase"], i))
    return main, sorted(extra)


def validar(main, extra):
    assert 40 <= len(main) <= 60, len(main)
    assert len(extra) <= 15
    for i in set(main) | set(extra):
        n = main.count(i) + extra.count(i)
        assert n <= min(3, C[i]["limite"]), (i, n, C[i]["limite"])
        assert C[i]["limite"] > 0
    for i in main:
        assert C[i]["clase"] != "fusion"


def main():
    os.makedirs(os.path.dirname(SALIDA), exist_ok=True)
    out = ["# rivales.txt - generado por remake-ps5/rivales_gen.py (no editar a mano). Ver docs/rivales.md",
           "# R|clave|nivel|personalidad|tema|nombre ES|nombre EN|lema ES|lema EN ; M/X|clave|variante|ids ; F|fusion|mat+mat ; T|ritual|magia ; N|ids"]
    resumen = []
    for r in RIVALES:
        out.append("R|" + "|".join(str(x) for x in r))
        for v in range(3):
            main_, extra = armar(r, v)
            validar(main_, extra)
            out.append("M|%s|%d|%s" % (r[0], v, " ".join(main_)))
            out.append("X|%s|%d|%s" % (r[0], v, " ".join(extra)))
            ms = [i for i in main_ if monstruo(C[i])]
            resumen.append("# %-10s n%d v%d %-11s %2d cartas, %2d monstruos (ATK medio %4d, %d de tributo), %2d magias, %2d trampas, %d fusiones"
                           % (r[0], r[1], v, r[2], len(main_), len(ms), sum(C[i]["atk"] for i in ms) / max(1, len(ms)),
                              sum(1 for i in ms if C[i]["nivel"] >= 5), sum(1 for i in main_ if C[i]["clase"] == "magia"),
                              sum(1 for i in main_ if C[i]["clase"] == "trampa"), len(extra)))
    for f in sorted(x for x in FUSION if not C[x].get("firma")):   # (las de las cartas firma: datos/personajes_rivales.txt)
        out.append("F|%s|%s" % (f, "+".join(FUSION[f])))
    for x in sorted(x for x in RITUAL if not C[x].get("firma")):
        out.append("T|%s|%s" % (x, RITUAL[x]))
    out.append("N|" + " ".join(i for i in NOMI if not C[i].get("firma")))
    out += resumen
    open(SALIDA, "w", encoding="utf-8", newline="\n").write("\n".join(out) + "\n")
    print("rivales: %d, mazos: %d, recetas de fusion: %d, de ritual: %d, no invocables: %d -> %s"
          % (len(RIVALES), len(RIVALES) * 3, len(FUSION), len(RITUAL), len(NOMI), SALIDA))
    for linea in resumen:
        print(linea)


if __name__ == "__main__":
    main()
