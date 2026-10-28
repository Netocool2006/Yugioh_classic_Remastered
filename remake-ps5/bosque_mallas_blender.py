# Yugi Reload - carta de campo Bosque: convierte un modelo gltf de Poly Haven (CC0) en una .malla liviana para el
# juego. Lo llama bosque_insumos.py:
#   blender -b --factory-startup --python bosque_mallas_blender.py -- <entrada.gltf> <salida.malla> <triangulos>
# - importa el gltf, aplica transformaciones y reduce (Decimate) hasta ~<triangulos> en total;
# - pasa a Y arriba (x, z, -y); cada objeto (parte) con su base en y = 0 y centrado en x, z;
# - formato .malla (little endian): "MALL", u32 version (1), u32 vertices, u32 indices, u32 partes,
#   por parte u32 primer indice y u32 indices (una parte por objeto, de mayor a menor), luego por vertice
#   pos[3] normal[3] uv[2] (float32) y los indices (u32)
import struct
import sys

import bpy
import bmesh

args = sys.argv[sys.argv.index("--") + 1:]
entrada, salida, meta = args[0], args[1], int(args[2])

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=entrada)
objetos = [o for o in bpy.context.scene.objects if o.type == "MESH"]
total = 0
for o in objetos:
    total += sum(len(p.vertices) - 2 for p in o.data.polygons)
ratio = min(1.0, meta / max(1, total))
print("[malla] %s: %d objetos, %d triangulos -> ratio %.3f" % (entrada, len(objetos), total, ratio))

partes = []
for o in objetos:
    bpy.context.view_layer.objects.active = o
    for s in bpy.context.scene.objects:
        s.select_set(s == o)
    if ratio < 0.999:
        m = o.modifiers.new("reducir", "DECIMATE")
        m.ratio = ratio
        m.use_collapse_triangulate = True
    m = o.modifiers.new("tri", "TRIANGULATE")
    dg = bpy.context.evaluated_depsgraph_get()
    ev = o.evaluated_get(dg)
    malla = ev.to_mesh()
    malla.transform(o.matrix_world)
    uv = malla.uv_layers.active.data if malla.uv_layers.active else None
    verts, idx, mapa = [], [], {}
    try:
        malla.calc_normals_split()
    except Exception:
        pass
    for poli in malla.polygons:
        tri = []
        for li in poli.loop_indices:
            l = malla.loops[li]
            p = malla.vertices[l.vertex_index].co
            n = l.normal if hasattr(l, "normal") else malla.vertices[l.vertex_index].normal
            u = uv[li].uv if uv else (0.0, 0.0)
            clave = (round(p.x, 5), round(p.y, 5), round(p.z, 5), round(n.x, 3), round(n.y, 3), round(n.z, 3), round(u[0], 5), round(u[1], 5))
            if clave not in mapa:
                mapa[clave] = len(verts)
                # Y arriba; la v de la textura al reves (Vulkan: v hacia abajo)
                verts.append((p.x, p.z, -p.y, n.x, n.z, -n.y, u[0], 1.0 - u[1]))
            tri.append(mapa[clave])
        if len(tri) == 3:
            idx.extend(tri)
    ev.to_mesh_clear()
    partes.append((verts, idx))
    print("[malla]   %s: %d vertices, %d triangulos" % (o.name, len(verts), len(idx) // 3))

partes.sort(key=lambda p: -len(p[1]))
# cada parte (objeto) con su base en y = 0 y centrada en x, z: son variantes (rocas, helechos) que se ponen sueltas
todos_v, todos_i, rangos, tams = [], [], [], []
for verts, idx in partes:
    xs = [v[0] for v in verts]
    ys = [v[1] for v in verts]
    zs = [v[2] for v in verts]
    cx, cz, y0 = (min(xs) + max(xs)) / 2, (min(zs) + max(zs)) / 2, min(ys)
    tams.append("%.2fx%.2fx%.2f" % (max(xs) - min(xs), max(ys) - min(ys), max(zs) - min(zs)))
    base = len(todos_v)
    rangos.append((len(todos_i), len(idx)))
    todos_v.extend((v[0] - cx, v[1] - y0, v[2] - cz) + v[3:] for v in verts)
    todos_i.extend(base + i for i in idx)
with open(salida, "wb") as f:
    f.write(b"MALL")
    f.write(struct.pack("<4I", 1, len(todos_v), len(todos_i), len(rangos)))
    for r in rangos:
        f.write(struct.pack("<2I", *r))
    for v in todos_v:
        f.write(struct.pack("<8f", *v))
    f.write(struct.pack("<%dI" % len(todos_i), *todos_i))
print("[malla] %s: %d vertices, %d triangulos, %d partes; tamanos (m) %s" % (
    salida, len(todos_v), len(todos_i) // 3, len(rangos), " ".join(tams)))
