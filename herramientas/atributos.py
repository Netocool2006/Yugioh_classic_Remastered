# ATRIBUTOS AUTOMATICOS de todo monstruo (regla del usuario 2026-10-17: "quemar la regla en el codigo"; no revisar
# figura por figura). El motor escanea cada modelo AL CARGARLO (remake-ps5/src/reglas/escaneo.h): alas, aliento,
# boca, agua. Esta herramienta:
#  1) genera remake-ps5/src/reglas/cartas_datos.h: los DATOS de cada carta del juego original (tipo, atributo, nombre;
#     de re-output/cards.csv) con su regla y su plantilla de cuerpo (remake/diseno/reglas_combate.json), y el id del
#     remake cuando es distinto (red_eyes, blue_eyes...). Cubre TODAS las cartas de monstruo del juego: un lote nuevo
#     de la serie no necesita regenerarla (sincronizar.sh la regenera igual si encuentra los datos);
#  2) imprime la tabla de comprobacion: el MISMO escaneo que hace el motor (en Python, sobre los .glb instalados) y lo
#     que decide para cada uno (alas, aliento, agua, regla, boca) y si el modelo fue girado para mirar al frente
#     (orientar_auto.py deja un nodo giro_frente_<g>).
# Uso: python -I atributos.py [--tabla] [<carpeta de modelos>]
import csv, json, os, re, sys

import numpy as np

AQUI = os.path.dirname(os.path.abspath(__file__))
BASE = os.path.dirname(AQUI)
DATOS = r"D:\Yugi-Reload"
CARDS = os.path.join(DATOS, "re-output", "cards.csv")
CARTAS_REMAKE = os.path.join(DATOS, "remake", "recursos", "cartas", "cartas.json")
REGLAS = os.path.join(BASE, "remake", "diseno", "reglas_combate.json")
SALIDA = os.path.join(BASE, "remake-ps5", "src", "reglas", "cartas_datos.h")
NO_SON = {"tablero", "esfera", "baculo", "espada", "rayo"}


def slug(s):
    return re.sub(r"[^a-z0-9]+", "_", s.lower()).strip("_")


def c_str(s):
    return '"%s"' % s.replace("\\", "\\\\").replace('"', '\\"')


def datos_cartas():
    reg = json.load(open(REGLAS, encoding="utf-8"))["cartas"]
    filas = {}
    for r in csv.DictReader(open(CARDS, encoding="utf-8")):
        if r["attr"] in ("-", "") or r["tipo"] in ("Magic", "Trap"):
            continue
        k = slug(r["nombre"])
        if k in filas:
            continue
        g = reg.get(k, {})
        filas[k] = dict(nombre=r["nombre"], tipo=r["tipo"], atributo=r["attr"], regla=g.get("regla", ""), cuerpo=g.get("cuerpo", ""))
    if os.path.exists(CARTAS_REMAKE):
        c = json.load(open(CARTAS_REMAKE, encoding="utf-8"))
        for x in c if isinstance(c, list) else c.get("cartas", []):
            k = slug(x.get("nombre_en", x["id"]))
            if x["id"] not in filas and k in filas:
                filas[x["id"]] = filas[k]
    return filas


def generar(filas):
    lineas = ["\t{ %s, %s, %s, %s, %s, %s }," % (c_str(k), c_str(v["nombre"]), c_str(v["tipo"]), c_str(v["atributo"]),
                                               c_str(v["regla"]), c_str(v["cuerpo"])) for k, v in sorted(filas.items())]
    texto = ("// GENERADO por herramientas/atributos.py (re-output/cards.csv + remake/diseno/reglas_combate.json): no editar a mano.\n"
             "// Datos de cada carta de monstruo del juego original para el escaneo de atributos (escaneo.h): nombre, tipo,\n"
             "// atributo, regla de combate del Excel del usuario y plantilla de cuerpo. Por id del remake o slug del nombre.\n"
             "#pragma once\n#include <cstring>\n\nnamespace combate {\n\n"
             "struct DatoCarta { const char* carta; const char* nombre; const char* tipo; const char* atributo; const char* regla; const char* cuerpo; };\n\n"
             "const DatoCarta TABLA_CARTAS[] = {\n" + "\n".join(lineas) + "\n};\n\n"
             "inline const DatoCarta* buscarDato(const char* carta)\n{\n"
             "\tfor (const DatoCarta& d : TABLA_CARTAS) {\n\t\tif (strcmp(d.carta, carta) == 0) {\n\t\t\treturn &d;\n\t\t}\n\t}\n"
             "\treturn nullptr;\n}\n\n} // namespace combate\n")
    viejo = open(SALIDA, encoding="utf-8").read() if os.path.exists(SALIDA) else ""
    if viejo != texto:
        open(SALIDA, "w", encoding="utf-8", newline="\n").write(texto)
    print(len(filas), "cartas ->", SALIDA, "(sin cambios)" if viejo == texto else "")


# ---- el mismo escaneo que escaneo.h (escanearMalla y clasificar), para la tabla de comprobacion
def escanear(p):
    p = p[:: max(1, len(p) // 30000)]
    lo, hi = p.min(0), p.max(0)
    t = np.maximum(hi - lo, 1e-4)
    H = t[1]
    cx = np.percentile(p[:, 0], 50, method="lower")
    dx = p[:, 0] - cx
    u = (p[:, 1] - lo[1]) / H
    nucleo = np.percentile(np.abs(dx[(u > 0.3) & (u < 0.7)]), 50, method="lower")
    fuera = max(1.8 * nucleo, 0.12 * H)
    lados = []
    for s in (-1, 1):
        m = (u > 0.35) & (dx * s > fuera)
        q = p[m]
        l = dict(n=int(m.sum()), alcance=float(np.abs(dx[m]).max() / H) if m.any() else 0.0, ancho=0.0, plano=0.0, q=q)
        if len(q) >= 50:
            ev = np.sqrt(np.maximum(np.sort(np.linalg.eigvalsh(np.cov((q - q.mean(0)).T, bias=True)))[::-1], 0))
            l["ancho"], l["plano"] = ev[1] / max(ev[0], 1e-6), ev[2] / max(ev[0], 1e-6)
        lados.append(l)
    a, b = lados
    n = len(p)
    alas = (a["n"] >= 0.04 * n and b["n"] >= 0.04 * n and min(a["alcance"], b["alcance"]) >= 0.45 and
            abs(a["alcance"] - b["alcance"]) < 0.12 and min(a["ancho"], b["ancho"]) >= 0.5 and abs(a["ancho"] - b["ancho"]) < 0.2 and
            max(a["plano"], b["plano"]) <= 0.40)
    col = (np.abs(dx) < 0.12 * t[0]) & (u > 0.35)
    zmax = p[col, 2].max()
    punta = p[col & (p[:, 2] > zmax - 0.04 * t.max())]
    return dict(alas=alas, boca=punta.mean(0), lo=lo, hi=hi)


def clasificar(k, e, d, base):
    tipo, cuerpo = (d or {}).get("tipo", ""), (d or {}).get("cuerpo", "")
    regla = base or (d or {}).get("regla") or "embestida"
    maquina = regla == "maquina" or tipo == "Machine"
    con_arma = regla in ("arma_mano", "arma_fuego", "mistico") or cuerpo in (
        "guerrero_espada", "guerrero_arma_larga", "tirador", "lanzador_baculo", "lanzador_manos", "jinete")
    agua = (d or {}).get("atributo") == "WATER" or tipo in ("Aqua", "Fish", "SeaSerpent")
    alas = (not maquina and (cuerpo == "volador_alado" or tipo == "WingedBeast")) or         (tipo in ("Dragon", "Insect", "Fairy", "Machine") and e["alas"])
    humano = tipo in ("Fiend", "Warrior", "Spellcaster", "Fairy", "BeastWarrior")
    aliento = (tipo == "Dragon" or (not humano and "dragon" in ((d or {}).get("nombre", k)).lower())) and not maquina and not con_arma
    if aliento:
        regla = "dragon"
    elif agua and regla == "embestida":
        regla = "agua"
    elif alas and regla == "embestida":
        regla = "insecto_aire" if tipo == "Insect" else "animal_aire"
    return dict(alas=alas, aliento=aliento, agua=agua, regla=regla)


def tabla(filas, carpeta):
    sys.path.insert(0, AQUI)
    from escaneo_glb import vertices
    from orientar_auto import ya_girado
    th = open(os.path.join(BASE, "remake-ps5", "src", "reglas", "tabla_reglas.h"), encoding="utf-8").read()
    base = dict(re.findall(r'\{ "(\w+)", "(\w+)"', th))
    print("%-34s %-5s %-8s %-7s %-5s %-13s %s" % ("slug", "alas", "giro", "aliento", "agua", "regla", "boca (modelo)"))
    for f in sorted(os.listdir(carpeta)):
        k = f[:-4]
        if not f.endswith(".glb") or k in NO_SON or k.endswith("_anim"):
            continue
        ruta = os.path.join(carpeta, f)
        e = escanear(vertices(ruta))
        c = clasificar(k, e, filas.get(k), base.get(k))
        giro = (ya_girado(ruta) or "-").replace("giro_frente_", "")
        boca = "%.2f %.2f %.2f" % tuple(e["boca"]) if c["aliento"] else ""
        print("%-34s %-5s %-8s %-7s %-5s %-13s %s" % (k, "SI" if c["alas"] else "-", giro, "SI" if c["aliento"] else "-",
                                                      "SI" if c["agua"] else "-", c["regla"], boca))


if __name__ == "__main__":
    filas = datos_cartas()
    generar(filas)
    if "--tabla" in sys.argv:
        args = [a for a in sys.argv[1:] if not a.startswith("--")]
        tabla(filas, args[0] if args else os.path.join(DATOS, "remake-ps5", "assets", "models", "yugi"))
