# Indice de todos los bundles de LocalData: bundle -> contenedores + conteo de tipos
import os,sys,json,collections
from multiprocessing import Pool
import UnityPy
ROOT=sys.argv[1]; OUT=sys.argv[2]
def trabajo(p):
    try:
        env=UnityPy.load(p)
        cont=[]
        for k,v in env.container.items():
            try: cont.append([k,v.type.name,v.path_id])
            except Exception: cont.append([k,'?',0])
        c=collections.Counter(o.type.name for o in env.objects)
        return {'b':os.path.relpath(p,ROOT),'c':cont,'t':dict(c)}
    except Exception as e:
        return {'b':os.path.relpath(p,ROOT),'err':repr(e)[:200]}
if __name__=='__main__':
    fs=[os.path.join(d,f) for d,_,ff in os.walk(ROOT) for f in ff]
    print('archivos',len(fs),flush=True)
    todo=[];n=0
    with Pool(10) as pool:
        for r in pool.imap_unordered(trabajo,fs,chunksize=32):
            todo.append(r); n+=1
            if n%4000==0: print(n,flush=True)
    json.dump(todo,open(OUT,'w'))
    print('listo',len(todo))
