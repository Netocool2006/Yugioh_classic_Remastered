# Resume un tl_*.json de volcar_timeline.py: por cada curva, tramos donde cambia (t: valor) a 30 fps.
# Uso: resumir_tl.py <tl.json> [umbral]
import sys,json
d=json.load(open(sys.argv[1])); um=float(sys.argv[2]) if len(sys.argv)>2 else 0.01
def tramos(ms,i):
    v=[f[i] for f in ms['muestras']]; fps=ms['fps']
    if max(v)-min(v)<1e-4: return 'const %.3f'%v[0]
    pts=[(0,v[0])]
    for k in range(1,len(v)-1):
        a=v[k]-v[k-1]; b=v[k+1]-v[k]
        if (abs(a)<1e-5)!=(abs(b)<1e-5) or a*b<0 or abs(b-a)>um*max(1,abs(v[k])): pts.append((k,v[k]))
    pts.append((len(v)-1,v[-1]))
    # compactar
    out=[]; last=None
    for k,x in pts:
        if last is None or abs(x-last)>um or k==len(v)-1: out.append('%.3f:%.3f'%(ms['inicio']+k/fps,x)); last=x
    return ' '.join(out)
def pista(p,n=0):
    for c in p['clips']:
        ms=c.get('muestras')
        print('  '*n+'%s -> %s  [%s %.3f+%.3f]'%(p['nombre'],p.get('ligado'),c.get('nombre'),c['inicio'],c['dur']))
        if not ms: continue
        for i,cv in enumerate(ms['curvas']):
            if i>=ms['n_curvas']: break
            t=tramos(ms,i)
            if t.startswith('const') and (':pos' in cv or ':rot' in cv): continue
            print('  '*n+'    %-45s %s'%(cv,t))
    for h in p['hijas']: pista(h,n+1)
print(d['raiz'],d['timeline'])
for p in d['pistas']:
    if p['clase']!='ActivationTrack': pista(p)
