# Paso "orientar de frente" del ciclo de produccion de monstruos 3D (despues de quitar la base y antes de copiar
# el modelo al juego). En el tablero TODO monstruo debe mirar a la camara (frente = glTF +Z = Blender -Y), aunque
# en el arte de la carta venga de lado (p.ej. Silver Fang).
#  1) PROPONE un giro (multiplo de 90 grados) midiendo hacia donde esta la cabeza: el centro de la parte alta de la
#     figura (ultimo 30 % del alto) respecto del centro de todo el cuerpo, en planta (si pasa del 12 % del tamano). En cuadrupedos de lado eso es
#     claro (la cabeza esta en una punta del cuerpo); si el desplazamiento es chico frente al tamano de la figura
#     se mira ademas el lado mas ancho de la silueta: el ancho debe quedar de lado a lado (eje X), no en profundidad.
#  2) RENDERIZA 4 vistas en una tira: la figura girada 0, 90, 180 y 270 grados (cada una con su numero debajo), con
#     la camara del juego (de frente y un poco desde arriba). La vista donde la cara mira a la camara dice cuantos
#     grados hay que pasarle a girar_modelo.py. La decision final la toma una persona mirando el render.
#  3) Deja constancia en <salida>.txt (propuesta, medidas, y despues la decision tomada).
# Uso: blender --background --python orientar_frente.py -- modelo.glb salida.png [constancia.txt]
#   luego, si hace falta: blender --background --python girar_modelo.py -- modelo.glb modelo.glb <grados>
import bpy, math, sys, os
from mathutils import Vector, Matrix

args = sys.argv[sys.argv.index("--") + 1:]
entrada, salida = args[0], args[1]
txt = args[2] if len(args) > 2 else os.path.splitext(salida)[0] + ".txt"

bpy.ops.wm.read_factory_settings(use_empty=True)
esc = bpy.context.scene
bpy.ops.import_scene.gltf(filepath=entrada)
mallas = [o for o in esc.objects if o.type == "MESH"]
bpy.context.view_layer.update()
pts = [o.matrix_world @ v.co for o in mallas for v in o.data.vertices]
if len(pts) > 200000:
    pts = pts[::len(pts) // 200000]
zlo, zhi = min(p.z for p in pts), max(p.z for p in pts)
alto = zhi - zlo
cx = sum(p.x for p in pts) / len(pts)
cy = sum(p.y for p in pts) / len(pts)
arriba = [p for p in pts if p.z > zlo + 0.7 * alto]
hx = sum(p.x for p in arriba) / len(arriba) - cx
hy = sum(p.y for p in arriba) / len(arriba) - cy
anx = max(p.x for p in pts) - min(p.x for p in pts)
any_ = max(p.y for p in pts) - min(p.y for p in pts)
tam = max(anx, any_, 1e-6)
desp = math.hypot(hx, hy) / tam
# giro que lleva la direccion de la cabeza a Blender -Y (glTF +Z); rotacion positiva = antihoraria vista desde arriba
if desp > 0.12:
    theta = math.degrees(math.atan2(hy, hx))
    propuesta = (round((-90 - theta) / 90) * 90) % 360
    motivo = "cabeza desplazada %.0f%% del tamano hacia %.0f grados (planta Blender)" % (100 * desp, theta)
elif any_ > anx * 1.25:
    propuesta = 90
    motivo = "cabeza centrada; silueta mas profunda (%.2f) que ancha (%.2f): de lado -> 90 (o 270)" % (any_, anx)
else:
    propuesta = 0
    motivo = "cabeza centrada y silueta ancha de lado a lado: ya de frente"

# --- 4 vistas: copias giradas lado a lado ---
s = 2.0 / max(alto, 1e-6)
for o in mallas:  # sin jerarquia: cada malla con su transformacion de mundo
    w = o.matrix_world.copy()
    o.parent = None
    o.matrix_world = w
grupos = [mallas]
for k in range(3):  # copias (misma malla, sin duplicar memoria) ANTES de mover el original
    grupo = []
    for o in mallas:
        n = o.copy()
        esc.collection.objects.link(n)
        grupo.append(n)
    grupos.append(grupo)
for k, grados in enumerate((0, 90, 180, 270)):
    grupo = grupos[k]
    giro = Matrix.Rotation(math.radians(grados), 4, "Z")
    mover = Matrix.Translation(Vector((k * 2.8, 0, 0))) @ Matrix.Scale(s, 4) @ giro @ \
        Matrix.Translation(Vector((-cx, -cy, -zlo)))
    for o in grupo:
        o.matrix_world = mover @ o.matrix_world
    tx = bpy.data.objects.new("t%d" % grados, bpy.data.curves.new("t%d" % grados, "FONT"))
    tx.data.body = str(grados)
    tx.data.size = 0.35
    tx.data.align_x = "CENTER"
    tx.location = (k * 2.8, -1.6, -0.45)
    tx.rotation_euler = (math.radians(90), 0, 0)
    esc.collection.objects.link(tx)
cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
esc.collection.objects.link(cam)
cam.data.type = "ORTHO"
cam.data.ortho_scale = 11.6
cam.location = (4.2, -12, 4.0)
cam.rotation_euler = (math.radians(75), 0, 0)
esc.camera = cam
sol = bpy.data.objects.new("sol", bpy.data.lights.new("sol", "SUN"))
sol.data.energy = 3.0
sol.rotation_euler = (math.radians(50), math.radians(10), math.radians(-25))
esc.collection.objects.link(sol)
esc.world = bpy.data.worlds.new("mundo")
esc.world.use_nodes = True
esc.world.node_tree.nodes["Background"].inputs[0].default_value = (0.35, 0.35, 0.4, 1)
esc.render.engine = "BLENDER_EEVEE"
esc.render.resolution_x, esc.render.resolution_y = 1800, 560
esc.render.filepath = salida
bpy.ops.render.render(write_still=True)

with open(txt, "a", encoding="utf-8") as f:
    f.write("%s\n  propuesta: girar %d grados (%s)\n  medidas: alto %.3f, ancho X %.3f, fondo Y %.3f\n"
            "  vistas: %s (0/90/180/270 = grados para girar_modelo.py)\n" %
            (entrada, propuesta, motivo, alto, anx, any_, salida))
print("PROPUESTA", propuesta, motivo)
