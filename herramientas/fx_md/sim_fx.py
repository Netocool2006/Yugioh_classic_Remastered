# Simulador pequeno de los ParticleSystem de Unity (Master Duel) para hornear un efecto a flipbook.
# Lee params.json (volcar_fx.py) + las texturas png de esa carpeta y dibuja cada cuadro con OpenGL (moderngl, sin
# ventana): billboards (vista, local, horizontal, vertical, estirados por la velocidad), particulas malla y las
# MeshRenderer del prefab (con sus transformaciones animadas por el Animator cuando el clip las trae).
# Los shaders de Master Duel son Shader Graph propios: se aproximan (textura principal x mascara x tinte; aditivo o
# alfa segun el nombre del shader / _DstBlend). Salida: atlas 8x8 de 256 (RGB premultiplicado con gamma 1/2.2, A
# lineal; mezcla ONE, ONE_MINUS_SRC_ALPHA) + json en el formato de fx.h.
# Uso: sim_fx.py <carpeta_volcada> <salida_sin_ext> [--elev 35] [--dur s] [--cuadros 64] [--lado 256] [--arriba]
#      [--previa png]   (previa: una tira de cuadros para revisar)
import sys,os,json,math,argparse,zlib
import numpy as np
from PIL import Image
import moderngl

# ------------------------------------------------------------------ curvas y gradientes de Unity
def curva_eval(c,t):
    ks=c.get('m_Curve') or []
    if not ks: return 1.0
    if len(ks)==1: return ks[0]['value']
    if t<=ks[0]['time']: return ks[0]['value']
    if t>=ks[-1]['time']: return ks[-1]['value']
    for i in range(len(ks)-1):
        a,b=ks[i],ks[i+1]
        if a['time']<=t<=b['time']:
            dt=b['time']-a['time']
            if dt<=1e-9: return b['value']
            s=(t-a['time'])/dt
            m0=a.get('outSlope',0.0); m1=b.get('inSlope',0.0)
            if not np.isfinite(m0) or not np.isfinite(m1): return a['value']   # escalon
            s2=s*s; s3=s2*s
            return (2*s3-3*s2+1)*a['value']+(s3-2*s2+s)*dt*m0+(-2*s3+3*s2)*b['value']+(s3-s2)*dt*m1
    return ks[-1]['value']

def mmc(c,t,r):
    """MinMaxCurve: t normalizado (0..1), r azar fijo de la particula"""
    if c is None: return 0.0
    st=c.get('minMaxState',0); s=c.get('scalar',0.0)
    if st==0: return s
    if st==1: return s*curva_eval(c['maxCurve'],t)
    if st==2: return s*(curva_eval(c['minCurve'],t)*(1-r)+curva_eval(c['maxCurve'],t)*r)
    return c.get('minScalar',0.0)*(1-r)+s*r

def mmc_max(c):
    st=c.get('minMaxState',0); s=c.get('scalar',0.0)
    if st==0: return s
    if st==3: return max(s,c.get('minScalar',0.0))
    vals=[k['value'] for k in (c['maxCurve'].get('m_Curve') or [])]+[k['value'] for k in (c.get('minCurve',{}).get('m_Curve') or [])]
    return s*max(vals or [1.0])

def grad_eval(g,t):
    nc=g.get('m_NumColorKeys',2); na=g.get('m_NumAlphaKeys',2)
    ck=[(g['ctime%d'%i]/65535.0,g['key%d'%i]) for i in range(nc)]
    ak=[(g['atime%d'%i]/65535.0,g['key%d'%i]['a']) for i in range(na)]
    fijo=g.get('m_Mode',0)==1
    def interp(keys,f):
        if t<=keys[0][0]: return f(keys[0][1])
        for i in range(len(keys)-1):
            if keys[i][0]<=t<=keys[i+1][0]:
                if fijo: return f(keys[i+1][1])
                d=keys[i+1][0]-keys[i][0]; s=(t-keys[i][0])/d if d>1e-9 else 1
                return f(keys[i][1])*(1-s)+f(keys[i+1][1])*s
        return f(keys[-1][1])
    rgb=interp(ck,lambda k: np.array([k['r'],k['g'],k['b']]))
    a=interp(ak,lambda k: k)
    return np.array([rgb[0],rgb[1],rgb[2],a])

def col(c): return np.array([c['r'],c['g'],c['b'],c['a']])

def mmg(g,t,r):
    st=g.get('minMaxState',0)
    if st==0: return col(g['maxColor'])
    if st==1: return grad_eval(g['maxGradient'],t)
    if st==2: return col(g['minColor'])*(1-r)+col(g['maxColor'])*r
    if st==3: return grad_eval(g['minGradient'],t)*(1-r)+grad_eval(g['maxGradient'],t)*r
    return grad_eval(g['maxGradient'],r)

# ------------------------------------------------------------------ matematica
def quat_mat(q):
    x,y,z,w=q
    return np.array([[1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w)],[2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w)],[2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)]])
def euler_mat(e):   # Unity: grados, orden ZXY (se aplica Z, luego X, luego Y)
    x,y,z=[math.radians(v) for v in e]
    cx,sx,cy,sy,cz,sz=math.cos(x),math.sin(x),math.cos(y),math.sin(y),math.cos(z),math.sin(z)
    Rx=np.array([[1,0,0],[0,cx,-sx],[0,sx,cx]]); Ry=np.array([[cy,0,sy],[0,1,0],[-sy,0,cy]]); Rz=np.array([[cz,-sz,0],[sz,cz,0],[0,0,1]])
    return Ry@Rx@Rz
def trs(p,q,s):
    m=np.eye(4); m[:3,:3]=quat_mat(q)@np.diag(s); m[:3,3]=p; return m

# ------------------------------------------------------------------ texturas procedurales
def _ruido_valor(n,escala,semilla):
    rng=np.random.default_rng(semilla); g=rng.random((escala+1,escala+1))
    g[-1,:]=g[0,:]; g[:,-1]=g[:,0]
    y,x=np.mgrid[0:n,0:n]/n*escala; x0=x.astype(int); y0=y.astype(int); fx=x-x0; fy=y-y0
    fx=fx*fx*(3-2*fx); fy=fy*fy*(3-2*fy)
    a=g[y0,x0]; b=g[y0,x0+1]; c=g[y0+1,x0]; d=g[y0+1,x0+1]
    return (a*(1-fx)+b*fx)*(1-fy)+(c*(1-fx)+d*fx)*fy
def remolino(n,fuerza,escala,borde):
    # nodo Twirl de Shader Graph (angulo = fuerza * distancia al centro) sobre ruido, con mascara radial
    y,x=np.mgrid[0:n,0:n]; u=(x+0.5)/n-0.5; v=(y+0.5)/n-0.5; r=np.hypot(u,v)
    ang=np.arctan2(v,u)+fuerza*r
    uu=(np.cos(ang)*r+0.5); vv=(np.sin(ang)*r+0.5)
    esc=int(max(4,min(32,escala)))
    base=sum(_ruido_valor(n,esc*k,7+k)*(0.5**i) for i,k in enumerate((1,2,4)))/1.75
    ix=np.clip((uu*(n-1)).astype(int),0,n-1); iy=np.clip((vv*(n-1)).astype(int),0,n-1)
    t=base[iy,ix]
    t=np.clip((t-0.35)/0.5,0,1)**1.5
    e0,e1=borde[0],max(borde[1],borde[0]+1e-3)
    rr=r*2
    masc=np.clip((rr-e0*0.3)/(e1),0,1)*np.clip((1-rr)/0.35,0,1)
    return t*masc

# ------------------------------------------------------------------ materiales (aproximados)
class Material:
    def __init__(s,d,carpeta,ctx,cache):
        s.nombre=d['nombre']; sh=(d.get('shader') or '').lower(); f=d.get('floats',{}); c=d.get('colores',{})
        tx=d.get('texturas',{})
        prio=['_MainTex','_Main_Texture','_BaseMap','_Main_Tex','_MainTexture','_Tex','_Texture']
        s.tex=None; s.esc=[1,1]; s.ofs=[0,0]
        for k in prio+[k for k in tx if 'mask' not in k.lower() and 'noise' not in k.lower() and 'dissolve' not in k.lower()]:
            if k in tx: s.tex=tx[k]['tex']; s.esc=tx[k]['escala']; s.ofs=tx[k]['offset']; break
        if s.tex is None and tx: k=list(tx)[0]; s.tex=tx[k]['tex']; s.esc=tx[k]['escala']; s.ofs=tx[k]['offset']
        s.mask=None
        for k in tx:
            if 'mask' in k.lower() and tx[k]['tex']!=s.tex: s.mask=tx[k]['tex']; s.mesc=tx[k]['escala']; s.mofs=tx[k]['offset']; break
        # canal: los shaders de mascara eligen un canal de la textura como intensidad
        sel=None
        for k in c:
            if 'main' in k.lower() and 'channel' in k.lower(): sel=c[k]
        s.canal=-1; s.acanal=3
        if sel is not None and sum(sel[:4])>0.5: s.canal=int(np.argmax(sel[:4]))
        msel=None
        for k in c:
            if 'mask' in k.lower() and 'channel' in k.lower(): msel=c[k]
        s.mcanal=int(np.argmax(msel[:4])) if msel is not None and sum(msel[:4])>0.5 else 3
        tinte=np.ones(4)
        for k in ('_TintColor','_Color','_BaseColor','_Main_Color','_MainColor','_Tint','_Emissive','_EmissionColor_'):
            if k in c:
                tinte=np.array(c[k],dtype=float)
                if k=='_TintColor': tinte=tinte*2.0
                if k=='_Emissive' and tinte[3]==0: tinte[3]=1.0
                break
        inten=1.0
        for k in ('_Intensity','_Emissive_Power','_EmissionPower','_Power','_Brightness','_Emission'):
            if k in f and 0<f[k]<50: inten=f[k]; break
        s.tinte=tinte; s.inten=inten
        # convencion de Master Duel: sufijo _k (kasan, aditivo) / _b (blend); y los factores de mezcla del material
        nm=s.nombre.lower()
        aditivo=True
        if sh.endswith('_b') or nm.endswith('_b') or 'alphablend' in sh: aditivo=False
        if 'add' in sh or sh.endswith('_k') or nm.endswith('_k'): aditivo=True
        for k in ('_DstBlend','_BlendDstFactor','_DstFactor'):
            if k in f: aditivo = f[k]==1.0
        s.aditivo=aditivo
        for k in c:
            if k.lower().startswith('_alpha_channel'):
                v=c[k]
                if sum(v[:4])>0.5: s.acanal=int(np.argmax(v[:4]))
        if 'tinte' not in locals(): pass
        if '_Emission' in c and not tx: s.tinte=np.array(c['_Emission'],dtype=float)
        s.proc=None; s.giro=0.0
        if s.tex is None:
            # shader procedural (anillo / punto suave): textura generada
            n=128; y,x=np.mgrid[0:n,0:n]; r=np.hypot(x-n/2+0.5,y-n/2+0.5)/(n/2)
            if 'twirl' in sh or 'twrl' in sh:
                a=remolino(n*2,f.get('_TwirlStrength',10.0),c.get('_VoronoiMaskParam',[3,7,1,0])[1],c.get('_MaskSmoothstepEdge',[0.1,0.25,0.1,0.2]))
                n=n*2
            elif f.get('_RING_ON',0)>0.5:
                w=0.12; a=np.clip(1-np.abs(r-0.8)/w,0,1)
            else:
                a=np.clip(1-r,0,1)**f.get('_Alphapow',2.0)
            a=a*f.get('_Alpha',1.0)
            col=np.ones(3)
            for kk in ('_Color01','_Color','_BaseColor'):
                if kk in c: col=np.array(c[kk][:3]); break
            img=np.dstack([np.ones((n,n))*255*col[0],np.ones((n,n))*255*col[1],np.ones((n,n))*255*col[2]]+[np.clip(a,0,1)*255]).astype(np.uint8)
            s.proc=Image.fromarray(img)
        s.giro=-2.5 if ('twirl' in sh or 'twrl' in sh) else 0.0
        s.gl=s._tex(s.tex,carpeta,ctx,cache); s.glm=s._tex(s.mask,carpeta,ctx,cache) if s.mask else None
    def _tex(s,k,carpeta,ctx,cache):
        if not k:
            if getattr(s,'proc',None) is not None:
                im=s.proc.transpose(Image.FLIP_TOP_BOTTOM); t=ctx.texture(im.size,4,im.tobytes()); t.build_mipmaps()
                t.filter=(moderngl.LINEAR_MIPMAP_LINEAR,moderngl.LINEAR); return t
            return None
        if k in cache: return cache[k]
        p=os.path.join(carpeta,k+'.png')
        if not os.path.exists(p): cache[k]=None; return None
        im=Image.open(p).convert('RGBA').transpose(Image.FLIP_TOP_BOTTOM)
        t=ctx.texture(im.size,4,im.tobytes()); t.build_mipmaps(); t.filter=(moderngl.LINEAR_MIPMAP_LINEAR,moderngl.LINEAR)
        t.repeat_x=True; t.repeat_y=True
        cache[k]=t; return t

# ------------------------------------------------------------------ emisores
class Emisor:
    def __init__(s,nombre,ps,psr,mundo,mat,mallas,carpeta,semilla):
        s.nombre=nombre; s.ps=ps; s.r=psr; s.M=mundo; s.mat=mat
        s.rng=np.random.default_rng(semilla)
        im=ps['InitialModule']
        s.dur=ps['lengthInSec']; s.loop=ps['looping']; s.vel_sim=ps.get('simulationSpeed',1.0)
        s.delay=mmc(ps['startDelay'],0,s.rng.random())
        s.max=im.get('maxNumParticles',1000)
        s.local=ps.get('moveWithTransform',0)==0
        s.parts=[]; s.acum=0.0; s.bursts_hechos={}
        s.modo=psr.get('m_RenderMode',0) if psr else 5
        s.alinear=psr.get('m_RenderAlignment',0) if psr else 0
        s.malla=None
        if s.modo==4:
            nm=psr.get('_malla') or (psr.get('_mallas') or [None])[0]
            if nm: s.malla=cargar_obj(os.path.join(carpeta,nm+'.obj'))
        s.orden=(psr.get('m_SortingLayer',0),psr.get('m_SortingOrder',0)) if psr else (0,0)
        s.fudge=psr.get('m_SortingFudge',0.0) if psr else 0
        s.lenScale=psr.get('m_LengthScale',2.0) if psr else 2; s.velScale=psr.get('m_VelocityScale',0.0) if psr else 0
        s.maxSize=psr.get('m_MaxParticleSize',0.5) if psr else 0.5
        s.escala=np.linalg.norm(mundo[:3,:3],axis=0)
        sm=ps.get('scalingMode',1)
        s.escTam=float(np.mean(s.escala)) if sm==0 else 1.0
    def nacer(s,t_sis,n):
        ps=s.ps; im=ps['InitialModule']; sh=ps['ShapeModule']
        tn=(t_sis%s.dur)/s.dur if s.dur>0 else 0
        for _ in range(n):
            if len(s.parts)>=s.max: return
            R=s.rng.random(16)
            pos,dir=forma(sh,s.rng)
            vel=dir*mmc(im['startSpeed'],tn,R[0])
            # a mundo
            M=s.M
            p=M[:3,:3]@pos+M[:3,3]
            v=(M[:3,:3]/np.maximum(s.escala,1e-6))@vel
            vida=max(mmc(im['startLifetime'],tn,R[1]),1e-3)
            if im.get('size3D'):
                tam=np.array([mmc(im['startSize'],tn,R[2]),mmc(im['startSizeY'],tn,R[3]),mmc(im['startSizeZ'],tn,R[4])])
            else:
                t0=mmc(im['startSize'],tn,R[2]); tam=np.array([t0,t0,t0])
            tam=tam*s.escTam
            if im.get('rotation3D'):
                rot=np.array([mmc(im['startRotationX'],tn,R[5]),mmc(im['startRotationY'],tn,R[6]),mmc(im['startRotation'],tn,R[7])])
            else:
                rot=np.array([0,0,mmc(im['startRotation'],tn,R[7])])
            if im.get('randomizeRotationDirection',0)>0 and R[8]<im['randomizeRotationDirection']*0.5: rot=-rot
            c=mmg(im['startColor'],tn,R[9])
            s.parts.append({'p':p,'v':v,'edad':0.0,'vida':vida,'tam':tam,'rot':rot,'c':c,'R':R,'ang':np.zeros(3),'nac':t_sis})
    def paso(s,t_sis,dt):
        ps=s.ps
        v0,v1=getattr(s,'ventana',(0.0,1e9))
        if t_sis<v0: return
        if t_sis>v1: s.parts=[]; return
        t_sis=t_sis-v0
        t=t_sis-s.delay
        if t>=0 and (s.loop or t<=s.dur):
            em=ps['EmissionModule']
            if em.get('enabled'):
                tn=(t%s.dur)/s.dur if s.dur>0 else 0
                s.acum+=mmc(em['rateOverTime'],tn,s.rng.random())*dt
                n=int(s.acum); s.acum-=n
                if n: s.nacer(t,n)
                ciclo=int(t//s.dur) if s.loop and s.dur>0 else 0
                tc=t-ciclo*s.dur
                for i,b in enumerate(em.get('m_Bursts',[])):
                    cyc=b.get('cycleCount',1); iv=max(b.get('repeatInterval',0.01),0.01)
                    for k in range(cyc if cyc>0 else 1000):
                        tb=b['time']+k*iv
                        if tb>s.dur: break
                        key=(ciclo,i,k)
                        if key in s.bursts_hechos: continue
                        if tc>=tb:
                            s.bursts_hechos[key]=1
                            if s.rng.random()<=b.get('probability',1.0):
                                s.nacer(t,int(round(mmc(b['countCurve'],tb/s.dur if s.dur else 0,s.rng.random()))))
        # moverse
        g=ps['InitialModule'].get('gravityModifier')
        vm=ps.get('VelocityModule',{}); fm=ps.get('ForceModule',{}); cl=ps.get('ClampVelocityModule',{}); nm=ps.get('NoiseModule',{})
        rm=ps.get('RotationModule',{})
        vivas=[]
        for q in s.parts:
            q['edad']+=dt
            if q['edad']>=q['vida']: continue
            tn=q['edad']/q['vida']; R=q['R']
            if g: q['v']=q['v']+np.array([0,-9.81,0])*mmc(g,tn,R[10])*dt
            if fm.get('enabled'):
                a=np.array([mmc(fm['x'],tn,R[11]),mmc(fm['y'],tn,R[11]),mmc(fm['z'],tn,R[11])])
                if not fm.get('inWorldSpace'): a=s.M[:3,:3]@a/np.maximum(s.escala,1e-6)
                q['v']=q['v']+a*dt
            if cl.get('enabled'):
                if cl.get('separateAxis'): pass
                else:
                    lim=mmc(cl['magnitude'],tn,R[12]); sp=np.linalg.norm(q['v'])
                    if sp>lim and sp>0:
                        damp=cl.get('dampen',0.0)
                        nueva=sp-(sp-lim)*min(1.0,damp*dt*30)
                        q['v']=q['v']*(nueva/sp)
                    dr=cl.get('drag')
                    if dr: q['v']=q['v']*max(0.0,1-mmc(dr,tn,R[12])*dt)
            extra=np.zeros(3)
            if vm.get('enabled'):
                e=np.array([mmc(vm['x'],tn,R[13]),mmc(vm['y'],tn,R[13]),mmc(vm['z'],tn,R[13])])
                if not vm.get('inWorldSpace'): e=s.M[:3,:3]@e/np.maximum(s.escala,1e-6)
                extra=e
                rad=mmc(vm.get('radial',{'scalar':0}),tn,R[13]) if vm.get('radial') else 0
                if rad:
                    d=q['p']-s.M[:3,3]; n=np.linalg.norm(d)
                    if n>1e-6: extra=extra+d/n*rad
                orb=np.array([mmc(vm.get('orbitalX',{'scalar':0}),tn,R[14]),mmc(vm.get('orbitalY',{'scalar':0}),tn,R[14]),mmc(vm.get('orbitalZ',{'scalar':0}),tn,R[14])])
                if np.any(orb):
                    # giro alrededor de los ejes locales del sistema
                    c0=s.M[:3,3]; Rl=s.M[:3,:3]/np.maximum(s.escala,1e-6)
                    pl=Rl.T@(q['p']-c0)
                    for ax,w in enumerate(orb):
                        if not w: continue
                        a=w*dt; ca,sa=math.cos(a),math.sin(a)
                        i,j=[(1,2),(2,0),(0,1)][ax]
                        x,y=pl[i],pl[j]; pl[i]=ca*x-sa*y; pl[j]=sa*x+ca*y
                    q['p']=Rl@pl+c0
                sm=vm.get('speedModifier')
                if sm and sm.get('scalar',1)!=1 or (sm and sm.get('minMaxState',0)!=0):
                    q['vmod']=mmc(sm,tn,R[13])
            if nm.get('enabled'):
                fr=max(mmc(nm['frequency'] if isinstance(nm['frequency'],dict) else {'scalar':nm['frequency']},tn,0),0.01) if isinstance(nm.get('frequency'),dict) else max(nm.get('frequency',0.5),0.01)
                st=mmc(nm['strength'],tn,R[15])
                ph=R[15]*100+q['edad']*fr*2
                extra=extra+st*np.array([math.sin(ph*1.7+1),math.sin(ph*1.3+2),math.sin(ph*1.1+3)])
            q['p']=q['p']+(q['v']*q.get('vmod',1.0)+extra)*dt
            q['vel_vis']=q['v']+extra
            if rm.get('enabled'):
                if rm.get('separateAxes'):
                    q['ang']=q['ang']+np.array([mmc(rm['x'],tn,R[5]),mmc(rm['y'],tn,R[6]),mmc(rm['curve'],tn,R[7])])*dt
                else:
                    q['ang'][2]+=mmc(rm['curve'],tn,R[7])*dt
            vivas.append(q)
        s.parts=vivas
    def tam_color(s,q):
        ps=s.ps; tn=q['edad']/q['vida']; R=q['R']
        tam=q['tam'].copy()
        sz=ps.get('SizeModule',{})
        if sz.get('enabled'):
            if sz.get('separateAxes'): tam=tam*np.array([mmc(sz['curve'],tn,R[3]),mmc(sz['y'],tn,R[3]),mmc(sz['z'],tn,R[3])])
            else: tam=tam*mmc(sz['curve'],tn,R[3])
        c=q['c'].copy()
        cm=ps.get('ColorModule',{})
        if cm.get('enabled'): c=c*mmg(cm['gradient'],tn,R[4])
        return tam,c
    def uv_rect(s,q):
        uv=s.ps.get('UVModule',{})
        if not uv.get('enabled') or uv.get('mode',0)!=0: return (0,0,1,1)
        tx=max(uv.get('tilesX',1),1); ty=max(uv.get('tilesY',1),1)
        tn=q['edad']/q['vida']; R=q['R']
        fila_unica=uv.get('animationType',0)==1
        total=tx if fila_unica else tx*ty
        cyc=uv.get('cycles',1.0)
        f=(mmc(uv['frameOverTime'],(tn*cyc)%1.0,R[0])+mmc(uv['startFrame'],0,R[1]))
        k=int(f*total) if uv['frameOverTime'].get('minMaxState',0)!=0 or True else 0
        k=max(0,min(total-1,k))
        if fila_unica:
            fila=int(R[2]*ty) if uv.get('rowMode',1)==1 else uv.get('rowIndex',0)
            cx,cy=k,fila
        else:
            cx,cy=k%tx,k//tx
        u0=cx/tx; v1=1-cy/ty
        return (u0,v1-1/ty,u0+1/tx,v1)

def forma(sh,rng):
    if not sh.get('enabled'): return np.zeros(3),np.array([0,0,1.0])
    t=sh.get('type',4); rad=sh.get('radius',{}).get('value',1.0) if isinstance(sh.get('radius'),dict) else sh.get('radius',1.0)
    rt=sh.get('radiusThickness',1.0); ang=math.radians(sh.get('angle',25)); arc=math.radians((sh.get('arc') or {}).get('value',360.0))
    def radio_vol():
        u=rng.random(); return rad*((1-rt)+rt*u) if rt<1 else rad*math.sqrt(u)
    if t in(0,1,2,3):
        d=rng.normal(size=3); d/=np.linalg.norm(d)
        if t in(2,3): d[2]=abs(d[2])
        r=rad if t in(1,3) else rad*(1-rt+rt*rng.random()**(1/3))
        pos=d*r; dirv=d
    elif t in(4,7,8,9):
        a=rng.random()*arc; r=rad*math.sqrt(1-rt+rt*rng.random()) if rt>0 else rad
        x,y=math.cos(a)*r,math.sin(a)*r
        fr=r/rad if rad>0 else 0
        dirv=np.array([math.cos(a)*math.sin(ang)*fr,math.sin(a)*math.sin(ang)*fr,math.cos(ang*fr)])
        if np.linalg.norm(dirv[:2])<1e-9: dirv=np.array([0,0,1.0])
        dirv/=np.linalg.norm(dirv)
        pos=np.array([x,y,0.0])
        if t in(8,9): pos=pos+dirv*sh.get('length',1.0)*rng.random()
    elif t in(5,15,16):
        pos=(rng.random(3)-0.5); dirv=np.array([0,0,1.0])
    elif t in(10,11):
        a=rng.random()*arc; r=rad if t==11 else rad*math.sqrt(1-rt+rt*rng.random())
        pos=np.array([math.cos(a)*r,math.sin(a)*r,0]); dirv=np.array([math.cos(a),math.sin(a),0])
    elif t==12:
        pos=np.array([(rng.random()*2-1)*rad,0,0]); dirv=np.array([0,1.0,0])
    elif t==17:
        a=rng.random()*arc; dr=sh.get('donutRadius',0.2)
        c=np.array([math.cos(a)*rad,math.sin(a)*rad,0]); d=rng.normal(size=3); d/=np.linalg.norm(d)
        pos=c+d*dr*rng.random()*rt; dirv=d
    elif t==18:
        pos=np.array([rng.random()-0.5,rng.random()-0.5,0]); dirv=np.array([0,0,1.0])
    else:
        d=rng.normal(size=3); d/=np.linalg.norm(d); pos=d*rad*rng.random(); dirv=d
    sc=np.array([sh['m_Scale']['x'],sh['m_Scale']['y'],sh['m_Scale']['z']])
    Rm=euler_mat([sh['m_Rotation']['x'],sh['m_Rotation']['y'],sh['m_Rotation']['z']])
    pos=Rm@(pos*sc)+np.array([sh['m_Position']['x'],sh['m_Position']['y'],sh['m_Position']['z']])
    dirv=Rm@dirv
    rda=sh.get('randomDirectionAmount',0); sda=sh.get('sphericalDirectionAmount',0)
    if rda:
        r=rng.normal(size=3); r/=np.linalg.norm(r); dirv=dirv*(1-rda)+r*rda
    if sda:
        n=np.linalg.norm(pos);
        if n>1e-6: dirv=dirv*(1-sda)+pos/n*sda
    n=np.linalg.norm(dirv)
    return pos,(dirv/n if n>1e-9 else np.array([0,0,1.0]))

def cargar_obj(p):
    if not os.path.exists(p): return None
    V=[];T=[];F=[]
    for l in open(p):
        if l.startswith('v '): V.append([float(x) for x in l.split()[1:4]])
        elif l.startswith('vt '): T.append([float(x) for x in l.split()[1:3]])
        elif l.startswith('f '):
            idx=[w.split('/') for w in l.split()[1:]]
            for k in range(1,len(idx)-1):
                F.append([idx[0],idx[k],idx[k+1]])
    V=np.array(V); T=np.array(T) if T else np.zeros((len(V),2))
    tri=[]
    for f in F:
        for w in f:
            vi=int(w[0])-1; ti=int(w[1])-1 if len(w)>1 and w[1] else vi
            tri.append((vi,ti))
    vi=np.array([a for a,_ in tri]); ti=np.array([b for _,b in tri])
    pos=V[vi]; uv=T[ti] if len(T) else np.zeros((len(vi),2))
    # UnityPy exporta con x negada (a mano derecha): se vuelve a Unity
    pos=pos*np.array([-1,1,1])
    return {'pos':pos,'uv':uv}

# ------------------------------------------------------------------ escena
class Escena:
    def __init__(s,carpeta,ctx,control=None):
        s.d=json.load(open(os.path.join(carpeta,'params.json')))
        s.control=control or {}
        s.carpeta=carpeta; s.ctx=ctx; s.cache={}
        s.mats={k:Material(v,carpeta,ctx,s.cache) for k,v in s.d['materiales'].items()}
        s.emisores=[]; s.mallas=[]
        s.recorrer(s.d['arbol'],np.eye(4),True,0,(0.0,1e9))
    def recorrer(s,n,padre,activo,i,ventana):
        if 'nombre' not in n: return
        M=padre@trs(n.get('pos',[0,0,0]),n.get('rot',[0,0,0,1]),n.get('esc',[1,1,1]))
        if n['nombre'] in s.control:
            # ControlTrack del Timeline: el objeto se activa en su clip (inicio, duracion)
            a0,du=s.control[n['nombre']]; ventana=(a0,a0+du); activo=True
        else:
            activo=activo and n.get('activo',True)
        ps=psr=None; mf=None; mr=None
        for c in n['comp']:
            if c['tipo']=='ParticleSystem': ps=c['ps']
            elif c['tipo']=='PSR': psr=c['r']
            elif c['tipo']=='MeshFilter': mf=c['malla']
            elif c['tipo'] in('MeshRenderer',): mr=c
        if activo and ps is not None and psr is not None and psr.get('m_Enabled',1) and psr.get('m_RenderMode',0)!=5:
            mk=(psr.get('_materiales') or [None])[0]
            mat=s.mats.get(mk)
            if mat is not None and mat.gl is not None:
                e=Emisor(n['nombre'],ps,psr,M,mat,None,s.carpeta,zlib.crc32(n['nombre'].encode())+i)
                e.ventana=ventana; s.emisores.append(e)
        if activo and mf and mr and mr.get('activo',True):
            mk=(mr.get('materiales') or [None])[0]; mat=s.mats.get(mk)
            m=cargar_obj(os.path.join(s.carpeta,mf+'.obj'))
            if mat is not None and mat.gl is not None and m is not None:
                s.mallas.append({'nombre':n['nombre'],'M':M,'malla':m,'mat':mat,'ventana':ventana})
        for k,h in enumerate(n.get('hijos',[])): s.recorrer(h,M,activo,i*31+k+1,ventana)
    def duracion(s):
        d=0
        for e in s.emisores:
            if e.loop: d=max(d,e.delay+e.dur)
            else:
                vida=mmc_max(e.ps['InitialModule']['startLifetime'])
                d=max(d,e.delay+e.dur+vida)
        return d

VS='''#version 330
in vec3 p; in vec2 uv; in vec4 c;
uniform mat4 VP;
out vec2 vuv; out vec4 vc;
void main(){ gl_Position=VP*vec4(p,1.0); vuv=uv; vc=c; }'''
FS='''#version 330
in vec2 vuv; in vec4 vc;
uniform sampler2D tex; uniform sampler2D mask; uniform int hayMask; uniform int canal; uniform int mcanal; uniform int acanal;
uniform vec4 tinte; uniform int aditivo; uniform float giro; uniform vec4 uvst; uniform vec4 muvst;
out vec4 o;
vec3 lin(vec3 c){ return pow(max(c,0.0),vec3(2.2)); }
void main(){
  vec2 uvg=vuv;
  if(giro!=0.0){ vec2 d=vuv-0.5; float cg=cos(giro),sg=sin(giro); uvg=vec2(cg*d.x-sg*d.y,sg*d.x+cg*d.y)+0.5; }
  vec4 t=texture(tex,uvg*uvst.xy+uvst.zw);
  if(canal>=0){ float v=t[canal]; t=vec4(1.0,1.0,1.0,v); } else { t.a=t[acanal]; }
  if(hayMask==1){ vec4 m=texture(mask,vuv*muvst.xy+muvst.zw); t.a*=m[mcanal]; }
  vec4 c=vc*tinte;
  vec3 rgb=lin(t.rgb)*lin(c.rgb);
  float a=clamp(t.a*c.a,0.0,1.0);
  if(aditivo==1){ o=vec4(rgb*t.a*c.a,0.0); }
  else { o=vec4(rgb*a,a); }
}'''

def mirar(ojo,obj,arriba=np.array([0,1.0,0])):
    f=obj-ojo; f/=np.linalg.norm(f)
    r=np.cross(arriba,f);
    if np.linalg.norm(r)<1e-6: r=np.array([1.0,0,0])
    r/=np.linalg.norm(r); u=np.cross(f,r)
    return r,u,f

def hornear(carpeta,salida,elev=35.0,dur=None,cuadros=64,lado=256,arriba=False,previa=None,margen=1.15,dt=1/60,timeline=None,frente=False,gris=False):
    ctx=moderngl.create_standalone_context()
    control={}
    if timeline:
        for r in json.load(open(timeline)):
            if r['clase']=='ControlTrack':
                for c in r['datos'].get('m_Clips',[]): control[c.get('m_DisplayName') or r['nombre']]=(c['m_Start'],c['m_Duration'])
    esc=Escena(carpeta,ctx,control)
    if dur is None:
        if control: tope_tl=max(a+b for a,b in control.values())
        # hasta que no queda ninguna particula (con un tope)
        tope=min(max(esc.duracion(),tope_tl if control else 0.5),6.0); t=0.0; ultimo=0.0
        while t<tope:
            for e in esc.emisores: e.paso(t,dt)
            if esc.control and t<max(a+b for a,b in esc.control.values()): ultimo=t
            if any(e.parts for e in esc.emisores) or esc.mallas: ultimo=t
            t+=dt
        dur=max(0.4,min(tope,ultimo+0.1))
        esc=Escena(carpeta,ctx,control)
    prog=ctx.program(vertex_shader=VS,fragment_shader=FS)
    prog['tex']=0; prog['mask']=1
    # camara: Unity mano izquierda, la mesa es XZ; el jugador mira hacia +Z
    if frente:
        ojo=np.array([0,0,-30.0]); R,U,F=mirar(ojo,np.zeros(3))
    elif arriba:
        ojo=np.array([0,30.0,0]); R,U,F=mirar(ojo,np.zeros(3),np.array([0,0,1.0]))
    else:
        e=math.radians(elev); ojo=np.array([0,math.sin(e),-math.cos(e)])*30
        R,U,F=mirar(ojo,np.zeros(3))
    # simular todo una vez para medir la caja en pantalla
    tiempos=[dur*k/cuadros for k in range(cuadros)]
    estados=[]
    t=0.0; k=0
    for tc in tiempos:
        while t<tc-1e-9:
            for e in esc.emisores: e.paso(t,dt)
            t+=dt
        estados.append(geom_cuadro(esc,R,U,F,ojo,tc))
    xs=[];ys=[]
    for g in estados:
        for it in g:
            if len(it['p']):
                pr=it['p']@np.stack([R,U]).T
                w=np.ones(len(pr))
                xs+=list(pr[:,0]); ys+=list(pr[:,1])
    if not xs: raise SystemExit('sin geometria')
    xs=np.array(xs); ys=np.array(ys)
    x0,x1=np.percentile(xs,0.5),np.percentile(xs,99.5); y0,y1=np.percentile(ys,0.5),np.percentile(ys,99.5)
    cx=(x0+x1)/2; cy=(y0+y1)/2; mitad=max(x1-x0,y1-y0)/2*margen
    cols=int(math.ceil(math.sqrt(cuadros)))
    def dibujar_todo(cx,cy,mitad,lado,sup):
        VP=np.eye(4)
        # vista ortografica: x=R, y=U, z=F (profundidad 0..60)
        V=np.eye(4); V[0,:3]=R; V[1,:3]=U; V[2,:3]=F; V[:3,3]=-np.array([R@ojo,U@ojo,F@ojo])
        P=np.eye(4); P[0,0]=1/mitad; P[1,1]=1/mitad; P[0,3]=-cx/mitad; P[1,3]=-cy/mitad; P[2,2]=2/60; P[2,3]=-1
        VP=P@V
        prog['VP'].write(VP.T.astype('f4').tobytes())
        fbo=ctx.simple_framebuffer((lado*sup,lado*sup),components=4,dtype='f4')
        atlas=np.zeros((cols*lado,cols*lado,4),dtype=np.float32)
        for k,g in enumerate(estados):
            fbo.use(); fbo.clear(0,0,0,0)
            ctx.enable(moderngl.BLEND)
            for it in g:
                m=it['mat']
                if m.aditivo: ctx.blend_func=(moderngl.ONE,moderngl.ONE,moderngl.ZERO,moderngl.ONE)
                else: ctx.blend_func=(moderngl.ONE,moderngl.ONE_MINUS_SRC_ALPHA,moderngl.ONE,moderngl.ONE_MINUS_SRC_ALPHA)
                datos=np.hstack([it['p'],it['uv'],it['c']]).astype('f4')
                vbo=ctx.buffer(datos.tobytes())
                vao=ctx.vertex_array(prog,[(vbo,'3f 2f 4f','p','uv','c')])
                m.gl.use(0)
                if m.glm is not None: m.glm.use(1); prog['hayMask']=1; prog['muvst']=(m.mesc[0],m.mesc[1],m.mofs[0],m.mofs[1])
                else: prog['hayMask']=0; prog['muvst']=(1,1,0,0)
                prog['canal']=m.canal; prog['mcanal']=m.mcanal; prog['acanal']=m.acanal
                prog['tinte']=tuple(float(v) for v in m.tinte*np.array([m.inten,m.inten,m.inten,1]))
                prog['aditivo']=1 if m.aditivo else 0
                prog['giro']=float(m.giro*it.get('t',0.0))
                prog['uvst']=(m.esc[0],m.esc[1],m.ofs[0],m.ofs[1]) if it.get('uvmat',True) else (1,1,0,0)
                vao.render(moderngl.TRIANGLES)
                vao.release(); vbo.release()
            img=np.frombuffer(fbo.read(components=4,dtype='f4'),dtype=np.float32).reshape(lado*sup,lado*sup,4)[::-1]
            img=img.reshape(lado,sup,lado,sup,4).mean(axis=(1,3))
            r,c=divmod(k,cols)
            atlas[r*lado:(r+1)*lado,c*lado:(c+1)*lado]=img
        return atlas
    # 1a pasada amplia y chica: donde hay algo de verdad; 2a pasada encuadrada
    prev=dibujar_todo(cx,cy,mitad*1.6,96,2)
    luz=np.maximum(prev[...,3],prev[...,:3].max(axis=2)).reshape(cols,96,cols,96).max(axis=(0,2))
    umbral=max(0.02,float(luz.max())*0.03)
    yy,xx=np.where(luz>umbral)
    if len(xx):
        m2=mitad*1.6
        ax0=cx-m2+xx.min()/96*2*m2; ax1=cx-m2+(xx.max()+1)/96*2*m2
        ay1=cy+m2-yy.min()/96*2*m2; ay0=cy+m2-(yy.max()+1)/96*2*m2
        cx=(ax0+ax1)/2; cy=(ay0+ay1)/2; mitad=max(ax1-ax0,ay1-ay0)/2*1.08
    atlas=dibujar_todo(cx,cy,mitad,lado,4 if lado<=256 else 2)
    # ganancia: que el pico no sature de mas (aditivo) -> rgb premultiplicado lineal limitado a 1
    rgb=np.clip(atlas[...,:3],0,None); a=np.clip(atlas[...,3],0,1)
    # lo que pasa de 0.8 se comprime suave hacia 1 (sin bajar todo el efecto por unas pocas chispas muy brillantes)
    gan=1.0
    exc=np.maximum(rgb-0.8,0); rgb=np.where(rgb>0.8,0.8+0.2*(1-np.exp(-exc/0.2)),rgb)
    rgb=np.clip(rgb,0,1)
    if gris:   # sin color: el motor lo tine (verde magia, rosa trampa)
        rgb=np.repeat(rgb.max(axis=2,keepdims=True),3,axis=2)
    out=np.dstack([np.power(rgb,1/2.2),a])
    out8=(out*255+0.5).astype(np.uint8)
    full=np.zeros((2048,2048,4),np.uint8) if cols*lado<=2048 else None
    Image.fromarray(out8).save(salida+'.png')
    # origen: el punto (0,0,0) del prefab en el cuadro; caja: donde hay algo
    o=np.array([0.0,0,0]); ox=(R@o-cx)/mitad*0.5+0.5; oy=0.5-(U@o-cy)/mitad*0.5
    alfa_o_luz=np.maximum(out8[...,3],out8[...,:3].max(axis=2)).reshape(cols,lado,cols,lado)
    m=alfa_o_luz.max(axis=(0,2))>8
    ysi=np.where(m.any(axis=1))[0]; xsi=np.where(m.any(axis=0))[0]
    caja=[float(xsi.min()/lado),float(ysi.min()/lado),float((xsi.max()+1)/lado),float((ysi.max()+1)/lado)] if len(xsi) else [0,0,1,1]
    if frente: ox,oy=(caja[0]+caja[2])/2,(caja[1]+caja[3])/2
    meta={'efecto':os.path.basename(salida),'fuente':'Yu-Gi-Oh! Master Duel (copia del usuario, uso personal) horneado con herramientas/fx_md/sim_fx.py',
          'tipo':'uno','vista':'arriba' if arriba else ('frente' if frente else 'lado'),'elevacion':90 if arriba else elev,'loop':False,'duracion':dur,
          'tamano_cm':float(2*mitad*100),'unidades_md':float(2*mitad),'origen':[float(ox),float(oy)],'atlas':os.path.basename(salida)+'.png',
          'columnas':cols,'filas':cols,'cuadros':cuadros,'fps':cuadros/dur,'lado_cuadro':lado,'ancho_atlas':cols*lado,'ganancia':float(gan),'caja':caja,
          'emisores':[e.nombre for e in esc.emisores],'mallas':[m['nombre'] for m in esc.mallas],
          'formato':'RGBA8: RGB premultiplicado (lineal)^(1/2.2), A lineal; mezcla ONE, ONE_MINUS_SRC_ALPHA'}
    json.dump(meta,open(salida+'.json','w'),indent=1)
    if previa:
        sel=list(range(0,cuadros,max(1,cuadros//8)))[:8]
        tira=Image.new('RGBA',(lado*len(sel),lado),(30,30,40,255))
        for i,k in enumerate(sel):
            r,c=divmod(k,cols)
            cuadro=out[r*lado:(r+1)*lado,c*lado:(c+1)*lado]
            fondo=np.array([0.12,0.12,0.16])
            comp=np.power(np.clip(np.power(cuadro[...,:3],2.2)+np.power(fondo,2.2)*(1-cuadro[...,3:4]),0,1),1/2.2)
            tira.paste(Image.fromarray((comp*255).astype(np.uint8)),(i*lado,0))
        tira.save(previa)
    return meta

def geom_cuadro(esc,R,U,F,ojo,t):
    items=[]
    for e in esc.emisores:
        if not e.parts: continue
        ps=[];uvs=[];cs=[]
        orden=e.parts
        if e.ps and e.r and e.r.get('m_SortMode',0)==1 or not e.mat.aditivo:
            orden=sorted(e.parts,key=lambda q: -float(F@(q['p']-ojo)))
        for q in orden:
            tam,c=e.tam_color(q)
            if c[3]<=0.002 and not e.mat.aditivo: continue
            u0,v0,u1,v1=e.uv_rect(q)
            rotz=q['rot'][2]+q['ang'][2]
            if e.modo==4 and e.malla is not None:
                rx,ry,rz=q['rot']+q['ang']
                Rm=euler_mat([math.degrees(rx),math.degrees(ry),math.degrees(rz)])
                Rl=e.M[:3,:3]/np.maximum(e.escala,1e-6) if e.alinear!=1 else np.eye(3)
                p=(Rl@Rm@(e.malla['pos']*tam).T).T+q['p']
                ps.append(p); uvs.append(e.malla['uv']*np.array([u1-u0,v1-v0])+np.array([u0,v0])); cs.append(np.tile(c,(len(p),1)))
                continue
            sx,sy=tam[0]*0.5,tam[1]*0.5
            if e.modo==1:
                v=q.get('vel_vis',q['v']); sp=np.linalg.norm(v)
                d=v/sp if sp>1e-6 else U
                # largo en la direccion de la velocidad, ancho de cara a la camara
                ancho=np.cross(d,F); n=np.linalg.norm(ancho); ancho=ancho/n if n>1e-6 else R
                largo=tam[0]*e.lenScale+sp*e.velScale
                c0=q['p']; ejeA=ancho*sx; ejeL=d*largo
                quad=[c0-ejeA-ejeL,c0+ejeA-ejeL,c0+ejeA,c0-ejeA]
                uvq=[(u0,v0),(u1,v0),(u1,v1),(u0,v1)]
            else:
                if e.modo==2: a,b=np.array([1.0,0,0]),np.array([0,0,1.0])
                elif e.modo==3:
                    a=np.cross(np.array([0,1.0,0]),F); a/=np.linalg.norm(a); b=np.array([0,1.0,0])
                elif e.alinear==2:
                    Rl=e.M[:3,:3]/np.maximum(e.escala,1e-6); a,b=Rl[:,0],Rl[:,1]
                elif e.alinear==1: a,b=np.array([1.0,0,0]),np.array([0,1.0,0])
                else: a,b=R,U
                ca,sa=math.cos(-rotz),math.sin(-rotz)
                a2=a*ca+b*sa; b2=-a*sa+b*ca
                c0=q['p']
                quad=[c0-a2*sx-b2*sy,c0+a2*sx-b2*sy,c0+a2*sx+b2*sy,c0-a2*sx+b2*sy]
                uvq=[(u0,v0),(u1,v0),(u1,v1),(u0,v1)]
            idx=[0,1,2,0,2,3]
            ps.append(np.array([quad[i] for i in idx])); uvs.append(np.array([uvq[i] for i in idx])); cs.append(np.tile(c,(6,1)))
        if ps:
            items.append({'t':t,'p':np.vstack(ps),'uv':np.vstack(uvs),'c':np.vstack(cs),'mat':e.mat,'orden':e.orden,
                          'prof':float(F@(e.M[:3,3]-ojo))+e.fudge*0.0})
    for m in esc.mallas:
        if not (m['ventana'][0]<=t<=m['ventana'][1]): continue
        M=m['M']; p=(M[:3,:3]@m['malla']['pos'].T).T+M[:3,3]
        items.append({'p':p,'uv':m['malla']['uv'],'c':np.ones((len(p),4)),'mat':m['mat'],'orden':(0,0),'prof':float(F@(M[:3,3]-ojo))})
    items.sort(key=lambda it:(it['orden'],-it['prof']))
    return items

if __name__=='__main__':
    ap=argparse.ArgumentParser()
    ap.add_argument('carpeta'); ap.add_argument('salida')
    ap.add_argument('--elev',type=float,default=35); ap.add_argument('--dur',type=float); ap.add_argument('--cuadros',type=int,default=64)
    ap.add_argument('--timeline'); ap.add_argument('--frente',action='store_true'); ap.add_argument('--gris',action='store_true')
    ap.add_argument('--lado',type=int,default=256); ap.add_argument('--arriba',action='store_true'); ap.add_argument('--previa')
    a=ap.parse_args()
    m=hornear(a.carpeta,a.salida,a.elev,a.dur,a.cuadros,a.lado,a.arriba,a.previa,timeline=a.timeline,frente=a.frente,gris=a.gris)
    print(json.dumps({k:m[k] for k in ('duracion','tamano_cm','origen','caja','ganancia','emisores','mallas')}))
