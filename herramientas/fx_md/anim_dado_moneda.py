# Pasa los clips del dado (dice_animation2) y de la moneda (coin02_anim3.9) de Master Duel a texto para el motor:
# t px py pz qx qy qz qw  (30 por segundo, unidades de MD, ya en mano derecha: el dado con X espejada como su
# malla exportada; la moneda con Z espejada para que su cara mire a la camara (+Z del marco)).
import json,sys
src,dst=sys.argv[1],sys.argv[2]
def escribir(clip,salida,espejo,hasta=None):
    d=json.load(open(clip)); L=['# '+d['nombre']+' (Yu-Gi-Oh! Master Duel, uso personal): t px py pz qx qy qz qw']
    for i,r in enumerate(d['muestras']):
        t=i/d['fps']
        if hasta and t>hasta+1e-6: break
        px,py,pz,qx,qy,qz,qw=r[:7]
        if espejo=='x': p=(-px,py,pz); q=(qx,-qy,-qz,qw)
        else: p=(px,py,-pz); q=(-qx,-qy,qz,qw)
        L.append('%.4f %.4f %.4f %.4f %.5f %.5f %.5f %.5f'%((t,)+p+q))
    open(salida,'w').write('\n'.join(L)+'\n')
escribir(src+'/dice_animation2.json',dst+'/dado_md.txt','x')
escribir(src+'/coin_3127.json',dst+'/moneda_md.txt','z')
