# Caida con RAGDOLL de verdad (fisica de cuerpos rigidos de Blender, Bullet) grabada como clip "muerte".
# Cada hueso que deforma es una capsula con masa (su grosor sale de los vertices que mueve), unida a su padre por una
# articulacion con limites de giro (rodillas, codos, columna...). Arranca en la pose de reposo, recibe un empujon desde
# el frente (el golpe del rival: campo de fuerza de un instante), cae por gravedad sobre la mesa (suelo con friccion)
# y se graba hueso por hueso: el clip reemplaza al "muerte" de la plantilla.
# La gravedad va escalada al tamano que tiene la figura en el juego (2.6 m): una figura de 1 m cayendo con 9.8 m/s2
# se veria a camara rapida.
# Uso: blender -b -P ragdoll.py -- anim.blend esqueleto.json salida.blend [segundos=2.4]
import bpy, sys, json, math
from mathutils import Vector, Matrix

a = sys.argv[sys.argv.index("--") + 1:]
entrada, ruta_json, salida = a[0], a[1], a[2]
segundos = float(a[3]) if len(a) > 3 else 2.4
E = json.load(open(ruta_json, encoding="utf-8"))
R = E.get("ragdoll", {})
bpy.ops.wm.open_mainfile(filepath=entrada)
esc = bpy.context.scene
FPS = 30
arm = next(o for o in esc.objects if o.type == "ARMATURE")
malla = next(o for o in esc.objects if o.type == "MESH")
arm.animation_data.action = None
for pb in arm.pose.bones:
    pb.rotation_quaternion = (1, 0, 0, 0)
    pb.location = (0, 0, 0)
bpy.context.view_layer.update()
pts = [malla.matrix_world @ v.co for v in malla.data.vertices]
alto = max(p.z for p in pts) - min(p.z for p in pts)
ESCALA_JUEGO = R.get("alto_juego", 2.6) / alto

# --- grosor de cada hueso: distancia media de los vertices que mueve sobre todo el
grupos = {g.index: g.name for g in malla.vertex_groups}
suma, cuenta = {}, {}
for v in malla.data.vertices:
    if not v.groups:
        continue
    g = max(v.groups, key=lambda x: x.weight)
    n = grupos[g.group]
    b = arm.data.bones.get(n)
    if not b:
        continue
    h, t = b.head_local, b.tail_local
    ab = t - h
    k = max(0.0, min(1.0, (v.co - h).dot(ab) / max(ab.length_squared, 1e-12)))
    d = (v.co - (h + ab * k)).length
    suma[n] = suma.get(n, 0.0) + d
    cuenta[n] = cuenta.get(n, 0) + 1
huesos = [b for b in arm.data.bones if b.use_deform]
radio = {b.name: max(R.get("radio_min", 0.018), min(R.get("radio_max", 0.09), 0.8 * suma.get(b.name, 0.0) / max(cuenta.get(b.name, 1), 1)))
         for b in huesos}
for n, r in R.get("radios", {}).items():
    radio[n] = r

# --- mundo fisico
if not esc.rigidbody_world:
    bpy.ops.rigidbody.world_add()
mundo = esc.rigidbody_world
mundo.point_cache.frame_start = 1
mundo.point_cache.frame_end = 1 + round(segundos * FPS)
mundo.substeps_per_frame = 20
mundo.solver_iterations = 40
esc.use_gravity = True
esc.gravity = (0, 0, -9.81 / ESCALA_JUEGO)
coleccion = bpy.data.collections.new("ragdoll")
esc.collection.children.link(coleccion)


def nuevo(obj):
    for c in obj.users_collection:
        c.objects.unlink(obj)
    coleccion.objects.link(obj)


bpy.ops.mesh.primitive_plane_add(size=50, location=(0, 0, min(p.z for p in pts)))
suelo = bpy.context.object
nuevo(suelo)
bpy.ops.rigidbody.object_add(type="PASSIVE")
suelo.rigid_body.friction = 0.9
suelo.rigid_body.collision_margin = 0.002
suelo.rigid_body.collision_collections = [True] * 20   # el suelo choca con todos

cuerpos = {}
for b in huesos:
    h, t = arm.matrix_world @ b.head_local, arm.matrix_world @ b.tail_local
    L = max((t - h).length, 0.02)
    r = radio[b.name]
    bpy.ops.mesh.primitive_cylinder_add(radius=r, depth=L + 2 * r, vertices=10, location=(h + t) / 2)
    c = bpy.context.object
    c.name = "rd_" + b.name
    c.rotation_mode = "QUATERNION"
    c.rotation_quaternion = Vector((0, 0, 1)).rotation_difference((t - h).normalized())
    nuevo(c)
    bpy.context.view_layer.update()
    bpy.ops.rigidbody.object_add(type="ACTIVE")
    rb = c.rigid_body
    rb.collision_shape = "CAPSULE"
    rb.mass = max(0.05, math.pi * r * r * L * 1000.0)   # agua: kg/m3 (solo importan las proporciones)
    rb.friction = 0.8
    rb.restitution = 0.05
    rb.linear_damping = R.get("amortigua", 0.25)
    rb.angular_damping = R.get("amortigua_giro", 0.6)
    rb.collision_margin = 0.002
    # cada cuerpo en su propia capa de choques: chocan con el suelo pero no entre ellos (empiezan metidos unos en
    # otros: el brazo en el pecho, la capa en las piernas; chocando entre si la figura salia volando)
    capas = [False] * 20
    capas[1 + len(cuerpos) % 19] = True
    rb.collision_collections = capas
    cuerpos[b.name] = c
bpy.context.view_layer.update()
# huesos sin cuerpo propio (los que no deforman): siguen al cuerpo de su padre
# articulaciones: limites de giro en grados por nombre (final del nombre); "fijo" = soldado (armas)
LIM = {"Hips": 0, "Spine": 22, "Spine1": 22, "Spine2": 22, "Neck": 30, "Head": 35, "Shoulder": 15, "Arm": 75, "ForeArm": 70,
       "Hand": 40, "UpLeg": 60, "Leg": 70, "Foot": 30, "Tail": 35, "Wing": 45, "Arma": -1}
LIM.update(R.get("limites", {}))


def limite(n):
    for k in sorted(LIM, key=len, reverse=True):
        if n.endswith(k) or n.startswith(k):
            return LIM[k]
    return 40


for b in huesos:
    p = b.parent
    while p and p.name not in cuerpos:
        p = p.parent
    if not p:
        continue
    vacio = bpy.data.objects.new("art_" + b.name, None)
    coleccion.objects.link(vacio)
    vacio.location = arm.matrix_world @ b.head_local
    bpy.context.view_layer.objects.active = vacio
    vacio.select_set(True)
    bpy.ops.rigidbody.constraint_add()
    k = vacio.rigid_body_constraint
    g = limite(b.name)
    k.type = "FIXED" if g < 0 else "GENERIC"
    k.object1 = cuerpos[p.name]
    k.object2 = cuerpos[b.name]
    k.disable_collisions = True
    if g >= 0:
        for eje in "xyz":
            setattr(k, "use_limit_lin_" + eje, True)
            setattr(k, "limit_lin_%s_lower" % eje, 0.0)
            setattr(k, "limit_lin_%s_upper" % eje, 0.0)
            setattr(k, "use_limit_ang_" + eje, True)
            setattr(k, "limit_ang_%s_lower" % eje, -math.radians(g))
            setattr(k, "limit_ang_%s_upper" % eje, math.radians(g))
    vacio.select_set(False)

# --- el empujon del golpe: un campo de fuerza delante del pecho que dura un instante (glTF +z = Blender -Y)
emp = R.get("empuje", {})
alto_pecho = min(p.z for p in pts) + emp.get("altura", 0.7) * alto
bpy.ops.object.effector_add(type="FORCE", location=(emp.get("x", 0.0), -emp.get("delante", 0.6) * alto, alto_pecho))
campo = bpy.context.object
nuevo(campo)
campo.field.falloff_type = "SPHERE"
campo.field.falloff_power = 0.0
f0 = emp.get("fuerza", 40.0)
for f, s in ((1, f0), (1 + round(emp.get("dura", 0.12) * FPS), f0), (2 + round(emp.get("dura", 0.12) * FPS), 0.0)):
    campo.field.strength = s
    campo.field.keyframe_insert("strength", frame=f)
mundo.effector_weights.force = 1.0

# --- simular y grabar
offs = {n: c.matrix_world.inverted() @ (arm.matrix_world @ arm.data.bones[n].matrix_local) for n, c in cuerpos.items()}
for b in huesos:
    pass
act = bpy.data.actions.get("muerte")
if act:
    bpy.data.actions.remove(act)
act = bpy.data.actions.new("muerte")
act.use_fake_user = True
arm.animation_data.action = act
orden = []


def recorrer(b):
    orden.append(b)
    for h in b.children:
        recorrer(h)


for b in arm.data.bones:
    if not b.parent:
        recorrer(b)
pbs = {pb.name: pb for pb in arm.pose.bones}
n_cuadros = round(segundos * FPS)
final_bajo = 0.0
for f in range(1, n_cuadros + 2):
    esc.frame_set(f)
    dg = bpy.context.evaluated_depsgraph_get()
    mundos = {}
    for b in orden:
        pb = pbs[b.name]
        if b.name in cuerpos:
            M = cuerpos[b.name].evaluated_get(dg).matrix_world @ offs[b.name]
        else:   # sin cuerpo: sigue a su padre como en reposo
            pm = mundos[b.parent.name]
            M = pm @ (b.parent.matrix_local.inverted() @ b.matrix_local)
        mundos[b.name] = M
        pb.matrix = arm.matrix_world.inverted() @ M
        bpy.context.view_layer.update()
        pb.keyframe_insert("rotation_quaternion", frame=f)
        pb.keyframe_insert("location", frame=f)
    if f % 15 == 1:
        z = [cuerpos[n].evaluated_get(dg).matrix_world.translation.z for n in cuerpos]
        print("YUGI cuadro", f, "altura min/max de los cuerpos %.3f / %.3f" % (min(z), max(z)))
act["repetir"] = False
arm.animation_data.action = None
# fuera la fisica del archivo (el juego solo lee el clip)
esc.frame_set(1)
for o in list(coleccion.objects):
    bpy.data.objects.remove(o, do_unlink=True)
bpy.data.collections.remove(coleccion)
bpy.ops.rigidbody.world_remove()
bpy.ops.wm.save_as_mainfile(filepath=salida)
print("YUGI ragdoll grabado:", n_cuadros + 1, "cuadros ->", salida)
