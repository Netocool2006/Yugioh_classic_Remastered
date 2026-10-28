# Vuelca prefabs de UI del duelo de Master Duel (resources.assets dentro de data.unity3d) con los MonoBehaviour
# leidos gracias a TypeTreeGeneratorAPI (GameAssembly.dll + global-metadata.dat), mas sprites/texturas y clips.
# Uso: volcar_ui.py <salida> <nombre_GameObject|path_id> ...      (no guarda nada en el repo: salida fuera)
import sys,os,json
import UnityPy
from UnityPy.helpers.TypeTreeGenerator import TypeTreeGenerator
JUEGO=r'D:/Steam/steamapps/common/Yu-Gi-Oh!  Master Duel'
DUMMY=r'D:/Ghidra-Reload/herramientas/il2cppdumper/DummyDll'
out=sys.argv[1]; os.makedirs(out,exist_ok=True)
env=UnityPy.load(os.path.join(JUEGO,'masterduel_Data','data.unity3d'))
ver=None
for f in env.files.values():
    for sf in getattr(f,'files',{}).values():
        if hasattr(sf,'unity_version'): ver=sf.unity_version; break
    if ver: break
print('unity',ver,flush=True)
gen=TypeTreeGenerator(ver); gen.load_local_dll_folder(DUMMY); env.typetree_generator=gen  # DummyDll de Il2CppDumper (load_local_game falla con metadata v31)
objs={}
for o in env.objects:
    if o.assets_file.name=='resources.assets': objs[o.path_id]=o
def nom(o):
    try: return o.peek_name()
    except Exception: return '?'
def leer(o):
    try: return o.read_typetree()
    except Exception as e: return {'error':repr(e)[:160]}
sprites={}
def guardar_ref(pid):
    o=objs.get(pid)
    if not o or pid in sprites: return
    if o.type.name in('Sprite','Texture2D'):
        try:
            d=o.read(); n=f'{d.m_Name}_{pid}'
            d.image.save(os.path.join(out,n+'.png')); sprites[pid]=n
        except Exception as e: sprites[pid]='err '+repr(e)[:80]
def refs(x):
    if isinstance(x,dict):
        if 'm_PathID' in x and x.get('m_FileID',0)==0 and x['m_PathID']: yield x['m_PathID']
        for v in x.values(): yield from refs(v)
    elif isinstance(x,list):
        for v in x: yield from refs(v)
visto=set()
def nodo(pid):
    o=objs[pid]; g=o.read_typetree()
    r={'nombre':g['m_Name'],'pid':pid,'activo':g.get('m_IsActive'),'comp':[],'hijos':[]}
    for c in g['m_Component']:
        cp=c['component']['m_PathID']; co=objs.get(cp)
        if not co: continue
        t=co.type.name; d=leer(co)
        ent={'tipo':t,'pid':cp}
        if t=='MonoBehaviour':
            try:
                import re; ent['script']=re.match(r'<UnknownObject<([^>]*)>',repr(co.read()))[1]
            except Exception: ent['script']='?'
        ent['datos']=d
        for rp in refs(d):
            guardar_ref(rp)
            ro=objs.get(rp)
            if ro and ro.type.name in('AnimationClip','AnimatorController') and rp not in visto:
                visto.add(rp)
        r['comp'].append(ent)
        if t in('Transform','RectTransform'):
            for ch in d.get('m_Children',[]):
                to=objs.get(ch['m_PathID'])
                if to: r['hijos'].append(nodo(to.read_typetree()['m_GameObject']['m_PathID']))
    return r
for q in sys.argv[2:]:
    if q.isdigit(): rs=[int(q)]
    else: rs=[p for p,o in objs.items() if o.type.name=='GameObject' and nom(o)==q]
    for i,p in enumerate(rs):
        try:
            arbol=nodo(p)
            json.dump(arbol,open(os.path.join(out,f'{q}_{p}.json'),'w'),indent=1,default=str)
            print(q,p,'ok',flush=True)
        except Exception as e: print(q,p,'ERROR',repr(e)[:200],flush=True)
# clips y controladores alcanzados
cl={}
for p in visto:
    o=objs[p]
    try:
        d=o.read()
        if o.type.name=='AnimationClip':
            cl[p]={'tipo':'clip','nombre':d.m_Name,'largo':d.m_MuscleClip.m_StopTime-d.m_MuscleClip.m_StartTime,'sr':d.m_SampleRate}
        else: cl[p]={'tipo':o.type.name,'nombre':nom(o),'datos':leer(o)}
    except Exception as e: cl[p]={'error':repr(e)[:100]}
json.dump(cl,open(os.path.join(out,'_clips.json'),'w'),indent=1,default=str)
print('sprites',len(sprites),'clips',len(cl))
