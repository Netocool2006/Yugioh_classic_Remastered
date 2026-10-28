# Resumen legible de un params.json (volcar_fx.py): por sistema de particulas los parametros que importan, y el
# catalogo _catalogo.txt de la carpeta fx_master_duel. Uso: resumen.py <carpeta fx_master_duel>
import sys,os,json,glob
base=sys.argv[1]
def mm(c):
    st=c.get('minMaxState',0)
    if st==0: return c.get('scalar')
    if st==3: return [c.get('minScalar'),c.get('scalar')]
    return {'escala':c.get('scalar'),'curva':[(round(k['time'],3),round(k['value'],3)) for k in c.get('maxCurve',{}).get('m_Curve',[])]}
def col(g):
    st=g.get('minMaxState',0)
    c=lambda x:[round(x['r'],3),round(x['g'],3),round(x['b'],3),round(x['a'],3)]
    if st in(0,): return c(g['maxColor'])
    if st==2: return [c(g['minColor']),c(g['maxColor'])]
    gr=g['maxGradient']; return {'gradiente':[(round(gr['ctime%d'%i]/65535,2),c(gr['key%d'%i])) for i in range(gr.get('m_NumColorKeys',2))],
                                  'alfa':[(round(gr['atime%d'%i]/65535,2),round(gr['key%d'%i]['a'],2)) for i in range(gr.get('m_NumAlphaKeys',2))]}
lineas=['# Efectos extraidos de Yu-Gi-Oh! Master Duel (copia del usuario en Steam, uso personal; NO va al repo).',
        '# prefabs/<nombre>/: params.json (arbol completo: transform, ParticleSystem, renderer, materiales), resumen.json,',
        '#   texturas png, mallas obj, timeline.json (pistas del Timeline: ControlTrack = cuando se enciende cada parte).',
        '# horneado/: flipbooks para el motor (sim_fx.py). dado_moneda/: mallas, texturas, glb y curvas del dado y la moneda.',
        '# indice.json: los 40421 bundles de LocalData con sus contenedores. artes.png: ilustraciones de los cid con efecto propio.',
        '# nombre | sistemas | duracion max (s) | texturas | origen en Master Duel']
for d in sorted(glob.glob(os.path.join(base,'prefabs','*'))):
    p=os.path.join(d,'params.json')
    if not os.path.exists(p): continue
    j=json.load(open(p)); sis=[]
    def rec(n,ruta=''):
        if 'nombre' not in n: return
        r=ruta+'/'+n['nombre']; psr=None; ps=None
        for c in n['comp']:
            if c['tipo']=='ParticleSystem': ps=c['ps']
            if c['tipo']=='PSR': psr=c['r']
        if ps:
            im=ps['InitialModule']; em=ps['EmissionModule']
            sis.append({'ruta':r,'activo':n.get('activo'),'duracion':ps['lengthInSec'],'loop':ps['looping'],'retardo':mm(ps['startDelay']),
                        'vida':mm(im['startLifetime']),'velocidad':mm(im['startSpeed']),'tamano':mm(im['startSize']),'color':col(im['startColor']),
                        'gravedad':mm(im['gravityModifier']),'max':im.get('maxNumParticles'),'emision':mm(em['rateOverTime']) if em.get('enabled') else 0,
                        'rafagas':[(b['time'],mm(b['countCurve'])) for b in em.get('m_Bursts',[])],'forma':ps['ShapeModule'].get('type') if ps['ShapeModule'].get('enabled') else None,
                        'tamano_vida':mm(ps['SizeModule']['curve']) if ps['SizeModule'].get('enabled') else None,
                        'color_vida':col(ps['ColorModule']['gradient']) if ps['ColorModule'].get('enabled') else None,
                        'hoja':[ps['UVModule'].get('tilesX'),ps['UVModule'].get('tilesY')] if ps['UVModule'].get('enabled') else None,
                        'render':psr.get('m_RenderMode') if psr else None,'material':(psr or {}).get('_materiales')})
        for h in n.get('hijos',[]): rec(h,r)
    rec(j['arbol'])
    json.dump({'sistemas':sis,'materiales':{k:{'shader':v['shader'],'texturas':{a:b['tex'] for a,b in v['texturas'].items()}} for k,v in j['materiales'].items()}},
              open(os.path.join(d,'resumen.json'),'w'),indent=1)
    dur=max([s['duracion'] for s in sis] or [0])
    lineas.append(f"{os.path.basename(d)} | {len(sis)} | {dur:.2f} | {len(j['texturas'])} | {j['arbol'].get('nombre')}")
lineas.append('')
lineas.append('# horneado (flipbook 8x8 de 256, RGB premultiplicado con gamma, A lineal): archivo | duracion | vista | uso en el juego')
usos={'md_activar':'activacion generica magia/trampa (gris, se tine)','md_destruir':'carta destruida por efecto','md_destruir_batalla':'carta destruida en combate (fxp_cardbrk_btl_001)','md_colocar':'humo al colocar magia/trampa',
      'md_agujero_negro':'Dark Hole (ef04342)','md_cuchillos':'Thousand Knives (ef05166)','md_plumero':"Harpie's Feather Duster (ef04678), Pot of Greed, Swords",
      'md_tornado':'MST / Heavy Storm (ef04909)','md_fusion':'Polymerization','md_fusion_marco':'Polymerization (marco)','md_ritual':'Monster Reborn / rituales'}
for f in sorted(glob.glob(os.path.join(base,'horneado','*.json'))):
    m=json.load(open(f)); n=os.path.basename(f)[:-5]
    lineas.append(f"{n} | {m['duracion']:.2f} | {m['vista']} | {usos.get(n,'')}")
open(os.path.join(base,'_catalogo.txt'),'w',encoding='utf-8').write('\n'.join(lineas)+'\n')
print('\n'.join(lineas))
