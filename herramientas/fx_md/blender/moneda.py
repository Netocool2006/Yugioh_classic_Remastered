# Moneda: cilindro (radio 0.5, canto 0.07) con la cara del icono de Master Duel adelante (+Z en glTF) y la cruz atras
import bpy,sys,math
a=sys.argv[sys.argv.index('--')+1:]; cara,cruz,out=a
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.mesh.primitive_cylinder_add(vertices=64,radius=0.5,depth=0.07)
o=bpy.context.active_object
# en Blender el eje del cilindro es Z; glTF (Y arriba) convierte Z de Blender en Y: girar para que el eje quede en -Y de Blender (=+Z glTF)
o.rotation_euler=(math.radians(90),0,0); bpy.ops.object.transform_apply(rotation=True)
me=o.data
import bmesh
bm=bmesh.new(); bm.from_mesh(me); uv=bm.loops.layers.uv.new()
for f in bm.faces:
    n=f.normal
    for l in f.loops:
        x,y,z=l.vert.co
        if abs(n.y)>0.9:
            u=x+0.5; v=z+0.5
            if n.y>0: u=1-u
            l[uv].uv=(u,v)
        else:
            l[uv].uv=(0.5,0.5)
    f.material_index=0 if n.y<-0.9 else (1 if n.y>0.9 else 2)
bm.to_mesh(me); bm.free()
def mat(nombre,img=None,col=None):
    m=bpy.data.materials.new(nombre); m.use_nodes=True; b=m.node_tree.nodes['Principled BSDF']
    if img:
        t=m.node_tree.nodes.new('ShaderNodeTexImage'); t.image=bpy.data.images.load(img); m.node_tree.links.new(t.outputs['Color'],b.inputs['Base Color'])
    else: b.inputs['Base Color'].default_value=col
    b.inputs['Metallic'].default_value=0.0; b.inputs['Roughness'].default_value=0.4
    return m
me.materials.append(mat('cara',cara)); me.materials.append(mat('cruz',cruz)); me.materials.append(mat('canto',col=(0.85,0.65,0.2,1)))
bpy.ops.export_scene.gltf(filepath=out,export_format='GLB',export_yup=True)
print('ok',out)
