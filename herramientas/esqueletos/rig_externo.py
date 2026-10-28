# Trae un rig hecho afuera (UniRig en Colab: <nombre>_rig.glb) al formato de rig_auto.py: renombra sus huesos
# (bone_0, bone_1...) a los nombres de las plantillas de clips (Hips, Spine, LeftArm...) con un mapa, y guarda el .blend
# listo para animar.py / ragdoll.py. Los huesos sin nombre en el mapa quedan (pelo, capa) y siguen a su padre.
# Uso: blender -b -P rig_externo.py -- rig.glb mapa.json salida.blend
#   mapa.json: {"bone_0": "Hips", "bone_1": "Spine", ...}
import bpy, sys, json
from mathutils import Matrix

a = sys.argv[sys.argv.index("--") + 1:]
entrada, ruta_mapa, salida = a[0], a[1], a[2]
mapa = json.load(open(ruta_mapa, encoding="utf-8"))
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=entrada)
esc = bpy.context.scene
arm = next(o for o in esc.objects if o.type == "ARMATURE")
malla = next(o for o in esc.objects if o.type == "MESH")
for b in arm.data.bones:
    if b.name in mapa:
        b.name = mapa[b.name]   # (Blender renombra tambien los grupos de vertices)
arm.name = "Armature"
malla.name = "cuerpo"
print("YUGI huesos:", [b.name for b in arm.data.bones])
bpy.ops.wm.save_as_mainfile(filepath=salida)
print("YUGI guardado", salida)
