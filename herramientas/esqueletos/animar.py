# Anima un esqueleto de rig_auto.py con clips escritos como poses clave (plantillas por arquetipo en clips/*.py) y
# guarda el .blend con una accion por clip (reposo, ataque, golpe, defensa, muerte...).
# Cada pose clave: tiempo (s) -> {hueso: [(eje, grados), ...], "@loc": [dx, dy, dz]} con ejes y desplazamientos en
# coordenadas glTF del PERSONAJE (x = su izquierda, y = arriba, z = su frente). Los giros son relativos al padre y
# van alrededor de la cabeza del hueso: +x baja lo que apunta al frente (inclinarse adelante), +y gira el frente hacia
# su izquierda, +z sube su lado izquierdo. "@loc" mueve la raiz (Hips). Lo que no se nombra vuelve a 0 (reposo).
# Uso: blender -b -P animar.py -- rig.blend plantilla.py salida.blend [ajustes.json]
import bpy, sys, json, math, runpy
from mathutils import Vector, Quaternion

a = sys.argv[sys.argv.index("--") + 1:]
entrada, plantilla, salida = a[0], a[1], a[2]
bpy.ops.wm.open_mainfile(filepath=entrada)
esc = bpy.context.scene
FPS = 30
esc.render.fps = FPS
arm = next(o for o in esc.objects if o.type == "ARMATURE")
mod = runpy.run_path(plantilla)
CLIPS = mod["CLIPS"]
EJES = {"x": Vector((1, 0, 0)), "y": Vector((0, 0, 1)), "z": Vector((0, -1, 0))}   # glTF -> Blender


def gl2b(v):
    return Vector((v[0], -v[2], v[1]))


def curvas(act):
    """las curvas de una accion (Blender 4.4+: capas/tiras/canales por ranura; antes: act.fcurves)"""
    if hasattr(act, "layers") and len(act.layers):
        for capa in act.layers:
            for tira in capa.strips:
                for cb in tira.channelbags:
                    yield from cb.fcurves
    elif hasattr(act, "fcurves"):
        yield from act.fcurves


raiz = next(b for b in arm.pose.bones if b.parent is None)
for pb in arm.pose.bones:
    pb.rotation_mode = "QUATERNION"
arm.animation_data_create()
for nombre, clip in CLIPS.items():
    act = bpy.data.actions.new(nombre)
    act.use_fake_user = True
    arm.animation_data.action = act
    claves = clip["claves"]
    repetir = clip.get("repetir", False)
    for t in sorted(claves):
        pose = claves[t]
        f = 1 + round(t * FPS)
        for pb in arm.pose.bones:
            D = Quaternion()
            for eje, grados in pose.get(pb.name, []):
                D = Quaternion(EJES[eje], math.radians(grados)) @ D
            Rq = pb.bone.matrix_local.to_quaternion()
            pb.rotation_quaternion = Rq.inverted() @ D @ Rq
            pb.keyframe_insert("rotation_quaternion", frame=f)
            if pb == raiz:
                d = gl2b(pose.get("@loc", [0, 0, 0]))
                pb.location = Rq.inverted() @ d
                pb.keyframe_insert("location", frame=f)
    # curvas: suaves (bezier auto) salvo que el clip pida lineal; los golpes van con "ease" en cada clave
    for fc in curvas(act):
        for k in fc.keyframe_points:
            k.interpolation = clip.get("interp", "BEZIER")
            k.handle_left_type = k.handle_right_type = "AUTO_CLAMPED"
    act["repetir"] = repetir
    print("YUGI clip", nombre, "cuadros", 1, "-", 1 + round(max(claves) * FPS))
arm.animation_data.action = None
bpy.ops.wm.save_as_mainfile(filepath=salida)
print("YUGI animado", salida)
