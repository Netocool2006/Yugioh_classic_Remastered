# hoja de contacto con las ilustraciones de Master Duel de una lista de cid (para identificar las cartas)
import sys,json,os,UnityPy
from PIL import Image,ImageDraw
d=json.load(open(sys.argv[1])); ids=sys.argv[3].split(','); root=sys.argv[2]; out=sys.argv[4]
m={}
for b in d:
    for k,t,p in b.get('c',[]):
        if k.startswith('assets/resources/card/images/illust/'):
            c=k.split('/')[-1].split('.')[0]
            if c in ids and c not in m: m[c]=(b['b'],p)
ims=[]
for c in ids:
    if c not in m: continue
    env=UnityPy.load(os.path.join(root,m[c][0]))
    for o in env.objects:
        if o.path_id==m[c][1]:
            im=o.read().image.convert('RGB').resize((160,160)); ImageDraw.Draw(im).text((4,4),c,fill=(255,255,0)); ims.append(im)
cols=8; H=Image.new('RGB',(160*cols,160*((len(ims)+cols-1)//cols)))
for i,im in enumerate(ims): H.paste(im,((i%cols)*160,(i//cols)*160))
H.save(out)
