# Vistas ortograficas para MEDIR un modelo (frente desde glTF +Z y costado desde glTF +X) con una cuadricula en
# coordenadas glTF, para ubicar a mano o revisar las articulaciones del esqueleto (rig_auto.py).
# Si el .glb trae esqueleto, dibuja los huesos encima (lineas rojas, articulaciones amarillas).
# Uso: blender --background --python vistas_medida.py -- modelo.glb salida_prefijo [accion] [cuadro]
#   -> salida_prefijo_frente.png, salida_prefijo_lado.png, salida_prefijo_arriba.png (con cuadricula cada 0.1)
import bpy, math, sys, os, json
from mathutils import Vector

a = sys.argv[sys.argv.index("--") + 1:]
entrada, pref = a[0], a[1]
accion = a[2] if len(a) > 2 else None
cuadro = int(a[3]) if len(a) > 3 else None
bpy.ops.wm.read_factory_settings(use_empty=True)
esc = bpy.context.scene
bpy.ops.import_scene.gltf(filepath=entrada)
mallas = [o for o in esc.objects if o.type == "MESH"]
arm = next((o for o in esc.objects if o.type == "ARMATURE"), None)
if arm and accion:
    act = bpy.data.actions.get(accion)
    if act:
        arm.animation_data_create()
        arm.animation_data.action = act
        if hasattr(arm.animation_data, "action_slot") and len(getattr(act, "slots", [])):
            arm.animation_data.action_slot = act.slots[0]
        for t in list(arm.animation_data.nla_tracks):
            arm.animation_data.nla_tracks.remove(t)
    esc.frame_set(cuadro if cuadro is not None else 1)
bpy.context.view_layer.update()
dg = bpy.context.evaluated_depsgraph_get()
pts = []
for o in mallas:
    ev = o.evaluated_get(dg)
    me = ev.to_mesh()
    pts += [o.matrix_world @ v.co for k, v in enumerate(me.vertices) if k % 7 == 0]
    ev.to_mesh_clear()
lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
c = (lo + hi) / 2
S = max(hi - lo) * 1.25
esc.render.engine = "BLENDER_EEVEE"
esc.render.resolution_x = esc.render.resolution_y = 900
esc.world = bpy.data.worlds.new("m")
esc.world.color = (0.5, 0.5, 0.55)
sol = bpy.data.objects.new("sol", bpy.data.lights.new("sol", "SUN"))
sol.data.energy = 3.0
sol.rotation_euler = (math.radians(40), 0, math.radians(30))
esc.collection.objects.link(sol)
amb = bpy.data.objects.new("amb", bpy.data.lights.new("amb", "SUN"))
amb.data.energy = 1.5
amb.rotation_euler = (math.radians(-60), 0, math.radians(200))
esc.collection.objects.link(amb)
cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
esc.collection.objects.link(cam)
esc.camera = cam
cam.data.type = "ORTHO"
cam.data.ortho_scale = S
cam.data.clip_end = 100
esc.view_settings.view_transform = "Standard"
# huesos: cada uno como un cono fino (rojo) y su cabeza como esfera (amarilla), por encima de la malla
if arm:
    import bmesh
    mr = bpy.data.materials.new("hueso")
    mr.use_nodes = True
    mr.node_tree.nodes["Principled BSDF"].inputs["Emission Color"].default_value = (1, 0.1, 0.1, 1)
    mr.node_tree.nodes["Principled BSDF"].inputs["Emission Strength"].default_value = 3
    my = bpy.data.materials.new("art")
    my.use_nodes = True
    my.node_tree.nodes["Principled BSDF"].inputs["Emission Color"].default_value = (1, 0.9, 0.1, 1)
    my.node_tree.nodes["Principled BSDF"].inputs["Emission Strength"].default_value = 3
    r = S * 0.006
    for pb in arm.pose.bones:
        h = arm.matrix_world @ pb.head
        t = arm.matrix_world @ pb.tail
        bpy.ops.mesh.primitive_uv_sphere_add(radius=r * 1.6, location=h, segments=8, ring_count=6)
        bpy.context.object.data.materials.append(my)
        bpy.context.object.show_in_front = True
        d = t - h
        bpy.ops.mesh.primitive_cone_add(radius1=r, radius2=r * 0.3, depth=d.length, location=(h + t) / 2)
        o = bpy.context.object
        o.rotation_mode = "QUATERNION"
        o.rotation_quaternion = Vector((0, 0, 1)).rotation_difference(d.normalized())
        o.data.materials.append(mr)
    for o in mallas:
        for m in o.data.materials:
            if m and m.node_tree:
                b = m.node_tree.nodes.get("Principled BSDF")
                if b:
                    m.blend_method = "BLEND" if hasattr(m, "blend_method") else None
                    b.inputs["Alpha"].default_value = 0.55
vistas = {
    # nombre: (posicion, rotacion, eje horizontal de la imagen (glTF), eje vertical (glTF))
    "frente": (Vector((c.x, lo.y - 10, c.z)), (math.radians(90), 0, 0), ("x", c.x, 1), ("y", c.z, 1)),
    "lado": (Vector((hi.x + 10, c.y, c.z)), (math.radians(90), 0, math.radians(90)), ("z", -c.y, -1), ("y", c.z, 1)),
    "arriba": (Vector((c.x, c.y, hi.z + 10)), (0, 0, 0), ("x", c.x, 1), ("z", -c.y, -1)),
}
info = {}
for nombre, (pos, rot, hz, vt) in vistas.items():
    cam.location = pos
    cam.rotation_euler = rot
    esc.render.filepath = "%s_%s.png" % (pref, nombre)
    bpy.ops.render.render(write_still=True)
    # pixel (u, v) -> glTF: h = h0 + sh * (u/900 - .5) * S ; v = v0 - (v/900 - .5) * S * sv
    info[nombre] = {"S": S, "h": hz, "v": vt}
json.dump(info, open(pref + "_medida.json", "w"), indent=1)
print("VISTAS", pref)
