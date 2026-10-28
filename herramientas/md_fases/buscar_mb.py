# Busca en resources.assets de Master Duel los MonoBehaviour de las clases pedidas y vuelca sus campos (y su GameObject).
# Uso: buscar_mb.py <Clase> [Clase...]   (salida por pantalla; sirve para leer valores serializados: DuelLP.countTime...)
import sys,os,re,json,struct
import UnityPy
from UnityPy.helpers.TypeTreeGenerator import TypeTreeGenerator
J=r'D:/Steam/steamapps/common/Yu-Gi-Oh!  Master Duel'
DUMMY=r'D:/Ghidra-Reload/herramientas/il2cppdumper/DummyDll'
env=UnityPy.load(os.path.join(J,'masterduel_Data','data.unity3d'))
objs={o.path_id:o for o in env.objects if o.assets_file.name=='resources.assets'}
gen=TypeTreeGenerator(next(iter(objs.values())).assets_file.unity_version); gen.load_local_dll_folder(DUMMY); env.typetree_generator=gen
quiero=set(sys.argv[1:])
porScript={}
for p,o in objs.items():
    if o.type.name!='MonoBehaviour': continue
    raw=o.get_raw_data()
    fid,sid=struct.unpack_from('<iq',raw,16)
    porScript.setdefault((fid,sid),[]).append(p)
for k,ps in porScript.items():
    try: c=re.match(r'<UnknownObject<([^>]*)>',repr(objs[ps[0]].read()))[1]
    except Exception: continue
    if c not in quiero: continue
    for p in ps:
        try: d=objs[p].read_typetree()
        except Exception as e: print(c,p,'error',e); continue
        go=d['m_GameObject']['m_PathID']
        try: gn=objs[go].peek_name()
        except Exception: gn='?'
        keep={a:b for a,b in d.items() if a not in('m_GameObject','m_Enabled','m_Script','m_Name') and not (isinstance(b,dict) and 'm_FileID' in b)}
        print('==',c,'pid',p,'go',gn,go)
        print(json.dumps(keep,default=str)[:1500])
