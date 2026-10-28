# Decodifica el StreamedClip/DenseClip/ConstantClip de un AnimationClip Mecanim y lo muestrea a fps fijo.
# Uso: clip_muestras.py <archivo> <path_id> <salida.json> [fps]   (ARCH=archivo serializado)
import sys,os,struct,json
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
import md
def decodificar(c):
    mc=c.m_MuscleClip; cl=mc.m_Clip.data
    raw=struct.pack('<%dI'%len(cl.m_StreamedClip.data),*cl.m_StreamedClip.data)
    curves=cl.m_StreamedClip.curveCount
    frames=[]; p=0
    while p<len(raw):
        t,n=struct.unpack_from('<fI',raw,p); p+=8
        ks=[]
        for i in range(n):
            idx,a,b,cc,d=struct.unpack_from('<iffff',raw,p); p+=20
            ks.append((idx,(a,b,cc,d)))
        frames.append((t,ks))
    # segmentos por curva
    seg={}
    for t,ks in frames:
        for idx,co in ks: seg.setdefault(idx,[]).append((t,co))
    dense=cl.m_DenseClip
    cons=list(cl.m_ConstantClip.data)
    return curves,seg,dense,cons
def valor(seg,t):
    s=seg[0]
    for x in seg:
        if x[0]<=t: s=x
        else: break
    dt=t-s[0]; a,b,c,d=s[1]
    return ((a*dt+b)*dt+c)*dt+d
def muestrear(c,fps=30):
    curves,seg,dense,cons=decodificar(c)
    mc=c.m_MuscleClip; t0=mc.m_StartTime; t1=mc.m_StopTime
    n=int(round((t1-t0)*fps))+1
    nd=dense.m_CurveCount; dv=list(dense.m_SampleArray)
    total=curves+nd+len(cons)
    out=[]
    for i in range(n):
        t=t0+i/fps; fila=[]
        for k in range(curves):
            fila.append(valor(seg[k],t) if k in seg else 0.0)
        if nd:
            f=min(int((t-dense.m_BeginTime)*dense.m_SampleRate),dense.m_FrameCount-1); f=max(f,0)
            fila+=dv[f*nd:(f+1)*nd]
        fila+=cons
        out.append(fila)
    # bindings -> nombres de curva
    b=c.m_ClipBindingConstant; nombres=[]
    tam={1:3,2:4,3:3,4:3}
    for g in b.genericBindings:
        k=tam.get(g.attribute,1) if g.typeID==4 else 1
        comp={1:'pos',2:'rot',3:'scale',4:'euler'}.get(g.attribute,'a%d'%g.attribute) if g.typeID==4 else 'attr%d_t%d'%(g.attribute,g.typeID)
        for j in range(k): nombres.append(f'{g.path}:{comp}.{"xyzw"[j] if k>1 else ""}')
    return {'nombre':c.m_Name,'fps':fps,'inicio':t0,'fin':t1,'curvas':nombres[:total],'n_curvas':total,'muestras':out}
if __name__=='__main__':
    env=md.abrir(sys.argv[1]); pid=int(sys.argv[2]); arch=os.environ.get('ARCH')
    o=[x for x in env.objects if x.path_id==pid and (not arch or x.assets_file.name==arch)][-1]
    r=muestrear(o.read(),int(sys.argv[4]) if len(sys.argv)>4 else 30)
    json.dump(r,open(sys.argv[3],'w'))
    print(r['nombre'],r['n_curvas'],r['curvas'],len(r['muestras']))
    for f in r['muestras'][::10]: print(['%.3f'%v for v in f])
