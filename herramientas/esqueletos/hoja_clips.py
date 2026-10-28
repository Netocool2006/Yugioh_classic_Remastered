# Hoja de cuadros de cada clip de un personaje con esqueleto (.blend de animar.py o .glb final), para revisar en el
# PC: N cuadros por clip vistos de 3/4 frente (y de costado con --lado). Deja carpeta/<clip>_<k>.png y, al final,
# carpeta/hoja_<clip>.png (los cuadros en fila, con el segundo de cada uno).
# Uso: blender -b -P hoja_clips.py -- personaje.(blend|glb) carpeta [n=6] [angulo=35] [clip1,clip2]
import bpy, sys, os, math
from mathutils import Vector

a = sys.argv[sys.argv.index("--") + 1:]
entrada, carpeta = a[0], a[1]
n = int(a[2]) if len(a) > 2 else 6
ang = float(a[3]) if len(a) > 3 else 35.0
solo = a[4].split(",") if len(a) > 4 else None
os.makedirs(carpeta, exist_ok=True)
if entrada.endswith(".blend"):
    bpy.ops.wm.open_mainfile(filepath=entrada)
else:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=entrada)
esc = bpy.context.scene
arm = next(o for o in esc.objects if o.type == "ARMATURE")
mallas = [o for o in esc.objects if o.type == "MESH"]
arm.animation_data_create()
for t in list(arm.animation_data.nla_tracks):
    arm.animation_data.nla_tracks.remove(t)
for o in [o for o in esc.objects if o.type in ("CAMERA", "LIGHT")]:
    bpy.data.objects.remove(o, do_unlink=True)
# encuadre fijo para todos los cuadros: la caja del modelo en reposo, agrandada (caidas y tajos salen de ella)
pts = [o.matrix_world @ v.co for o in mallas for k, v in enumerate(o.data.vertices) if k % 9 == 0]
lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
c = (lo + hi) / 2
R = max(hi - lo) * 1.05
cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
esc.collection.objects.link(cam)
esc.camera = cam
cam.data.type = "ORTHO"
cam.data.ortho_scale = 1.5 * R
d = Vector((math.sin(math.radians(ang)), -math.cos(math.radians(ang)), 0.35)).normalized()
cam.location = Vector((c.x, c.y, lo.z + 0.5 * (hi.z - lo.z))) + d * 10
cam.rotation_euler = (-d).to_track_quat("-Z", "Y").to_euler()
for e, r in ((3.0, (50, 0, -30)), (1.2, (-40, 0, 160))):
    s = bpy.data.objects.new("sol", bpy.data.lights.new("sol", "SUN"))
    s.data.energy = e
    s.rotation_euler = [math.radians(x) for x in r]
    esc.collection.objects.link(s)
# suelo (la mesa) para ver los pies y la caida
bpy.ops.mesh.primitive_plane_add(size=6 * R, location=(c.x, c.y, lo.z))
suelo = bpy.context.object
ms = bpy.data.materials.new("suelo")
ms.diffuse_color = (0.25, 0.3, 0.25, 1)
suelo.data.materials.append(ms)
esc.world = bpy.data.worlds.new("m")
esc.world.color = (0.45, 0.45, 0.5)
esc.render.engine = "BLENDER_EEVEE"
esc.render.resolution_x = esc.render.resolution_y = int(os.environ.get("HOJA_PX", "420"))
esc.view_settings.view_transform = "Standard"
acciones = [x for x in bpy.data.actions if (not solo or x.name in solo)]
for act in acciones:
    arm.animation_data.action = act
    if hasattr(arm.animation_data, "action_slot") and len(getattr(act, "slots", [])):
        arm.animation_data.action_slot = act.slots[0]
    f0, f1 = act.frame_range
    cuadros = [round(f0 + (f1 - f0) * k / max(n - 1, 1)) for k in range(n)]
    if os.environ.get("HOJA_CUADROS"):   # cuadros exactos (1 = el primero), p. ej. HOJA_CUADROS=25,31
        cuadros = [int(x) for x in os.environ["HOJA_CUADROS"].split(",")]
    for k, f in enumerate(cuadros):
        esc.frame_set(int(f))
        esc.render.filepath = os.path.join(carpeta, "%s_%d.png" % (act.name, k))
        bpy.ops.render.render(write_still=True)
    print("HOJA", act.name, f0, f1)
