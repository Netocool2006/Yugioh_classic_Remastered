"""Piezas de arquitectura gotica por codigo para los escenarios (Blender 5.2): ventanales ojivales con traceria,
matacanes, almenas, torres con talud, impostas y techo de pizarra irregular con buhardillas, pinaculos con
crochets, arbotantes, gargolas, escalinatas y estandartes al viento. Todo en coordenadas del juego (comun.B).
"""
import math
import random

import bpy
from mathutils import Vector

import comun as C
from comun import B

R = random.Random(5)


# ------------------------------------------------------------------------------------------------ utilidades
def _plano(base, u_eje, v_eje):
    """funcion (u, v, w) -> punto del juego en un plano: base + u * u_eje + v * arriba + w * normal"""
    n = Vector(u_eje).cross(Vector(v_eje)) * -1.0
    b, u_e, v_e = Vector(base), Vector(u_eje), Vector(v_eje)
    return lambda u, v, w=0.0: b + u_e * u + v_e * v + n * w


def _contorno_ojival(ancho, alto_recto, n=10, apuntado=0.25):
    """puntos (u, v) del contorno de un vano ojival (de abajo a la izquierda, sentido horario por arriba)"""
    h = ancho / 2.0
    r = ancho * (0.5 + apuntado)
    pts = [(-h, 0.0), (-h, alto_recto)]
    c_izq, c_der = -h + r, h - r   # centros de los arcos (cada uno del otro lado)
    tope = math.acos((r - h) / r)
    for k in range(1, n + 1):   # arco izquierdo: centro a la derecha
        a = math.pi - tope * k / n
        pts.append((c_izq + math.cos(a) * r, alto_recto + math.sin(a) * r))
    for k in range(n - 1, -1, -1):   # arco derecho
        a = tope * k / n
        pts.append((c_der + math.cos(a) * r, alto_recto + math.sin(a) * r))
    pts.append((h, 0.0))
    return pts


def _anillo(nombre, ext, inn, P, fondo, mat):
    """marco entre dos contornos (misma cantidad de puntos) con canto (fondo) hacia la normal"""
    v, f = [], []
    n = len(ext)
    for (u, vv) in ext:
        v.append(B(*P(u, vv, 0.0)))
    for (u, vv) in inn:
        v.append(B(*P(u, vv, 0.0)))
    for (u, vv) in ext:
        v.append(B(*P(u, vv, -fondo)))
    for (u, vv) in inn:
        v.append(B(*P(u, vv, -fondo)))
    for k in range(n - 1):
        f.append((k, k + 1, n + k + 1, n + k))                       # frente
        f.append((3 * n + k, 3 * n + k + 1, n + k + 1, n + k))       # canto interior
        f.append((2 * n + k + 1, 2 * n + k, k, k + 1))               # canto exterior
    return C._desde_pydata(nombre, v, f, mat)


def _tapa(nombre, pts, P, w, mat):
    v = [B(*P(u, vv, w)) for (u, vv) in pts]
    return C._desde_pydata(nombre, v, [tuple(range(len(pts)))], mat)


# ------------------------------------------------------------------------------------------------ ventanales
def ventanal(cx, y0, z, ancho, alto_recto, marco, cristal, hueco, eje="z", sentido=1.0, traceria=True, fondo=0.6, luz=0.0):
    """ventanal ojival con marco de piedra moldurado, parteluz y traceria (dos arquillos y un oculo) y vidriera
    encendida. eje 'z': en un muro z = cte que mira hacia +z*sentido; eje 'x': muro x = cte."""
    if eje == "z":
        P = _plano((cx, y0, z), (sentido, 0, 0), (0, 1, 0))
    else:
        P = _plano((cx, y0, z), (0, 0, -sentido), (0, 1, 0))
    ext = _contorno_ojival(ancho + 0.7, alto_recto, 12)
    inn = _contorno_ojival(ancho, alto_recto, 12)
    _anillo("ventanal_marco", ext, inn, P, fondo, marco)
    # vidriera hundida y un hueco oscuro detras
    _tapa("vidriera", inn, P, -fondo * 0.7, cristal)
    if traceria:
        h = ancho / 2.0
        g = max(0.08, ancho * 0.05)
        # parteluz
        pts = [(-g, 0.0), (-g, alto_recto + h * 0.3), (g, alto_recto + h * 0.3), (g, 0.0)]
        _anillo_caja(pts, P, -fondo * 0.2, -fondo * 0.6, marco)
        # dos arquillos ojivales y un oculo con lobulos
        for s in (-1, 1):
            sub_ext = [(u * 0.5 + s * h * 0.5, v) for (u, v) in _contorno_ojival(ancho * 0.48, alto_recto, 8)]
            sub_inn = [(u * 0.5 + s * h * 0.5, v) for (u, v) in _contorno_ojival(ancho * 0.48 - 2 * g * 2, alto_recto, 8)]
            # solo la parte curva del arquillo (de alto_recto para arriba)
            sub_ext = [(u, v) for (u, v) in sub_ext if v >= alto_recto - 1e-3]
            sub_inn = [(u, v) for (u, v) in sub_inn if v >= alto_recto - 1e-3]
            if len(sub_ext) == len(sub_inn):
                _anillo("arquillo", sub_ext, sub_inn, lambda u, v, w=0.0: P(u, v, w - fondo * 0.25), fondo * 0.35, marco)
        r = h * 0.42
        cy = alto_recto + ancho * 0.48
        oc_e = [(math.cos(2 * math.pi * k / 24) * r, cy + math.sin(2 * math.pi * k / 24) * r) for k in range(25)]
        oc_i = [(math.cos(2 * math.pi * k / 24) * (r - g * 1.6), cy + math.sin(2 * math.pi * k / 24) * (r - g * 1.6)) for k in range(25)]
        _anillo("oculo", oc_e, oc_i, lambda u, v, w=0.0: P(u, v, w - fondo * 0.25), fondo * 0.35, marco)
    if luz > 0:
        C.luz_punto(B(*P(0, alto_recto * 0.6, -fondo - 1.0)), luz, color=(1.0, 0.55, 0.25), radio=ancho * 0.4)


def _anillo_caja(pts, P, w0, w1, mat):
    v = [B(*P(u, vv, w0)) for (u, vv) in pts] + [B(*P(u, vv, w1)) for (u, vv) in pts]
    n = len(pts)
    f = [tuple(range(n)), tuple(range(2 * n - 1, n - 1, -1))]
    for k in range(n):
        j = (k + 1) % n
        f.append((k, j, n + j, n + k))
    return C._desde_pydata("pieza", v, f, mat)


# ------------------------------------------------------------------------------------------------ torres
def matacanes_redondos(x, z, y, r, mat, merlon_mat=None):
    """corona de matacanes: mensulas alrededor, parapeto volado y almenas"""
    n = max(12, int(2 * math.pi * r / 1.6))
    for k in range(n):
        a = 2 * math.pi * k / n
        C.bloque("mensula", (x + math.cos(a) * (r + 0.35), y - 0.9, z + math.sin(a) * (r + 0.35)), (0.55, 0.9, 0.28), mat, giro_y=-a)
    C.cilindro("parapeto_volado", (x, y, z), r + 0.9, r + 0.9, 2.0, mat, lados=max(48, n * 2), tapa=False)
    C.cilindro("parapeto_int", (x, y, z), r + 0.3, r + 0.3, 2.0, mat, lados=max(48, n * 2), tapa=False)
    C.cilindro("parapeto_suelo", (x, y - 0.3, z), r + 0.9, r + 0.9, 0.3, mat, lados=max(48, n * 2))
    m = int(2 * math.pi * (r + 0.6) / 2.4)
    for k in range(m):
        a = 2 * math.pi * (k + 0.5) / m
        C.bloque("merlon", (x + math.cos(a) * (r + 0.6), y + 2.6, z + math.sin(a) * (r + 0.6)), (0.6, 0.6, 0.75), merlon_mat or mat, giro_y=-a + math.pi / 2)


def techo_pizarra(x, z, y, r, alto, mat, remate, buhardillas=3, irregular=0.12):
    """cono de pizarra con la superficie irregular (ruido), buhardillas y una veleta/aguja"""
    bpy.ops.mesh.primitive_cone_add(vertices=64, radius1=r, radius2=0.0, depth=alto, location=B(x, y + alto / 2, z))
    o = bpy.context.object
    o.data.materials.append(mat)
    md = o.modifiers.new("sub", "SUBSURF")
    md.levels = md.render_levels = 2
    md.subdivision_type = "SIMPLE"
    tx = bpy.data.textures.new("pizarra_irregular", "CLOUDS")
    tx.noise_scale = 1.5
    md2 = o.modifiers.new("irregular", "DISPLACE")
    md2.texture = tx
    md2.texture_coords = "GLOBAL"
    md2.strength = irregular * r
    bpy.ops.object.shade_smooth()
    C.aplicar(o)
    for k in range(buhardillas):
        a = 2 * math.pi * k / buhardillas + R.uniform(0, 1)
        yy = y + alto * 0.18
        rr = r * 0.78
        px, pz = x + math.cos(a) * rr, z + math.sin(a) * rr
        C.bloque("buhardilla", (px, yy + 0.7, pz), (0.55, 0.75, 0.6), remate, giro_y=-a)
        C.cilindro("buhardilla_techo", (px, yy + 1.45, pz), 0.75, 0.0, 1.1, mat, lados=4, suave=False)
    C.cilindro("aguja", (x, y + alto * 0.97, z), 0.12 + r * 0.02, 0.0, alto * 0.3, C.mat_simple("plomo", (0.05, 0.05, 0.055), metal=1.0, aspereza=0.45), lados=8)
    C.cilindro("bola", (x, y + alto * 1.02, z), 0.22, 0.22, 0.3, remate, lados=12)


def pinaculo(x, y, z, s, mat):
    """pinaculo: cuerpo con remate piramidal y crochets"""
    C.caja("pinaculo_cuerpo", (x, y, z), (s * 0.5, s * 1.1, s * 0.5), mat)
    C.cilindro("pinaculo_punta", (x, y + s * 2.2, z), s * 0.62, 0.0, s * 2.8, mat, lados=4, suave=False)
    for k in range(4):
        a = math.pi / 4 + k * math.pi / 2
        for j in range(3):
            t = (j + 1) / 4.0
            rr = s * 0.62 * (1 - t) + 0.05
            C.bloque("crochet", (x + math.cos(a) * rr, y + s * 2.2 + s * 2.8 * t, z + math.sin(a) * rr), (0.12 * s, 0.12 * s, 0.12 * s), mat)


def torre(x, z, y0, r, alto, piedra, moldura, techo, cristal, hueco, techo_alto=None, ventanas=3, gargolas=None, bandera=None, luz=0.0):
    """torre redonda gotica: talud, impostas, saeteras con marco, ventanales ojivales hacia +z, matacanes con
    almenas y techo conico de pizarra con buhardillas (o terraza almenada si techo_alto == 0)"""
    C.cilindro("torre_talud", (x, y0, z), r * 1.22, r * 1.02, min(6.0, alto * 0.18), piedra, lados=64)
    C.cilindro("torre", (x, y0, z), r, r * 0.97, alto, piedra, lados=64)
    for k in range(1, int(alto / 9.0)):
        C.cilindro("imposta", (x, y0 + k * 9.0, z), r * 1.02, r * 1.02, 0.35, moldura, lados=64)
    for k in range(ventanas):
        a = math.pi / 2 + (k - (ventanas - 1) / 2) * 0.55
        yy = y0 + alto * (0.3 + 0.5 * k / max(1, ventanas - 1)) if ventanas > 1 else y0 + alto * 0.6
        px, pz = x + math.cos(a) * r * 0.985, z + math.sin(a) * r * 0.985
        # ventanal mirando hacia afuera (normal radial)
        P = _plano((px, yy, pz), (-math.sin(a), 0, math.cos(a)), (0, 1, 0))
        n = Vector((math.cos(a), 0, math.sin(a)))
        ancho = max(0.9, r * 0.22)
        ext = _contorno_ojival(ancho + 0.5, ancho * 1.6, 8)
        inn = _contorno_ojival(ancho, ancho * 1.6, 8)
        Q = lambda u, v, w=0.0, P=P, n=n: P(u, v, 0.0) + n * (w + 0.25)
        _anillo("saetera_marco", ext, inn, Q, 0.7, moldura)
        _tapa("saetera_luz", inn, Q, -0.5, cristal if R.random() < 0.65 else hueco)
    yt = y0 + alto
    matacanes_redondos(x, z, yt, r, moldura)
    if gargolas:
        for k in range(4):
            a = math.pi / 4 + k * math.pi / 2
            for o in gargolas[0]:
                n = o.copy()
                bpy.context.scene.collection.objects.link(n)
                n.location = B(x + math.cos(a) * (r + 1.6), yt - 1.2, z + math.sin(a) * (r + 1.6))
                n.rotation_euler = (0.0, 0.0, -a - math.pi / 2)
                n.scale = (gargolas[1],) * 3
                n.hide_render = n.hide_viewport = False
    if techo_alto != 0:
        techo_pizarra(x, z, yt + 0.2, r * 1.05, techo_alto or r * 2.6, techo, moldura)
    if bandera:
        h = (techo_alto or r * 2.6) * 1.25 + yt
        estandarte_viento(x, h, z, 3.0, 1.6, bandera)
    if luz > 0:
        C.luz_punto(B(x, y0 + alto * 0.55, z + r + 1.2), luz, color=(1.0, 0.55, 0.25), radio=1.0)


# ------------------------------------------------------------------------------------------------ muros
def almenado(x0, z0, x1, z1, y, mat, alto=1.4, grueso=1.0, paso=2.4, matacan=False, moldura=None):
    """parapeto con almenas (y matacanes volados si matacan) de x0,z0 a x1,z1"""
    L = math.hypot(x1 - x0, z1 - z0)
    g = math.atan2(-(z1 - z0), x1 - x0)
    d = Vector(((x1 - x0) / L, 0, (z1 - z0) / L))
    nrm = Vector((d.z, 0, -d.x))   # hacia afuera (a la derecha del recorrido)
    off = nrm * (0.5 if matacan else 0.0)
    C.caja("parapeto", ((x0 + x1) / 2 + off.x, y, (z0 + z1) / 2 + off.z), (L / 2, alto * 0.45, grueso / 2), mat, giro=g)
    n = int(L / paso)
    for k in range(n):
        t = (k + 0.5) / n
        C.caja("merlon", (x0 + (x1 - x0) * t + off.x, y + alto * 0.9, z0 + (z1 - z0) * t + off.z), (paso * 0.27, alto * 0.45, grueso / 2), mat, giro=g)
        if matacan:
            C.caja("mensula", (x0 + (x1 - x0) * t + nrm.x * 0.45, y - 1.4, z0 + (z1 - z0) * t + nrm.z * 0.45), (0.3, 0.7, 0.55), moldura or mat, giro=g)


def contrafuerte(x, z, y0, alto, ancho, fondo, mat, remate, eje_z=True, pin=True):
    """contrafuerte escalonado (dos retallos con vierteaguas) y pinaculo arriba"""
    hz = fondo / 2
    for k, (fy, ff) in enumerate(((0.0, 1.0), (alto * 0.45, 0.72), (alto * 0.8, 0.45))):
        top = alto * (0.45, 0.8, 1.0)[k]
        h = top - fy
        if eje_z:
            C.caja("contrafuerte", (x, y0 + fy, z + hz * ff), (ancho / 2, h / 2, hz * ff), mat)
        else:
            C.caja("contrafuerte", (x + hz * ff, y0 + fy, z), (hz * ff, h / 2, ancho / 2), mat)
    if pin:
        pinaculo(x, y0 + alto, z + (hz * 0.45 if eje_z else 0.0), ancho * 0.8, remate)


def arbotante(pie, muro, y_pie, y_muro, mat, n=9, canto=0.9, ancho=1.0):
    """arbotante: medio arco de dovelas desde la cabeza de un contrafuerte exento (pie) hasta el muro (puntos x,z)"""
    p0 = Vector((pie[0], y_pie, pie[1]))
    p1 = Vector((muro[0], y_muro, muro[1]))
    d = p1 - p0
    horiz = Vector((d.x, 0, d.z))
    L = horiz.length
    giro = math.atan2(-horiz.z, horiz.x)
    for k in range(n):
        t0, t1 = k / n, (k + 1) / n
        # arco: y sube con forma de cuarto de elipse
        f = lambda t: p0 + horiz * t + Vector((0, (y_muro - y_pie) * math.sin(t * math.pi / 2), 0))
        a, b = f(t0), f(t1)
        c = (a + b) * 0.5
        seg = b - a
        inc = math.atan2(seg.y, Vector((seg.x, 0, seg.z)).length)
        o = C.caja("dovela", (0, -canto / 2, 0), (seg.length * 0.52, canto / 2, ancho / 2), mat)
        o.location = B(c.x, c.y, c.z)
        o.rotation_euler = (0.0, -inc, giro)


def escalinata(cx, y0, z_frente, z_fondo, ancho, alto, mat):
    n = max(3, int(alto / 0.3))
    paso = (z_frente - z_fondo) / n
    for i in range(n):
        zf = z_frente - i * paso
        C.caja("escalon", (cx, y0, (zf + z_fondo) / 2), (ancho / 2, alto * (i + 1) / n / 2, (zf - z_fondo) / 2), mat)


# ------------------------------------------------------------------------------------------------ telas
def estandarte_viento(x, y, z, largo, alto, mat, fase=0.0):
    """bandera en un mastil, flameando hacia +x (ondas que crecen hacia la punta)"""
    nx, ny = 24, 8
    v, f = [], []
    for j in range(ny + 1):
        for i in range(nx + 1):
            u, t = i / nx, j / ny
            onda = math.sin(u * 7.0 + fase) * 0.35 * u + math.sin(u * 13.0 + t * 2.0) * 0.08 * u
            v.append(B(x + u * largo, y - t * alto - u * u * 0.4, z + onda))
    for j in range(ny):
        for i in range(nx):
            a = j * (nx + 1) + i
            f.append((a, a + 1, a + nx + 2, a + nx + 1))
    C._desde_pydata("bandera", v, f, mat, suave=True, hacia=(0, -1, 0))
    C.cilindro("mastil", (x, y - alto - 0.5, z), 0.07, 0.05, alto + 1.5, C.mat_simple("mastil", (0.05, 0.05, 0.05), metal=1.0), lados=8)


def estandarte(x, y_arriba, ancho, largo, z, mat, hacia=(0, -1, 0), plano_x=False, viento=0.4, borde=None):
    """estandarte colgado (punta abajo) con pliegues y el empuje del viento hacia un lado"""
    nx, ny = 12, 36
    v, f = [], []
    for j in range(ny + 1):
        t = j / ny
        for i in range(nx + 1):
            u = i / nx
            px = (u - 0.5) * ancho + viento * t * t * ancho * 0.6
            py = y_arriba - t * (largo + 0.22 * largo * (1.0 - abs(u - 0.5) * 2.0))
            pl = 0.14 * math.sin(u * math.pi * 5.0 + 0.3 + t * 2.5) * (0.4 + 0.6 * t) + 0.5 * t * t
            v.append(B(x + pl, py, z + px) if plano_x else B(x + px, py, z + pl))
    for j in range(ny):
        for i in range(nx):
            a = j * (nx + 1) + i
            f.append((a, a + 1, a + nx + 2, a + nx + 1))
    C._desde_pydata("estandarte", v, f, mat, suave=True, hacia=hacia)
