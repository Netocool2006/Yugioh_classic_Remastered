# Arbol de un GameObject (por path_id o nombre) con sus componentes. Uso: jerarquia.py <archivo> <path_id|nombre> [prof]
import sys,os
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
import md
env=md.abrir(sys.argv[1])
fa=md.por_archivo(env)
arch=os.environ.get('ARCH')
objs=fa[arch] if arch else max(fa.values(),key=len)
q=sys.argv[2]; prof=int(sys.argv[3]) if len(sys.argv)>3 else 99
def nom(o):
    try: return o.peek_name()
    except Exception: return '?'
if q.isdigit(): raices=[int(q)]
else: raices=[p for p,o in objs.items() if o.type.name=='GameObject' and nom(o)==q]
def comp_desc(c):
    t=c.type.name
    s=t
    try:
        if t=='MonoBehaviour':
            d=c.read(); sc=d.m_Script.read() if d.m_Script else None
            s='MB:'+(sc.m_ClassName if sc else '?')
        elif t=='ParticleSystemRenderer':
            d=c.read(); ms=[m.read().m_Name for m in d.m_Materials if m.path_id]
            s=f'PSR mats={ms} mode={d.m_RenderMode}'
        elif t in('MeshFilter',):
            d=c.read(); s='MeshFilter '+(d.m_Mesh.read().m_Name if d.m_Mesh.path_id else '-')
        elif t in('MeshRenderer','SkinnedMeshRenderer'):
            d=c.read(); s=t+' '+str([m.read().m_Name for m in d.m_Materials if m.path_id])
        elif t in('Animator',):
            d=c.read(); s='Animator ctrl='+(d.m_Controller.read().m_Name if d.m_Controller.path_id else '-')
        elif t=='Animation':
            d=c.read(); s='Animation '+str([a.read().m_Name for a in d.m_Animations if a.path_id])
        elif t=='PlayableDirector':
            d=c.read(); s='PlayableDirector asset='+str(d.m_PlayableAsset.path_id)
    except Exception as e: s+=' !'+repr(e)[:60]
    return s
def arbol(pid,n):
    o=objs[pid]; g=o.read()
    comps=[]
    tr=None
    for c in g.m_Component:
        cp=c.component if hasattr(c,'component') else c
        co=objs.get(cp.path_id)
        if not co: continue
        if co.type.name in('Transform','RectTransform'): tr=co.read(); continue
        comps.append(comp_desc(co))
    print('  '*n+f'{g.m_Name} [{pid}] '+'; '.join(comps))
    if tr and n<prof:
        for ch in tr.m_Children:
            t=objs.get(ch.path_id)
            if t: arbol(t.read().m_GameObject.path_id,n+1)
for r in raices:
    # subir a la raiz
    arbol(r,0); print()
