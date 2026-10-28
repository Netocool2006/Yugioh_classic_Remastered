# Rama classic-md-ritmo: exporta los movimientos de cartas de Master Duel (BezierMotionSetting / CardShow* de
# masterduel_Data/data.unity3d, ya volcados con typetree a md_extraido/ritmo/settings.json por el agente md-ritmo:
# UnityPy + TypeTreeGeneratorAPI + DummyDll de Il2CppDumper) a <assets>/md_ritmo/movimientos.txt (FUERA del repo).
# Una linea por movimiento, separada por "|":
#   nombre|tiempo|base_inicio|base_via|base_fin|via_lerp|via_x,y,z|fin_lerp|fin_x,y,z|ini_lerp|ini_x,y,z|{acel}|{giro}
#   base: 0 = Origin (donde esta la carta), 1 = Target (su destino). lerp: la suma de los offsets tipo 0 (fraccion del
#   camino origen -> destino); x,y,z: la suma de los offsets tipo 1 (designation * intensity, en unidades de MD).
#   {acel} / {giro}: accelerationCurve / rotationAccelerationCurve como {"keys":[[t,v,in,out,inW,outW,modo],...],"pre":p,"post":q}
#   (lo lee clipsmd::CurvaUnity::desdeJSON de clips_md.h).
# Uso: python -I exportar_movimientos.py <settings.json> [ataque_bezier.json ...] <salida.txt>
#   (rama classic-md-combate: tambien los BezierMotion del ataque, Strike*/Flash*/Shoot*: md_extraido/ritmo/ataque_bezier.json)
import sys, json

def curva(c):
    ks = [[k['time'], k['value'], k['inSlope'], k['outSlope'], k.get('inWeight', 0), k.get('outWeight', 0), k.get('weightedMode', 0)] for k in c['m_Curve']]
    return json.dumps({'keys': ks, 'pre': c.get('m_PreInfinity', 2), 'post': c.get('m_PostInfinity', 2)}, separators=(',', ':'))

def punto(op):
    lerp = 0.0
    add = [0.0, 0.0, 0.0]
    for o in op.get('offsetList', []):
        d = o['designation']
        if o['type'] == 0:
            lerp += o['intensity']
        else:
            add = [add[0] + d['x'] * o['intensity'], add[1] + d['y'] * o['intensity'], add[2] + d['z'] * o['intensity']]
    return op['basePosition']['type'], lerp, add

def main():
    if len(sys.argv) < 3 or not sys.argv[-1].endswith('.txt'):
        sys.exit('uso: exportar_movimientos.py <settings.json> [ataque_bezier.json ...] <salida.txt>')
    d = {}
    for i, ruta in enumerate(sys.argv[1:-1]):
        for k, v in json.load(open(ruta, encoding='utf-8')).items():
            d['%d:%s' % (i, k)] = v
    n = 0
    vistos = set()
    with open(sys.argv[-1], 'w', encoding='utf-8', newline='\n') as f:
        f.write('# movimientos de cartas de Master Duel (assets de Konami: NO va al repo). exportar_movimientos.py\n')
        for v in d.values():
            if 'animationTime' not in v or 'accelerationCurve' not in v or v['m_Name'] in vistos:
                continue
            vistos.add(v['m_Name'])
            bi, li, ai = punto(v['startPositionOperation'])
            bv, lv, av = punto(v['viaPositionOperation'])
            bf, lf, af = punto(v['endPositionOperation'])
            c = lambda a: '%g,%g,%g' % tuple(a)
            f.write('|'.join([v['m_Name'], '%g' % v['animationTime'], str(bi), str(bv), str(bf), '%g' % lv, c(av), '%g' % lf, c(af), '%g' % li, c(ai),
                              curva(v['accelerationCurve']), curva(v['rotationAccelerationCurve'])]) + '\n')
            n += 1
    print('movimientos:', n)

main()
