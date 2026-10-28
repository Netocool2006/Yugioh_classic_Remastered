# Mide la BOQUILLA del arma a distancia de un monstruo (canon, pistola, arco...) en su modelo .glb y la da en
# coordenadas glTF del modelo: el mismo espacio en que lo dibuja el motor (yugireload.cpp carga los monstruos con
# PreTransformVertices = vertices ya con las transformaciones de sus nodos, y luego aplica matrizMonstruo).
# Va en remake-ps5/src/reglas/armas_distancia.h.
#
# Busca los vertices mas alejados en la direccion de disparo (por defecto el frente del modelo, glTF +Z) y toma el
# centro de la franja final (los ultimos "franja" metros del arma). Hace renders de comprobacion (lado, frente y
# arriba) con un marcador rojo en el punto.
#
# Uso: blender --background --python medir_boquilla.py -- modelo.glb salida_prefijo [franja=0.02] [dir=0,0,1]
#      [ymin=-inf] [xabs=inf]   (ymin: ignora vertices por debajo de esa altura glTF, p. ej. para que no gane el
#      casco; xabs: solo vertices con |x - xc| <= xabs, p. ej. para que no ganen los colmillos de los lados) [xc=0]
# Imprime: BOQUILLA <x> <y> <z>   (glTF del modelo)
import bpy, math, sys
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
ruta, prefijo = args[0], args[1]
opc = dict(a.split("=", 1) for a in args[2:])
franja = float(opc.get("franja", "0.02"))
dirg = Vector([float(v) for v in opc.get("dir", "0,0,1").split(",")]).normalized()
ymin = float(opc.get("ymin", "-1e9"))
xabs = float(opc.get("xabs", "1e9"))
xc = float(opc.get("xc", "0"))

bpy.ops.wm.read_factory_settings(use_empty=True)
esc = bpy.context.scene
bpy.ops.import_scene.gltf(filepath=ruta)
dg = bpy.context.evaluated_depsgraph_get()


def a_gltf(p):   # Blender (Z arriba) -> glTF (Y arriba): (x, z, -y)
    return Vector((p.x, p.z, -p.y))


def a_blender(g):
    return Vector((g.x, -g.z, g.y))


puntos = []
for o in esc.objects:
    if o.type != "MESH":
        continue
    ev = o.evaluated_get(dg)
    m = ev.to_mesh()
    puntos += [a_gltf(o.matrix_world @ v.co) for v in m.vertices]
    ev.to_mesh_clear()
lo = Vector((min(p.x for p in puntos), min(p.y for p in puntos), min(p.z for p in puntos)))
hi = Vector((max(p.x for p in puntos), max(p.y for p in puntos), max(p.z for p in puntos)))
print("CAJA glTF min", tuple(round(c, 4) for c in lo), "max", tuple(round(c, 4) for c in hi), "vertices", len(puntos))
cand = [p for p in puntos if p.y >= ymin and abs(p.x - xc) <= xabs]
dmax = max(p.dot(dirg) for p in cand)
fin = [p for p in cand if p.dot(dirg) >= dmax - franja]
c = sum(fin, Vector()) / len(fin)
# el centro del hueco de la boca: punto medio de la caja de la franja (no el promedio, que se carga hacia donde
# haya mas vertices)
flo = Vector((min(p.x for p in fin), min(p.y for p in fin), min(p.z for p in fin)))
fhi = Vector((max(p.x for p in fin), max(p.y for p in fin), max(p.z for p in fin)))
medio = (flo + fhi) / 2
medio = medio + dirg * (dmax - medio.dot(dirg))   # sobre la cara final del arma
print("FRANJA", len(fin), "vertices; caja", tuple(round(v, 4) for v in flo), tuple(round(v, 4) for v in fhi),
      "promedio", tuple(round(v, 4) for v in c))
print("BOQUILLA %.4f %.4f %.4f" % (medio.x, medio.y, medio.z))

# ---- renders de comprobacion con un marcador ----
tam = max(hi - lo)
bpy.ops.mesh.primitive_uv_sphere_add(radius=0.025 * tam, location=a_blender(medio))
marca = bpy.context.active_object
mat = bpy.data.materials.new("marca")
mat.use_nodes = True
nt = mat.node_tree
nt.nodes.clear()
em = nt.nodes.new("ShaderNodeEmission")
em.inputs[0].default_value = (1, 0, 0, 1)
em.inputs[1].default_value = 8.0
out = nt.nodes.new("ShaderNodeOutputMaterial")
nt.links.new(em.outputs[0], out.inputs[0])
marca.data.materials.append(mat)
marca.show_in_front = True

sol = bpy.data.objects.new("sol", bpy.data.lights.new("sol", "SUN"))
sol.data.energy = 3.0
sol.rotation_euler = (math.radians(50), math.radians(10), math.radians(-25))
esc.collection.objects.link(sol)
esc.world = bpy.data.worlds.new("mundo")
esc.world.use_nodes = True
esc.world.node_tree.nodes["Background"].inputs[0].default_value = (0.35, 0.35, 0.4, 1)
esc.render.engine = "BLENDER_EEVEE"
esc.render.resolution_x = 700
esc.render.resolution_y = 600
cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
esc.collection.objects.link(cam)
esc.camera = cam
centro = a_blender((lo + hi) / 2)
vistas = {
    "lado": (Vector((1, 0, 0)), (math.radians(90), 0, math.radians(90))),
    "frente": (Vector((0, -1, 0)), (math.radians(90), 0, 0)),
    "arriba": (Vector((0, 0, 1)), (0, 0, 0)),
    "cerca": (None, None),
}
for nombre, (eje, rot) in vistas.items():
    if eje is None:   # 3/4 en perspectiva, cerca de la boca
        cam.data.type = "PERSP"
        cam.data.lens = 50
        p = a_blender(medio)
        cam.location = p + Vector((0.6, -0.9, 0.45)) * tam * 0.5
        cam.rotation_euler = (p - cam.location).to_track_quat("-Z", "Y").to_euler()
    else:
        cam.data.type = "ORTHO"
        cam.data.ortho_scale = tam * 1.25
        cam.location = centro + eje * tam * 4
        cam.rotation_euler = rot
        cam.data.clip_end = tam * 10
    esc.render.filepath = "%s_%s.png" % (prefijo, nombre)
    bpy.ops.render.render(write_still=True)
print("RENDERS", prefijo)
