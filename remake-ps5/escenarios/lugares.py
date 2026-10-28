"""Los demas escenarios pre-renderizados (todo menos el castillo, que tiene su propio castillo.py).

Blender 5.2 sin interfaz:
  blender --background --factory-startup --python lugares.py -- --lugar <clave> --modo vista|pano [--ancho 8192]
Para los lugares que existen de verdad (ciudad, estadio, museo, playa, bosque...) el fondo lejano es una foto
panoramica HDRI de Poly Haven (CC0) y lo cercano (suelo, plataformas, columnas, rocas...) se modela con texturas
escaneadas; los lugares de fantasia (templo, Atlantida, laberinto, Noah, sombras) se arman enteros.
Lista de insumos: third_party/LICENCIAS_entornos.md.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import comun as C  # noqa: E402
from comun import B  # noqa: E402
from mathutils import Vector  # noqa: E402

import bpy  # noqa: E402

op = C.args()
argv = sys.argv[sys.argv.index("--") + 1:]
LUGAR = argv[argv.index("--lugar") + 1]
Y0 = C.SUELO
ENV = os.environ


def base(hdri, u_centro, fuerza=1.0, expo=0.0, look="AgX - Medium High Contrast", niebla=None):
    sc = C.escena_nueva()
    sc.view_settings.exposure = float(ENV.get("ESC_EXPO", expo))
    try:
        sc.view_settings.look = look
    except TypeError:
        pass
    if hdri:
        C.mundo_hdri(hdri, fuerza=fuerza, giro_grados=C.giro_para(float(ENV.get("ESC_U", u_centro))))
    else:
        w = bpy.data.worlds.new("negro")
        w.use_nodes = True
        w.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.0
        sc.world = w
    sin = []
    if niebla:
        sin.append(C.niebla_mundo(niebla[0], color=niebla[1], anisotropia=0.4, radio=niebla[2] if len(niebla) > 2 else 200.0,
                                  alto=niebla[3] if len(niebla) > 3 else 40.0))
    C.mesa_sombra()
    return sc, sin


def anillo_de(n, r, z0=0.0, desde=0.0, hasta=2 * math.pi):
    for k in range(n):
        a = desde + (hasta - desde) * (k + 0.5) / n
        yield math.cos(a) * r, math.sin(a) * r + z0, a


# ================================================================================================== lugares
def ciudad_batallas():
    """plaza de Battle City de dia: rascacielos de vidrio de verdad (Canary Wharf) y la plaza de adoquines"""
    sc, sin = base("canary_wharf_8k.hdr", 0.0, fuerza=1.0, niebla=(0.0005, (0.85, 0.9, 1.0), 220.0, 60.0))
    plaza = C.mat_ph("plaza", "concrete_pavers", tam=3.0, tinte=(0.85, 0.85, 0.85), variar=0.3, mugre=None, brillo_humedo=(14.0, 0.3))
    C.plano("suelo", -140, -140, 140, 140, Y0, plaza)
    borde = C.mat_ph("borde", "concrete_floor_worn_001", tam=3.0, mugre=None)
    farol = C.modelo("street_lamp_01", 2.4)
    for lado in (-1, 1):
        for z in (-30.0, -8.0, 14.0):
            C.colocar_grupo(farol, (lado * 30.0, Y0, z), giro_y=math.pi / 2 * lado)
    # macetones/bordes de la plaza
    for x, z in ((-22, -34), (22, -34), (-36, 26), (36, 26)):
        C.caja("macetero", (x, Y0, z), (3.0, 0.5, 3.0), borde)
    return sc, sin


def rascacielos_noche():
    """azotea de un rascacielos al anochecer: azoteas y torres de una ciudad de verdad (sunset_jhbcentral, tomada desde
    una azotea) y la azotea de hormigon con sus equipos, el helipuerto y la antena"""
    sc, sin = base("sunset_jhbcentral_8k.hdr", 0.45, fuerza=0.5, expo=-0.3, niebla=(0.0012, (0.6, 0.6, 0.85), 120.0, 30.0))
    piso = C.mat_ph("azotea", "concrete_floor_worn_001", tam=3.0, tinte=(0.7, 0.7, 0.72), mugre=None, brillo_humedo=(9.0, 0.7))
    pared = C.mat_ph("parapeto", "concrete_floor_worn_001", tam=2.0, tinte=(0.6, 0.6, 0.62))
    metal = C.mat_ph("metal", "rusty_metal_02", tam=2.0, tinte=(0.6, 0.6, 0.62))
    X0, X1, Z0, Z1 = -38.0, 38.0, -34.0, 26.0
    C.plano("azotea", X0, Z0, X1, Z1, Y0, piso)
    C.caja("edificio", ((X0 + X1) / 2, Y0 - 120.0, (Z0 + Z1) / 2), ((X1 - X0) / 2, 59.9, (Z1 - Z0) / 2), pared)
    for (a, b) in (((X0, Z0), (X1, Z0)), ((X0, Z0), (X0, Z1)), ((X1, Z0), (X1, Z1)), ((X0, Z1), (X1, Z1))):
        cx, cz = (a[0] + b[0]) / 2, (a[1] + b[1]) / 2
        C.caja("parapeto", (cx, Y0, cz), (max(abs(b[0] - a[0]) / 2, 0.25), 0.6, max(abs(b[1] - a[1]) / 2, 0.25)), pared)
    # equipos del techo: aire acondicionado, tanque de agua, la antena con su luz roja
    for i, (x, z) in enumerate(((-30, -26), (-24, -26), (28, -20), (28, -12), (-30, 12))):
        C.caja("equipo", (x, Y0, z), (2.2, 1.1, 1.6), metal)
        C.cilindro("ventilador", (x, Y0 + 2.2, z), 0.9, 0.9, 0.15, C.mat_simple("rejilla", (0.05, 0.05, 0.05), metal=1.0, aspereza=0.4), lados=24)
    C.cilindro("tanque", (-28, Y0 + 4.0, 22), 3.0, 3.0, 4.5, C.mat_ph("madera_t", "wood_floor_deck", tam=2.0), lados=40)
    for k in range(4):
        a = k * math.pi / 2 + 0.6
        C.cilindro("pata", (-28 + math.cos(a) * 2.6, Y0, 22 + math.sin(a) * 2.6), 0.15, 0.15, 4.0, metal, lados=8)
    C.cilindro("antena", (30, Y0, 20), 0.25, 0.06, 22.0, metal, lados=8)
    C.cilindro("faro_rojo", (30, Y0 + 22.0, 20), 0.35, 0.35, 0.4, C.mat_emision("rojo", (1.0, 0.05, 0.02), 30.0), lados=12)
    # el helipuerto: la H y el circulo pintados
    pintura = C.mat_simple("pintura", (0.7, 0.68, 0.6), aspereza=0.6)
    for k in range(48):
        a = 2 * math.pi * k / 48
        C.bloque("circulo", (math.cos(a) * 11.0, Y0 + 0.01, math.sin(a) * 11.0 - 4.0), (0.35, 0.01, 0.75), pintura, giro_y=-a)
    for lado in (-1, 1):
        C.caja("luz_borde", (lado * (X1 - 1.0), Y0 + 0.6, Z0 + 1.0), (0.15, 0.15, 0.15), C.mat_emision("ambar", (1.0, 0.5, 0.1), 40.0))
    return sc, sin


def estadio_kc():
    """el estadio del Gran Campeonato KC: un estadio de verdad (stadium_01) y el escenario de duelo en el centro"""
    sc, sin = base("stadium_01_8k.hdr", 0.75, fuerza=1.0, niebla=(0.0008, (0.95, 0.9, 0.85), 200.0, 40.0))
    piso = C.mat_ph("escenario", "metal_plate", tam=2.5, tinte=(0.45, 0.47, 0.52), mugre=None)
    cesped = C.mat_ph("cesped", "grass_path_2", tam=4.0, tinte=(0.8, 0.9, 0.75), mugre=None, variar=0.2)
    C.plano("cesped", -150, -150, 150, 150, Y0 - 0.6, cesped)
    C.cilindro("escenario", (0, Y0 - 0.6, 0), 30.0, 29.5, 0.6, piso, lados=96)
    luz = C.mat_emision("borde_azul", (0.15, 0.5, 1.0), 25.0)
    C.cilindro("borde_luz", (0, Y0 - 0.05, 0), 30.05, 30.05, 0.08, luz, lados=96, tapa=False)
    # los postes con el logo KC (pantallas) a los lados
    pantalla = C.mat_emision("pantalla", (0.3, 0.6, 1.0), 6.0)
    for lado in (-1, 1):
        C.caja("poste", (lado * 34.0, Y0, -22.0), (0.4, 6.0, 0.4), piso)
        C.caja("pantalla_kc", (lado * 34.0, Y0 + 8.0, -22.0), (5.0, 2.8, 0.25), pantalla, giro=-lado * 0.5)
    return sc, sin


def museo_egipcio():
    """la sala egipcia del Museo de Domino: un museo de verdad (museum_of_ethnography) con columnas, obeliscos y
    vitrinas de piedra arenisca"""
    sc, sin = base("museum_of_ethnography_8k.hdr", 0.62, fuerza=1.0)
    piso = C.mat_ph("marmol", "marble_tiles", tam=3.0, tinte=(0.8, 0.75, 0.68), mugre=None)
    arenisca = C.mat_ph("arenisca", "sandstone_blocks_08", tam=2.0, tinte=(0.95, 0.85, 0.7))
    C.plano("piso", -60, -60, 60, 60, Y0, piso)
    for lado in (-1, 1):
        for z in (-26.0, -8.0, 10.0):
            C.columna("columna", (lado * 24.0, Y0, z), 1.6, 14.0, arenisca)
        C.obelisco("obelisco", (lado * 12.0, Y0, -32.0), 1.3, 13.0, arenisca)
    vidrio = C.mat_vidrio()
    oro = C.mat_simple("oro", (0.8, 0.55, 0.2), aspereza=0.25, metal=1.0)
    for x, z in ((-32, -18), (32, -18), (-32, 4), (32, 4)):
        C.caja("vitrina_base", (x, Y0, z), (2.4, 0.7, 1.4), arenisca)
        C.caja("vitrina", (x, Y0 + 1.4, z), (2.3, 1.0, 1.3), vidrio)
        C.caja("reliquia", (x, Y0 + 1.4, z), (0.9, 0.5, 0.5), oro)
    for x in (-20.0, 20.0):
        C.luz_punto(B(x, Y0 + 12.0, -20.0), 4000, color=(1.0, 0.8, 0.55), radio=1.0)
    return sc, sin


def tienda_juegos():
    """calle de Ciudad Domino de dia: una calle de verdad (urban_street_04) con el asfalto y las veredas"""
    sc, sin = base("urban_street_04_8k.hdr", 0.62, fuerza=1.0, niebla=(0.0008, (0.9, 0.93, 1.0), 200.0, 40.0))
    asfalto = C.mat_ph("asfalto", "asphalt_02", tam=4.0, tinte=(0.8, 0.8, 0.8), mugre=None, variar=0.25)
    vereda = C.mat_ph("vereda", "concrete_pavers", tam=2.0, tinte=(0.9, 0.88, 0.85), mugre=None)
    C.plano("calle", -160, -160, 160, 160, Y0, asfalto)
    for lado in (-1, 1):
        C.caja("vereda", (lado * 40.0, Y0, 0), (8.0, 0.12, 140.0), vereda)
    pintura = C.mat_simple("linea", (0.85, 0.85, 0.8), aspereza=0.7)
    for z in range(-130, 140, 10):
        if abs(z) > 14:
            C.caja("linea", (0.0, Y0 + 0.001, z), (0.12, 0.005, 2.5), pintura)
    farol = C.modelo("street_lamp_01", 2.2)
    for lado in (-1, 1):
        for z in (-36.0, -6.0, 24.0):
            C.colocar_grupo(farol, (lado * 34.0, Y0 + 0.24, z), giro_y=math.pi / 2 * lado)
    return sc, sin


def isla_bosque():
    """bosque y pradera de la isla: un bosque de verdad (mossy_forest) con suelo, rocas con musgo y helechos"""
    sc, sin = base("mossy_forest_8k.hdr", 0.6, fuerza=1.1, niebla=(0.004, (0.75, 0.85, 0.75), 160.0, 30.0))
    suelo = C.mat_ph("suelo", "forest_ground_04", tam=4.0, mugre=None, variar=0.3)
    C.plano("suelo", -150, -150, 150, 150, Y0, suelo)
    rocas = C.modelo("rock_moss_set_02", 3.0)
    roca = C.modelo("boulder_01", 3.0)
    helecho = C.modelo("fern_02", 3.0)
    tronco = C.modelo("dead_tree_trunk", 2.5)
    import random
    r = random.Random(7)
    for i in range(70):
        x, z = r.uniform(-60, 60), r.uniform(-60, 30)
        if abs(x) < C.MESA[0] + 6 and abs(z) < C.MESA[1] + 6:
            continue
        if z > 8 and abs(x) < 30:
            continue
        g = r.uniform(0, 6.28)
        k = r.random()
        if k < 0.45:
            C.colocar_grupo(helecho, (x, Y0, z), giro_y=g, escala=r.uniform(2.5, 4.0))
        elif k < 0.75:
            C.colocar_grupo(rocas, (x, Y0, z), giro_y=g, escala=r.uniform(2.0, 4.0))
        elif k < 0.9:
            C.colocar_grupo(roca, (x, Y0 - 0.3, z), giro_y=g, escala=r.uniform(1.5, 3.0))
        else:
            C.colocar_grupo(tronco, (x, Y0, z), giro_y=g, escala=r.uniform(2.0, 3.0))
    return sc, sin


def acantilado_noche():
    """acantilados y cementerio de la isla de noche: campo con niebla de verdad (kloppenheim_04), luna fria,
    rocas grandes y lapidas"""
    sc, sin = base("kloppenheim_04_8k.hdr", 0.2, fuerza=0.12, expo=0.6, niebla=(0.006, (0.55, 0.62, 0.8), 160.0, 14.0))
    C.sol(B(0.3, 0.5, -0.8).normalized(), fuerza=0.6, color=(0.6, 0.7, 1.0), angulo=0.6)
    suelo = C.mat_ph("suelo", "rock_ground", tam=4.0, tinte=(0.6, 0.62, 0.66), mugre=None, variar=0.3)
    C.plano("suelo", -150, -150, 150, 150, Y0, suelo)
    piedra = C.mat_ph("lapida", "sandstone_cracks", tam=1.5, tinte=(0.5, 0.52, 0.56))
    madera = C.mat_ph("cruz", "wood_floor_deck", tam=1.0, tinte=(0.35, 0.32, 0.3))
    roca = C.modelo("namaqualand_boulder_04", 1.0)
    import random
    r = random.Random(3)
    for i in range(14):
        a = math.pi * (1.05 + 0.9 * i / 13)
        rr = r.uniform(45, 70)
        C.colocar_grupo(roca, (math.cos(a) * rr * 1.2, Y0 - 1.5, math.sin(a) * rr - 4), giro_y=r.uniform(0, 6), escala=r.uniform(6, 11))
    for i in range(80):
        x, z = r.uniform(-40, 40), r.uniform(-36, 6)
        if abs(x) < C.MESA[0] + 3 and abs(z) < C.MESA[1] + 3:
            continue
        if z > 4 and abs(x) < 28:
            continue
        g = r.uniform(-0.3, 0.3)
        if r.random() < 0.6:
            C.caja("lapida", (x, Y0, z), (0.6, r.uniform(0.6, 1.0), 0.16), piedra, giro=g)
        else:
            C.caja("cruz", (x, Y0, z), (0.1, 1.0, 0.1), madera, giro=g)
            C.caja("cruz_b", (x, Y0 + 1.25, z), (0.5, 0.09, 0.09), madera, giro=g)
    fuego = C.mat_emision("fatuo", (0.3, 0.8, 1.0), 20.0)
    for i in range(6):
        x, z = r.uniform(-30, 30), r.uniform(-30, -16)
        o = C.cilindro("fatuo", (x, Y0 + 1.8, z), 0.18, 0.18, 0.3, fuego, lados=10)
        C.luz_punto(B(x, Y0 + 2.0, z), 300, color=(0.3, 0.8, 1.0), radio=0.2)
    return sc, sin


def playa_mar():
    """la plataforma de madera en el mar (Mako): mar y cielo de verdad (umhlanga_sunrise), agua y pilotes"""
    sc, sin = base("umhlanga_sunrise_8k.hdr", 0.6, fuerza=1.0, niebla=(0.0008, (0.9, 0.85, 0.85), 260.0, 30.0))
    tablas = C.mat_ph("tablas", "wood_floor_deck", tam=3.0, tinte=(0.75, 0.68, 0.62), mugre=None, variar=0.3)
    agua = C.mat_agua(color=(0.02, 0.08, 0.1), olas=0.6)
    C.plano("mar", -600, -600, 600, 600, Y0 - 1.6, agua)
    C.caja("plataforma", (0, Y0 - 1.2, -5.0), (30.0, 0.6, 21.0), tablas)
    pilote = C.mat_ph("pilote", "wood_floor_deck", tam=1.5, tinte=(0.45, 0.4, 0.35))
    for x in range(-28, 29, 8):
        for z in (-26.0, 16.0):
            C.cilindro("pilote", (x, Y0 - 5.0, z), 0.5, 0.5, 6.2, pilote, lados=16)
    for z in range(-18, 9, 8):
        for x in (-30.0, 30.0):
            C.cilindro("pilote", (x, Y0 - 5.0, z), 0.5, 0.5, 6.2, pilote, lados=16)
    cuerda = C.mat_simple("cuerda", (0.4, 0.33, 0.22), aspereza=0.9)
    for lado in (-1, 1):
        C.caja("cuerda", (lado * 30.0, Y0 + 4.5, -5.0), (0.05, 0.05, 21.0), cuerda)
    return sc, sin


def torre_alcatraz():
    """la torre de duelo de Alcatraz al atardecer: cielo de fuego de verdad (the_sky_is_on_fire), plataforma de
    acero en lo alto de la torre y el mar abajo"""
    sc, sin = base("the_sky_is_on_fire_8k.hdr", 0.55, fuerza=1.0, niebla=(0.0012, (1.0, 0.75, 0.6), 300.0, 120.0))
    acero = C.mat_ph("acero", "metal_plate", tam=3.0, tinte=(0.55, 0.52, 0.5), mugre=None, brillo_humedo=(10.0, 0.4))
    oxido = C.mat_ph("oxido", "rusty_metal_02", tam=3.0, tinte=(0.7, 0.6, 0.55))
    agua = C.mat_agua(color=(0.03, 0.04, 0.06), olas=0.3)
    C.plano("mar", -900, -900, 900, 900, Y0 - 90.0, agua)
    C.cilindro("plataforma", (0, Y0 - 1.0, 0), 34.0, 34.0, 1.0, acero, lados=96)
    C.cilindro("torre", (0, Y0 - 90.0, 0), 20.0, 30.0, 89.0, oxido, lados=96, tapa=False)
    for x, z, a in anillo_de(64, 33.6):
        C.cilindro("poste", (x, Y0, z), 0.08, 0.08, 1.4, acero, lados=8)
    C.cilindro("baranda", (0, Y0 + 1.4, 0), 33.7, 33.7, 0.1, acero, lados=96, tapa=False)
    for x, z, a in anillo_de(8, 31.0):
        C.antorcha((x, Y0 + 1.6, z), alto_llama=1.0, potencia=1500, mat_hierro=oxido, mat_llama=C.mat_fuego(fuerza=6.0))
    return sc, sin


def templo_egipcio(sombras=False):
    """el palacio y templo del Egipto antiguo a pleno sol (goegap: desierto de verdad) o, con sombras=True, el Reino
    de las Sombras (noche violeta, niebla y ruinas)"""
    if sombras:
        sc, sin = base("rogland_moonlit_night_8k.hdr", 0.55, fuerza=0.5, expo=0.4, niebla=(0.008, (0.45, 0.2, 0.55), 160.0, 25.0))
        C.sol(B(-0.2, 0.35, -1.0).normalized(), fuerza=0.8, color=(0.7, 0.4, 1.0), angulo=1.0)
    else:
        sc, sin = base("goegap_8k.hdr", 0.55, fuerza=1.0, niebla=(0.0008, (1.0, 0.92, 0.8), 220.0, 30.0))
    arenisca = C.mat_ph("arenisca", "sandstone_blocks_08", tam=2.5, tinte=(1.0, 0.9, 0.75) if not sombras else (0.45, 0.4, 0.45), variar=0.3)
    losa = C.mat_ph("losa", "sandstone_cracks", tam=4.0, tinte=(1.0, 0.9, 0.78) if not sombras else (0.4, 0.36, 0.42), mugre=None)
    arena = C.mat_ph("arena", "sand_01", tam=5.0, tinte=(1.0, 0.88, 0.7) if not sombras else (0.35, 0.3, 0.38), mugre=None)
    C.plano("arena", -200, -200, 200, 200, Y0 - 0.15, arena)
    C.plano("patio", -46, -60, 46, 34, Y0, losa)
    rota = (lambda i: 0.35 + 0.1 * (i % 3)) if sombras else (lambda i: 0.0)
    i = 0
    for lado in (-1, 1):
        for z in range(-50, 31, 10):
            C.columna("columna", (lado * 34.0, Y0, float(z)), 1.8, 16.0, arenisca, rota=rota(i) if i % 2 == 0 else 0.0)
            i += 1
        C.obelisco("obelisco", (lado * 15.0, Y0, -40.0), 1.6, 18.0 if not sombras else 12.0, arenisca)
    # el pilono (portada del templo) al fondo, con la puerta
    for lado in (-1, 1):
        x = lado * 15.0
        v = []
        for yy, hx, hz in ((0.0, 9.0, 4.5), (24.0, 7.2, 3.0)):
            for sx, sz in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
                v.append(B(x + sx * hx, Y0 + yy, -60.0 + sz * hz))
        C._desde_pydata("pilono", v, [(0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7), (4, 5, 6, 7), (3, 2, 1, 0)], arenisca)
    C.caja("dintel", (0, Y0 + 15.0, -60.0), (6.5, 2.0, 3.0), arenisca)
    C.caja("puerta", (0, Y0, -60.5), (6.0, 7.5, 2.0), C.mat_simple("sombra", (0.01, 0.008, 0.006)))
    if not sombras:
        for x in (-6.0, 6.0):
            C.antorcha((x, Y0 + 3.5, -57.0), alto_llama=1.2, potencia=1200, mat_hierro=C.mat_simple("bronce", (0.5, 0.3, 0.1), metal=1.0, aspereza=0.4),
                       mat_llama=C.mat_fuego(fuerza=6.0))
    else:
        ojo = C.mat_emision("ojo_zorc", (1.0, 0.1, 0.2), 15.0)
        C.cilindro("ojo", (0, Y0 + 17.0, -56.9), 1.4, 1.4, 0.1, ojo, lados=48)
        for x in (-6.0, 6.0):
            C.antorcha((x, Y0 + 3.5, -57.0), alto_llama=1.2, potencia=900, mat_hierro=C.mat_simple("bronce", (0.2, 0.1, 0.2), metal=1.0),
                       mat_llama=C.mat_fuego(fuerza=6.0))
    return sc, sin


def reino_sombras():
    return templo_egipcio(sombras=True)


def dirigible_kaiba():
    """la arena en la cubierta del dirigible de Kaiba sobre un mar de nubes al anochecer"""
    sc, sin = base("kloppenheim_06_puresky_8k.hdr", 0.5, fuerza=1.0)
    acero = C.mat_ph("cubierta", "metal_plate", tam=3.0, tinte=(0.6, 0.62, 0.66), mugre=None, brillo_humedo=(12.0, 0.5))
    casco = C.mat_simple("casco", (0.55, 0.57, 0.62), aspereza=0.35, metal=0.8)
    C.plano("cubierta", -32, -30, 32, 22, Y0, acero)
    C.caja("cubierta_borde", (0, Y0 - 2.05, -4.0), (32.0, 1.0, 26.0), casco)
    for lado in (-1, 1):
        for z in range(-30, 23, 3):
            C.cilindro("poste", (lado * 31.5, Y0, float(z)), 0.07, 0.07, 1.3, casco, lados=8)
        C.caja("baranda", (lado * 31.5, Y0 + 1.3, -4.0), (0.06, 0.06, 26.0), casco)
    for x in range(-30, 31, 3):
        C.cilindro("poste", (float(x), Y0, -30.0), 0.07, 0.07, 1.3, casco, lados=8)
    C.caja("baranda", (0, Y0 + 1.3, -30.0), (31.5, 0.06, 0.06), casco)
    # el casco del dirigible por debajo (elipsoide)
    bpy.ops.mesh.primitive_uv_sphere_add(segments=96, ring_count=48, radius=1.0, location=B(0, Y0 - 22.0, -20.0))
    h = bpy.context.object
    h.scale = (34.0, 120.0, 20.0)
    h.data.materials.append(casco)
    bpy.ops.object.shade_smooth()
    # mar de nubes: un relieve de ruido muy suave abajo, blanco y con la niebla encima
    bpy.ops.mesh.primitive_grid_add(x_subdivisions=400, y_subdivisions=400, size=3000, location=B(0, Y0 - 140.0, 0))
    nubes = bpy.context.object
    tex = bpy.data.textures.new("nubes", "CLOUDS")
    tex.noise_scale = 1.2
    tex.noise_depth = 4
    md = nubes.modifiers.new("relieve", "DISPLACE")
    md.texture = tex
    md.texture_coords = "GLOBAL"
    md.strength = 28.0
    tex.noise_scale = 160.0
    m = bpy.data.materials.new("nube")
    m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (0.9, 0.88, 0.92, 1.0)
    b.inputs["Roughness"].default_value = 1.0
    b.inputs["Subsurface Weight"].default_value = 0.4
    nubes.data.materials.append(m)
    bpy.ops.object.shade_smooth()
    sin.append(C.niebla_mundo(0.006, color=(0.95, 0.9, 0.95), radio=1400.0, alto=60.0))
    sin[-1].location = B(0, Y0 - 150.0, 0) - B(0, Y0 - 1.0, 0)
    return sc, sin


def atlantida_oricalco():
    """Atlantida/Doma: ruinas de marmol bajo las estrellas y el brillo verde del Oricalco"""
    sc, sin = base("kloppenheim_02_4k.hdr", 0.5, fuerza=0.4, expo=0.5, niebla=(0.006, (0.3, 0.8, 0.55), 160.0, 20.0))
    marmol = C.mat_ph("marmol", "marble_01", tam=3.0, tinte=(0.75, 0.8, 0.78), variar=0.35)
    losa = C.mat_ph("losa", "marble_tiles", tam=4.0, tinte=(0.5, 0.55, 0.55), mugre=None)
    roca = C.mat_ph("roca", "rock_wall_10", tam=6.0, tinte=(0.45, 0.5, 0.5))
    C.plano("suelo", -150, -150, 150, 150, Y0, losa)
    i = 0
    for x, z, a in anillo_de(16, 40.0, -6.0, math.pi * 1.02, math.pi * 1.98):
        C.columna("columna", (x, Y0, z), 1.7, 18.0, marmol, rota=(0.3 + 0.2 * (i % 3)) if i % 3 != 1 else 0.0)
        i += 1
    for x, z, a in anillo_de(10, 70.0, -6.0, math.pi * 1.0, math.pi * 2.0):
        C.cilindro("risco", (x, Y0 - 2, z), 9.0, 3.0, 26.0 + 10 * math.sin(a * 5), roca, lados=7, suave=False)
    verde = C.mat_emision("oricalco", (0.2, 1.0, 0.4), 12.0)
    for r in (17.0, 18.0, 26.0):
        C.cilindro("sello", (0, Y0 + 0.01, 0), r, r, 0.02, verde, lados=128, tapa=False)
    for x, z, a in anillo_de(6, 22.0):
        C.cilindro("cristal", (x, Y0, z), 0.8, 0.0, 4.5, verde, lados=6, suave=False)
        C.luz_punto(B(x, Y0 + 2.0, z), 800, color=(0.3, 1.0, 0.5), radio=0.5)
    return sc, sin


def laberinto():
    """el laberinto subterraneo de los Hermanos Paradoja: muros de piedra, techo en la oscuridad y antorchas"""
    sc, sin = base(None, 0.5, expo=0.8, niebla=(0.01, (0.5, 0.42, 0.35), 160.0, 20.0))
    muro = C.mat_ph("muro", "castle_wall_slates", tam=4.0, tinte=(0.7, 0.66, 0.62), variar=0.35)
    piso = C.mat_ph("piso", "floor_pattern_01", tam=3.0, tinte=(0.6, 0.56, 0.52), mugre=None, brillo_humedo=(7.0, 0.6))
    techo = C.mat_ph("techo", "rock_wall_10", tam=6.0, tinte=(0.4, 0.38, 0.36), mugre=None)
    C.plano("piso", -120, -120, 120, 120, Y0, piso)
    t = C.plano("techo", -120, -120, 120, 120, Y0 + 16.0, techo)
    import random
    r = random.Random(4242)
    celda = 8.0
    llama = C.mat_fuego(fuerza=6.0)
    hierro = C.mat_simple("hierro", (0.04, 0.035, 0.03), metal=1.0, aspereza=0.5)
    for i in range(-10, 11):
        for j in range(-10, 4):
            x, z = i * celda, j * celda
            if x * x + z * z > 85 * 85:
                continue
            hor = r.random() < 0.5
            x1, z1 = (x + celda, z) if hor else (x, z + celda)
            mx, mz = (x + x1) / 2, (z + z1) / 2
            if abs(mx) < C.MESA[0] + 5 and abs(mz) < C.MESA[1] + 5:
                continue
            if mz > 4 and abs(mx) < 30:
                continue
            C.caja("muro", (mx, Y0, mz), ((abs(x1 - x) + 1.4) / 2, 4.5, (abs(z1 - z) + 1.4) / 2), muro)
            if r.random() < 0.2:
                off = 0.95
                C.antorcha((mx + (0 if hor else off), Y0 + 5.2, mz + (off if hor else 0)), alto_llama=0.9, potencia=900,
                           mat_hierro=hierro, mat_llama=llama)
    for lado in (-1, 1):
        C.caja("puerta", (lado * 7.0, Y0, -36.0), (3.5, 5.5, 0.4), C.mat_ph("madera", "medieval_wood", tam=2.5, tinte=(0.5, 0.4, 0.33)))
        C.caja("marco", (lado * 7.0, Y0 + 11.0, -36.2), (4.2, 0.7, 0.8), muro)
    return sc, sin


def mundo_virtual_noah():
    """el mundo virtual de Noah: estilizado digital (reticula verde, columnas de datos, cubos flotantes)"""
    sc, sin = base(None, 0.5, expo=0.3, niebla=(0.006, (0.1, 0.9, 0.4), 220.0, 60.0))
    m = bpy.data.materials.new("reticula")
    m.use_nodes = True
    nt = m.node_tree
    b = nt.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (0.005, 0.02, 0.012, 1.0)
    b.inputs["Roughness"].default_value = 0.15
    b.inputs["Metallic"].default_value = 0.6
    geo = nt.nodes.new("ShaderNodeNewGeometry")
    br = nt.nodes.new("ShaderNodeTexBrick")
    br.inputs["Scale"].default_value = 0.25
    br.inputs["Mortar Size"].default_value = 0.012
    br.offset = 0.0
    br.inputs["Color1"].default_value = (0, 0, 0, 1)
    br.inputs["Color2"].default_value = (0, 0, 0, 1)
    br.inputs["Mortar"].default_value = (0.2, 1.0, 0.5, 1)
    nt.links.new(geo.outputs["Position"], br.inputs["Vector"])
    nt.links.new(br.outputs["Color"], b.inputs["Emission Color"])
    b.inputs["Emission Strength"].default_value = 6.0
    C.plano("reticula", -220, -220, 220, 220, Y0, m)
    verde = C.mat_emision("datos", (0.15, 1.0, 0.45), 8.0)
    oscuro = C.mat_simple("cubo", (0.01, 0.03, 0.02), aspereza=0.2, metal=0.8)
    import random
    r = random.Random(12)
    for i in range(60):
        a = r.uniform(math.pi * 0.95, math.pi * 2.05)
        d = r.uniform(40, 120)
        x, z = math.cos(a) * d, math.sin(a) * d - 6.0
        h = r.uniform(10, 70)
        C.caja("torre_datos", (x, Y0, z), (r.uniform(1.5, 4.0), h / 2, r.uniform(1.5, 4.0)), oscuro)
        C.caja("borde_datos", (x, Y0 + h, z), (0.3, 0.3, 0.3), verde)
        for k in range(int(h / 6)):
            C.caja("franja", (x, Y0 + k * 6.0 + 2.0, z), (r.uniform(1.6, 4.1), 0.06, r.uniform(1.6, 4.1)), verde)
    for i in range(40):
        x, z = r.uniform(-60, 60), r.uniform(-70, 10)
        if abs(x) < C.MESA[0] + 8 and abs(z) < C.MESA[1] + 8:
            continue
        if z > 4 and abs(x) < 30:
            continue
        s = r.uniform(0.6, 2.0)
        C.bloque("cubo", (x, Y0 + r.uniform(6, 22), z), (s, s, s), oscuro if r.random() < 0.6 else verde, giro_z=r.uniform(0, 1), giro_y=r.uniform(0, 1))
    return sc, sin


LUGARES = {f.__name__: f for f in (ciudad_batallas, rascacielos_noche, estadio_kc, museo_egipcio, tienda_juegos, isla_bosque, acantilado_noche,
                                   playa_mar, torre_alcatraz, templo_egipcio, reino_sombras, dirigible_kaiba, atlantida_oricalco, laberinto,
                                   mundo_virtual_noah)}

sc, sin = LUGARES[LUGAR]()
C.renderizar(LUGAR, sc, op, sin_distancia=sin)
