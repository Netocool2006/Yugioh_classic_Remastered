# Retarget de animaciones de captura real (Quaternius Universal Animation Library 1 y 2, CC0) al esqueleto de
# rig_auto.py (nombres estilo Mixamo + "Arma" hija de RightHand). Guarda el .blend con una accion por clip (reposo,
# ataque, golpe, defensa, muerte) listo para exportar.py.
# Metodo (por hueso, en el mundo): giro_destino(t) = giro_fuente(t) * giro_fuente_reposo^-1 * C * giro_destino_reposo
#  - brazo izquierdo sin mapear: Neo lo tiene levantado con la magia azul (su pose de la carta); llevarlo a la
#    direccion de la captura lo deforma, asi que sigue rigido al pecho
#  - claviculas (Left/RightShoulder) sin mapear: quedan fijas al pecho (la capa y las hombreras no se estiran; el
#    brazo igual toma su giro del mundo)
#  - cadera, columna, cuello y brazos/piernas: C lleva la direccion del hueso destino (cabeza -> cola) a la del hueso fuente en su pose T, asi
#    el brazo sigue la DIRECCION de la captura aunque la figura este modelada con los brazos en otra pose
#  - mano derecha: C alinea dos ejes (antebrazo -> mano y la hoja del arma) con los de la mano fuente (muneca -> dedo
#    medio y menique -> indice: por donde sale la hoja en un puno cerrado), asi la espada queda empunada como en la
#    captura. La espada ("Arma") sigue rigida a la mano.
#  - "frente" (0..1 por clip): gira todo sobre el eje vertical esa fraccion de lo que haga falta para que el pecho
#    mire al frente en el primer cuadro (la cadera; las guardias de espada van de perfil; en la mesa debe mirar al rival)
#  - cadera: ademas se mueve lo que se mueve la cadera fuente, escalado por la altura de cadera.
# El tiempo de cada clip se remapea con tramos (fuente -> destino) para que el golpe del ataque caiga en 1.0 s.
# Uso: blender -b -P retarget_quaternius.py -- rig.blend ual1.glb ual2.glb clips.json salida.blend
import bpy, sys, json
from mathutils import Vector, Quaternion, Matrix

a = sys.argv[sys.argv.index("--") + 1:]
entrada, glb1, glb2, ruta_clips, salida = a[:5]
CLIPS = json.load(open(ruta_clips, encoding="utf-8"))
bpy.ops.wm.open_mainfile(filepath=entrada)
esc = bpy.context.scene
FPS = 30
esc.render.fps = FPS
esc.render.fps_base = 1.0
tgt = next(o for o in esc.objects if o.type == "ARMATURE")
tgt.animation_data_create()
for act in list(bpy.data.actions):
    bpy.data.actions.remove(act)

# mapas: hueso destino -> (hueso fuente, hueso fuente donde termina su direccion)
MAPA = {
    "ual1": {"Hips": ("DEF-hips", "DEF-spine.002"), "Spine": ("DEF-spine.002", "DEF-spine.003"), "Spine1": ("DEF-spine.003", "DEF-neck"),
             "Neck": ("DEF-neck", "DEF-head"), "Head": ("DEF-head", "=Neck"),
             "RightArm": ("DEF-upper_arm.R", "DEF-forearm.R"),
             "RightForeArm": ("DEF-forearm.R", "DEF-hand.R"), "RightHand": ("DEF-hand.R", "DEF-f_middle.01.R"),
             "LeftUpLeg": ("DEF-thigh.L", "DEF-shin.L"), "LeftLeg": ("DEF-shin.L", "DEF-foot.L"), "LeftFoot": ("DEF-foot.L", "DEF-toe.L"),
             "RightUpLeg": ("DEF-thigh.R", "DEF-shin.R"), "RightLeg": ("DEF-shin.R", "DEF-foot.R"), "RightFoot": ("DEF-foot.R", "DEF-toe.R")},
    "ual2": {"Hips": ("pelvis", "spine_02"), "Spine": ("spine_02", "spine_03"), "Spine1": ("spine_03", "neck_01"),
             "Neck": ("neck_01", "Head"), "Head": ("Head", "=Neck"),
             "RightArm": ("upperarm_r", "lowerarm_r"),
             "RightForeArm": ("lowerarm_r", "hand_r"), "RightHand": ("hand_r", "middle_01_r"),
             "LeftUpLeg": ("thigh_l", "calf_l"), "LeftLeg": ("calf_l", "foot_l"), "LeftFoot": ("foot_l", "ball_l"),
             "RightUpLeg": ("thigh_r", "calf_r"), "RightLeg": ("calf_r", "foot_r"), "RightFoot": ("foot_r", "ball_r")},
}
HOJA_MANO = {"ual1": ("DEF-f_pinky.01.R", "DEF-f_index.01.R"), "ual2": ("pinky_01_r", "index_01_r")}


def importar(ruta, clave):
    antes = set(bpy.data.objects)
    acts0 = set(bpy.data.actions)
    bpy.ops.import_scene.gltf(filepath=ruta)
    nuevos = [o for o in bpy.data.objects if o not in antes]
    arm = next(o for o in nuevos if o.type == "ARMATURE")
    for o in nuevos:
        if o.type == "MESH":
            bpy.data.objects.remove(o, do_unlink=True)
    acts = {x.name: x for x in bpy.data.actions if x not in acts0}
    arm.name = "fuente_" + clave
    arm.animation_data_create()
    for t in list(arm.animation_data.nla_tracks):
        arm.animation_data.nla_tracks.remove(t)
    return arm, acts


FUENTES = {"ual1": importar(glb1, "ual1"), "ual2": importar(glb2, "ual2")}
# (los nombres de accion del importador pueden venir con sufijo del objeto: se busca por prefijo)


def accion(clave, nombre):
    acts = FUENTES[clave][1]
    if nombre in acts:
        return acts[nombre]
    for k, v in acts.items():
        if k.split("_Rig")[0] == nombre or k.startswith(nombre + "_") and not k[len(nombre) + 1:].startswith(("RM", "Rec", "Loop")):
            return v
    sys.exit("no hay accion %s en %s: %s" % (nombre, clave, sorted(acts)))


def mundo_rest(arm, nombre):
    return arm.matrix_world @ arm.data.bones[nombre].matrix_local


def cabeza_rest(arm, nombre):
    return (arm.matrix_world @ arm.data.bones[nombre].head_local.to_4d()).to_3d()


def giro_entre(u, v):
    return u.normalized().rotation_difference(v.normalized())


def marco(x, y):
    """matriz 3x3 con eje1 = x y eje2 = y ortogonalizado"""
    x = x.normalized()
    y = (y - x * x.dot(y)).normalized()
    z = x.cross(y)
    return Matrix((x, y, z)).transposed()


Tw = tgt.matrix_world
Tw_q = Tw.to_quaternion()
huesos_t = [b for b in tgt.data.bones]   # (orden de la jerarquia: padres antes)


def correcciones(clave):
    src = FUENTES[clave][0]
    M = MAPA[clave]
    C = {}
    for tn, (sn, fin) in M.items():
        if fin is None:
            C[tn] = Quaternion()
            continue
        if fin.startswith("="):   # (la cabeza: misma correccion que el cuello, conserva su giro sobre el cuello)
            C[tn] = C[fin[1:]]
            continue
        tb = tgt.data.bones[tn]
        dt = (Tw @ tb.tail_local.to_4d()).to_3d() - (Tw @ tb.head_local.to_4d()).to_3d()
        ds = cabeza_rest(src, fin) - cabeza_rest(src, sn)
        if tn == "RightHand" and "Arma" in tgt.data.bones:
            ab = tgt.data.bones["Arma"]
            hoja_t = (Tw @ ab.tail_local.to_4d()).to_3d() - (Tw @ ab.head_local.to_4d()).to_3d()
            # eje de la mano destino: del codo a la muneca (el hueso mano de rig_auto es muy corto)
            fa = tgt.data.bones["RightForeArm"]
            dt = (Tw @ fa.tail_local.to_4d()).to_3d() - (Tw @ fa.head_local.to_4d()).to_3d()
            p, i = HOJA_MANO[clave]
            hoja_s = cabeza_rest(src, i) - cabeza_rest(src, p)
            Ft, Fs = marco(dt, hoja_t), marco(ds, hoja_s)
            C[tn] = (Fs @ Ft.inverted()).to_quaternion()
        else:
            C[tn] = giro_entre(dt, ds)
    return C


CORR = {k: correcciones(k) for k in FUENTES}
ALTURA_T = cabeza_rest(tgt, "Hips").z - min((Tw @ b.head_local.to_4d()).z for b in tgt.data.bones)


def escala_cadera(clave):
    src = FUENTES[clave][0]
    h = MAPA[clave]["Hips"][0]
    pies = [MAPA[clave][x][0] for x in ("LeftFoot", "RightFoot")]
    return ALTURA_T / (cabeza_rest(src, h).z - min(cabeza_rest(src, p).z for p in pies))


RZ = Quaternion()


def pose_destino(clave):
    """lee la pose actual de la fuente y devuelve {hueso destino: matriz pose (espacio armadura)}"""
    src = FUENTES[clave][0]
    M = MAPA[clave]
    C = CORR[clave]
    Sw = src.matrix_world
    out = {}
    for b in huesos_t:
        pm = out[b.parent.name] if b.parent else Matrix.Identity(4)
        rel = (b.parent.matrix_local.inverted() @ b.matrix_local) if b.parent else b.matrix_local
        base = pm @ rel
        if b.name not in M:
            out[b.name] = base
            continue
        sn = M[b.name][0]
        qS = (Sw @ src.pose.bones[sn].matrix).to_quaternion()
        qSr = mundo_rest(src, sn).to_quaternion()
        qTr = (Tw @ b.matrix_local).to_quaternion()
        qT = RZ @ qS @ qSr.inverted() @ C[b.name] @ qTr
        qA = Tw_q.inverted() @ qT   # al espacio de la armadura destino
        pos = base.to_translation()
        if b.parent is None:
            d = (Sw @ src.pose.bones[sn].head.to_4d()).to_3d() - cabeza_rest(src, sn)
            d = RZ @ (d * escala_cadera(clave))
            pos = (Tw.inverted() @ (cabeza_rest(tgt, b.name) + d).to_4d()).to_3d()
        out[b.name] = Matrix.Translation(pos) @ qA.to_matrix().to_4x4()
    return out


def poner(clave, act, t_seg):
    src = FUENTES[clave][0]
    src.animation_data.action = act
    if hasattr(src.animation_data, "action_slot") and len(getattr(act, "slots", [])):
        src.animation_data.action_slot = act.slots[0]
    f = act.frame_range[0] + t_seg * FPS
    esc.frame_set(int(f), subframe=f - int(f))


def remapear(tramos, t):
    """tramos: [[t_destino, t_fuente], ...] crecientes; lineal entre ellos"""
    if t <= tramos[0][0]:
        return tramos[0][1]
    for (d0, s0), (d1, s1) in zip(tramos, tramos[1:]):
        if t <= d1:
            return s0 + (s1 - s0) * (t - d0) / (d1 - d0)
    return tramos[-1][1]


for pb in tgt.pose.bones:
    pb.rotation_mode = "QUATERNION"
for nombre, cl in CLIPS.items():
    clave = cl["fuente"]
    act_s = accion(clave, cl["clip"])
    largo_s = (act_s.frame_range[1] - act_s.frame_range[0]) / FPS
    tramos = cl.get("tramos") or [[0.0, 0.0], [largo_s, largo_s]]
    dur = tramos[-1][0]
    RZ = Quaternion()
    if cl.get("frente"):
        poner(clave, act_s, remapear(tramos, 0.0))
        src = FUENTES[clave][0]
        sn = MAPA[clave]["Hips"][0]
        q = (src.matrix_world @ src.pose.bones[sn].matrix).to_quaternion() @ mundo_rest(src, sn).to_quaternion().inverted()
        f = q @ Vector((0, -1, 0))
        yaw = Vector((f.x, f.y)).angle_signed(Vector((0, -1)))
        RZ = Quaternion((0, 0, 1), -yaw * cl["frente"])
        print("YUGI", nombre, "pecho a %.0f grados, giro %.0f" % (-yaw * 57.3, -yaw * cl["frente"] * 57.3))
    act = bpy.data.actions.new(nombre)
    act.use_fake_user = True
    tgt.animation_data.action = act
    previo = {}
    nf = int(round(dur * FPS))
    for k in range(nf + 1):
        ts = remapear(tramos, k / FPS)
        poner(clave, act_s, ts)
        P = pose_destino(clave)
        for b in huesos_t:
            pb = tgt.pose.bones[b.name]
            pm = P[b.parent.name] if b.parent else Matrix.Identity(4)
            rel = (b.parent.matrix_local.inverted() @ b.matrix_local) if b.parent else b.matrix_local
            basis = (pm @ rel).inverted() @ P[b.name]
            loc, q, _ = basis.decompose()
            if b.name in previo and previo[b.name].dot(q) < 0:   # (sin saltos de signo entre cuadros)
                q.negate()
            previo[b.name] = q.copy()
            pb.rotation_quaternion = q
            pb.keyframe_insert("rotation_quaternion", frame=1 + k)
            if b.parent is None:
                pb.location = loc
                pb.keyframe_insert("location", frame=1 + k)
            else:
                pb.location = (0, 0, 0)
    act["repetir"] = bool(cl.get("repetir", False))
    print("YUGI clip", nombre, "<-", clave, act_s.name, "fuente %.2f s" % largo_s, "destino %.2f s" % dur, "cuadros", nf + 1)
tgt.animation_data.action = None
for clave, (src, acts) in FUENTES.items():
    for x in acts.values():
        bpy.data.actions.remove(x)
    bpy.data.objects.remove(src, do_unlink=True)
bpy.ops.wm.save_as_mainfile(filepath=salida)
print("YUGI retarget", salida)
