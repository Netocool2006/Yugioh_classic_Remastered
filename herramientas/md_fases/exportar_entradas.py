# Rama classic-md-entradas: los sprites de las entradas de FUSION y RITUAL de Yu-Gi-Oh! Master Duel (copia del usuario)
# y los generados para la transicion del fin del duelo, en un atlas aparte: <classic-assets>/md_ui/md_entradas.png +
# md_entradas_atlas.txt (nombre x y w h). NO va al repo. Lo lee src/entrada_md_ui.h.
# Las texturas de MD son mascaras (blanco/gris sobre negro, sin alfa util): aca quedan blancas con alfa = brillo, para
# teñirlas con el color de cada sistema de particulas de MD.
# Uso: exportar_entradas.py [salida]   (python de D:\tools\venv_unitypy con -I)
import sys,os,json
import UnityPy
from PIL import Image
import numpy as np
ROOT=r'D:/Steam/steamapps/common/Yu-Gi-Oh!  Master Duel/LocalData/ad28c7c3'
IND=r'D:/tools/fx_md/work/indice.json'   # (herramientas/fx_md/indice.py)
OUT=sys.argv[1] if len(sys.argv)>1 else r'D:/Docker/compartido/classic-assets/md_ui'
B='assets/resourcesassetbundle/duel/timeline/duel/universal/summon/'
# nombre en el atlas: (contenedor, lado en px)
TEX={'md_vortice':(B+'summonfusion/normalfusion/textures/effusion009.png',256),      # remolino (guruguruRed/Blue)
     'md_vortice_aro':(B+'summonfusion/normalfusion/textures/effusion012.png',256),  # aro de humo del vortice
     'md_fusion_aro':(B+'summonfusion/normalfusion/textures/effusion010.png',256),   # anillo (SummonFusionOut)
     'md_fusion_estallido':(B+'summonfusion/normalfusion/textures/effusion011.png',256),   # estallido (PostFusion)
     'md_fusion_espiral':(B+'summonfusion/normalfusion/summonfusionbg/fxt_ef07445_swirl_001.png',256),
     'md_fusion_rayo':(B+'summonfusion/normalfusion/textures/efthunder002.png',256),
     'md_ritual_circulo1':(B+'summonritual/summonritualmain/textures/tlt_ritualcircle01.png',384),
     'md_ritual_circulo2':(B+'summonritual/summonritualmain/textures/tlt_ritualcircle02.png',512),
     'md_ritual_circulo3':(B+'summonritual/summonritualmain/textures/tlt_ritualcircle03.png',512),
     'md_ritual_circulo4':(B+'summonritual/summonritualmain/textures/tlt_ritualcircle04.png',512),
     'md_ritual_circulo5':(B+'summonritual/summonritualmain/textures/tlt_ritualcircle05.png',384),
     'md_ritual_fuego':(B+'summonritual/summonritualmain/textures/tlt_ritualfire01.png',256)}
d=json.load(open(IND))
ims=[]
for nom,(cont,lado) in TEX.items():
    b=next((x['b'] for x in d for k,t,p in x.get('c',[]) if k==cont),None)
    if not b: print('falta',cont); continue
    env=UnityPy.load(os.path.join(ROOT,b))
    tx=next(o.read() for o in env.objects if o.type.name=='Texture2D')
    im=tx.image.convert('RGBA').resize((lado,lado),Image.LANCZOS)
    arr=np.array(im).astype(np.float32)
    al=arr[...,:3].max(axis=2)*arr[...,3]/255.0
    out=np.zeros_like(arr); out[...,:3]=255; out[...,3]=al
    ims.append((nom,Image.fromarray(out.astype(np.uint8),'RGBA')))
    print(nom,'<-',tx.m_Name,lado)
# generados (sin datos de MD): el agujero de la transicion (DuelEndTransition: material SoloTrans_fxp_BK_hole, negro con
# un agujero redondo de borde suave) y un anillo de luz
n=256; y,x=np.mgrid[0:n,0:n]; rr=np.hypot(x-(n-1)/2,y-(n-1)/2)/(n/2)
hole=np.zeros((n,n,4),np.float32); hole[...,3]=np.clip((rr-0.80)/0.2,0,1)*255   # negro afuera, transparente adentro
ims.append(('md_agujero',Image.fromarray(hole.astype(np.uint8),'RGBA')))
ring=np.zeros((n,n,4),np.float32); ring[...,:3]=255; ring[...,3]=np.exp(-((rr-0.9)/0.05)**2)*255
ims.append(('md_anillo_luz',Image.fromarray(ring.astype(np.uint8),'RGBA')))
ims.sort(key=lambda z:-z[1].height)
W=2048; x=y=1; fila=0; pos=[]
for nom,im in ims:
    if x+im.width+1>W: x=1; y+=fila+2; fila=0
    pos.append((nom,x,y,im.width,im.height)); x+=im.width+2; fila=max(fila,im.height)
H=(y+fila+2+3)//4*4
atlas=Image.new('RGBA',(W,H),(0,0,0,0))
for (nom,im),(n2,x,y,w,h) in zip(ims,pos): atlas.paste(im,(x,y))
os.makedirs(OUT,exist_ok=True)
atlas.save(os.path.join(OUT,'md_entradas.png'))
with open(os.path.join(OUT,'md_entradas_atlas.txt'),'w',newline='\n') as o:
    o.write('# sprites de las entradas de fusion/ritual de Master Duel (copia del usuario, NO va al repo): nombre x y w h en md_entradas.png (%dx%d)\n'%(W,H))
    for nom,x,y,w,h in pos: o.write('%s\t%d\t%d\t%d\t%d\n'%(nom,x,y,w,h))
print('atlas',len(pos),'sprites',W,'x',H)
