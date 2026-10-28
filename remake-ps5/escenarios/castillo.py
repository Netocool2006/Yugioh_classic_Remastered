"""reino_duelistas_castillo: el castillo de Pegasus como Boletaria (Demon's Souls), al atardecer.

El duelo es en el patio de armas, en lo alto de la roca; al fondo, pasando un parapeto con almenas y un puente de
piedra roto sobre un abismo con niebla, se levanta el castillo gotico: muralla con matacanes y torres, el palacio
con contrafuertes, pinaculos, arbotantes, ventanales con traceria, roseton, gargolas, portada con arquivoltas y
escalinata, tejado de pizarra con buhardillas y cresteria, y la torre del homenaje con la aguja mayor; banderas al
viento y luz calida adentro. Todo a contraluz del sol poniente, con niebla entre las capas. El castillo esta
corrido a la izquierda (el hueco entre los LP del rival y su mano en la vista de juego, que mira 7 grados mas
arriba: yugireload.cpp MIRA_EXTRA_GRADOS).

Blender 5.2 sin interfaz:
  blender --background --factory-startup --python castillo.py -- --modo vista            (cuadro de la camara de juego)
  blender --background --factory-startup --python castillo.py -- --modo pano --ancho 8192 (panorama + distancias)
Insumos CC0: Poly Haven (polyhaven.py) hdri industrial_sunset_puresky; texturas stone_wall_05, castle_wall_slates,
castle_wall_varriation, medieval_blocks_03, stone_brick_wall_001, floor_pattern_01, grey_roof_tiles_02, rock_wall_10,
medieval_wood; modelos modular_fort_01, gothic_statue, lion_head (gargolas), wooden_barrels_01, wooden_crate_02,
wine_barrel_01. Musgo: ambientCG Moss002 (el del bosque).
"""
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import comun as C  # noqa: E402
import gotico as G  # noqa: E402
from comun import B  # noqa: E402
from mathutils import Vector  # noqa: E402

import bpy  # noqa: E402

CLAVE = "reino_duelistas_castillo"
op = C.args()
ENV = os.environ
sc = C.escena_nueva()
sc.view_settings.exposure = float(ENV.get("ESC_EXPO", "0.3"))
sc.view_settings.look = ENV.get("ESC_LOOK", "AgX - Punchy")
Y0 = C.SUELO
R = random.Random(11)

# ------------------------------------------------------------------------------------------------ luz y aire
HDRI = ENV.get("ESC_HDRI", "industrial_sunset_puresky_8k.hdr")
SOL = Vector([float(t) for t in ENV.get("ESC_SOL", "-0.30,0.085,-1.0").split(",")]).normalized()   # detras del castillo
C.mundo_hdri(HDRI, fuerza=float(ENV.get("ESC_CIELO", "0.9")))
nt = sc.world.node_tree
# cielo mas dramatico: mas saturado y calido
hs = nt.nodes.new("ShaderNodeHueSaturation")
hs.inputs["Saturation"].default_value = 1.45
env = [n for n in nt.nodes if n.type == "TEX_ENVIRONMENT"][0]
bg = [n for n in nt.nodes if n.type == "BACKGROUND"][0]
calido = nt.nodes.new("ShaderNodeMix")
calido.data_type = "RGBA"
calido.blend_type = "MULTIPLY"
calido.inputs["Factor"].default_value = 1.0
calido.inputs[7].default_value = (1.0, 0.78, 0.62, 1.0)
nt.links.new(env.outputs["Color"], hs.inputs["Color"])
nt.links.new(hs.outputs["Color"], calido.inputs[6])
nt.links.new(calido.outputs[2], bg.inputs["Color"])
d0 = C.sol_del_hdri(HDRI)
sb = B(*SOL)
giro = math.degrees(math.atan2(d0.y, d0.x) - math.atan2(sb.y, sb.x))
nt.nodes["Mapping"].inputs["Rotation"].default_value[2] = math.radians(giro)
C.sol(sb, fuerza=float(ENV.get("ESC_SOLF", "7")), color=(1.0, 0.62, 0.36), angulo=1.2)
aire = C.niebla_mundo(float(ENV.get("ESC_NIEBLA", "0.0004")), color=(1.0, 0.86, 0.74), anisotropia=0.75, radio=1600.0, alto=300.0)
aire.location = B(0, -240.0, 0) - B(0, Y0 - 1.0, 0)   # de y = -241 a 59
bruma = C.niebla_mundo(float(ENV.get("ESC_BRUMA", "0.012")), color=(0.95, 0.85, 0.8), anisotropia=0.6, radio=1600.0, alto=140.0)
bruma.location = B(0, -230.0, 0) - B(0, Y0 - 1.0, 0)   # el abismo lleno de niebla: de y = -231 a -91

# ------------------------------------------------------------------------------------------------ materiales
sillar = C.mat_ph("sillar", "stone_wall_05", tam=4.0, tinte=(0.78, 0.78, 0.8), relieve=1.0, variar=0.3, musgo=0.5)
sillar2 = C.mat_ph("sillar2", "castle_wall_varriation", tam=3.0, tinte=(0.72, 0.72, 0.74), relieve=1.0, musgo=0.35)
moldura = C.mat_ph("moldura", "medieval_blocks_03", tam=2.0, tinte=(0.7, 0.7, 0.72), relieve=1.0, musgo=0.4)
almohadillado = C.mat_ph("almohadillado", "stone_brick_wall_001", tam=3.0, tinte=(0.75, 0.74, 0.74), relieve=1.0, musgo=0.6)
mamposteria = C.mat_ph("mamposteria", "castle_wall_slates", tam=6.0, tinte=(0.75, 0.75, 0.78), relieve=1.0, variar=0.35, musgo=0.7)
roca = C.mat_ph("roca", "rock_wall_10", tam=14.0, tinte=(0.55, 0.53, 0.52), variar=0.35, mugre=None, musgo=0.6)
suelo = C.mat_ph("suelo", "floor_pattern_01", tam=2.6, tinte=(0.8, 0.78, 0.76), relieve=1.0, brillo_humedo=(11.0, 0.7), variar=0.35, mugre=None)
techo = C.mat_ph("techo", "grey_roof_tiles_02", tam=2.4, tinte=(0.3, 0.34, 0.42), relieve=1.0, mugre=None, musgo=0.25)
hierro = C.mat_simple("hierro", (0.04, 0.035, 0.03), aspereza=0.5, metal=1.0)
madera = C.mat_ph("madera", "medieval_wood", tam=2.5, tinte=(0.5, 0.4, 0.33), mugre=(1.5, 0.3))
llama = C.mat_fuego(fuerza=6.0)
llama.node_tree.nodes["Emission"].inputs["Color"].default_value = (1.0, 0.28, 0.04, 1.0)
vidriera = C.mat_emision("vidriera", (1.0, 0.45, 0.14), 7.0)
oscuro = C.mat_simple("hueco", (0.008, 0.007, 0.006), aspereza=0.9)
tela = C.mat_simple("estandarte", (0.2, 0.006, 0.01), aspereza=0.85, brillo_tela=0.35)
gargola = C.modelo("lion_head", 3.0)


def torre(x, z, y0, r, alto, techo_alto=None, ventanas=3, gargolas=False, bandera=False, luz=0.0, mat=None):
    G.torre(x, z, y0, r, alto, mat or sillar, moldura, techo, vidriera, oscuro, techo_alto=techo_alto, ventanas=ventanas,
            gargolas=gargola if gargolas else None, bandera=tela if bandera else None, luz=luz)


# ------------------------------------------------------------------------------------------------ el patio
ZP = -36.0   # el borde del patio (parapeto con almenas) y el comienzo del abismo
C.plano("suelo", -56, ZP, 56, 72, Y0, suelo)
C.mesa_sombra()
C.caja("cimiento", (0, -150.0, ZP - 6.0), (60.0, (150.0 + Y0) / 2, 6.0), mamposteria)
C.caja("cimiento_roca", (0, -260.0, ZP - 20.0), (90.0, 50.0, 30.0), roca)
for k in range(9):
    C.caja("contrafuerte", (-48 + k * 12.0, -110.0, ZP - 13.0), (2.2, (110.0 + Y0) / 2, 2.0), mamposteria)
G.almenado(-56, ZP, -7.5, ZP, Y0, moldura)
G.almenado(7.5, ZP, 56, ZP, Y0, moldura)
for lado in (-1, 1):   # la puerta del puente: dos pilares con pinaculos y un arco
    C.caja("pilar_puerta", (lado * 8.2, Y0, ZP), (1.4, 4.2, 1.4), almohadillado)
    G.pinaculo(lado * 8.2, Y0 + 8.4, ZP, 1.8, moldura)
C.arco("arco_puerta", 0.0, Y0 + 6.5, ZP + 0.2, 14.0, moldura, dovelas=13, canto=0.9, fondo=1.4)
# torres grandes en las esquinas del patio (enmarcan la vista): gargolas, bandera y ventanas encendidas
for lado in (-1, 1):
    torre(lado * 47.0, ZP + 6.0, Y0, 7.2, 30.0, techo_alto=17.0, ventanas=4, gargolas=True, bandera=True, luz=1500)
    G.estandarte(lado * 47.0, Y0 + 24.0, 3.4, 10.0, ZP + 6.0 + 7.8, tela, viento=0.5 * lado)
# murallas de los lados y de atras (modular_fort_01 a escala 1.5)
fort = {o.name: o for o in C.importar_gltf("modular_fort_01")}
C.ocultar(fort.values())
recto = fort["modular_fort_01_wall_thick_straight_01"]
torre_r = fort["modular_fort_01_tower_round"]
ESC = 1.5
largo = 14.56 * ESC
XM = 44.0
for lado in (-1, 1):
    z = ZP + 14.0 + largo * 0.5
    while z < 66.0:
        o = C.colocar(recto, (lado * XM, Y0, z), escala=ESC)
        o.hide_render = o.hide_viewport = False
        z += largo
    t = C.colocar(torre_r, (lado * XM, Y0, 70.0), escala=1.3)
    t.hide_render = t.hide_viewport = False
x = -XM + largo * 0.5
while x < XM:
    o = C.colocar(recto, (x, Y0, 72.0), giro_y=math.pi / 2, escala=ESC)
    o.hide_render = o.hide_viewport = False
    x += largo
for lado in (-1, 1):
    for z in (-10.0, 12.0, 34.0):
        G.estandarte(lado * (XM - 2.9), Y0 + 11.0, 2.6, 7.0, z, tela, hacia=(-lado, 0, 0), plano_x=True, viento=0.3)
# antorchas, braseros, estatuas y barriles
for lado in (-1, 1):
    for z in (-20.0, 2.0, 24.0, 46.0):
        C.antorcha((lado * (XM - 3.0), Y0 + 5.0, z), alto_llama=0.9, potencia=1100, mat_madera=madera, mat_hierro=hierro, mat_llama=llama)
    C.antorcha((lado * 8.2, Y0 + 9.0, ZP + 1.6), alto_llama=1.0, potencia=1500, mat_hierro=hierro, mat_llama=llama)
for x in (-20.0, 20.0):
    C.cilindro("brasero_pie", (x, Y0, ZP + 6.0), 0.5, 0.28, 1.5, hierro, lados=16)
    C.cilindro("brasero_cuenco", (x, Y0 + 1.5, ZP + 6.0), 0.3, 1.0, 0.55, hierro, lados=24)
    o = C.cilindro("llama_brasero", (x, Y0 + 1.95, ZP + 6.0), 0.65, 0.0, 1.1, llama, lados=12)
    o.visible_shadow = False
    o.visible_camera = False
    C.luz_punto(B(x, Y0 + 2.8, ZP + 6.0), 2800, radio=0.5)
    C.LLAMAS.append((x, Y0 + 1.95, ZP + 6.0, 1.6))
estatua = C.modelo("gothic_statue", 3.0)
for x in (-13.0, 13.0):
    C.caja("pedestal", (x, Y0, ZP + 3.0), (1.6, 1.4, 1.6), almohadillado)
    C.caja("pedestal_tapa", (x, Y0 + 2.8, ZP + 3.0), (1.8, 0.15, 1.8), moldura)
    C.colocar_grupo(estatua, (x, Y0 + 3.1, ZP + 3.0))
grupos = [C.modelo("wooden_barrels_01", 1.5), C.modelo("wooden_crate_02", 1.6), C.modelo("wine_barrel_01", 1.6)]
for (x, z, g, k) in ((-39.0, -12.0, 0.3, 0), (-38.5, -6.0, 1.0, 1), (-39.0, 18.0, 1.2, 2), (39.0, -10.0, 2.0, 0),
                     (38.6, -3.5, 0.4, 1), (39.0, 24.0, 0.7, 2)):
    C.colocar_grupo(grupos[k], (x, Y0, z), giro_y=g)

# ------------------------------------------------------------------------------------------------ el puente
ZG = -250.0   # el puente esta roto aqui: lo demas se perdio en el abismo (como en Boletaria)
ANCHO = 6.0
C.caja("puente", (0, Y0 - 2.5, (ZP + ZG) / 2), (ANCHO, 1.25, (ZP - ZG) / 2), sillar)
C.plano("puente_suelo", -ANCHO + 0.6, ZG, ANCHO - 0.6, ZP, Y0 + 0.01, suelo)
for lado in (-1, 1):
    G.almenado(lado * (ANCHO - 0.4), ZG, lado * (ANCHO - 0.4), ZP - 0.5, Y0, moldura, alto=1.2, grueso=0.8, paso=3.0)
z = ZP - 30.0
while z > ZG + 10:
    C.caja("pila", (0, -175.0, z), (ANCHO * 0.75, (175.0 + Y0 - 2.5) / 2, 3.0), mamposteria)
    for lado in (-1, 1):
        C.antorcha((lado * (ANCHO + 0.3), Y0 + 1.9, z), alto_llama=0.8, potencia=1600, mat_hierro=hierro, mat_llama=llama)
    z -= 34.0
for k in range(14):   # el borde roto: piedras sueltas
    s_ = R.uniform(0.4, 1.1)
    C.bloque("escombro", (R.uniform(-5, 5), Y0 - 2.0 + R.uniform(-1, 0.5), ZG + R.uniform(-1, 4)), (s_, s_, s_), sillar,
             giro_z=R.uniform(0, 1), giro_y=R.uniform(0, 3))

# ------------------------------------------------------------------------------------------------ el castillo
antes_castillo = set(bpy.data.objects)
ZC = -380.0
YC = -40.0
C.cilindro("pena", (0, -420.0, ZC - 70.0), 190.0, 130.0, 300.0 + YC, roca, lados=24, suave=False)
# muralla exterior con matacanes y torres redondas
pts = [(-130, ZC - 20), (-60, ZC + 4), (60, ZC + 4), (130, ZC - 20), (150, ZC - 110), (-150, ZC - 110)]
for i in range(len(pts)):
    (x0, z0), (x1, z1) = pts[i], pts[(i + 1) % len(pts)]
    L = math.hypot(x1 - x0, z1 - z0)
    g = math.atan2(-(z1 - z0), x1 - x0)
    C.caja("muralla", ((x0 + x1) / 2, YC, (z0 + z1) / 2), (L / 2, 14.0, 3.0), mamposteria, giro=g)
    G.almenado(x1, z1, x0, z0, YC + 28.0, moldura, alto=1.6, grueso=2.0, paso=3.2, matacan=True)
    torre(x0, z0, YC, 9.0, 44.0, techo_alto=24.0, ventanas=4, bandera=True)
# el palacio: fachada gotica con contrafuertes y pinaculos, ventanales con traceria, roseton y la portada
FZ = ZC - 30.0
C.caja("palacio", (0, YC, ZC - 60.0), (70.0, 34.0, 30.0), sillar)
for k in range(-6, 7):
    G.contrafuerte(k * 11.0, FZ, YC, 66.0, 2.6, 4.5, moldura, moldura)
    if k < 6:
        xv = k * 11.0 + 5.5
        if abs(xv) > 8:
            G.ventanal(xv, YC + 34.0, FZ + 0.05, 4.2, 12.0, moldura, vidriera, oscuro, luz=900)
            G.ventanal(xv, YC + 12.0, FZ + 0.05, 3.0, 6.0, moldura, vidriera, oscuro, traceria=False)
G.almenado(70.0, FZ, -70.0, FZ, YC + 68.0, moldura, alto=1.8, grueso=2.0, paso=3.4, matacan=True)
for k in range(-6, 7):
    C.caja("gargola_mensula", (k * 11.0, YC + 64.0, FZ + 3.0), (0.4, 0.4, 1.4), moldura)
    C.colocar_grupo(gargola, (k * 11.0, YC + 63.4, FZ + 4.2), giro_y=math.pi, escala=2.4)
# roseton sobre la portada
bpy.ops.mesh.primitive_torus_add(major_radius=5.6, minor_radius=0.55, location=B(0, YC + 50.0, FZ + 0.4), rotation=(math.pi / 2, 0, 0))
bpy.context.object.data.materials.append(moldura)
bpy.ops.mesh.primitive_cylinder_add(vertices=64, radius=5.2, depth=0.3, location=B(0, YC + 50.0, FZ - 0.2), rotation=(math.pi / 2, 0, 0))
bpy.context.object.data.materials.append(vidriera)
for k in range(12):
    a = k * math.pi / 6
    C.bloque("radio_roseton", (math.cos(a) * 2.6, YC + 50.0 + math.sin(a) * 2.6, FZ + 0.3), (2.6, 0.12, 0.25), moldura, giro_z=-a)
# la portada: arquivoltas, porton de tablones con herrajes y la escalinata
for i, k in enumerate((0.0, 1.0, 2.0, 3.0)):
    luz_ = 8.0 + 2 * k
    C.arco("arquivolta", 0.0, YC + 16.0, FZ + 2.4 - k * 0.6, luz_, moldura if i % 2 == 0 else sillar2, dovelas=15, canto=0.8, fondo=0.8)
    for lado in (-1, 1):
        C.caja("jamba", (lado * (luz_ * 0.5 + 0.4), YC + 4.0, FZ + 2.4 - k * 0.6), (0.45, 6.0, 0.4), moldura if i % 2 == 0 else sillar2)
C.caja("porton", (0, YC + 4.0, FZ + 0.6), (4.0, 8.0, 0.25), madera)
for y in (2.0, 6.0, 10.0, 14.0):
    C.caja("herraje", (0, YC + 4.0 + y, FZ + 0.9), (3.9, 0.15, 0.05), hierro)
G.escalinata(0.0, YC, FZ + 14.0, FZ + 2.0, 22.0, 4.0, moldura)
for x in (-14.0, 14.0):
    C.antorcha((x, YC + 6.0, FZ + 4.0), alto_llama=1.2, potencia=2500, mat_hierro=hierro, mat_llama=llama)
# arbotantes en los costados del palacio
for lado in (-1, 1):
    for z in (ZC - 40.0, ZC - 60.0, ZC - 80.0):
        xp = lado * 84.0
        C.caja("estribo", (xp, YC, z), (2.5, 27.0, 2.5), moldura)
        G.pinaculo(xp, YC + 54.0, z, 2.6, moldura)
        G.arbotante((xp, z), (lado * 70.5, z), YC + 44.0, YC + 60.0, moldura, n=10, canto=1.3, ancho=1.6)
# tejado de pizarra con buhardillas y la cresteria
v = [B(-70, YC + 68.5, ZC - 31.0), B(70, YC + 68.5, ZC - 31.0), B(70, YC + 92.0, ZC - 60.0), B(-70, YC + 92.0, ZC - 60.0),
     B(-70, YC + 68.5, ZC - 89.0), B(70, YC + 68.5, ZC - 89.0)]
C._desde_pydata("tejado", v, [(0, 1, 2, 3), (3, 2, 5, 4), (0, 3, 4), (1, 5, 2)], techo)
for k in range(-5, 6):
    if k == 0:
        continue
    C.caja("buhardilla", (k * 12.0, YC + 72.0, ZC - 40.0), (1.6, 2.6, 2.5), sillar2)
    G.ventanal(k * 12.0, YC + 72.6, ZC - 37.45, 1.4, 1.8, moldura, vidriera, oscuro, traceria=False, fondo=0.3)
    C.cilindro("buhardilla_techo", (k * 12.0, YC + 77.2, ZC - 40.0), 2.4, 0.0, 3.4, techo, lados=4, suave=False)
for k in range(-34, 35, 2):
    C.cilindro("cresteria", (k * 2.0, YC + 92.0, ZC - 60.0), 0.12, 0.0, 1.4, hierro, lados=6)
for x in (-70.0, -35.0, 35.0, 70.0):
    torre(x, ZC - 30.0, YC + 40.0, 4.2, 40.0, techo_alto=24.0, ventanas=3, bandera=True)
# la torre del homenaje: ventanales, impostas, matacanes, torrecillas en las esquinas y la aguja mayor con crochets
HZ = ZC - 62.0
C.caja("homenaje", (0, YC + 60.0, HZ), (15.0, 45.0, 15.0), sillar)
for y in (74.0, 104.0, 128.0):
    for k in (-1, 1):
        G.ventanal(k * 6.0, YC + y, HZ + 15.05, 2.6, 6.0, moldura, vidriera, oscuro, luz=600 if y > 100 else 0)
    C.caja("imposta_h", (0, YC + y - 2.0, HZ), (15.4, 0.3, 15.4), moldura)
G.almenado(15.0, HZ + 15.0, -15.0, HZ + 15.0, YC + 150.0, moldura, alto=1.6, grueso=1.2, paso=2.6, matacan=True)
for sx in (-1, 1):
    for sz in (-1, 1):
        torre(sx * 15.0, HZ + sz * 15.0, YC + 116.0, 3.6, 36.0, techo_alto=26.0, ventanas=2, bandera=(sx > 0 and sz > 0))
C.caja("homenaje_cornisa", (0, YC + 150.0, HZ), (16.5, 1.2, 16.5), moldura)
v = [B(-13, YC + 152.4, HZ + 13.0), B(13, YC + 152.4, HZ + 13.0), B(13, YC + 152.4, HZ - 13.0), B(-13, YC + 152.4, HZ - 13.0), B(0, YC + 215.0, HZ)]
C._desde_pydata("aguja_mayor", v, [(0, 1, 4), (1, 2, 4), (2, 3, 4), (3, 0, 4), (3, 2, 1, 0)], techo)
for k in range(4):
    a = math.pi / 4 + k * math.pi / 2
    for j in range(8):
        t = (j + 1) / 10.0
        rr = 13.0 * math.sqrt(2) * (1 - t)
        C.bloque("crochet", (math.cos(a) * rr, YC + 152.4 + 62.6 * t, HZ + math.sin(a) * rr), (0.45, 0.45, 0.45), moldura)
    G.pinaculo(math.cos(a) * 12.0, YC + 152.0, HZ + math.sin(a) * 12.0, 1.6, moldura)
C.cilindro("aguja_mayor_punta", (0, YC + 214.0, HZ), 0.4, 0.0, 14.0, hierro, lados=8)
G.estandarte_viento(0.0, YC + 236.0, HZ, 9.0, 4.0, tela)
# todo el castillo se corre a la izquierda y un poco abajo (el hueco de la interfaz en la vista de juego)
for o in set(bpy.data.objects) - antes_castillo:
    o.location += Vector((float(ENV.get("ESC_CX", "-175")), 0.0, float(ENV.get("ESC_CY", "-12"))))
# torres lejanas en penas a los lados (capas con niebla en medio) y un puente alto
for (x, z, alto, r, tope) in ((-118.0, -150.0, 120.0, 13.0, 95.0), (78.0, -175.0, 130.0, 14.0, 105.0), (-300.0, -420.0, 100.0, 12.0, 60.0),
                              (330.0, -500.0, 110.0, 14.0, 70.0), (-420.0, -700.0, 100.0, 13.0, 34.0), (450.0, -760.0, 110.0, 14.0, 36.0)):
    y0 = tope - alto - r * 2.8
    C.cilindro("pena_lateral", (x, -300.0, z), r * 3.2, r * 1.6, 300.0 + y0, roca, lados=16, suave=False)
    torre(x, z, y0, r, alto, techo_alto=r * 2.8, ventanas=4, bandera=True)
C.caja("puente_alto", (-60.0, 2.0, -265.0), (66.0, 2.5, 5.0), sillar, giro=0.62)

C.renderizar(CLAVE, sc, op, sin_distancia=[aire, bruma])
