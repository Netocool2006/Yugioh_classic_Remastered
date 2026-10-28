# Convierte lo extraido de Master Duel (FSB5) a un catalogo OGG y a la seleccion del juego (WAV 48k/OGG + audio.txt)
import os,sys,subprocess,re,json,tempfile,shutil
from concurrent.futures import ThreadPoolExecutor
VGM=r'D:\tools\vgmstream\vgmstream-cli.exe'
FSB=[r'D:\tools\audio_md\fsb_data', r'D:\tools\audio_md\fsb']
CAT=r'D:\Docker\compartido\classic-assets\audio_master_duel'
SEL=r'D:\Docker\compartido\classic-assets\audio'
TMP=r'D:\tools\audio_md\tmp'
modo=sys.argv[1]  # catalogo | seleccion
os.makedirs(TMP,exist_ok=True)
def nombre(f):
    n=f[:-4].replace('sound__audioclip__','').replace('builtin__','base__')
    n=n.lower()
    parts=n.split('__')
    cat=parts[0] if len(parts)>1 else 'otros'
    return cat, parts[-1]
def info(p):
    o=subprocess.run([VGM,'-m',p],capture_output=True,text=True).stdout
    g=lambda k: (re.search(k+r': (\d+)',o) or [None,None])[1]
    return {'rate':g('sample rate'),'ch':g('channels'),'total':g('stream total samples'),'loop_start':g('loop start'),'loop_end':g('loop end')}
def wav(p,dst):
    subprocess.run([VGM,'-i','-o',dst,p],capture_output=True,check=True)
todos={}
for d in FSB:
    for f in os.listdir(d):
        if f.endswith('.fsb'):
            cat,n=nombre(f)
            if cat=='voice': continue
            todos.setdefault((cat,n),os.path.join(d,f))
if modo=='catalogo':
    def uno(item):
        (cat,n),p=item
        out=os.path.join(CAT,cat,n+'.ogg')
        if os.path.exists(out): return None
        os.makedirs(os.path.dirname(out),exist_ok=True)
        t=os.path.join(TMP,cat+'_'+n+'.wav')
        try:
            wav(p,t)
            subprocess.run(['ffmpeg','-v','error','-y','-i',t,'-c:a','libvorbis','-q:a','4',out],check=True)
        except Exception as e:
            print('fallo',cat,n,e,flush=True); return None
        finally:
            if os.path.exists(t): os.remove(t)
        return n
    with ThreadPoolExecutor(6) as ex: list(ex.map(uno,todos.items()))
    with open(os.path.join(CAT,'_catalogo.txt'),'w',encoding='utf-8') as h:
        h.write('# categoria/archivo.ogg|segundos|canales|bucle_inicio(muestras)|bucle_fin  (Yu-Gi-Oh! Master Duel, copia del usuario; uso personal)\n')
        for (cat,n),p in sorted(todos.items()):
            i=info(p); s=int(i['total'] or 0)/int(i['rate'] or 48000)
            h.write(f"{cat}/{n}.ogg|{s:.2f}|{i['ch']}|{i['loop_start'] or ''}|{i['loop_end'] or ''}\n")
    print('catalogo',len(todos))
else:
    sel=json.load(open(sys.argv[2],encoding='utf-8'))
    os.makedirs(SEL,exist_ok=True)
    lineas=['# clave|archivo|volumen|bucle  (bucle: 0, 1, o 1:<inicio>:<fin> en muestras: los puntos de bucle de la pista). Origen: Yu-Gi-Oh! Master Duel',
            '# (copia del usuario, uso personal, NO va al repo). Para cambiar un sonido: elegir otro de ../audio_master_duel',
            '# (_catalogo.txt) y correr herramientas: convertir.py seleccion, o editar esta linea a mano.']
    for clave,(src,vol,bucle) in sel.items():
        cat,n=src.split('/')
        p=todos[(cat,n)]
        i=info(p)
        t=os.path.join(TMP,'sel_'+clave+'.wav'); wav(p,t)
        if clave.startswith('musica'):
            arch=clave+'.ogg'
            subprocess.run(['ffmpeg','-v','error','-y','-i',t,'-ar','48000','-ac','2','-c:a','libvorbis','-q:a','3',os.path.join(SEL,arch)],check=True)
            b='0'
            if bucle: b=('1:'+i['loop_start']+':'+(i['loop_end'] or '')).rstrip(':') if i['loop_start'] else '1'
        else:
            arch=clave+'.wav'
            subprocess.run(['ffmpeg','-v','error','-y','-i',t,'-ar','48000','-ac','2','-c:a','pcm_s16le',os.path.join(SEL,arch)],check=True)
            b='1' if bucle else '0'
        os.remove(t)
        lineas.append(f'# <- {src} ({int(i["total"] or 0)/48000:.1f} s)')
        lineas.append(f'{clave}|{arch}|{vol}|{b}')
        print(clave,'<-',src)
    open(os.path.join(SEL,'audio.txt'),'w',encoding='utf-8',newline='\n').write('\n'.join(lineas)+'\n')
