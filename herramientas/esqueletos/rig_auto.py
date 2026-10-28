# Esqueleto LOCAL (sin Meshy ni GPU) para una figura de TRELLIS: arma el esqueleto desde un .json de articulaciones
# (coordenadas glTF del modelo: x = izquierda del personaje, y = arriba, z = hacia la camara/frente) y le pone pesos
# automaticos de Blender (calor de huesos). La malla queda IDENTICA a la del modelo (mismas coordenadas: el juego la
# dibuja con la misma matriz que la figura rigida).
#  - Si el calor falla en la malla original (mallas de TRELLIS con agujeros o piezas sueltas), se calcula sobre un
#    "proxy" remallado por voxeles (cerrado) y se pasan los pesos a la original por cercania (Data Transfer).
#  - "rigidos": piezas que NO se deben doblar (armas): los vertices a menos de "radio" del segmento van 100 % al hueso
#    indicado (el arma sigue a la mano entera, nunca se dobla).
#  - "solo": limita un hueso a una region (caja glTF) para que no "robe" vertices lejanos (p. ej. la cola).
# Uso: blender -b -P rig_auto.py -- modelo.glb esqueleto.json salida.blend
import bpy, bmesh, json, sys, math
from mathutils import Vector, Matrix

a = sys.argv[sys.argv.index("--") + 1:]
entrada, ruta_json, salida = a[0], a[1], a[2]
E = json.load(open(ruta_json, encoding="utf-8"))


# El calor de huesos de Blender falla en modelos chicos (1 m): se trabaja todo a escala K y al final se vuelve a 1
K = 10.0


def B(p):   # glTF -> Blender (a escala K)
    return Vector((p[0], -p[2], p[1])) * K


bpy.ops.wm.read_factory_settings(use_empty=True)
esc = bpy.context.scene
bpy.ops.import_scene.gltf(filepath=entrada)
mallas = [o for o in esc.objects if o.type == "MESH"]
# una sola malla (el juego dibuja un nodo con un skin)
bpy.ops.object.select_all(action="DESELECT")
for o in mallas:
    o.select_set(True)
bpy.context.view_layer.objects.active = mallas[0]
if len(mallas) > 1:
    bpy.ops.object.join()
malla = bpy.context.view_layer.objects.active
# sin padres ni transformaciones (los vertices quedan en coordenadas del modelo)
m = malla.matrix_world.copy()
malla.parent = None
malla.matrix_world = m
malla.matrix_world = Matrix.Scale(K, 4) @ malla.matrix_world
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
for o in list(esc.objects):
    if o != malla:
        bpy.data.objects.remove(o, do_unlink=True)
malla.name = "cuerpo"

# --- esqueleto
datos = bpy.data.armatures.new("Esqueleto")
arm = bpy.data.objects.new("Armature", datos)
esc.collection.objects.link(arm)
bpy.context.view_layer.objects.active = arm
bpy.ops.object.mode_set(mode="EDIT")
eb = {}
for h in E["huesos"]:
    b = datos.edit_bones.new(h["n"])
    b.head = B(h["h"])
    b.tail = B(h["t"])
    if h.get("rollo") is not None:
        b.roll = math.radians(h["rollo"])
    eb[h["n"]] = b
for h in E["huesos"]:
    if h.get("p"):
        eb[h["n"]].parent = eb[h["p"]]
        eb[h["n"]].use_connect = False
    eb[h["n"]].use_deform = h.get("deforma", True)
# rollo automatico coherente: el eje z de cada hueso hacia el frente del personaje (glTF +z = Blender -Y)
bpy.ops.armature.select_all(action="SELECT")
bpy.ops.armature.calculate_roll(type="GLOBAL_NEG_Y")
bpy.ops.object.mode_set(mode="OBJECT")

# --- pesos
# "distancia" (por defecto): pesos propios por distancia a cada hueso, normalizada por el grosor de la parte del cuerpo
# que rodea a ese hueso (asi el torso grueso y los brazos finos se reparten bien), con potencia alta (transiciones
# cortas en las articulaciones, sin gelatina) y lados separados (los huesos Left* no tocan el lado -x ni los Right* el
# +x: una pierna no arrastra a la otra). No depende de que la malla sea cerrada (TRELLIS trae mil islas y capas
# dobles: el calor de Blender falla). "calor": el automatico de Blender (con proxy por voxeles si falla).
import numpy as np


def pesos_distancia():
    n = len(malla.data.vertices)
    co = np.empty(n * 3)
    malla.data.vertices.foreach_get("co", co)
    co = co.reshape(n, 3)
    P = np.c_[co[:, 0], co[:, 2], -co[:, 1]] / K   # glTF
    hs = [h for h in E["huesos"] if h.get("deforma", True)]
    D = np.empty((n, len(hs)))
    for j, h in enumerate(hs):
        a0, b0 = np.array(h["h"], float), np.array(h["t"], float)
        ab = b0 - a0
        t = np.clip(((P - a0) @ ab) / max(ab @ ab, 1e-12), 0, 1)
        D[:, j] = np.linalg.norm(P - (a0 + t[:, None] * ab), axis=1)
        nom = h["n"]
        lado = E.get("margen_lado", 0.015)
        if nom.startswith("Left") or h.get("lado") == 1:
            D[P[:, 0] < -lado, j] = np.inf
        if nom.startswith("Right") or h.get("lado") == -1:
            D[P[:, 0] > lado, j] = np.inf
    for r in E.get("solo", []):
        j = [h["n"] for h in hs].index(r["hueso"])
        lo, hi = np.array(r["min"]), np.array(r["max"])
        fuera = ~np.all((P >= lo) & (P <= hi), axis=1)
        D[fuera, j] = np.inf
    # grosor de cada parte: mediana de la distancia de los vertices que tienen a ese hueso como el mas cercano
    cerca = np.argmin(D, axis=1)
    # (percentil 30 y tope: las capas, capas de ropa y piezas lejanas no inflan el grosor de un brazo)
    radio = np.array([np.percentile(D[cerca == j, j], 30) if np.any(cerca == j) else 0.03 for j in range(len(hs))])
    radio = np.clip(radio, E.get("grosor_min", 0.02), E.get("grosor_max", 0.08))
    # lo que esta lejos de todo hueso (capas, colas de ropa, armas a la espalda) va solo con los huesos del tronco
    lejos = E.get("lejos")
    if lejos:
        nombres_h = [h["n"] for h in hs]
        fuera = D.min(1) > lejos["dist"]
        for j, nom in enumerate(nombres_h):
            if nom not in lejos["huesos"]:
                D[fuera, j] = np.inf
    for j, h in enumerate(hs):
        if h.get("radio"):
            radio[j] = h["radio"]
    Dn = D / radio[None, :]
    pot = E.get("potencia", 8.0)
    W = np.where(np.isfinite(Dn), 1.0 / np.maximum(Dn, 1e-3) ** pot, 0.0)
    # 4 huesos por vertice
    orden = np.argsort(-W, axis=1)[:, :4]
    W4 = np.take_along_axis(W, orden, 1)
    W4 /= np.maximum(W4.sum(1, keepdims=True), 1e-12)
    grupos = [malla.vertex_groups.new(name=h["n"]) for h in hs]
    for v in range(n):
        for k in range(4):
            w = W4[v, k]
            if w > 0.01:
                grupos[orden[v, k]].add([v], float(w), "REPLACE")
    print("YUGI pesos por distancia; grosor por hueso:", {h["n"]: round(float(radio[j]), 3) for j, h in enumerate(hs)})


if E.get("pesos", "distancia") == "distancia":
    pesos_distancia()
    malla.parent = arm
    mm = malla.modifiers.new("Armature", "ARMATURE")
    mm.object = arm
else:
    def pesos_calor(obj):
        # los huesos de piezas rigidas (armas) no entran al calor: solo reciben sus vertices "rigidos"
        rig = {r["hueso"] for r in E.get("rigidos", [])}
        for b in datos.bones:
            if b.name in rig:
                b.use_deform = False
        bpy.ops.object.select_all(action="DESELECT")
        obj.select_set(True)
        arm.select_set(True)
        bpy.context.view_layer.objects.active = arm
        bpy.ops.object.parent_set(type="ARMATURE_AUTO")
        for b in datos.bones:
            if b.name in rig:
                b.use_deform = True
        sin = sum(1 for v in obj.data.vertices if not any(x.weight > 1e-4 for x in v.groups))
        return sin


    sin = pesos_calor(malla)
    print("YUGI calor en la malla original: vertices sin peso", sin, "de", len(malla.data.vertices))
    if sin > 0.02 * len(malla.data.vertices) or E.get("proxy"):
        # proxy cerrado por voxeles y transferencia de pesos a la malla original
        for g in list(malla.vertex_groups):
            malla.vertex_groups.remove(g)
        for md in list(malla.modifiers):
            malla.modifiers.remove(md)
        malla.parent = None
        proxy = malla.copy()
        proxy.data = malla.data.copy()
        esc.collection.objects.link(proxy)
        proxy.name = "proxy"
        dims = malla.dimensions
        vox = E.get("voxel", 0.006) * max(dims)
        md = proxy.modifiers.new("vox", "REMESH")
        md.mode = "VOXEL"
        md.voxel_size = vox
        md.use_smooth_shade = False
        bpy.context.view_layer.objects.active = proxy
        bpy.ops.object.modifier_apply(modifier="vox")
        # el calor resuelve un solo sistema para toda la malla: una isla suelta sin hueso a la vista lo deja sin solucion
        # (falla TODO). En el proxy quedan solo las islas grandes; las chicas reciben pesos por cercania igual
        bm = bmesh.new()
        bm.from_mesh(proxy.data)
        bm.verts.ensure_lookup_table()
        visto = set()
        islas = []
        for v in bm.verts:
            if v.index in visto:
                continue
            pila, isla = [v], []
            visto.add(v.index)
            while pila:
                x = pila.pop()
                isla.append(x)
                for e in x.link_edges:
                    o = e.other_vert(x)
                    if o.index not in visto:
                        visto.add(o.index)
                        pila.append(o)
            islas.append(isla)
        islas.sort(key=len, reverse=True)
        minimo = E.get("isla_min", 0.02) * len(bm.verts)
        borrar = [v for isla in islas if len(isla) < minimo for v in isla]
        bmesh.ops.delete(bm, geom=borrar, context="VERTS")
        bm.to_mesh(proxy.data)
        bm.free()
        print("YUGI proxy por voxeles", vox, "->", len(proxy.data.vertices), "vertices;", len(islas), "islas, quedan",
              sum(1 for i in islas if len(i) >= minimo))
        sinp = pesos_calor(proxy)
        print("YUGI calor en el proxy: sin peso", sinp)
        for g in proxy.vertex_groups:
            malla.vertex_groups.new(name=g.name)
        dt = malla.modifiers.new("dt", "DATA_TRANSFER")
        dt.object = proxy
        dt.use_vert_data = True
        dt.data_types_verts = {"VGROUP_WEIGHTS"}
        dt.vert_mapping = "POLYINTERP_NEAREST"
        dt.layers_vgroup_select_src = "ALL"
        dt.layers_vgroup_select_dst = "NAME"
        bpy.context.view_layer.objects.active = malla
        bpy.ops.object.modifier_apply(modifier="dt")
        bpy.data.objects.remove(proxy, do_unlink=True)
        malla.parent = arm
        mm = malla.modifiers.new("Armature", "ARMATURE")
        mm.object = arm


# --- regiones y piezas rigidas (en coordenadas glTF)
idx = {g.name: g.index for g in malla.vertex_groups}
for h in E["huesos"]:
    if h["n"] not in idx and h.get("deforma", True):
        malla.vertex_groups.new(name=h["n"])
idx = {g.name: g.index for g in malla.vertex_groups}
nombres = {g.index: g.name for g in malla.vertex_groups}


def gl(v):
    return Vector((v.x, v.z, -v.y)) / K


def dist_segmento(p, a, b):
    ab = b - a
    t = max(0.0, min(1.0, (p - a).dot(ab) / max(ab.length_squared, 1e-12)))
    return (p - (a + ab * t)).length


for r in E.get("solo", []):
    lo, hi = Vector(r["min"]), Vector(r["max"])
    g = malla.vertex_groups[r["hueso"]]
    for v in malla.data.vertices:
        p = gl(v.co)
        if not all(lo[k] <= p[k] <= hi[k] for k in range(3)):
            g.remove([v.index])
# alcance maximo de cada hueso de extremidad (m desde su segmento, en reposo): el calor del proxy pega el brazo cruzado
# por delante del pecho con el pecho y la capa (al levantar el brazo se estiraban); fuera de ese alcance el peso del
# hueso se quita y el vertice queda con los demas (normalizado). Claves: final del nombre (Arm, ForeArm, UpLeg...)
limite = E.get("limite", {})
if limite:
    hs = {b.name: (gl(b.head_local), gl(b.tail_local)) for b in datos.bones}
    quitar = {}
    for v in malla.data.vertices:
        p = gl(v.co)
        for x in v.groups:
            nom = nombres.get(x.group)
            if nom is None or nom not in hs:
                continue
            lim = next((l for k, l in sorted(limite.items(), key=lambda kv: -len(kv[0])) if nom.endswith(k)), None)
            if lim is not None and dist_segmento(p, *hs[nom]) > lim:
                quitar.setdefault(nom, []).append(v.index)
    for nom, vs in quitar.items():
        malla.vertex_groups[nom].remove(vs)
    print("YUGI fuera de alcance (quitados):", {k: len(v) for k, v in quitar.items()})
rigidos = 0
rig_nombres = {r["hueso"] for r in E.get("rigidos", [])}
for r in E.get("rigidos", []):
    a0, b0 = Vector(r["a"]), Vector(r["b"])
    g = malla.vertex_groups[r["hueso"]]
    caja = r.get("caja")
    for v in malla.data.vertices:
        p = gl(v.co)
        dentro = dist_segmento(p, a0, b0) < r["radio"]
        if caja and not dentro:
            dentro = all(caja[0][k] <= p[k] <= caja[1][k] for k in range(3))
        if dentro:
            for x in list(v.groups):
                malla.vertex_groups[x.group].remove([v.index])
            g.add([v.index], 1.0, "REPLACE")
            rigidos += 1
print("YUGI vertices rigidos (armas):", rigidos)

# TRELLIS funde el arma con lo que toca (capa, mano): los triangulos que unen la pieza rigida con el cuerpo se
# estirarian como una sabana al mover el arma. Se cortan (salvo junto a la mano que la agarra: "agarre", m)
for r in E.get("rigidos", []):
    if not r.get("cortar", True):
        continue
    gi = malla.vertex_groups[r["hueso"]].index
    hb = datos.bones[r["hueso"]]
    mano = hb.parent.head_local if hb.parent else hb.head_local
    agarre = r.get("agarre", 0.07) * K
    bm = bmesh.new()
    bm.from_mesh(malla.data)
    capa = bm.verts.layers.deform.verify()
    rigido = [v[capa].get(gi, 0.0) > 0.99 for v in bm.verts]
    borrar = []
    for f in bm.faces:
        dentro = [rigido[v.index] for v in f.verts]
        if any(dentro) and not all(dentro):
            if all((v.co - mano).length > agarre for v, d in zip(f.verts, dentro) if not d):
                borrar.append(f)
    bmesh.ops.delete(bm, geom=borrar, context="FACES_ONLY")
    bm.to_mesh(malla.data)
    bm.free()
    print("YUGI triangulos cortados entre", r["hueso"], "y el cuerpo:", len(borrar))
# hasta 4 huesos por vertice, normalizados (lo que lee el juego)
bpy.context.view_layer.objects.active = malla
bpy.ops.object.mode_set(mode="WEIGHT_PAINT")
bpy.ops.object.vertex_group_clean(group_select_mode="ALL", limit=0.01)
bpy.ops.object.vertex_group_limit_total(group_select_mode="ALL", limit=4)
bpy.ops.object.vertex_group_normalize_all(group_select_mode="ALL", lock_active=False)
bpy.ops.object.mode_set(mode="OBJECT")
sin = sum(1 for v in malla.data.vertices if not v.groups)
if sin:
    # los que quedaron sin hueso: al hueso mas cercano
    huesos = [(b.name, arm.matrix_world @ b.head_local, arm.matrix_world @ b.tail_local) for b in datos.bones if b.use_deform]
    for v in malla.data.vertices:
        if not v.groups:
            def costo(h):
                d = dist_segmento(v.co, h[1], h[2]) / K
                lim = next((l for k, l in sorted(limite.items(), key=lambda kv: -len(kv[0])) if h[0].endswith(k)), None)
                return d + (10.0 if lim is not None and d > lim else 0.0) + (10.0 if h[0] in rig_nombres else 0.0)
            n = min(huesos, key=costo)[0]
            malla.vertex_groups[n].add([v.index], 1.0, "REPLACE")
print("YUGI sin peso al final (asignados al hueso mas cercano):", sin)
cuenta = {}
for v in malla.data.vertices:
    for x in v.groups:
        if x.weight > 0.5:
            cuenta[nombres[x.group]] = cuenta.get(nombres[x.group], 0) + 1
print("YUGI vertices por hueso:", json.dumps(dict(sorted(cuenta.items()))))
# de vuelta a la escala del modelo (1/K), sin transformaciones en los objetos
malla.parent = None
for o in (malla, arm):
    o.matrix_world = Matrix.Scale(1.0 / K, 4) @ o.matrix_world
    bpy.ops.object.select_all(action="DESELECT")
    o.select_set(True)
    bpy.context.view_layer.objects.active = o
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
malla.parent = arm
malla.matrix_parent_inverse = Matrix.Identity(4)
bpy.ops.wm.save_as_mainfile(filepath=salida)
print("YUGI guardado", salida)
