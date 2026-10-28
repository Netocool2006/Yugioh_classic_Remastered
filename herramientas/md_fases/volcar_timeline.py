# Vuelca los Timeline (PlayableDirector) de prefabs de UI del duelo de Master Duel: pistas, clips (inicio/duracion),
# y cada AnimationClip muestreado a 30 fps con los nombres de ruta resueltos (crc32 de la jerarquia).
# Uso: volcar_timeline.py <salida> <GameObject> ...   (la salida va fuera del repo: md_extraido)
import sys,os,json,zlib,re
sys.path.insert(0,os.path.join(os.path.dirname(os.path.abspath(__file__)),'..','fx_md'))
import UnityPy,clip_muestras
from UnityPy.helpers.TypeTreeGenerator import TypeTreeGenerator
J=r'D:/Steam/steamapps/common/Yu-Gi-Oh!  Master Duel'
DUMMY=r'D:/Ghidra-Reload/herramientas/il2cppdumper/DummyDll'
out=sys.argv[1]; os.makedirs(out,exist_ok=True)
env=UnityPy.load(os.path.join(J,'masterduel_Data','data.unity3d'))
objs={o.path_id:o for o in env.objects if o.assets_file.name=='resources.assets'}
ver=next(iter(objs.values())).assets_file.unity_version
gen=TypeTreeGenerator(ver); gen.load_local_dll_folder(DUMMY); env.typetree_generator=gen
def tt(p):
    o=objs.get(p)
    if not o: return None
    try: return o.read_typetree()
    except Exception as e: return {'error':repr(e)[:120]}
def clase(p):
    try: return re.match(r'<UnknownObject<([^>]*)>',repr(objs[p].read()))[1]
    except Exception: return objs[p].type.name if p in objs else '?'
def nombre_go(p):
    try: return objs[p].peek_name()
    except Exception: return '?'
def rutas(go_pid,pref=''):
    r={}
    g=tt(go_pid)
    for c in g['m_Component']:
        cp=c['component']['m_PathID']
        if objs.get(cp) and objs[cp].type.name in('Transform','RectTransform'):
            for ch in tt(cp)['m_Children']:
                cg=tt(ch['m_PathID'])['m_GameObject']['m_PathID']; n=tt(cg)['m_Name']
                ruta=(pref+'/'+n) if pref else n
                r[zlib.crc32(ruta.encode())]=ruta; r.update(rutas(cg,ruta))
    return r
ATRS=['m_Color.r','m_Color.g','m_Color.b','m_Color.a','m_IsActive','m_Enabled','m_LocalScale.x','m_LocalScale.y','m_LocalScale.z',
 'm_fontColor.a','m_fontColor.r','m_fontColor.g','m_fontColor.b','m_fontSize','m_characterSpacing','m_Alpha','m_AnchoredPosition.x','m_AnchoredPosition.y',
 'material._Alpha','material._Color.a','material._Color.r','material._Color.g','material._Color.b','material._FaceColor.a','material._FaceColor.r',
 'material._GlowPower','material._OutlineWidth','material._FaceDilate','material._UnderlayColor.a','material._GlowColor.a','material._OutlineColor.a',
 'm_SizeDelta.x','m_SizeDelta.y','m_Size.x','m_Size.y','material._Dissolve','material._Offset','m_FlipX','m_wordSpacing','m_lineSpacing',
 'material._MainTex_ST.x','material._MainTex_ST.y','material._MainTex_ST.z','material._MainTex_ST.w','material._TintColor.a','material._BaseColor.a']
def nombre_attr(h):
    for a in ATRS:
        if zlib.crc32(a.encode())==h: return a
    return 'attr%d'%h
def muestrear(ac,ck,rt):
    try:
        cobj=objs[ac].read()
        m=clip_muestras.muestrear(cobj,30)
        ck['anim']=m['nombre']
        b=cobj.m_ClipBindingConstant; nom=[]
        for gb in b.genericBindings:
            rr=rt.get(gb.path,'?%d'%gb.path)
            if gb.typeID==4:
                comp={1:'pos',2:'rot',3:'scale',4:'euler'}.get(gb.attribute,'?'); kk=4 if gb.attribute==2 else 3
                nom+=[rr+':'+comp+'.'+'xyzw'[j] for j in range(kk)]
            else: nom.append(rr+':'+nombre_attr(gb.attribute)+'(t%d)'%gb.typeID)
        m['curvas']=nom
        ck['muestras']=m
    except Exception as e: ck['anim_error']=repr(e)[:120]
for q in sys.argv[2:]:
    for p,o in objs.items():
        if o.type.name!='GameObject' or nombre_go(p)!=q: continue
        g=tt(p); rt=rutas(p); rt[0]='(raiz)'
        for c in g['m_Component']:
            cp=c['component']['m_PathID']
            if objs.get(cp) is None or objs[cp].type.name!='PlayableDirector': continue
            pd=tt(cp); ta=pd['m_PlayableAsset']['m_PathID']; tl=tt(ta)
            ent={'raiz':q,'pid':p,'timeline':tl.get('m_Name'),'duracion_fija':tl.get('m_FixedDuration'),'modo':tl.get('m_DurationMode'),'pistas':[]}
            def pista(tp):
                t=tt(tp); cl=clase(tp)
                pi={'clase':cl,'nombre':t.get('m_Name'),'mute':t.get('m_Muted'),'clips':[],'hijas':[]}
                for sb in pd.get('m_SceneBindings',[]):
                    if sb['key']['m_PathID']==tp:
                        v=sb['value']['m_PathID']
                        if v in objs:
                            gv=tt(v); gid=gv['m_GameObject']['m_PathID'] if 'm_GameObject' in gv else v
                            pi['ligado']=nombre_go(gid)
                for k in t.get('m_Clips',[]):
                    ck={'inicio':round(k['m_Start'],4),'dur':round(k['m_Duration'],4),'nombre':k.get('m_DisplayName'),'ease_in':k.get('m_EaseInDuration'),'ease_out':k.get('m_EaseOutDuration'),
                        'blend_in':k.get('m_BlendInDuration'),'blend_out':k.get('m_BlendOutDuration'),'clip_in':k.get('m_ClipIn'),'escala':k.get('m_TimeScale')}
                    ap=k['m_Asset']['m_PathID']
                    if ap in objs:
                        a=tt(ap); ck['asset']=clase(ap)
                        ac=a['m_Clip']['m_PathID'] if isinstance(a.get('m_Clip'),dict) else None
                        if ac and ac in objs: muestrear(ac,ck,rt)
                        for kx in('m_Duration','m_ControlDuration','m_Volume'):
                            if kx in a: ck[kx]=a[kx]
                        if 'm_Clip' in a and ck.get('asset','').startswith('Audio'):
                            try: ck['audio']=objs[a['m_Clip']['m_PathID']].peek_name()
                            except Exception: ck['audio']=str(a['m_Clip'])
                        if 'sourceGameObject' in a: ck['control']=str(a['sourceGameObject'])
                    pi['clips'].append(ck)
                ic=t.get('m_InfiniteClip')
                if isinstance(ic,dict) and ic.get('m_PathID') in objs:
                    ck={'inicio':0.0,'dur':-1,'nombre':'(infinito)'}
                    sub=rt
                    if pi.get('ligado'):
                        # rutas relativas al objeto ligado
                        pref=[v for v in rt.values() if v==pi['ligado'] or v.endswith('/'+pi['ligado'])]
                        if pref:
                            pp=pref[0]+'/'; sub={k2:(v[len(pp):] if v.startswith(pp) else v) for k2,v in rt.items()}
                            sub={zlib.crc32(v.encode()):v for v in sub.values()}; sub[0]='(ligado)'
                    muestrear(ic['m_PathID'],ck,sub)
                    if 'muestras' in ck: ck['dur']=ck['muestras']['fin']-ck['muestras']['inicio']
                    pi['clips'].append(ck)
                for h in t.get('m_Children',[]): pi['hijas'].append(pista(h['m_PathID']))
                return pi
            for tr in tl.get('m_Tracks',[]): ent['pistas'].append(pista(tr['m_PathID']))
            json.dump(ent,open(os.path.join(out,'tl_%s_%d.json'%(q,p)),'w'),indent=1,default=str)
            print('==',q,p,ent['timeline'],'modo',ent['modo'],'fija',ent['duracion_fija'],flush=True)
            def imp(pi,n=1):
                print('  '*n+'[%s] %s -> %s'%(pi['clase'],pi['nombre'],pi.get('ligado')))
                for k in pi['clips']:
                    print('  '*n+'   %.3f+%.3f %s %s %s easeIn=%s out=%s %s %s'%(k['inicio'],k['dur'],k.get('nombre'),k.get('asset',''),k.get('anim',''),k.get('ease_in'),k.get('ease_out'),k.get('audio',''),k.get('anim_error','')))
                for h in pi['hijas']: imp(h,n+1)
            for pi in ent['pistas']: imp(pi)
