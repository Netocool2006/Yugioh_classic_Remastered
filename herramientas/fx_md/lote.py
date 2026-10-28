# Vuelca varios prefabs (y su timeline .playable) de LocalData en una sola carga de bundles.
# Uso: lote.py <indice.json> <salida_base> <nombre=contenedor_prefab[,contenedor_playable]> ...
import sys,os,json
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
import UnityPy,volcar_fx
ROOT=r'D:/Steam/steamapps/common/Yu-Gi-Oh!  Master Duel/LocalData/ad28c7c3'
P='assets/resourcesassetbundle/'
d=json.load(open(sys.argv[1])); base=sys.argv[2]
pedidos=[]
for a in sys.argv[3:]:
    n,c=a.split('=',1); cs=[P+x for x in c.split(',')]; pedidos.append((n,cs))
pre=[P+'duel/timeline/card/common',P+'duel/effects']+[os.path.dirname(c[0]) for _,c in pedidos]
bs=set(); loc={}
for b in d:
    for k,t,p in b.get('c',[]):
        if any(k.startswith(x) for x in pre): bs.add(b['b'])
        loc.setdefault(k,(b['b'],p))
env=UnityPy.load(*[os.path.join(ROOT,b) for b in sorted(bs)])
por={}
for o in env.objects: por.setdefault(o.path_id,[]).append(o)
for n,cs in pedidos:
    out=os.path.join(base,n)
    try:
        b,p=loc[cs[0]]
        go=[o for o in por.get(p,[]) if o.type.name=='GameObject']
        v=volcar_fx.volcar(env,go[0],out)
        print(n,'ok',len(v.tex),'tex',len(v.mallas),'mallas',len(v.mats),'mats',flush=True)
    except Exception as e:
        print(n,'ERROR',repr(e)[:200],flush=True); continue
    if len(cs)>1:
        b,p=loc[cs[1]]
        envt=UnityPy.load(os.path.join(ROOT,b))
        res=[]
        for o in envt.objects:
            if o.type.name!='MonoBehaviour': continue
            try: t=o.read_typetree()
            except Exception: continue
            try: c=o.read().m_Script.read().m_ClassName
            except Exception: c='?'
            res.append({'pid':o.path_id,'clase':c,'nombre':t.get('m_Name'),'datos':t})
        json.dump(res,open(os.path.join(out,'timeline.json'),'w'),default=str)
