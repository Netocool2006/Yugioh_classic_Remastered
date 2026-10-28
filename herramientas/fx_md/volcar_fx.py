# Vuelca un prefab de efecto de Master Duel (GameObject raiz) a una carpeta: params.json con el arbol
# (transform local, ParticleSystem completo, renderer, material: shader, texturas, colores, floats), las texturas png,
# las mallas (obj) y los clips de Animator/Animation muestreados.
# Uso: volcar_fx.py <bundle|data.unity3d> <nombre_o_pathid_raiz> <carpeta_salida> [ARCH=archivo serializado]
import sys,os,json
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
import UnityPy
import clip_muestras

def cargar_todo(rutas):
    env=UnityPy.load(*rutas) if len(rutas)>1 else UnityPy.load(rutas[0])
    return env

class Volcador:
    def __init__(s,env,out):
        s.env=env; s.out=out; os.makedirs(out,exist_ok=True)
        s.tex={}; s.mallas={}; s.clips={}; s.mats={}
    def obj(s,pptr,desde):
        try:
            if pptr.path_id==0: return None
            return pptr.deref(desde.assets_file) if hasattr(pptr,'deref') else pptr.get_obj()
        except Exception:
            try: return pptr.read()
            except Exception: return None
    def leer(s,pptr):
        try:
            if pptr.path_id==0: return None
            return pptr.read()
        except Exception as e:
            return None
    def textura(s,pptr):
        t=s.leer(pptr)
        if t is None: return None
        k=f'{t.m_Name}_{pptr.path_id & 0xffffffff:x}'
        if k not in s.tex:
            try:
                img=t.image
                img.save(os.path.join(s.out,k+'.png'))
                s.tex[k]={'w':t.m_Width,'h':t.m_Height}
            except Exception as e:
                s.tex[k]={'error':repr(e)[:100]}
        return k
    def malla(s,pptr):
        m=s.leer(pptr)
        if m is None: return None
        k=f'{m.m_Name}_{pptr.path_id & 0xffffffff:x}'
        if k not in s.mallas:
            try:
                open(os.path.join(s.out,k+'.obj'),'w').write(m.export()); s.mallas[k]=True
            except Exception as e: s.mallas[k]=repr(e)[:100]
        return k
    def material(s,pptr):
        m=s.leer(pptr)
        if m is None: return None
        k=f'{m.m_Name}_{pptr.path_id & 0xffffffff:x}'
        if k in s.mats: return k
        sp=m.m_SavedProperties
        d={'nombre':m.m_Name,'shader':None,'texturas':{},'colores':{},'floats':{},'keywords':getattr(m,'m_ValidKeywords',None) or getattr(m,'m_ShaderKeywords',None),
           'cola':getattr(m,'m_CustomRenderQueue',None)}
        sh=s.leer(m.m_Shader)
        if sh is not None:
            try: d['shader']=sh.m_ParsedForm.m_Name
            except Exception:
                try: d['shader']=sh.m_Name
                except Exception: d['shader']='?'
        for nom,te in sp.m_TexEnvs:
            if te.m_Texture.path_id:
                d['texturas'][nom]={'tex':s.textura(te.m_Texture),'escala':[te.m_Scale.x,te.m_Scale.y],'offset':[te.m_Offset.x,te.m_Offset.y]}
        for nom,c in sp.m_Colors: d['colores'][nom]=[c.r,c.g,c.b,c.a]
        for nom,f in sp.m_Floats: d['floats'][nom]=f
        s.mats[k]=d
        return k
    def clip(s,pptr):
        c=s.leer(pptr)
        if c is None: return None
        k=f'{c.m_Name}_{pptr.path_id & 0xffffffff:x}'
        if k not in s.clips:
            try:
                r=clip_muestras.muestrear(c,30)
                json.dump(r,open(os.path.join(s.out,'clip_'+k+'.json'),'w'))
                s.clips[k]={'curvas':len(r['curvas']),'fin':r['fin']}
            except Exception as e:
                s.clips[k]={'error':repr(e)[:150]}
        return k
    def nodo(s,go_obj,prof=0):
        g=go_obj.read()
        n={'nombre':g.m_Name,'activo':bool(getattr(g,'m_IsActive',True)),'comp':[]}
        hijos=[]
        for c in g.m_Component:
            cp=c.component if hasattr(c,'component') else c
            try: co=cp.get_obj() if hasattr(cp,'get_obj') else None
            except Exception: co=None
            if co is None:
                try: co=cp.deref()
                except Exception: co=None
            if co is None: continue
            t=co.type.name
            try:
                if t in('Transform','RectTransform'):
                    tr=co.read()
                    n['pos']=[tr.m_LocalPosition.x,tr.m_LocalPosition.y,tr.m_LocalPosition.z]
                    q=tr.m_LocalRotation; n['rot']=[q.x,q.y,q.z,q.w]
                    sc=tr.m_LocalScale; n['esc']=[sc.x,sc.y,sc.z]
                    hijos=[h for h in tr.m_Children]
                elif t=='ParticleSystem':
                    n['comp'].append({'tipo':'ParticleSystem','ps':co.read_typetree()})
                elif t=='ParticleSystemRenderer':
                    r=co.read_typetree()
                    rr=co.read()
                    r['_materiales']=[s.material(m) for m in rr.m_Materials]
                    if rr.m_Mesh.path_id: r['_malla']=s.malla(rr.m_Mesh)
                    for i,mm in enumerate(getattr(rr,'m_Meshes',[]) or []):
                        if mm.path_id: r.setdefault('_mallas',[]).append(s.malla(mm))
                    n['comp'].append({'tipo':'PSR','r':r})
                elif t=='MeshFilter':
                    n['comp'].append({'tipo':'MeshFilter','malla':s.malla(co.read().m_Mesh)})
                elif t in('MeshRenderer','SkinnedMeshRenderer'):
                    rr=co.read()
                    e={'tipo':t,'materiales':[s.material(m) for m in rr.m_Materials],'activo':bool(getattr(rr,'m_Enabled',1))}
                    if t=='SkinnedMeshRenderer': e['malla']=s.malla(rr.m_Mesh)
                    n['comp'].append(e)
                elif t=='Animator':
                    a=co.read(); ctl=s.leer(a.m_Controller); e={'tipo':'Animator','ctl':None,'clips':[]}
                    if ctl is not None:
                        e['ctl']=ctl.m_Name
                        for cl in getattr(ctl,'m_AnimationClips',[]) or []: e['clips'].append(s.clip(cl))
                    n['comp'].append(e)
                elif t=='Animation':
                    a=co.read(); n['comp'].append({'tipo':'Animation','clips':[s.clip(cl) for cl in a.m_Animations if cl.path_id]})
                elif t=='TrailRenderer' or t=='LineRenderer':
                    r=co.read_typetree(); rr=co.read(); r['_materiales']=[s.material(m) for m in rr.m_Materials]
                    n['comp'].append({'tipo':t,'r':r})
                elif t=='MonoBehaviour':
                    mb=co.read(); sc=s.leer(mb.m_Script)
                    e={'tipo':'MB','script':sc.m_ClassName if sc else None}
                    try: e['datos']=co.read_typetree()
                    except Exception: pass
                    n['comp'].append(e)
                elif t=='PlayableDirector':
                    n['comp'].append({'tipo':'PlayableDirector','datos':co.read_typetree()})
                else:
                    n['comp'].append({'tipo':t})
            except Exception as ex:
                n['comp'].append({'tipo':t,'error':repr(ex)[:200]})
        n['hijos']=[]
        for h in hijos:
            try:
                tro=h.get_obj() if hasattr(h,'get_obj') else h.deref()
                gob=tro.read().m_GameObject
                go2=gob.get_obj() if hasattr(gob,'get_obj') else gob.deref()
                n['hijos'].append(s.nodo(go2,prof+1))
            except Exception as ex:
                n['hijos'].append({'error':repr(ex)[:200]})
        return n

def volcar(env,raiz_obj,out):
    v=Volcador(env,out)
    arbol=v.nodo(raiz_obj)
    json.dump({'arbol':arbol,'materiales':v.mats,'texturas':v.tex,'mallas':list(v.mallas),'clips':v.clips},
              open(os.path.join(out,'params.json'),'w'),indent=1,default=str)
    return v

if __name__=='__main__':
    env=UnityPy.load(sys.argv[1])
    q=sys.argv[2]; arch=os.environ.get('ARCH')
    cand=[o for o in env.objects if o.type.name=='GameObject' and (not arch or o.assets_file.name==arch)]
    if q.isdigit(): r=[o for o in cand if o.path_id==int(q)]
    else:
        r=[o for o in cand if o.peek_name()==q]
    if not r: print('no esta',q); sys.exit(1)
    v=volcar(env,r[0],sys.argv[3])
    print('ok',len(v.tex),'texturas',len(v.mallas),'mallas',len(v.clips),'clips',len(v.mats),'materiales')
