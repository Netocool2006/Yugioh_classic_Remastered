# Recorta lo que quede por debajo de una altura relativa (restos de pedestal/roca que quitar_base.py no detecta,
# p.ej. una roca angosta bajo una cola ancha, como en Faith Bird), borra las islas sueltas muy chicas que queden
# colgando y apoya el modelo en z=0. Uso:
#   blender --background --python recortar_altura.py -- entrada.glb salida.glb <alto_rel> [min_isla=200]
# alto_rel: fraccion del alto (0.10 = se borra el 10 % de abajo). Mirar despues el resultado en un render.
import bpy, bmesh, sys

a = sys.argv[sys.argv.index("--") + 1:]
entrada, salida, rel = a[0], a[1], float(a[2])
min_isla = int(a[3]) if len(a) > 3 else 200
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=entrada)
ob = next(o for o in bpy.data.objects if o.type == "MESH")
bpy.context.view_layer.objects.active = ob
ob.select_set(True)
bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)  # z = arriba en los datos
bm = bmesh.new()
bm.from_mesh(ob.data)
zs = [v.co.z for v in bm.verts]
lo, alto = min(zs), max(zs) - min(zs)
corte = lo + rel * alto
abajo = [v for v in bm.verts if v.co.z < corte]
bmesh.ops.delete(bm, geom=abajo, context="VERTS")
# islas (componentes conexas) chicas que quedaron sueltas; los vertices en la misma posicion cuentan como uno
# (los modelos de Meshy/TRELLIS suelen venir con la malla partida en las costuras de la textura)
bm.verts.ensure_lookup_table()
padre = list(range(len(bm.verts)))


def raiz(i):
    while padre[i] != i:
        padre[i] = padre[padre[i]]
        i = padre[i]
    return i


def unir(i, j):
    i, j = raiz(i), raiz(j)
    if i != j:
        padre[i] = j


pos = {}
for v in bm.verts:
    k = (round(v.co.x, 5), round(v.co.y, 5), round(v.co.z, 5))
    if k in pos:
        unir(v.index, pos[k])
    else:
        pos[k] = v.index
for e in bm.edges:
    unir(e.verts[0].index, e.verts[1].index)
tam = {}
for v in bm.verts:
    r = raiz(v.index)
    tam[r] = tam.get(r, 0) + 1
quitar = [v for v in bm.verts if tam[raiz(v.index)] < min_isla]
bmesh.ops.delete(bm, geom=quitar, context="VERTS")
bm.to_mesh(ob.data)
bm.free()
zmin = min(v.co.z for v in ob.data.vertices)
for v in ob.data.vertices:
    v.co.z -= zmin
print("RECORTE: %d vertices bajo %.1f%% del alto, %d en islas sueltas" % (len(abajo), 100 * rel, len(quitar)))
bpy.ops.export_scene.gltf(filepath=salida, export_format="GLB", export_yup=True, export_image_format="JPEG",
                          export_jpeg_quality=92)
print("LISTO", salida)
