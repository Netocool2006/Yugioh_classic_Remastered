import os,sys,json
from multiprocessing import Pool
import UnityPy
from UnityPy.helpers.ResourceReader import get_resource_data
ROOT=sys.argv[1]; OUT=sys.argv[2]
def trabajo(p):
    res=[]
    try:
        env=UnityPy.load(p)
        names={}
        for k,v in env.container.items():
            try: names[v.path_id]=k
            except Exception: pass
        for o in env.objects:
            if o.type.name!='AudioClip': continue
            d=o.read(); r=d.m_Resource
            b=get_resource_data(r.m_Source, d.assets_file, r.m_Offset, r.m_Size) if r.m_Size else d.m_AudioData
            cont=names.get(o.path_id, d.m_Name)
            rel=cont.replace('assets/resourcesassetbundle/','').replace('/','__')
            rel=os.path.splitext(rel)[0]+'.fsb'
            dst=os.path.join(OUT,rel)
            with open(dst,'wb') as h: h.write(bytes(b))
            res.append({'bundle':p,'container':cont,'name':d.m_Name,'len':getattr(d,'m_Length',0),'fsb':rel,'size':len(b)})
    except Exception as e:
        res.append({'bundle':p,'error':repr(e)[:200]})
    return res
if __name__=='__main__':
    os.makedirs(OUT,exist_ok=True)
    fs=[os.path.join(d,f) for d,_,ff in os.walk(ROOT) for f in ff]
    print('archivos',len(fs),flush=True)
    todo=[]; n=0
    with Pool(8) as pool:
        for r in pool.imap_unordered(trabajo,fs,chunksize=16):
            todo+= [x for x in r if 'fsb' in x]
            n+=1
            if n%2000==0: print(n,len(todo),flush=True)
    json.dump(todo,open(os.path.join(OUT,'_indice.json'),'w'),indent=1)
    print('listo',len(todo))
