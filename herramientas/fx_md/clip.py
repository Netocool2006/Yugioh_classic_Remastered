# Vuelca un AnimationClip (por path_id) a json: curvas de rotacion/posicion/escala/float por ruta
import sys,json,UnityPy
env=UnityPy.load(sys.argv[1]); pid=int(sys.argv[2])
import os
arch=os.environ.get('ARCH')
o=[x for x in env.objects if x.path_id==pid and (not arch or x.assets_file.name==arch)][-1]
c=o.read()
print(c.m_Name,'legacy',c.m_Legacy,'compressed',c.m_Compressed,'sr',c.m_SampleRate)
def keys(curve):
    return [[k.time,[getattr(k.value,a,None) for a in 'xyzw'] if not isinstance(k.value,float) else k.value] for k in curve.m_Curve]
for nm in ('m_RotationCurves','m_EulerCurves','m_PositionCurves','m_ScaleCurves','m_FloatCurves','m_PPtrCurves'):
    for cv in getattr(c,nm,[]) or []:
        print(nm, getattr(cv,'path',''), getattr(cv,'attribute',''), len(cv.curve.m_Curve) if hasattr(cv,'curve') else '')
mc=c.m_MuscleClip
print('muscle start/stop',mc.m_StartTime,mc.m_StopTime)
cl=mc.m_Clip.data
print('streamed',len(cl.m_StreamedClip.data),'dense',cl.m_DenseClip.m_FrameCount,cl.m_DenseClip.m_CurveCount,'const',len(cl.m_ConstantClip.data))
b=c.m_ClipBindingConstant
for g in b.genericBindings[:50]:
    print(' bind path',g.path,'attr',g.attribute,'type',g.typeID,'custom',g.customType)
