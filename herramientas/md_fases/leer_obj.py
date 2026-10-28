# Imprime el typetree (MonoBehaviour incluidos) de objetos de resources.assets de Master Duel por path_id,
# o, con "tl:<pid_director_go>", los assets de las pistas de sonido del timeline. Uso: leer_obj.py <pid> ...
import sys,os,json,re
import UnityPy
from UnityPy.helpers.TypeTreeGenerator import TypeTreeGenerator
J=r'D:/Steam/steamapps/common/Yu-Gi-Oh!  Master Duel'
DUMMY=r'D:/Ghidra-Reload/herramientas/il2cppdumper/DummyDll'
env=UnityPy.load(os.path.join(J,'masterduel_Data','data.unity3d'))
objs={o.path_id:o for o in env.objects if o.assets_file.name=='resources.assets'}
gen=TypeTreeGenerator(next(iter(objs.values())).assets_file.unity_version); gen.load_local_dll_folder(DUMMY); env.typetree_generator=gen
def tt(p):
    try: return objs[p].read_typetree()
    except Exception as e: return {'error':repr(e)[:120]}
def clase(p):
    try: return re.match(r'<UnknownObject<([^>]*)>',repr(objs[p].read()))[1]
    except Exception: return objs[p].type.name
for a in sys.argv[1:]:
    if a.startswith('tl:'):
        g=tt(int(a[3:]))
        for c in g['m_Component']:
            cp=c['component']['m_PathID']
            if objs[cp].type.name!='PlayableDirector': continue
            tl=tt(tt(cp)['m_PlayableAsset']['m_PathID'])
            for tr in tl['m_Tracks']:
                t=tt(tr['m_PathID'])
                for k in t.get('m_Clips',[]):
                    ap=k['m_Asset']['m_PathID']
                    if clase(ap) in('SoundPlayableAsset','EventPlayableAsset'): print(t['m_Name'],k['m_Start'],clase(ap),json.dumps(tt(ap),default=str)[:600])
        continue
    p=int(a); print('==',p,objs[p].type.name,clase(p)); print(json.dumps(tt(p),indent=1,default=str)[:6000])
