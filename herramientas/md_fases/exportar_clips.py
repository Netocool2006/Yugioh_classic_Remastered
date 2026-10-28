# Exporta de Yu-Gi-Oh! Master Duel (copia del usuario) los datos de la UI del duelo, TAL CUAL, para el reproductor
# generico src/clips_md.h. No va nada al repo: la salida es <classic-assets>/md_ui/.
#   <nombre>.mdtl   un Timeline (PlayableDirector): sus pistas de animacion con las curvas EXACTAS de cada AnimationClip
#                   (los segmentos cubicos del StreamedClip de Mecanim: v = ((a*dt+b)*dt+c)*dt+d, como Unity), las
#                   muestras densas, las constantes; las pistas de activacion y los eventos/sonidos con su tiempo.
#   <nombre>.mdui   un prefab de UI (uGUI): RectTransform de cada objeto, las Image (sprite y color) y los Tween de MD
#                   (TweenAlpha/Position/Scale/Size/Color/Rotation: label, easing, style, duracion, demora, from, to).
#   md_ui.png + md_ui_atlas.txt   los sprites usados, empaquetados (nombre x y w h).
# Uso: exportar_clips.py [salida]   (por defecto D:/Docker/compartido/classic-assets/md_ui)
# Formato de texto, campos separados por TAB (las rutas tienen espacios). Ver src/clips_md.h.
import sys,os,re,zlib,struct
import UnityPy
from UnityPy.helpers.TypeTreeGenerator import TypeTreeGenerator
from PIL import Image
J=r'D:/Steam/steamapps/common/Yu-Gi-Oh!  Master Duel'
DUMMY=r'D:/Ghidra-Reload/herramientas/il2cppdumper/DummyDll'
OUT=sys.argv[1] if len(sys.argv)>1 else r'D:/Docker/compartido/classic-assets/md_ui'
TIMELINES=['DuelTurnChange00','DuelTurnChange01','DuelTextStart','DuelTextWin2','DueltextLose','DueltextDraw']+[f'Duel{p}Phase_{l}' for p in ('Draw','Stanby','Main01','Battle','Main02','End') for l in ('near','far')]
TIMELINES+=['DuelEndTransition']   # rama classic-md-entradas: la transicion del fin del duelo (2.0 s)
PREFABS=['PhaseSelectWindow','DuelMenuButton','SurrenderButton','MenuButtons','DuelLPNear','DuelLPFar']
PREFABS+=['SettingMenuUI']         # rama classic-md-entradas: el menu del duelo (Push/Pop, cursor, esquinas)
# MD_SOLO=a,b: exporta solo esos Timeline/prefabs y NO rehace el atlas (para sumar archivos sin pisar md_ui.png)
SOLO=[x for x in os.environ.get('MD_SOLO','').split(',') if x]
if SOLO: TIMELINES=[x for x in TIMELINES if x in SOLO]; PREFABS=[x for x in PREFABS if x in SOLO]
SPRITES=['GUI_PhaseDP','GUI_PhaseSP','GUI_PhaseMP1','GUI_PhaseBP','GUI_PhaseMP2','GUI_PhaseEP','GUI_PhaseCircle','GUI_PhaseButtonBase2',
         'GUI_PhaseButtonCursor2','GUI_PhaseCursorArrow','GUI_T_DuelButtonSubmenu01','GUI_T_DuelButtonSubmenu02','GUI_T_DuelButtonSubmenu03',
         'GUI_T_DuelButtonSubmenu04','img_TunChange_bg01','img_TunChange_bg02','img_Phase_near','img_Phase_far','GUI_CommonSelectCursor_CornerAll']
os.makedirs(OUT,exist_ok=True)
env=UnityPy.load(os.path.join(J,'masterduel_Data','data.unity3d'))
objs={o.path_id:o for o in env.objects if o.assets_file.name=='resources.assets'}
gen=TypeTreeGenerator(next(iter(objs.values())).assets_file.unity_version); gen.load_local_dll_folder(DUMMY); env.typetree_generator=gen
def tt(p):
    try: return objs[p].read_typetree()
    except Exception as e: return {'error':repr(e)[:120]}
def clase(p):
    try: return re.match(r'<UnknownObject<([^>]*)>',repr(objs[p].read()))[1]
    except Exception: return objs[p].type.name if p in objs else '?'
def nombre(p):
    try: return objs[p].peek_name()
    except Exception: return '?'
ATRS=['m_Color.r','m_Color.g','m_Color.b','m_Color.a','m_IsActive','m_Enabled','m_fontColor.r','m_fontColor.g','m_fontColor.b','m_fontColor.a',
 'm_fontSize','m_characterSpacing','m_wordSpacing','m_lineSpacing','m_Alpha','m_AnchoredPosition.x','m_AnchoredPosition.y','m_SizeDelta.x','m_SizeDelta.y',
 'm_Size.x','m_Size.y','m_FlipX','m_FlipY','m_SortingOrder','m_fontColor32.rgba','m_maxVisibleCharacters','m_FillAmount',
 'material._Alpha','material._Color.r','material._Color.g','material._Color.b','material._Color.a','material._FaceColor.r','material._FaceColor.g',
 'material._FaceColor.b','material._FaceColor.a','material._GlowPower','material._OutlineWidth','material._FaceDilate','material._UnderlayColor.a',
 'material._GlowColor.a','material._OutlineColor.a','material._TintColor.a','material._BaseColor.a','material._Dissolve','material._Offset',
 'material._MainTex_ST.x','material._MainTex_ST.y','material._MainTex_ST.z','material._MainTex_ST.w','material._SwitchTurn','material._Active']
HASH={zlib.crc32(a.encode()):a for a in ATRS}
def attr(h): return HASH.get(h,'attr%d'%h)

# ---- arbol del prefab: pid del GameObject -> ruta unica ("A/B/C", hermanos repetidos con #2, #3...)
def arbol(raiz):
    rutas={raiz:''}; trans={}
    def hijos(go):
        g=tt(go)
        for c in g['m_Component']:
            cp=c['component']['m_PathID']
            if cp in objs and objs[cp].type.name in('Transform','RectTransform'):
                trans[go]=cp
                return [tt(ch['m_PathID'])['m_GameObject']['m_PathID'] for ch in tt(cp)['m_Children']]
        return []
    def rec(go,pref):
        vistos={}
        for h in hijos(go):
            n=nombre(h); vistos[n]=vistos.get(n,0)+1
            r=(pref+'/' if pref else '')+n+('#%d'%vistos[n] if vistos[n]>1 else '')
            rutas[h]=r; rec(h,r)
    rec(raiz,'')
    return rutas,trans
def crcRutas(rutas,base):
    # rutas relativas a "base" (el objeto del Animator) -> crc32 de la ruta de Unity (sin los #k)
    b=rutas[base]; r={0:b}
    for p,ru in rutas.items():
        if p==base or not ru.startswith(b+'/' if b else ''): continue
        rel=ru[len(b)+1:] if b else ru
        r.setdefault(zlib.crc32(re.sub(r'#\d+','',rel).encode()),ru)
    return r

# ---- curvas de un AnimationClip (Mecanim): streamed (segmentos cubicos), dense (muestras), constant
def curvas(c,crcs):
    mc=c.m_MuscleClip; cl=mc.m_Clip.data
    raw=struct.pack('<%dI'%len(cl.m_StreamedClip.data),*cl.m_StreamedClip.data)
    ns=cl.m_StreamedClip.curveCount; seg={}; p=0
    while p<len(raw):
        t,n=struct.unpack_from('<fI',raw,p); p+=8
        for i in range(n):
            idx,a,b,cc,d=struct.unpack_from('<iffff',raw,p); p+=20
            seg.setdefault(idx,[]).append((t,a,b,cc,d))
    dense=cl.m_DenseClip; nd=dense.m_CurveCount; dv=list(dense.m_SampleArray); cons=list(cl.m_ConstantClip.data)
    nombres=[]
    for g in c.m_ClipBindingConstant.genericBindings:
        ruta=crcs.get(g.path,'?%d'%g.path)
        if g.typeID==4:
            comp={1:'pos',2:'rot',3:'scale',4:'euler'}.get(g.attribute,'t%d'%g.attribute); k=4 if g.attribute==2 else 3
            nombres+=[(ruta,comp+'.'+'xyzw'[j]) for j in range(k)]
        else: nombres.append((ruta,attr(g.attribute)))
    out=[]
    for i,(ruta,a) in enumerate(nombres):
        if i<ns:
            s=sorted(seg.get(i,[]))
            out.append((ruta,a,'s',s))
        elif i<ns+nd:
            k=i-ns; vals=[dv[f*nd+k] for f in range(dense.m_FrameCount)]
            out.append((ruta,a,'d',(dense.m_BeginTime,dense.m_SampleRate,vals)))
        else:
            k=i-ns-nd
            if k<len(cons): out.append((ruta,a,'c',cons[k]))
    return out,mc.m_StartTime,mc.m_StopTime
def escribirCurvas(f,lista):
    for ruta,a,tipo,d in lista:
        if tipo=='s':
            f.write('curva\t%s\t%s\ts\t%d\n'%(ruta,a,len(d)))
            for t,a1,b,c,e in d: f.write('%.6g\t%.9g\t%.9g\t%.9g\t%.9g\n'%(t,a1,b,c,e))
        elif tipo=='d':
            t0,sr,vals=d
            f.write('curva\t%s\t%s\td\t%d\t%.6g\t%.6g\n'%(ruta,a,len(vals),t0,sr))
            f.write('\t'.join('%.9g'%v for v in vals)+'\n')
        else: f.write('curva\t%s\t%s\tc\t%.9g\n'%(ruta,a,d))

def exportarTimeline(nom,raiz):
    rutas,trans=arbol(raiz); g=tt(raiz)
    pd=None
    for c in g['m_Component']:
        cp=c['component']['m_PathID']
        if cp in objs and objs[cp].type.name=='PlayableDirector': pd=tt(cp)
    tl=tt(pd['m_PlayableAsset']['m_PathID'])
    lig={sb['key']['m_PathID']:sb['value']['m_PathID'] for sb in pd.get('m_SceneBindings',[])}
    def goDe(v):
        if v not in objs: return None
        d=tt(v); return d['m_GameObject']['m_PathID'] if 'm_GameObject' in d else v
    lineas=[]; fin=0.0
    from io import StringIO
    f=StringIO()
    def pista(tp):
        nonlocal fin
        t=tt(tp); cl=clase(tp); go=goDe(lig.get(tp,0)); obj=rutas.get(go,'') if go else ''
        if cl=='AnimationTrack' and go:
            crcs=crcRutas(rutas,go)
            ic=t.get('m_InfiniteClip')
            if isinstance(ic,dict) and ic.get('m_PathID') in objs:
                cv,t0,t1=curvas(objs[ic['m_PathID']].read(),crcs)
                f.write('pista\t%s\t0\t-1\t0\t1\t1\t1\t%d\n'%(obj,len(cv))); escribirCurvas(f,cv); fin=max(fin,t1)
            for k in t.get('m_Clips',[]):
                a=tt(k['m_Asset']['m_PathID']); ac=a.get('m_Clip',{}).get('m_PathID') if isinstance(a.get('m_Clip'),dict) else None
                if not ac or ac not in objs: continue
                cv,t0,t1=curvas(objs[ac].read(),crcs)
                f.write('pista\t%s\t%.6g\t%.6g\t%.6g\t%.6g\t%d\t%d\t%d\n'%(obj,k['m_Start'],k['m_Duration'],k.get('m_ClipIn',0),k.get('m_TimeScale',1),
                        k.get('m_PreExtrapolationMode',0),k.get('m_PostExtrapolationMode',0),len(cv)))
                escribirCurvas(f,cv); fin=max(fin,k['m_Start']+k['m_Duration'])
        elif cl=='ActivationTrack' and go:
            for k in t.get('m_Clips',[]):
                f.write('activa\t%s\t%.6g\t%.6g\n'%(obj,k['m_Start'],k['m_Duration'])); fin=max(fin,k['m_Start']+k['m_Duration'])
            f.write('activa_fin\t%s\t%d\n'%(obj,t.get('m_PostPlaybackState',3)))
        elif cl in('SoundTrack','EventTrack'):
            for k in t.get('m_Clips',[]):
                a=tt(k['m_Asset']['m_PathID'])
                if 'startLabel' in a: f.write('sonido\t%s\t%.6g\n'%(a['startLabel'],k['m_Start']))
                for e in a.get('eventList',[]): f.write('evento\t%s\t%.6g\n'%(e['label'],k['m_Start']+e['time']))
        for h in t.get('m_Children',[]): pista(h['m_PathID'])
    for tr in tl.get('m_Tracks',[]): pista(tr['m_PathID'])
    with open(os.path.join(OUT,nom+'.mdtl'),'w',encoding='utf-8',newline='\n') as o:
        o.write('# %s (Yu-Gi-Oh! Master Duel, resources.assets pid %d; copia del usuario, NO va al repo). Ver src/clips_md.h\n'%(tl.get('m_Name'),raiz))
        o.write('timeline\t%s\t%.6g\n'%(nom,fin)); o.write(f.getvalue())
    print('timeline',nom,'%.3f s'%fin,flush=True)

usados=set()
def exportarPrefab(nom,raiz):
    rutas,trans=arbol(raiz)
    with open(os.path.join(OUT,nom+'.mdui'),'w',encoding='utf-8',newline='\n') as o:
        o.write('# %s (Yu-Gi-Oh! Master Duel, resources.assets pid %d; copia del usuario, NO va al repo). Ver src/clips_md.h\n'%(nom,raiz))
        for go,ru in rutas.items():
            g=tt(go); ru=ru or '.'
            o.write('objeto\t%s\t%d\n'%(ru,int(g.get('m_IsActive',1))))
            for c in g['m_Component']:
                cp=c['component']['m_PathID']
                if cp not in objs: continue
                ty=objs[cp].type.name; d=tt(cp)
                if ty=='RectTransform':
                    v=lambda k:(d[k]['x'],d[k]['y'])
                    o.write('rect\t%s\t%g\t%g\t%g\t%g\t%g\t%g\t%g\t%g\t%g\t%g\t%g\t%g\n'%((ru,)+v('m_AnchorMin')+v('m_AnchorMax')+v('m_AnchoredPosition')+v('m_SizeDelta')+v('m_Pivot')+(d['m_LocalScale']['x'],d['m_LocalScale']['y'])))
                if ty!='MonoBehaviour' or 'error' in d: continue
                cl=clase(cp)
                if cl in('Image','RawImage') and 'm_Sprite' in d:
                    sp=d['m_Sprite']['m_PathID']; sn=nombre(sp) if sp in objs else '-'
                    if sn!='-': usados.add((sn,sp))
                    col=d['m_Color']; o.write('imagen\t%s\t%s\t%g\t%g\t%g\t%g\n'%(ru,sn,col['r'],col['g'],col['b'],col['a']))
                elif cl.startswith('Tween') and 'duration' in d:
                    def val(x):
                        if isinstance(x,dict): return ' '.join('%g'%x[k] for k in ('x','y','z','w','r','g','b','a') if k in x)
                        return '%g'%x if isinstance(x,(int,float)) else str(x)
                    o.write('tween\t%s\t%s\t%s\t%d\t%d\t%g\t%g\t%s\t%s\n'%(ru,cl,d.get('label',''),d['easing'],d['style'],d['duration'],d.get('startDelay',0),
                            val(d.get('from','')),val(d.get('to',''))))
                    # Customize (easing 10): su AnimationCurve (t v entra sale pesoEntra pesoSale modo; ...) en la linea siguiente
                    if d['easing']==10:
                        ks=d.get('curve',{}).get('m_Curve',[])
                        o.write('curva_tween\t'+';'.join('%g %g %g %g %g %g %d'%(k['time'],k['value'],k['inSlope'],k['outSlope'],k.get('inWeight',1/3),k.get('outWeight',1/3),k.get('weightedMode',0)) for k in ks)+'\n')
                    # la cadena: lo que hace al terminar (PlayLabel de otro tween...)
                    for c in d.get('onFinished',{}).get('m_PersistentCalls',{}).get('m_Calls',[]):
                        tp=c.get('m_Target',{}).get('m_PathID')
                        dest='?'
                        if tp in objs:
                            try: dest=rutas.get(objs[tp].read_typetree()['m_GameObject']['m_PathID'],'?') or '.'
                            except Exception: pass
                        o.write('al_terminar\t%s\t%s\t%s\n'%(c.get('m_MethodName',''),c.get('m_Arguments',{}).get('m_StringArgument',''),dest))
                elif cl=='DuelLP':
                    for k in('countTime','EmergencyPeriod'):
                        if k in d: o.write('campo\t%s\t%s.%s\t%g\n'%(ru,cl,k,d[k]))
                    for k in('lpColorDamage','lpColorRecover','lpColorNormal'):
                        if k in d: o.write('campo\t%s\t%s.%s\t%s\n'%(ru,cl,k,' '.join('%g'%d[k][c] for c in 'rgba')))
    print('prefab',nom,flush=True)

for q in TIMELINES:
    rs=[p for p,o in objs.items() if o.type.name=='GameObject' and nombre(p)==q]
    for p in rs[:1]: exportarTimeline(q,p)
for q in PREFABS:
    rs=[p for p,o in objs.items() if o.type.name=='GameObject' and nombre(p)==q]
    for p in rs[:1]: exportarPrefab(q,p)
# ---- atlas de sprites
if SOLO: sys.exit(0)
ims=[]
for p,o in objs.items():
    if o.type.name=='Sprite' and nombre(p) in SPRITES and nombre(p) not in [n for n,_ in ims]:
        try: ims.append((nombre(p),o.read().image.convert('RGBA')))
        except Exception as e: print('sprite',nombre(p),'error',e)
# sprites extra de otras ramas (ya extraidos a md_extraido; classic-md-ritmo: contadores y brillo de las activas)
EXTRA={'md_turnos':'ritmo/contadores/GUI_TurnCounter_4635.png','md_turnos_aguja':'ritmo/contadores/GUI_TurnNeedle_4613.png',
       'md_turnos_icono':'ritmo/contadores/GUI_Icon_TurnCounter_4231.png','md_contador_magia':'ritmo/contadores/icon_counter/icon_counter_magic.png',
       'md_brillo_marco':'ritmo/brillo/EfDuelHighlight00_01_01_1238.png'}
for n,r in EXTRA.items():
    f=os.path.join(r'D:/Docker/compartido/md_extraido',r)
    if os.path.exists(f): ims.append((n,Image.open(f).convert('RGBA')))
    else: print('falta',f)
ims.sort(key=lambda x:-x[1].height)
W=1024; x=y=1; fila=0; pos=[]
for n,im in ims:
    if x+im.width+1>W: x=1; y+=fila+2; fila=0
    pos.append((n,x,y,im.width,im.height)); x+=im.width+2; fila=max(fila,im.height)
H=(y+fila+2+3)//4*4   # (alto justo, multiplo de 4: no hace falta potencia de 2)
atlas=Image.new('RGBA',(W,H),(0,0,0,0))
for (n,im),(n2,x,y,w,h) in zip(ims,pos): atlas.paste(im,(x,y))
atlas.save(os.path.join(OUT,'md_ui.png'))
with open(os.path.join(OUT,'md_ui_atlas.txt'),'w',newline='\n') as o:
    o.write('# sprites de Master Duel (copia del usuario, NO va al repo): nombre x y w h en md_ui.png (%dx%d)\n'%(W,H))
    for n,x,y,w,h in pos: o.write('%s\t%d\t%d\t%d\t%d\n'%(n,x,y,w,h))
print('atlas',len(pos),'sprites',W,'x',H)
