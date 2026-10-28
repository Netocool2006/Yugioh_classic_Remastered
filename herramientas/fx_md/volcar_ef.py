# Vuelca un prefab de LocalData (por su contenedor) cargando juntos los bundles de los prefijos dados (para resolver
# las referencias entre bundles). Uso: volcar_ef.py <indice.json> <contenedor.prefab> <salida> [prefijo ...]
import sys,os,json
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
import UnityPy,volcar_fx
ROOT=r'D:/Steam/steamapps/common/Yu-Gi-Oh!  Master Duel/LocalData/ad28c7c3'
d=json.load(open(sys.argv[1])); cont=sys.argv[2]; out=sys.argv[3]; pre=sys.argv[4:]+[os.path.dirname(cont)]
bs=set(); dest=None
for b in d:
    for k,t,p in b.get('c',[]):
        if any(k.startswith(x) for x in pre): bs.add(b['b'])
        if k==cont: dest=(b['b'],p)
env=UnityPy.load(*[os.path.join(ROOT,b) for b in sorted(bs)])
pref=[o for o in env.objects if o.path_id==dest[1] and o.type.name=='GameObject']
if not pref: print('no hallado',dest); sys.exit(1)
v=volcar_fx.volcar(env,pref[0],out)
print('ok',len(bs),'bundles',len(v.tex),'tex',len(v.mallas),'mallas',len(v.clips),'clips',len(v.mats),'mats')
