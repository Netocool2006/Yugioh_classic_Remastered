# Vuelca un TimelineAsset (.playable) de LocalData: pistas, clips (inicio, duracion, nombre), objeto ligado
import sys,os,json
import UnityPy
ROOT=r'D:/Steam/steamapps/common/Yu-Gi-Oh!  Master Duel/LocalData/ad28c7c3'
d=json.load(open(sys.argv[1])); cont=sys.argv[2]
dest=None
for b in d:
    for k,t,p in b.get('c',[]):
        if k==cont: dest=(b['b'],p)
env=UnityPy.load(os.path.join(ROOT,dest[0]))
objs={o.path_id:o for o in env.objects}
def tt(o):
    try: return o.read_typetree()
    except Exception as e: return {'error':repr(e)[:100]}
def scr(o):
    try:
        x=o.read(); s=x.m_Script.read(); return s.m_ClassName
    except Exception: return '?'
res=[]
for p,o in objs.items():
    if o.type.name!='MonoBehaviour': continue
    t=tt(o); c=scr(o)
    res.append({'pid':p,'clase':c,'nombre':t.get('m_Name'),'datos':t})
json.dump(res,open(sys.argv[3],'w'),indent=1,default=str)
for r in res:
    t=r['datos']; print(r['pid'],r['clase'],r['nombre'],'clips:',[(round(c.get('m_Start',0),2),round(c.get('m_Duration',0),2),c.get('m_DisplayName')) for c in t.get('m_Clips',[])][:12])
