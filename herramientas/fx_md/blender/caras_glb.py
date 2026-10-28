import bpy,sys
a=sys.argv[sys.argv.index('--')+1:]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=a[0])
o=[x for x in bpy.context.scene.objects if x.type=='MESH'][0]
me=o.data; uv=me.uv_layers.active.data
import collections
acc=collections.defaultdict(list)
for p in me.polygons:
    n=p.normal
    # Blender Z arriba = glTF Y; glTF Z = -Blender Y
    g=(round(n.x),round(n.z),round(-n.y))
    if abs(n.x)+abs(n.y)+abs(n.z)>0 and max(abs(n.x),abs(n.y),abs(n.z))>0.95:
        for li in p.loop_indices: acc[g].append(tuple(uv[li].uv))
for g,l in acc.items():
    u=sum(x for x,_ in l)/len(l); v=sum(y for _,y in l)/len(l)
    print('cara glTF',g,'uv',round(u,2),round(v,2),'celda',int(u*3)+1+3*int((1-v)*3))
