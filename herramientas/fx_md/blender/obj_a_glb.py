# Blender --background --python obj_a_glb.py -- <in.obj> <textura.png> <out.glb> [centrar=1] [escala=1]
import bpy,sys
a=sys.argv[sys.argv.index('--')+1:]
obj,tex,out=a[0],a[1],a[2]; centrar=len(a)<4 or a[3]=='1'; esc=float(a[4]) if len(a)>4 else 1.0
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.wm.obj_import(filepath=obj)
o=bpy.context.selected_objects[0]
bpy.context.view_layer.objects.active=o
if centrar:
    bpy.ops.object.origin_set(type='ORIGIN_GEOMETRY',center='BOUNDS'); o.location=(0,0,0)
o.scale=(esc,esc,esc); bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
bpy.ops.object.shade_smooth() if False else None
m=bpy.data.materials.new('mat'); m.use_nodes=True
b=m.node_tree.nodes['Principled BSDF']; t=m.node_tree.nodes.new('ShaderNodeTexImage'); t.image=bpy.data.images.load(tex)
m.node_tree.links.new(t.outputs['Color'],b.inputs['Base Color'])
b.inputs['Roughness'].default_value=0.35
o.data.materials.clear(); o.data.materials.append(m)
bpy.ops.export_scene.gltf(filepath=out,export_format='GLB',export_yup=True,export_apply=True)
print('ok',out,[round(v,3) for v in o.dimensions])
