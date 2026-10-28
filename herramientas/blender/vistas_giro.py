# Renderiza un modelo .glb desde la camara frontal del juego (frente = glTF +Z = Blender -Y, un poco desde arriba)
# girado N veces alrededor del eje vertical (0, 360/N, ...). Imagenes planas (color de la textura, sin luz, fondo
# transparente) para que atributos.py las compare con la figura de Gemini (siempre de frente) y elija el giro.
# Uso: blender --background --python vistas_giro.py -- modelo.glb carpeta_salida [N=8] [px=192]
import bpy, math, sys, os
from mathutils import Vector, Matrix

a = sys.argv[sys.argv.index("--") + 1:]
entrada, salida = a[0], a[1]
n = int(a[2]) if len(a) > 2 else 8
px = int(a[3]) if len(a) > 3 else 192
os.makedirs(salida, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
esc = bpy.context.scene
bpy.ops.import_scene.gltf(filepath=entrada)
raices = [o for o in esc.objects if o.parent is None]
mallas = [o for o in esc.objects if o.type == "MESH"]
bpy.context.view_layer.update()
pts = [o.matrix_world @ Vector(c) for o in mallas for c in o.bound_box]
lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
centro = (lo + hi) / 2
radio = (hi - lo).length / 2
pivote = bpy.data.objects.new("pivote", None)
esc.collection.objects.link(pivote)
pivote.location = (centro.x, centro.y, 0)
for o in raices:
    m = o.matrix_world.copy()
    o.parent = pivote
    o.matrix_world = m
cam_d = bpy.data.cameras.new("cam")
cam_d.type = "ORTHO"
cam_d.ortho_scale = 2.05 * radio
cam = bpy.data.objects.new("cam", cam_d)
esc.collection.objects.link(cam)
elev = math.radians(12)
cam.location = centro + Vector((0, -math.cos(elev), math.sin(elev))) * radio * 4
cam.rotation_euler = (math.pi / 2 - elev, 0, 0)
esc.camera = cam
esc.render.engine = "BLENDER_WORKBENCH"
esc.display.shading.light = "FLAT"
esc.display.shading.color_type = "TEXTURE"
esc.render.film_transparent = True
esc.render.resolution_x = esc.render.resolution_y = px
esc.render.image_settings.file_format = "PNG"
esc.render.image_settings.color_mode = "RGBA"
esc.view_settings.view_transform = "Standard"
for k in range(n):
    grados = 360.0 * k / n
    pivote.rotation_euler = (0, 0, math.radians(grados))
    esc.render.filepath = os.path.join(salida, "giro_%03d.png" % round(grados))
    bpy.ops.render.render(write_still=True)
print("VISTAS", n, salida)
