"""Herramientas comunes de los escenarios pre-renderizados (Blender 5.2, Cycles, sin interfaz).

Cada escenario (castillo.py, ...) arma su lugar con insumos CC0 (polyhaven.py) y llama a `renderizar()`, que deja en
<classic-assets>/entornos/pano/:
  <clave>.jpg        panorama equirectangular visto desde el origen (la camara de juego), sRGB con el tono AgX
  <clave>_prof.png   distancia desde el origen por direccion (16 bits: log2(m) / 12 * 65535; 65535 = cielo)
  <clave>.txt        "origen x y z" y "llama x y z alto" (coordenadas del juego) para el motor (src/entornos.h)
Modo "vista": un solo cuadro con la camara de juego (misma proyeccion que el juego, con la columna de la interfaz)
para revisar rapido la composicion: <salida>/<clave>_vista.png.

Coordenadas: el juego usa x derecha, y arriba, z hacia el jugador; Blender x derecha, y adelante, z arriba.
B(x, y, z) pasa del juego a Blender. La mesa (26.9 x 20.2 m) esta centrada en el origen, su tapa en y = 0; el suelo
alrededor en y = -0.3.
"""
import math
import os
import struct
import sys
import time
import zlib

import bpy
import numpy as np
from mathutils import Vector

SRC = os.environ.get("ESC_SRC", "D:/Docker/compartido/classic-assets/entornos_src")
OUT = os.environ.get("ESC_OUT", "D:/Docker/compartido/classic-assets/entornos/pano")
ESCALA_TABLERO = 1.4
OJO = (0.0, 6.4 * ESCALA_TABLERO * 1.08, 12.8 * ESCALA_TABLERO * 1.08)
OBJETIVO = (0.0, 0.0, -0.6 * ESCALA_TABLERO)
SUELO = -0.3
MESA = (13.44, 10.08)   # medio ancho (x) y medio largo (z)
MIRA_EXTRA = float(os.environ.get("ESC_MIRA", "7"))   # grados que la camara de juego mira hacia arriba


def B(x, y, z):
    return Vector((x, -z, y))


def args():
    """--modo vista|pano  --ancho N  --muestras N"""
    a = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    r = {"modo": "vista", "ancho": 8192, "muestras": 0}
    for i in range(0, len(a) - 1, 2):
        k = a[i].lstrip("-")
        r[k] = a[i + 1] if k in ("modo", "lugar") else int(a[i + 1])
    return r


# ---------------------------------------------------------------------------------------------------- escena
def escena_nueva():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    cy = sc.cycles
    cy.device = "GPU" if os.environ.get("ESC_GPU", "1") == "1" else "CPU"
    if cy.device == "GPU":
        p = bpy.context.preferences.addons["cycles"].preferences
        p.compute_device_type = "OPTIX"
        p.get_devices()
        for d in p.devices:
            d.use = d.type in ("OPTIX",)
    cy.use_denoising = True
    cy.denoiser = "OPENIMAGEDENOISE"
    cy.max_bounces = 6
    cy.diffuse_bounces = 3
    cy.glossy_bounces = 3
    cy.transmission_bounces = 4
    cy.volume_bounces = 1
    cy.transparent_max_bounces = 8
    cy.caustics_reflective = False
    cy.caustics_refractive = False
    cy.sample_clamp_indirect = 6.0
    cy.use_adaptive_sampling = True
    cy.adaptive_threshold = 0.02
    sc.view_settings.view_transform = "AgX"
    try:
        sc.view_settings.look = "AgX - Medium High Contrast"
    except TypeError:
        pass
    sc.render.film_transparent = False
    return sc


def mundo_hdri(hdri, fuerza=1.0, giro_grados=0.0):
    """Cielo con un HDRI (CC0). giro: vuelta en z (grados) para poner el sol donde se quiere"""
    w = bpy.data.worlds.new("mundo")
    bpy.context.scene.world = w
    w.use_nodes = True
    nt = w.node_tree
    nt.nodes.clear()
    tc = nt.nodes.new("ShaderNodeTexCoord")
    mp = nt.nodes.new("ShaderNodeMapping")
    mp.inputs["Rotation"].default_value[2] = math.radians(giro_grados)
    env = nt.nodes.new("ShaderNodeTexEnvironment")
    env.image = bpy.data.images.load(os.path.join(SRC, "hdris", hdri))
    bg = nt.nodes.new("ShaderNodeBackground")
    bg.inputs["Strength"].default_value = fuerza
    out = nt.nodes.new("ShaderNodeOutputWorld")
    nt.links.new(tc.outputs["Generated"], mp.inputs["Vector"])
    nt.links.new(mp.outputs["Vector"], env.inputs["Vector"])
    nt.links.new(env.outputs["Color"], bg.inputs["Color"])
    nt.links.new(bg.outputs["Background"], out.inputs["Surface"])
    return w


def sol_del_hdri(hdri, giro_grados=0.0):
    """Direccion (Blender) del punto mas brillante del HDRI ya girado"""
    img = bpy.data.images.load(os.path.join(SRC, "hdris", hdri), check_existing=True)
    w, h = img.size
    px = np.empty(w * h * 4, dtype=np.float32)
    img.pixels.foreach_get(px)
    px = px.reshape(h, w, 4)
    lum = px[..., 0] * 0.2 + px[..., 1] * 0.7 + px[..., 2] * 0.1
    y, x = np.unravel_index(np.argmax(lum), lum.shape)
    u = (x + 0.5) / w
    v = (y + 0.5) / h   # fila 0 abajo en Blender
    lon = (u - 0.5) * 2 * math.pi
    lat = (v - 0.5) * math.pi
    # equirect de Cycles: u = 0.5 - atan2(y, x) / 2pi. El nodo Mapping gira el vector antes de buscar: el sol queda
    # en Rz(-giro) * s0
    s0 = Vector((math.cos(lon) * math.cos(lat), -math.sin(lon) * math.cos(lat), math.sin(lat)))
    g = -math.radians(giro_grados)
    return Vector((s0.x * math.cos(g) - s0.y * math.sin(g), s0.x * math.sin(g) + s0.y * math.cos(g), s0.z))


def giro_para(u_centro):
    """giro (grados) del HDRI para que su columna u_centro (0..1 en la imagen) quede delante de la camara de juego"""
    return (0.25 - u_centro) * 360.0


def niebla_mundo(densidad, color=(1.0, 1.0, 1.0), anisotropia=0.5, radio=320.0, alto=70.0, caida=0.0):
    """Niebla en una caja grande alrededor de la escena (volumen; uno en el mundo entero se comeria la luz del sol y
    del cielo): aire con perspectiva y rayos de luz. caida > 0: mas espesa abajo (densidad * exp(-y / caida))"""
    m = bpy.data.materials.new("niebla")
    m.use_nodes = True
    nt = m.node_tree
    nt.nodes.clear()
    vol = nt.nodes.new("ShaderNodeVolumePrincipled")
    vol.inputs["Density"].default_value = densidad
    vol.inputs["Color"].default_value = (*color, 1.0)
    vol.inputs["Anisotropy"].default_value = anisotropia
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    nt.links.new(vol.outputs["Volume"], out.inputs["Volume"])
    if caida > 0:
        geo = nt.nodes.new("ShaderNodeNewGeometry")
        sep = nt.nodes.new("ShaderNodeSeparateXYZ")
        nt.links.new(geo.outputs["Position"], sep.inputs[0])
        mt = nt.nodes.new("ShaderNodeMath")
        mt.operation = "MULTIPLY"
        mt.inputs[1].default_value = -1.0 / caida
        nt.links.new(sep.outputs[2], mt.inputs[0])
        ex = nt.nodes.new("ShaderNodeMath")
        ex.operation = "EXPONENT"
        nt.links.new(mt.outputs[0], ex.inputs[0])
        k = nt.nodes.new("ShaderNodeMath")
        k.operation = "MULTIPLY"
        k.inputs[1].default_value = densidad
        nt.links.new(ex.outputs[0], k.inputs[0])
        nt.links.new(k.outputs[0], vol.inputs["Density"])
    o = caja("niebla", (0.0, SUELO - 1.0, 0.0), (radio, alto * 0.5, radio), m)
    o.visible_shadow = True
    return o


def sol(direccion, fuerza=4.0, color=(1.0, 0.75, 0.5), angulo=1.0):
    d = bpy.data.lights.new("sol", "SUN")
    d.energy = fuerza
    d.color = color
    d.angle = math.radians(angulo)
    o = bpy.data.objects.new("sol", d)
    bpy.context.scene.collection.objects.link(o)
    o.rotation_euler = (-Vector(direccion)).to_track_quat("-Z", "Y").to_euler()
    return o


def luz_punto(p, potencia, color=(1.0, 0.55, 0.22), radio=0.25, nombre="luz"):
    d = bpy.data.lights.new(nombre, "POINT")
    d.energy = potencia
    d.color = color
    d.shadow_soft_size = radio
    o = bpy.data.objects.new(nombre, d)
    bpy.context.scene.collection.objects.link(o)
    o.location = p
    return o


# ------------------------------------------------------------------------------------------------ materiales
def _img(ruta, color):
    im = bpy.data.images.load(ruta, check_existing=True)
    if not color:
        im.colorspace_settings.name = "Non-Color"
    return im


def mat_ph(nombre, tex, tam=2.0, tinte=(1.0, 1.0, 1.0), relieve=1.0, aspereza=1.0, uv=False, mezcla=0.25, res="2k",
           desplazar=0.0, brillo_humedo=None, variar=0.25, mugre=(3.0, 0.45), musgo=0.0):
    """Material PBR con una textura de Poly Haven (CC0). tam: metros que ocupa la textura. Sin uv: proyeccion de caja
    en coordenadas del objeto (las mallas por codigo llevan la transformacion aplicada: objeto = mundo).
    desplazar > 0: desplazamiento real (m) ademas del relieve. brillo_humedo: (escala, cuanto) charcos/humedad"""
    m = bpy.data.materials.new(nombre)
    m.use_nodes = True
    nt = m.node_tree
    nt.nodes.clear()
    base = os.path.join(SRC, "texturas", tex, f"{tex}_")
    tc = nt.nodes.new("ShaderNodeTexCoord")
    mp = nt.nodes.new("ShaderNodeMapping")
    mp.inputs["Scale"].default_value = (1.0 / tam,) * 3
    nt.links.new(tc.outputs["UV" if uv else "Object"], mp.inputs["Vector"])

    def tx(sufijo, color):
        for ext in ("jpg", "png"):
            r = f"{base}{sufijo}_{res}.{ext}"
            if os.path.exists(r):
                n = nt.nodes.new("ShaderNodeTexImage")
                n.image = _img(r, color)
                if not uv:
                    n.projection = "BOX"
                    n.projection_blend = mezcla
                nt.links.new(mp.outputs["Vector"], n.inputs["Vector"])
                return n
        return None

    bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    nt.links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
    dif = tx("diff", True)
    if dif:
        mul = nt.nodes.new("ShaderNodeMix")
        mul.data_type = "RGBA"
        mul.blend_type = "MULTIPLY"
        mul.inputs["Factor"].default_value = 1.0
        mul.inputs[7].default_value = (*tinte, 1.0)
        nt.links.new(dif.outputs["Color"], mul.inputs[6])
        if variar > 0:
            # manchas grandes (mugre, humedad, sol): el tinte cambia +-variar con un ruido de ~7 m
            nzv = nt.nodes.new("ShaderNodeTexNoise")
            nzv.inputs["Scale"].default_value = 1.0 / 7.0
            nzv.inputs["Detail"].default_value = 3.0
            geov = nt.nodes.new("ShaderNodeNewGeometry")
            nt.links.new(geov.outputs["Position"], nzv.inputs["Vector"])
            rg = nt.nodes.new("ShaderNodeMapRange")
            rg.inputs["From Min"].default_value = 0.3
            rg.inputs["From Max"].default_value = 0.7
            rg.inputs["To Min"].default_value = 1.0 - variar
            rg.inputs["To Max"].default_value = 1.0 + variar * 0.4
            nt.links.new(nzv.outputs["Fac"], rg.inputs["Value"])
            mv = nt.nodes.new("ShaderNodeMix")
            mv.data_type = "RGBA"
            mv.blend_type = "MULTIPLY"
            mv.inputs["Factor"].default_value = 1.0
            nt.links.new(mul.outputs[2], mv.inputs[6])
            cmb = nt.nodes.new("ShaderNodeCombineXYZ")
            for k in range(3):
                nt.links.new(rg.outputs["Result"], cmb.inputs[k])
            nt.links.new(cmb.outputs[0], mv.inputs[7])
            mul = mv
        if mugre:
            # mugre y humedad que suben desde el suelo: mas oscuro abajo (mugre[0] m, hasta mugre[1] de oscuridad)
            geom = nt.nodes.new("ShaderNodeNewGeometry")
            sz = nt.nodes.new("ShaderNodeSeparateXYZ")
            nt.links.new(geom.outputs["Position"], sz.inputs[0])
            nzm = nt.nodes.new("ShaderNodeTexNoise")
            nzm.inputs["Scale"].default_value = 0.6
            nt.links.new(geom.outputs["Position"], nzm.inputs["Vector"])
            ad = nt.nodes.new("ShaderNodeMath")
            ad.operation = "MULTIPLY_ADD"
            ad.inputs[1].default_value = mugre[0] * 0.6
            ad.inputs[2].default_value = mugre[0] * -0.3
            nt.links.new(nzm.outputs["Fac"], ad.inputs[0])
            sm = nt.nodes.new("ShaderNodeMapRange")
            sm.interpolation_type = "SMOOTHSTEP"
            sm.inputs["From Min"].default_value = SUELO
            nt.links.new(sz.outputs[2], sm.inputs["Value"])
            fm = nt.nodes.new("ShaderNodeMath")
            fm.operation = "ADD"
            fm.inputs[1].default_value = SUELO + mugre[0]
            nt.links.new(ad.outputs[0], fm.inputs[0])
            nt.links.new(fm.outputs[0], sm.inputs["From Max"])
            sm.inputs["To Min"].default_value = 1.0 - mugre[1]
            sm.inputs["To Max"].default_value = 1.0
            mg = nt.nodes.new("ShaderNodeMix")
            mg.data_type = "RGBA"
            mg.blend_type = "MULTIPLY"
            mg.inputs["Factor"].default_value = 1.0
            nt.links.new(mul.outputs[2], mg.inputs[6])
            cm = nt.nodes.new("ShaderNodeCombineXYZ")
            nt.links.new(sm.outputs["Result"], cm.inputs[0])
            nt.links.new(sm.outputs["Result"], cm.inputs[1])
            nt.links.new(sm.outputs["Result"], cm.inputs[2])
            nt.links.new(cm.outputs[0], mg.inputs[7])
            mul = mg
        if musgo > 0:
            # musgo (ambientCG Moss002, CC0) en lo que mira hacia arriba y en manchas que bajan por la piedra
            mimg = os.path.join(os.path.dirname(SRC), "bosque", "musgo_color.png")
            if os.path.exists(mimg):
                mt_ = nt.nodes.new("ShaderNodeTexImage")
                mt_.image = _img(mimg, True)
                mt_.projection = "BOX"
                mt_.projection_blend = 0.3
                mpm = nt.nodes.new("ShaderNodeMapping")
                mpm.inputs["Scale"].default_value = (0.5, 0.5, 0.5)
                nt.links.new(tc.outputs["Object"], mpm.inputs["Vector"])
                nt.links.new(mpm.outputs["Vector"], mt_.inputs["Vector"])
                gm = nt.nodes.new("ShaderNodeNewGeometry")
                sn = nt.nodes.new("ShaderNodeSeparateXYZ")
                nt.links.new(gm.outputs["Normal"], sn.inputs[0])
                nzm2 = nt.nodes.new("ShaderNodeTexNoise")
                nzm2.inputs["Scale"].default_value = 0.35
                nzm2.inputs["Detail"].default_value = 6.0
                nt.links.new(gm.outputs["Position"], nzm2.inputs["Vector"])
                arr = nt.nodes.new("ShaderNodeMath")
                arr.operation = "MULTIPLY_ADD"
                arr.inputs[1].default_value = 0.55
                nt.links.new(sn.outputs[2], arr.inputs[0])
                nt.links.new(nzm2.outputs["Fac"], arr.inputs[2])
                if alt_m := tx("disp", False):
                    jj = nt.nodes.new("ShaderNodeMath")
                    jj.operation = "MULTIPLY_ADD"
                    jj.inputs[1].default_value = -0.35
                    nt.links.new(alt_m.outputs["Color"], jj.inputs[0])
                    nt.links.new(arr.outputs[0], jj.inputs[2])
                    arr_o = jj.outputs[0]
                else:
                    arr_o = arr.outputs[0]
                mr = nt.nodes.new("ShaderNodeMapRange")
                mr.inputs["From Min"].default_value = 0.78 - 0.3 * musgo
                mr.inputs["From Max"].default_value = 0.9 - 0.3 * musgo
                nt.links.new(arr_o, mr.inputs["Value"])
                mm = nt.nodes.new("ShaderNodeMix")
                mm.data_type = "RGBA"
                nt.links.new(mr.outputs["Result"], mm.inputs["Factor"])
                nt.links.new(mul.outputs[2], mm.inputs[6])
                tm = nt.nodes.new("ShaderNodeMix")
                tm.data_type = "RGBA"
                tm.blend_type = "MULTIPLY"
                tm.inputs["Factor"].default_value = 1.0
                tm.inputs[7].default_value = (0.55, 0.6, 0.45, 1.0)
                nt.links.new(mt_.outputs["Color"], tm.inputs[6])
                nt.links.new(tm.outputs[2], mm.inputs[7])
                mul = mm
        nt.links.new(mul.outputs[2], bsdf.inputs["Base Color"])
    ru = tx("rough", False)
    rough_sock = None
    if ru:
        mt = nt.nodes.new("ShaderNodeMath")
        mt.operation = "MULTIPLY"
        mt.inputs[1].default_value = aspereza
        nt.links.new(ru.outputs["Color"], mt.inputs[0])
        rough_sock = mt.outputs[0]
    else:
        bsdf.inputs["Roughness"].default_value = 0.8 * aspereza
    alt = tx("disp", False)
    if brillo_humedo and rough_sock is not None:
        # charcos: ruido grande y las juntas bajas (disp bajo) quedan humedas (menos asperas y mas oscuras)
        nz = nt.nodes.new("ShaderNodeTexNoise")
        nz.inputs["Scale"].default_value = 1.0 / brillo_humedo[0]
        nz.inputs["Detail"].default_value = 4.0
        geo = nt.nodes.new("ShaderNodeNewGeometry")
        nt.links.new(geo.outputs["Position"], nz.inputs["Vector"])
        cr = nt.nodes.new("ShaderNodeMapRange")
        cr.inputs["From Min"].default_value = 0.55
        cr.inputs["From Max"].default_value = 0.62
        nt.links.new(nz.outputs["Fac"], cr.inputs["Value"])
        if alt:
            junta = nt.nodes.new("ShaderNodeMapRange")
            junta.inputs["From Min"].default_value = 0.45
            junta.inputs["From Max"].default_value = 0.2
            nt.links.new(alt.outputs["Color"], junta.inputs["Value"])
            mx = nt.nodes.new("ShaderNodeMath")
            mx.operation = "MAXIMUM"
            nt.links.new(cr.outputs["Result"], mx.inputs[0])
            ju = nt.nodes.new("ShaderNodeMath")
            ju.operation = "MULTIPLY"
            ju.inputs[1].default_value = 0.6
            nt.links.new(junta.outputs["Result"], ju.inputs[0])
            nt.links.new(ju.outputs[0], mx.inputs[1])
            hum = mx.outputs[0]
        else:
            hum = cr.outputs["Result"]
        hm = nt.nodes.new("ShaderNodeMath")
        hm.operation = "MULTIPLY"
        hm.inputs[1].default_value = brillo_humedo[1]
        nt.links.new(hum, hm.inputs[0])
        lr = nt.nodes.new("ShaderNodeMix")
        lr.data_type = "FLOAT"
        lr.inputs[3].default_value = 0.06
        nt.links.new(hm.outputs[0], lr.inputs["Factor"])
        nt.links.new(rough_sock, lr.inputs[2])
        rough_sock = lr.outputs[0]
        if dif:
            osc = nt.nodes.new("ShaderNodeMix")
            osc.data_type = "RGBA"
            osc.blend_type = "MULTIPLY"
            osc.inputs[7].default_value = (0.55, 0.55, 0.58, 1.0)
            nt.links.new(hm.outputs[0], osc.inputs["Factor"])
            nt.links.new(mul.outputs[2], osc.inputs[6])
            nt.links.new(osc.outputs[2], bsdf.inputs["Base Color"])
    if rough_sock is not None:
        nt.links.new(rough_sock, bsdf.inputs["Roughness"])
    if alt:
        bump = nt.nodes.new("ShaderNodeBump")
        bump.inputs["Strength"].default_value = min(1.0, relieve)
        bump.inputs["Distance"].default_value = 0.04 * tam * relieve
        nt.links.new(alt.outputs["Color"], bump.inputs["Height"])
        nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
        if desplazar > 0:
            dp = nt.nodes.new("ShaderNodeDisplacement")
            dp.inputs["Scale"].default_value = desplazar
            dp.inputs["Midlevel"].default_value = 0.5
            nt.links.new(alt.outputs["Color"], dp.inputs["Height"])
            nt.links.new(dp.outputs["Displacement"], out.inputs["Displacement"])
            m.displacement_method = "BOTH"
    elif uv:
        nor = tx("nor_gl", False)
        if nor:
            nm = nt.nodes.new("ShaderNodeNormalMap")
            nm.inputs["Strength"].default_value = relieve
            nt.links.new(nor.outputs["Color"], nm.inputs["Color"])
            nt.links.new(nm.outputs["Normal"], bsdf.inputs["Normal"])
    return m


def mat_simple(nombre, color, aspereza=0.7, metal=0.0, emision=None, fuerza=0.0, brillo_tela=0.0):
    m = bpy.data.materials.new(nombre)
    m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*color, 1.0)
    b.inputs["Roughness"].default_value = aspereza
    b.inputs["Metallic"].default_value = metal
    if emision:
        b.inputs["Emission Color"].default_value = (*emision, 1.0)
        b.inputs["Emission Strength"].default_value = fuerza
    if brillo_tela > 0:
        b.inputs["Sheen Weight"].default_value = brillo_tela
    return m


def mat_fuego(nombre="fuego", fuerza=60.0):
    m = bpy.data.materials.new(nombre)
    m.use_nodes = True
    nt = m.node_tree
    nt.nodes.clear()
    em = nt.nodes.new("ShaderNodeEmission")
    em.inputs["Color"].default_value = (1.0, 0.42, 0.1, 1.0)
    em.inputs["Strength"].default_value = fuerza
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    nt.links.new(em.outputs[0], out.inputs["Surface"])
    return m


# ------------------------------------------------------------------------------------------------- geometria
def _obj(nombre, malla, mat):
    o = bpy.data.objects.new(nombre, malla)
    bpy.context.scene.collection.objects.link(o)
    if mat:
        o.data.materials.append(mat)
    return o


def _desde_pydata(nombre, v, f, mat, suave=False, hacia=None):
    """malla desde vertices y caras. Normales hacia afuera (recalculadas: en Cycles una normal al reves confunde la
    niebla y las sombras); hacia: para superficies abiertas, el lado al que miran (vector de Blender)"""
    import bmesh
    me = bpy.data.meshes.new(nombre)
    me.from_pydata([tuple(p) for p in v], [], f)
    me.validate()
    bm = bmesh.new()
    bm.from_mesh(me)
    if hacia is None:
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    else:
        bm.faces.ensure_lookup_table()
        if bm.faces and bm.faces[0].normal.dot(Vector(hacia)) < 0:
            bmesh.ops.reverse_faces(bm, faces=bm.faces)
    bm.to_mesh(me)
    bm.free()
    me.update()
    if suave:
        me.shade_smooth()
    return _obj(nombre, me, mat)


def caja(nombre, c, medio, mat, giro=0.0, bisel=0.0):
    """caja con su centro abajo en c (juego) y medias medidas (x, y, z del juego); giro en y (rad)"""
    hx, hy, hz = medio
    cs, sn = math.cos(giro), math.sin(giro)
    v = []
    for y in (0.0, 2 * hy):
        for x, z in ((-hx, -hz), (hx, -hz), (hx, hz), (-hx, hz)):
            v.append(B(c[0] + cs * x + sn * z, c[1] + y, c[2] - sn * x + cs * z))
    f = [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]
    o = _desde_pydata(nombre, v, f, mat)
    if bisel > 0:
        md = o.modifiers.new("bisel", "BEVEL")
        md.width = bisel
        md.segments = 2
    return o


def bloque(nombre, centro, medio, mat, giro_z=0.0, giro_y=0.0):
    """caja centrada en 'centro' (juego), girada giro_z en el plano de la fachada (eje z del juego) y giro_y"""
    o = caja(nombre, (0.0, -medio[1], 0.0), medio, mat)
    o.location = B(*centro)
    o.rotation_euler = (0.0, giro_z, giro_y)
    return o


def arco(nombre, cx, y0, z, luz, mat, dovelas=9, canto=0.5, fondo=0.6, apuntado=0.25):
    """arco apuntado de dovelas sobre un vano de ancho 'luz' (centro cx, arranque y0, plano z de la fachada)"""
    r = luz * (0.5 + apuntado)
    piezas = []
    for lado in (-1, 1):
        c = cx - lado * (r - luz * 0.5)   # centro de cada mitad (del otro lado)
        a0 = 0.0 if lado > 0 else math.pi
        tope = math.acos((r - luz * 0.5) / r)
        a1 = tope if lado > 0 else math.pi - tope
        for k in range(dovelas):
            t = (k + 0.5) / dovelas
            a = a0 + (a1 - a0) * t
            rr = r + canto * 0.5
            p = (c + math.cos(a) * rr, y0 + math.sin(a) * rr, z)
            largo = abs(a1 - a0) * rr / dovelas * 0.48
            piezas.append(bloque(nombre, p, (canto * 0.5, largo, fondo * 0.5), mat, giro_z=-a))
    return piezas


def cilindro(nombre, c, r0, r1, alto, mat, lados=48, tapa=True, suave=True):
    v, f = [], []
    for k in range(lados):
        a = 2 * math.pi * k / lados
        v.append(B(c[0] + math.cos(a) * r0, c[1], c[2] + math.sin(a) * r0))
        v.append(B(c[0] + math.cos(a) * r1, c[1] + alto, c[2] + math.sin(a) * r1))
    for k in range(lados):
        j = (k + 1) % lados
        f.append((2 * k, 2 * j, 2 * j + 1, 2 * k + 1))
    if tapa and r1 > 1e-3:
        f.append(tuple(2 * k + 1 for k in reversed(range(lados))))
    o = _desde_pydata(nombre, v, f, mat, suave)
    if suave:
        md = o.modifiers.new("aristas", "EDGE_SPLIT")
        md.split_angle = math.radians(40)
    return o


def plano(nombre, x0, z0, x1, z1, y, mat, div=1):
    v, f = [], []
    for j in range(div + 1):
        for i in range(div + 1):
            v.append(B(x0 + (x1 - x0) * i / div, y, z0 + (z1 - z0) * j / div))
    for j in range(div):
        for i in range(div):
            a = j * (div + 1) + i
            f.append((a, a + div + 1, a + div + 2, a + 1))
    return _desde_pydata(nombre, v, f, mat, hacia=(0, 0, 1))


def aplicar(o):
    """aplica modificadores y transformacion (coordenadas del objeto = mundo, para la proyeccion de caja)"""
    bpy.context.view_layer.objects.active = o
    for ob in bpy.context.selected_objects:
        ob.select_set(False)
    o.select_set(True)
    for md in list(o.modifiers):
        bpy.ops.object.modifier_apply(modifier=md.name)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)


def importar_gltf(ident, res="2k"):
    """importa un modelo de Poly Haven; devuelve sus objetos (malla) ya sin padre"""
    antes = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=os.path.join(SRC, "modelos", ident, f"{ident}_{res}.gltf"))
    nuevos = [o for o in bpy.data.objects if o not in antes]
    for o in nuevos:
        if o.parent:
            mw = o.matrix_world.copy()
            o.parent = None
            o.matrix_world = mw
    return nuevos


def colocar(o, p, giro_y=0.0, escala=1.0, copia=True):
    """copia enlazada del objeto o (o el mismo) en p (juego), girado en y del juego (rad), base en p"""
    n = o.copy() if copia else o
    if copia:
        bpy.context.scene.collection.objects.link(n)
    n.location = B(*p)
    n.rotation_euler = (0.0, 0.0, giro_y)
    n.scale = (escala,) * 3
    return n


def centrar_base(objs):
    """lleva un grupo de objetos para que su base quede en z = 0 y centrado en xy (en su propio lugar)"""
    pts = []
    for o in objs:
        pts += [o.matrix_world @ Vector(c) for c in o.bound_box]
    mn = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
    mx = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    d = Vector(((mn.x + mx.x) / 2, (mn.y + mx.y) / 2, mn.z))
    for o in objs:
        o.location -= d
    return mx - mn


# ------------------------------------------------------------------------------------------------ las llamas
LLAMAS = []


def antorcha(p, alto_llama=0.9, potencia=900.0, mat_madera=None, mat_hierro=None, mat_llama=None):
    """antorcha de pared o de pie: mango, brasero de hierro, llama y su luz. p = base de la llama (juego)"""
    x, y, z = p
    if mat_hierro:
        cilindro("antorcha_cuenco", (x, y - 0.25, z), 0.08, 0.2, 0.28, mat_hierro, lados=12)
    if mat_madera:
        cilindro("antorcha_mango", (x, y - 1.0, z), 0.05, 0.07, 0.8, mat_madera, lados=8)
    if mat_llama:
        o = cilindro("llama", (x, y, z), 0.11, 0.0, alto_llama * 0.6, mat_llama, lados=10)
        o.visible_shadow = False
        o.visible_camera = False   # la llama que se ve la pone el motor, animada, en el mismo lugar
    luz_punto(B(x, y + alto_llama * 0.45, z), potencia, radio=0.2)
    LLAMAS.append((x, y, z, alto_llama))


# ------------------------------------------------------------------------------------------------ el render
def camara_juego(sc):
    """camara perspectiva con la proyeccion del juego (FOV 62, zoom 0.71 y la columna de la interfaz a 1920x1080)"""
    c = bpy.data.cameras.new("juego")
    k = (1920 - 512) / 1920 * 0.71
    cx = ((512 + 1920) * 0.5) / 1920 * 2 - 1
    t = math.tan(math.radians(31.0))
    tv = t / (16 / 9)
    izq, der = (-1 - cx) / k * t, (1 - cx) / k * t
    arr, aba = (-1 + 0.17) / k * tv, (1 + 0.17) / k * tv   # vulkan: y hacia abajo
    ancho = der - izq
    c.sensor_fit = "HORIZONTAL"
    c.angle = 2 * math.atan(ancho / 2)
    c.shift_x = (der + izq) / 2 / ancho
    c.shift_y = -(aba + arr) / 2 / ancho
    c.clip_end = 5000
    o = bpy.data.objects.new("juego", c)
    sc.collection.objects.link(o)
    o.location = B(*OJO)
    # la mira sube MIRA_EXTRA grados (como yugireload.cpp: MIRA_EXTRA_GRADOS)
    d = Vector(OBJETIVO) - Vector(OJO)
    h = math.hypot(d.x, d.z)
    p = math.atan2(d.y, h) + math.radians(MIRA_EXTRA)
    d = Vector((d.x / h * math.cos(p), math.sin(p), d.z / h * math.cos(p)))
    o.rotation_euler = B(*d).to_track_quat("-Z", "Y").to_euler()
    return o


def camara_pano(sc):
    c = bpy.data.cameras.new("pano")
    c.type = "PANO"
    c.panorama_type = "EQUIRECTANGULAR"
    c.clip_end = 5000
    o = bpy.data.objects.new("pano", c)
    sc.collection.objects.link(o)
    o.location = B(*OJO)
    o.rotation_euler = (math.pi / 2, 0.0, 0.0)   # el centro de la imagen mira a -z del juego, +x a la derecha
    return o


def _png16(ruta, datos):
    """PNG gris de 16 bits (datos: uint16 h x w)"""
    h, w = datos.shape
    crudo = b"".join(b"\x00" + datos[y].astype(">u2").tobytes() for y in range(h))

    def trozo(t, d):
        c = struct.pack(">I", len(d)) + t + d
        return c + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)

    with open(ruta, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(trozo(b"IHDR", struct.pack(">IIBBBBB", w, h, 16, 0, 0, 0, 0)))
        f.write(trozo(b"IDAT", zlib.compress(crudo, 6)))
        f.write(trozo(b"IEND", b""))


def distancias(sc, ancho):
    """mapa de distancias del panorama por rayos desde el origen (ancho x ancho/2)"""
    alto = ancho // 2
    dg = bpy.context.evaluated_depsgraph_get()
    o = B(*OJO)
    out = np.full((alto, ancho), 65535, dtype=np.uint16)
    t0 = time.time()
    for j in range(alto):
        th = (j + 0.5) / alto * math.pi
        st, ct = math.sin(th), math.cos(th)
        for i in range(ancho):
            lon = ((i + 0.5) / ancho - 0.5) * 2 * math.pi
            d = Vector((math.sin(lon) * st, math.cos(lon) * st, ct))
            ok, p, n, idx, ob, mt = sc.ray_cast(dg, o, d, distance=4000.0)
            if ok:
                m = max((p - o).length, 1.0)
                out[j, i] = min(65534, int(math.log2(m) / 12.0 * 65535 + 0.5))
    print(f"[escenarios] distancias {ancho}x{alto} en {time.time() - t0:.0f} s")
    return out


def distancias_render(sc, ancho, ocultos):
    """mapa de distancias con Cycles (1 muestra por pixel, cada superficie emite su distancia al origen): segundos
    en vez de minutos de rayos desde Python"""
    alto = ancho // 2
    for o in ocultos:
        o.hide_render = True
    m = bpy.data.materials.new("distancia")
    m.use_nodes = True
    nt = m.node_tree
    nt.nodes.clear()
    geo = nt.nodes.new("ShaderNodeNewGeometry")
    dv = nt.nodes.new("ShaderNodeVectorMath")
    dv.operation = "DISTANCE"
    dv.inputs[1].default_value = B(*OJO)
    nt.links.new(geo.outputs["Position"], dv.inputs[0])
    em = nt.nodes.new("ShaderNodeEmission")
    em.inputs["Color"].default_value = (1.0, 1.0, 1.0, 1.0)
    nt.links.new(dv.outputs["Value"], em.inputs["Strength"])
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    nt.links.new(em.outputs[0], out.inputs["Surface"])
    vl = bpy.context.view_layer
    vl.material_override = m
    w = bpy.data.worlds.new("negro")
    w.use_nodes = True
    w.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.0
    sc.world = w
    cy = sc.cycles
    cy.samples = 1
    cy.use_adaptive_sampling = False
    cy.use_denoising = False
    cy.max_bounces = 0
    cy.filter_width = 0.01
    sc.view_settings.view_transform = "Standard"
    sc.view_settings.exposure = 0.0
    sc.render.resolution_x, sc.render.resolution_y = ancho, alto
    sc.render.image_settings.file_format = "OPEN_EXR"
    sc.render.image_settings.color_depth = "32"
    tmp = os.path.join(OUT, "_distancias.exr")
    sc.render.filepath = tmp
    t0 = time.time()
    bpy.ops.render.render(write_still=True)
    img = bpy.data.images.load(tmp)
    px = np.empty(ancho * alto * 4, dtype=np.float32)
    img.pixels.foreach_get(px)
    d = px.reshape(alto, ancho, 4)[::-1, :, 0]
    os.remove(tmp)
    out16 = np.full((alto, ancho), 65535, dtype=np.uint16)
    hay = d > 0.5
    out16[hay] = np.clip(np.log2(np.maximum(d[hay], 1.0)) / 12.0 * 65535 + 0.5, 0, 65534).astype(np.uint16)
    print(f"[escenarios] distancias {ancho}x{alto} en {time.time() - t0:.0f} s")
    return out16


def ocultar_al_rayo(objs):
    """lo que no debe entrar al mapa de distancias (llamas, niebla local): se oculta mientras se trazan los rayos"""
    for o in objs:
        o.hide_viewport = True


def renderizar(clave, sc, opciones, muestras_pano=128, muestras_vista=48, sin_distancia=()):
    os.makedirs(OUT, exist_ok=True)
    modo = opciones["modo"]
    if modo == "vista":
        sc.camera = camara_juego(sc)
        if os.environ.get("ESC_CAMARA"):
            # revision: otra camara "x,y,z,mx,my,mz" (juego), con 70 grados
            v = [float(t) for t in os.environ["ESC_CAMARA"].split(",")]
            cam = sc.camera
            cam.data.shift_x = cam.data.shift_y = 0.0
            cam.data.angle = math.radians(70)
            cam.location = B(*v[:3])
            cam.rotation_euler = (B(*v[3:]) - B(*v[:3])).to_track_quat("-Z", "Y").to_euler()
        sc.render.resolution_x, sc.render.resolution_y = 960, 540
        sc.cycles.samples = opciones["muestras"] or muestras_vista
        sc.render.image_settings.file_format = "PNG"
        sc.render.filepath = os.path.join(OUT, f"{clave}_vista.png")
        t0 = time.time()
        bpy.ops.render.render(write_still=True)
        print(f"[escenarios] vista {clave} en {time.time() - t0:.0f} s -> {sc.render.filepath}")
        return
    sc.camera = camara_pano(sc)
    ancho = opciones["ancho"]
    if modo != "prof":   # "prof": solo rehace las distancias y el .txt (el color ya esta)
        sc.render.resolution_x, sc.render.resolution_y = ancho, ancho // 2
        sc.cycles.samples = opciones["muestras"] or muestras_pano
        sc.render.image_settings.file_format = "JPEG"
        sc.render.image_settings.quality = 92
        sc.render.filepath = os.path.join(OUT, f"{clave}.jpg")
        t0 = time.time()
        bpy.ops.render.render(write_still=True)
        print(f"[escenarios] panorama {clave} {ancho}x{ancho // 2} en {time.time() - t0:.0f} s")
    ocultos = list(sin_distancia) + [o for o in bpy.data.objects if o.name.startswith("llama") or o.type == "LIGHT"]
    prof = distancias_render(sc, min(2048, ancho // 2), ocultos)
    _png16(os.path.join(OUT, f"{clave}_prof.png"), prof)
    with open(os.path.join(OUT, f"{clave}.txt"), "w") as f:
        f.write(f"# {clave}: fondo pre-renderizado (escenarios/{clave}.py, Blender + Cycles, insumos CC0)\n")
        f.write("origen %.4f %.4f %.4f\n" % OJO)
        for ll in LLAMAS:
            f.write("llama %.3f %.3f %.3f %.3f\n" % ll)
    print(f"[escenarios] listo {clave}: {len(LLAMAS)} llamas")


# ------------------------------------------------------------------------------------------- piezas comunes
def mat_agua(nombre="agua", color=(0.02, 0.06, 0.08), olas=1.5, aspereza=0.04):
    """agua: casi espejo con olas de ruido (relieve), algo de color en lo profundo"""
    m = bpy.data.materials.new(nombre)
    m.use_nodes = True
    nt = m.node_tree
    b = nt.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*color, 1.0)
    b.inputs["Roughness"].default_value = aspereza
    b.inputs["IOR"].default_value = 1.33
    b.inputs["Specular IOR Level"].default_value = 0.6
    geo = nt.nodes.new("ShaderNodeNewGeometry")
    mp = nt.nodes.new("ShaderNodeMapping")
    mp.inputs["Scale"].default_value = (0.25, 0.6, 1.0)
    nt.links.new(geo.outputs["Position"], mp.inputs["Vector"])
    nz = nt.nodes.new("ShaderNodeTexNoise")
    nz.inputs["Scale"].default_value = olas
    nz.inputs["Detail"].default_value = 8.0
    nz.inputs["Roughness"].default_value = 0.6
    nt.links.new(mp.outputs["Vector"], nz.inputs["Vector"])
    bump = nt.nodes.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.35
    bump.inputs["Distance"].default_value = 0.3
    nt.links.new(nz.outputs["Fac"], bump.inputs["Height"])
    nt.links.new(bump.outputs["Normal"], b.inputs["Normal"])
    return m


def mat_vidrio(nombre="vidrio", color=(0.9, 0.95, 1.0)):
    m = bpy.data.materials.new(nombre)
    m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*color, 1.0)
    b.inputs["Roughness"].default_value = 0.02
    b.inputs["Transmission Weight"].default_value = 1.0
    return m


def mat_emision(nombre, color, fuerza):
    m = bpy.data.materials.new(nombre)
    m.use_nodes = True
    nt = m.node_tree
    nt.nodes.clear()
    em = nt.nodes.new("ShaderNodeEmission")
    em.inputs["Color"].default_value = (*color, 1.0)
    em.inputs["Strength"].default_value = fuerza
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    nt.links.new(em.outputs[0], out.inputs["Surface"])
    return m


def columna(nombre, p, r, alto, mat, capitel=None, rota=0.0, lados=40):
    """columna: basa, fuste (rota: fraccion que falta arriba) y capitel/abaco"""
    x, y, z = p
    caja(nombre + "_basa", (x, y, z), (r * 1.35, r * 0.25, r * 1.35), capitel or mat)
    h = alto * (1.0 - rota)
    cilindro(nombre, (x, y + r * 0.5, z), r, r * 0.88, h - r * 0.5 - (0 if rota else r * 0.8), mat, lados=lados)
    if not rota:
        cilindro(nombre + "_capitel", (x, y + alto - r * 0.8, z), r * 0.88, r * 1.45, r * 0.55, capitel or mat, lados=lados)
        caja(nombre + "_abaco", (x, y + alto - r * 0.25, z), (r * 1.55, r * 0.125, r * 1.55), capitel or mat)


def obelisco(nombre, p, base, alto, mat):
    x, y, z = p
    v = []
    for yy, hb in ((0.0, base), (alto, base * 0.62)):
        for sx, sz in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
            v.append(B(x + sx * hb, y + yy, z + sz * hb))
    v.append(B(x, y + alto + base * 1.1, z))
    f = [(0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7), (4, 5, 8), (5, 6, 8), (6, 7, 8), (7, 4, 8), (3, 2, 1, 0)]
    return _desde_pydata(nombre, v, f, mat)


def mesa_sombra():
    """la mesa (invisible a la camara): su sombra y su oclusion en el suelo del render"""
    o = caja("mesa_sombra", (0, SUELO, 0), (MESA[0], 0.15, MESA[1]), mat_simple("mesa", (0.02, 0.02, 0.02)))
    o.visible_camera = False
    o.visible_glossy = False
    o.visible_shadow = False   # sin sombra del sol (la mesa del juego no la tiene igual): solo su oclusion
    return o


def ocultar(objs):
    for o in objs:
        o.hide_render = o.hide_viewport = True


def modelo(ident, escala=1.0):
    """importa un modelo de Poly Haven, lo deja con la base en 0 y oculto (para copiar con colocar_grupo)"""
    objs = importar_gltf(ident)
    centrar_base(objs)
    ocultar(objs)
    return (objs, escala)


def colocar_grupo(grupo, p, giro_y=0.0, escala=None):
    objs, esc = grupo
    out = []
    e = escala or esc
    c, sn = math.cos(giro_y), math.sin(giro_y)
    for o in objs:
        n = o.copy()
        bpy.context.scene.collection.objects.link(n)
        rel = o.location * e
        n.location = B(*p) + Vector((c * rel.x - sn * rel.y, sn * rel.x + c * rel.y, rel.z))
        n.rotation_euler = (o.rotation_euler.x, o.rotation_euler.y, o.rotation_euler.z + giro_y)
        n.scale = o.scale * e
        n.hide_render = n.hide_viewport = False
        out.append(n)
    return out
