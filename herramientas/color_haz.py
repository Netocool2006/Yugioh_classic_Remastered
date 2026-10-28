# Color del haz de invocacion (rayo de abduccion) de cada monstruo segun su naturaleza (pedido del usuario
# 2026-10-16: "si es de fuego, rojo; si es de hielo, blanco (tal cual esta ahora); si es una maquina, amarillo; y asi
# sucesivamente. Usa ML para distinguir esto en los monstruos que ya estan y en los que faltan").
#
# Como decide (en orden):
#  1. conocimiento: cartas cuyo color se conoce de la serie o lo dijo el usuario (Mago Oscuro azul, la Maga morado...)
#     y el color de magia ya revisado de los misticos (color_magia en reglas_combate.json, ver color_magia.py)
#  2. nombre: palabras clave (fire/flame/blaze -> fuego, ice/frost/snow -> hielo, thunder/electric -> trueno, ...)
#  3. tipo y atributo: cada tipo tiene su familia; los tipos "fuertes" (Pyro, Machine, Thunder, Plant, Insect, Zombie,
#     Aqua/Fish/Sea Serpent, Rock, Dinosaur) mandan siempre; los "debiles" (Warrior, Beast, Fiend, Fairy, Spellcaster,
#     Winged Beast, Reptile, Dragon) ceden ante FIRE/WATER, y si el atributo sugiere otra familia el caso es AMBIGUO
#  4. ML para los ambiguos: de la ilustracion (figura de Gemini nano-banana/todo/figura/<slug>/resultado.png, fondo
#     negro, o el arte de la carta del juego) se sacan los colores dominantes con k-means (pixeles de la figura,
#     pesando los saturados y luminosos) y un histograma de tono; un k-NN entrenado con los monstruos NO ambiguos
#     (su familia es segura) vota la familia del ambiguo entre sus candidatas, junto con la cercania del tono
#     dominante al tono de cada familia y un sesgo a favor del tipo
#  5. afinado: dentro de su familia, el tono se corre hacia el tono dominante de la ilustracion (hasta 14 grados) si
#     esta cerca; el blanco-hielo y el plata no se tocan (el hielo queda tal cual estaba el haz)
# Escribe remake/diseno/color_haz.json (carta -> rgb, familia, fuente) y remake-ps5/src/reglas/color_haz.h.
# Uso: python -I herramientas/color_haz.py
import colorsys, json, os, re
import numpy as np
from PIL import Image
from sklearn.cluster import KMeans
from sklearn.neighbors import KNeighborsClassifier

BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# los datos que no van al repo (figuras, arte, cartas del remake) pueden estar en el repo principal
RAICES = [BASE, os.environ.get("YUGI_DATOS", r"D:\Yugi-Reload")]


def buscar(*rel):
    for r in RAICES:
        p = os.path.join(r, *rel)
        if os.path.exists(p):
            return p
    return None


REGLAS = os.path.join(BASE, "remake", "diseno", "reglas_combate.json")
SALIDA_JSON = os.path.join(BASE, "remake", "diseno", "color_haz.json")
SALIDA_H = os.path.join(BASE, "remake-ps5", "src", "reglas", "color_haz.h")

# familias: color del borde del haz (RGB lineal, escala 0-1.6 como los halos del motor). El nucleo es casi blanco.
FAMILIAS = {
    "hielo":      ((0.30, 0.78, 1.45), "blanco-hielo / cian (el haz original)"),
    "fuego":      ((1.60, 0.30, 0.05), "rojo-naranja"),
    "maquina":    ((1.55, 1.25, 0.08), "amarillo"),
    "trueno":     ((0.70, 0.40, 1.60), "violeta electrico"),
    "planta":     ((0.15, 1.50, 0.22), "verde"),
    "insecto":    ((0.90, 1.55, 0.05), "verde lima"),
    "demonio":    ((0.85, 0.08, 1.15), "morado oscuro"),
    "zombi":      ((0.55, 1.10, 0.30), "verde putrido"),
    "hada":       ((1.50, 1.25, 0.55), "dorado-blanco"),
    "arcano":     ((0.20, 0.40, 1.60), "azul arcano"),
    "roca":       ((1.15, 0.68, 0.22), "ocre"),
    "bestia":     ((1.55, 0.85, 0.10), "ambar"),
    "aire":       ((0.35, 1.50, 0.85), "verde-aire (menta)"),
    "guerrero":   ((0.95, 1.05, 1.30), "plata-acero"),
    "dinosaurio": ((1.50, 0.52, 0.12), "naranja tierra"),
    "reptil":     ((0.80, 1.00, 0.15), "verde oliva"),
}
SIN_TONO = {"hielo", "guerrero"}   # no se afinan con la ilustracion

TIPO = {"Pyro": "fuego", "Machine": "maquina", "Thunder": "trueno", "Plant": "planta", "Insect": "insecto",
        "Zombie": "zombi", "Aqua": "hielo", "Fish": "hielo", "SeaSerpent": "hielo", "Rock": "roca",
        "Dinosaur": "dinosaurio", "Fiend": "demonio", "Fairy": "hada", "Spellcaster": "arcano", "Beast": "bestia",
        "BeastWarrior": "bestia", "WingedBeast": "aire", "Warrior": "guerrero", "Reptile": "reptil"}
FUERTES = {"Pyro", "Machine", "Thunder", "Plant", "Insect", "Zombie", "Aqua", "Fish", "SeaSerpent", "Rock", "Dinosaur"}
ATRIBUTO = {"FIRE": "fuego", "WATER": "hielo", "WIND": "aire", "EARTH": "roca", "LIGHT": "hada", "DARK": "demonio"}

PALABRAS = [  # (expresion, familia): palabras enteras del nombre en ingles
    (r"fire|flame|flames|blaze|blazing|burn|burning|inferno|magma|lava|volcanic|ember|heat|hinotama|fireball|"
     r"phoenix|scorch|pyro|solar", "fuego"),
    (r"ice|icy|frost|frozen|freeze|snow|blizzard|glacier|winter|cold|crystal", "hielo"),
    (r"thunder|thunderbolt|lightning|electric|electro|spark|sparks|volt|voltage|bolt|plasma|storm|kaminari|"
     r"raigeki", "trueno"),
    (r"poison|venom|toxic|rotten|decay|corpse|ghoul|mummy|skeleton|undead", "zombi"),
    (r"tree|forest|flower|rose|leaf|leaves|vine|petal|seed|mushroom|bamboo|lotus", "planta"),
    (r"angel|holy|shining|celestial|heaven|heavenly|divine|sun", "hada"),
    (r"wind|cyclone|tornado|hurricane|gale|breeze|sky", "aire"),
]

# conocimiento (serie / usuario): carta -> (familia, rgb o None para el de la familia, motivo)
CONOCIDO = {
    "dark_magician": ("arcano", (0.18, 0.45, 1.60), "usuario: el Mago Oscuro azul"),
    "dark_magician_girl": ("arcano", (0.55, 0.18, 1.50), "usuario: la Maga con su morado"),
    "red_eyes_b_dragon": ("fuego", (1.60, 0.22, 0.06), "serie: dragon de ojos rojos, llama negra-roja"),
    "blue_eyes_white_dragon": ("hielo", (0.55, 0.82, 1.60), "serie: rafaga de luz blanca-azul"),
    "summoned_skull": ("trueno", None, "serie: ataca con rayos"),
    "baby_dragon": ("fuego", (1.60, 0.55, 0.10), "serie: dragon naranja que escupe fuego"),
    "flame_swordsman": ("fuego", None, "carta: espadachin de las llamas"),
    "time_wizard": ("hada", (1.40, 1.10, 0.30), "carta: ruleta del tiempo dorada"),
    "exodia_the_forbidden_one": ("hada", (1.40, 1.00, 0.25), "serie: energia dorada de Exodia"),
    "jinzo": ("maquina", None, "serie: androide"),
    "kuriboh": ("demonio", None, "serie: bola de pelo demonio"),
    "zoa": ("demonio", None, "serie: demonio acorazado"),
    "meteor_dragon": ("fuego", None, "carta: dragon meteoro en llamas"),
    "black_skull_dragon": ("fuego", None, "serie: fusion de Red-Eyes, llama negra-roja"),
    "blue_eyes_ultimate_dragon": ("hielo", (0.55, 0.82, 1.60), "serie: como el Blue-Eyes, luz blanca-azul"),
    "blue_eyes_toon_dragon": ("hielo", (0.55, 0.82, 1.60), "serie: como el Blue-Eyes, luz blanca-azul"),
    "kagemusha_of_the_blue_flame": ("fuego", (0.25, 0.55, 1.60), "carta: su llama es azul"),
    "giant_turtle_who_feeds_on_flames": ("hielo", None, "carta: tortuga de agua (come llamas, no es de fuego)"),
}
# alias del motor (id corto del remake -> slug del nombre en ingles)
ALIAS_REMAKE = {"red_eyes": "red_eyes_b_dragon", "blue_eyes": "blue_eyes_white_dragon", "gaia": "gaia_the_fierce_knight"}


def slug(s):
    return re.sub(r"[^a-z0-9]+", "_", s.lower()).strip("_")


def hue_de(rgb):
    m = max(rgb)
    return colorsys.rgb_to_hsv(*(x / m for x in rgb))[0] if m > 0 else 0.0


def dist_tono(a, b):
    d = abs(a - b) % 1.0
    return min(d, 1.0 - d)


BLANCO = (0.92, 0.97, 1.0)   # el nucleo del haz original (efecto_haz.frag)


def nucleo_de(rgb):
    """nucleo casi blanco con un toque del color: el del hielo queda igual al blanco original (misma cuenta que
    nucleoHaz en efecto_haz.frag)"""
    f = [x / max(FAMILIAS["hielo"][0]) for x in FAMILIAS["hielo"][0]]
    return tuple(round(max(0.0, min(1.0, b + 0.10 * (x / max(rgb) - fx))), 2) for b, x, fx in zip(BLANCO, rgb, f))


# ---------------------------------------------------------------- imagen
def arte_del_juego():
    """nombre de la carta -> ruta del bmp del juego (list_card.txt de Getallcard)"""
    lista = buscar("datadat-extraido", "card", "list_card.txt")
    if not lista:
        return {}
    carpeta, actual, res = os.path.dirname(lista), None, {}
    for l in open(lista, encoding="latin-1").read().splitlines():
        l = l.strip()
        if l.startswith("//") and not re.match(r"//\s*\d{4}:", l):
            actual = l[2:].strip()
        elif l.lower().endswith(".bmp") and actual:
            res.setdefault(actual, os.path.join(carpeta, l))
    return res


def cargar_imagen(k, nombre, artes):
    fig = buscar("nano-banana", "todo", "figura", k, "resultado.png")
    if fig:
        return Image.open(fig).convert("RGB"), "figura"
    bmp = artes.get(nombre)
    if bmp and os.path.exists(bmp):
        im = Image.open(bmp).convert("RGB")
        w, h = im.size
        return im.crop((int(w * 0.12), int(h * 0.19), int(w * 0.88), int(h * 0.71))), "arte"
    return None, None


BINS = 18


def rasgos(im, origen):
    """k-means de los colores de la figura -> (histograma de tono 18 bins + 2 rasgos, tono dominante, croma)"""
    a = np.asarray(im.resize((128, 128), Image.BILINEAR), dtype=np.float32) / 255.0
    px = a.reshape(-1, 3)
    mx, mn = px.max(1), px.min(1)
    if origen == "figura":
        px, mx, mn = px[mx > 0.10], mx[mx > 0.10], mn[mx > 0.10]   # fuera el fondo negro
    if len(px) < 200:
        return None
    sat = (mx - mn) / np.maximum(mx, 1e-6)
    km = KMeans(n_clusters=6, n_init=4, random_state=0).fit(px)
    hist = np.zeros(BINS)
    dominante, mejor = None, -1.0
    for c in range(6):
        sel = km.labels_ == c
        cen = km.cluster_centers_[c]
        h, s, v = colorsys.rgb_to_hsv(*np.clip(cen, 0, 1))
        peso = sel.mean() * (s ** 1.5) * (v ** 2)   # pesa lo saturado y luminoso (lo que se lee como "su color")
        hist[int(h * BINS) % BINS] += peso
        if s > 0.22 and peso > mejor:
            mejor, dominante = float(peso), float(h)
    croma = float((sat > 0.35).mean())
    hist = np.convolve(np.concatenate([hist[-1:], hist, hist[:1]]), [0.25, 0.5, 0.25], "valid")
    n = hist.sum()
    hist = hist / n if n > 0 else hist
    return np.concatenate([hist, [croma, float(np.median(mx))]]), dominante, croma


# ---------------------------------------------------------------- decision
def por_nombre(nombre):
    pal = set(re.findall(r"[a-z]+", nombre.lower()))
    for expr, fam in PALABRAS:
        hit = pal & set(expr.split("|"))
        if hit:
            return fam, sorted(hit)[0]
    return None, None


def candidatas(c):
    """familia segura (o None) y candidatas si es ambiguo"""
    tipo, attr = c["tipo"], c["atributo"]
    ft, fa = TIPO.get(tipo), ATRIBUTO.get(attr)
    if tipo == "Dragon":
        # el dragon toma el color de su elemento; si no es fuego/agua/luz, puede ser el del fuego que escupe
        return (fa, None) if attr in ("FIRE", "WATER", "LIGHT") else (None, [fa, "fuego"])
    if tipo in FUERTES or ft == fa:
        return ft, None
    if attr in ("FIRE", "WATER"):
        return fa, None                         # el elemento manda sobre los tipos debiles
    if ft is None:
        return fa, None
    if attr == "EARTH":
        return ft, None                         # tierra es el atributo "neutro": no le discute al tipo
    return None, [ft, fa]


def afinar(rgb, fam, tono):
    if tono is None or fam in SIN_TONO:
        return rgb, None
    h0 = hue_de(rgb)
    d = ((tono - h0 + 0.5) % 1.0) - 0.5
    if abs(d) > 40 / 360:
        return rgb, None
    corr = max(-14 / 360, min(14 / 360, 0.5 * d))
    m = max(rgb)
    h, s, v = colorsys.rgb_to_hsv(*(x / m for x in rgb))
    r = colorsys.hsv_to_rgb((h + corr) % 1.0, s, v)
    return tuple(round(x * m, 2) for x in r), round(corr * 360)


def main():
    reg = json.load(open(REGLAS, encoding="utf-8"))
    cartas = reg["cartas"]
    artes = arte_del_juego()

    datos = {}
    for k, c in cartas.items():
        im, origen = cargar_imagen(k, c["nombre"], artes)
        r = rasgos(im, origen) if im is not None else None
        datos[k] = (r, origen)
        if len(datos) % 100 == 0:
            print("imagenes %d/%d" % (len(datos), len(cartas)), flush=True)

    # entrenamiento del k-NN: los monstruos cuya familia es segura por tipo/atributo (sin conocimiento ni nombre)
    X, y = [], []
    for k, c in cartas.items():
        segura, _ = candidatas(c)
        if segura and datos[k][0] is not None:
            X.append(datos[k][0][0])
            y.append(segura)
    X = np.array(X)

    salida = {}
    for k, c in sorted(cartas.items()):
        r, origen = datos[k]
        tono = r[1] if r else None
        fuente, rgb = None, None
        segura, cand = candidatas(c)
        fam_nombre, palabra = por_nombre(c["nombre"])
        if k in CONOCIDO:
            fam, rgb, motivo = CONOCIDO[k]
            fuente = "conocimiento: " + motivo
        elif c.get("color_magia") and c["tipo"] == "Spellcaster" and fam_nombre not in ("fuego", "hielo", "trueno"):
            fam, rgb = "arcano", tuple(c["color_magia"]["halo"])
            fuente = "color de su magia (color_magia: %s)" % c["color_magia"]["fuente"]
        elif fam_nombre and not (c["tipo"] in FUERTES and TIPO[c["tipo"]] != fam_nombre and fam_nombre in ("hada", "aire", "planta")):
            fam, fuente = fam_nombre, "nombre ('%s')" % palabra
        elif segura:
            fam, fuente = segura, "tipo/atributo (%s, %s)" % (c["tipo"], c["atributo"])
        elif r is None:
            fam, fuente = cand[0], "tipo (%s; ambiguo sin imagen)" % c["tipo"]
        else:
            # ambiguo: k-NN sobre la ilustracion + tono dominante + sesgo al tipo
            # k-NN entre los ejemplos seguros de SUS familias candidatas (cuales se le parecen mas en color)
            sub = [j for j, f in enumerate(y) if f in cand]
            knn = KNeighborsClassifier(n_neighbors=min(9, len(sub)), weights="distance", metric="cosine")
            knn.fit(X[sub], [y[j] for j in sub])
            proba = dict(zip(knn.classes_, knn.predict_proba([r[0]])[0]))
            puntos = {}
            for j, f in enumerate(cand):
                afin = 0.0
                if tono is not None and f not in SIN_TONO:
                    afin = float(np.exp(-(dist_tono(tono, hue_de(FAMILIAS[f][0])) * 360 / 35) ** 2))
                elif f in SIN_TONO and f == "guerrero":
                    afin = max(0.0, 1.0 - 2.5 * r[2])   # plata: figura poco saturada
                puntos[f] = 0.6 * proba.get(f, 0.0) + 0.4 * afin + (0.15 if j == 0 else 0.0)
            fam = max(puntos, key=puntos.get)
            fuente = "ML (k-NN %s + tono %s de la %s): %s" % (
                "/".join("%s %.2f" % (f, proba.get(f, 0.0)) for f in cand),
                "%d grados" % round(tono * 360) if tono is not None else "-", origen,
                " vs ".join("%s %.2f" % (f, puntos[f]) for f in cand))
        if rgb is None:
            rgb, corr = afinar(FAMILIAS[fam][0], fam, tono)
            if corr:
                fuente += "; tono afinado %+d grados por la %s (k-means)" % (corr, origen)
        nucleo = nucleo_de(rgb)
        salida[k] = {"nombre": c["nombre"], "tipo": c["tipo"], "atributo": c["atributo"], "familia": fam,
                     "rgb": [round(x, 2) for x in rgb], "nucleo": list(nucleo), "fuente": fuente,
                     "imagen": origen, "tono_imagen": round(tono * 360) if tono is not None else None}

    doc = {"_nota": "GENERADO por herramientas/color_haz.py: color del haz de invocacion de cada monstruo "
                    "(rgb = borde del haz, escala 0-1.6; nucleo casi blanco). No editar a mano.",
           "familias": {f: {"rgb": list(v[0]), "color": v[1]} for f, v in FAMILIAS.items()},
           "tipo": TIPO, "atributo": ATRIBUTO, "cartas": salida}
    json.dump(doc, open(SALIDA_JSON, "w", encoding="utf-8", newline=""), indent=1, ensure_ascii=False)

    # header C++: por carta (slug del nombre y alias del motor) y la regla por tipo/atributo para las que falten
    filas = []
    for k, s in salida.items():
        filas.append('\t{ "%s", { %.2ff, %.2ff, %.2ff }, { %.2ff, %.2ff, %.2ff } },   // %s (%s, %s): %s'
                     % (k, *s["rgb"], *s["nucleo"], s["nombre"], s["tipo"], s["atributo"], s["familia"]))
    for corto, largo in ALIAS_REMAKE.items():
        s = salida[largo]
        filas.append('\t{ "%s", { %.2ff, %.2ff, %.2ff }, { %.2ff, %.2ff, %.2ff } },   // alias de %s: %s'
                     % (corto, *s["rgb"], *s["nucleo"], largo, s["familia"]))

    nuc = nucleo_de

    def fila_regla(clave, fam):
        rgb = FAMILIAS[fam][0]
        return '\t{ "%s", { %.2ff, %.2ff, %.2ff }, { %.2ff, %.2ff, %.2ff } },   // %s' % (clave, *rgb, *nuc(rgb), fam)

    reglas_tipo = [fila_regla(t, f) for t, f in TIPO.items()]
    reglas_attr = [fila_regla(a, f) for a, f in ATRIBUTO.items()]
    hielo = FAMILIAS["hielo"][0]
    open(SALIDA_H, "w", encoding="utf-8", newline="").write(
        "// GENERADO por herramientas/color_haz.py (remake/diseno/color_haz.json): no editar a mano.\n"
        "// Color del haz de invocacion de cada monstruo segun su naturaleza: rgb = borde del haz, nucleo = casi blanco.\n"
        "// Fuego rojo, agua/hielo blanco-cian (el haz original), maquina amarillo, trueno violeta, etc. Las cartas sin\n"
        "// dato usan la regla por tipo/atributo (colorHazPorTipo).\n"
        "#pragma once\n#include <cstring>\n\nnamespace combate {\n\n"
        "struct ColorHaz { const char* carta; float rgb[3], nucleo[3]; };\n\n"
        "const ColorHaz TABLA_COLOR_HAZ[] = {\n" + "\n".join(filas) + "\n};\n\n"
        "// regla por tipo (manda) y por atributo, para las cartas que no estan en la tabla\n"
        "const ColorHaz COLOR_HAZ_TIPO[] = {\n" + "\n".join(reglas_tipo) + "\n};\n"
        "const ColorHaz COLOR_HAZ_ATRIBUTO[] = {\n" + "\n".join(reglas_attr) + "\n};\n"
        "const ColorHaz COLOR_HAZ_HIELO = { \"hielo\", { %.2ff, %.2ff, %.2ff }, { %.2ff, %.2ff, %.2ff } };\n\n"
        % (*hielo, *nuc(hielo)) +
        "inline const ColorHaz* colorHazBuscar(const ColorHaz* t, size_t n, const char* clave)\n{\n"
        "\tfor (size_t k = 0; clave && k < n; k++) {\n\t\tif (strcmp(t[k].carta, clave) == 0) {\n\t\t\treturn &t[k];\n\t\t}\n\t}\n"
        "\treturn nullptr;\n}\n\n"
        "// por tipo/atributo (\"Machine\", \"FIRE\"...): el elemento FIRE/WATER manda sobre los tipos debiles\n"
        "inline const ColorHaz& colorHazPorTipo(const char* tipo, const char* atributo)\n{\n"
        "\tconst bool elemento = atributo && (strcmp(atributo, \"FIRE\") == 0 || strcmp(atributo, \"WATER\") == 0);\n"
        "\tconst char* fuertes[] = { %s };\n"
        "\tbool fuerte = false;\n\tfor (const char* f : fuertes) {\n\t\tfuerte = fuerte || (tipo && strcmp(tipo, f) == 0);\n\t}\n"
        "\tconst ColorHaz* t = colorHazBuscar(COLOR_HAZ_TIPO, sizeof(COLOR_HAZ_TIPO) / sizeof(COLOR_HAZ_TIPO[0]), tipo);\n"
        "\tconst ColorHaz* a = colorHazBuscar(COLOR_HAZ_ATRIBUTO, sizeof(COLOR_HAZ_ATRIBUTO) / sizeof(COLOR_HAZ_ATRIBUTO[0]), atributo);\n"
        "\tif (t && (fuerte || !elemento)) {\n\t\treturn *t;\n\t}\n"
        "\treturn a ? *a : COLOR_HAZ_HIELO;\n}\n\n"
        "// por carta (id del motor o slug del nombre en ingles); sin dato: el haz original (hielo)\n"
        "inline const ColorHaz& colorHazDe(const char* carta)\n{\n"
        "\tconst ColorHaz* c = colorHazBuscar(TABLA_COLOR_HAZ, sizeof(TABLA_COLOR_HAZ) / sizeof(TABLA_COLOR_HAZ[0]), carta);\n"
        "\treturn c ? *c : COLOR_HAZ_HIELO;\n}\n\n} // namespace combate\n"
        % ", ".join('"%s"' % t for t in sorted(FUERTES)))

    from collections import Counter
    print(Counter(s["familia"] for s in salida.values()))
    print(Counter(s["fuente"].split(" ")[0].split(":")[0] for s in salida.values()))
    print(len(salida), "monstruos ->", SALIDA_JSON, "y", SALIDA_H)


if __name__ == "__main__":
    main()
