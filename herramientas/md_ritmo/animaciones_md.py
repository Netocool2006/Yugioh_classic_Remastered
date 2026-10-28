# Rama classic-ps5-efectos (3a vuelta): las AnimationTrack del Timeline de un efecto de carta de MD -> las curvas de los
# Transform del prefab (posicion, rotacion euler/cuaternion, escala) por ruta de nodo, para exportar_pistas.py.
# Unity: PlayableDirector.m_SceneBindings ata cada pista (pid) a un Animator; las curvas del AnimationClip ("Recorded",
# infinito o en clips) van por CRC32 de la ruta relativa al GameObject de ese Animator (0 = el mismo). Se muestrean con
# herramientas/fx_md/clip_muestras.py.
# Uso: animaciones_md.py <indice.json> <contenedor.prefab> <salida.json> [fps]
import sys, os, json, zlib
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'fx_md'))
import UnityPy
import clip_muestras

ROOT = r'D:/Steam/steamapps/common/Yu-Gi-Oh!  Master Duel/LocalData/ad28c7c3'

def main():
    d = json.load(open(sys.argv[1])); cont = sys.argv[2]; sal = sys.argv[3]; fps = int(sys.argv[4]) if len(sys.argv) > 4 else 30
    pre = os.path.dirname(cont)
    bs = set(); dest = None
    for b in d:
        for k, t, p in b.get('c', []):
            if k.startswith(pre): bs.add(b['b'])
            if k == cont: dest = (b['b'], p)
    env = UnityPy.load(*[os.path.join(ROOT, b) for b in sorted(bs)])
    objs = {}
    for o in env.objects: objs.setdefault(o.path_id, o)
    raiz = objs[dest[1]].read()
    # rutas: GameObject -> ruta desde la raiz del prefab ("" la raiz; "A/B" como en params.json sin la raiz)
    ruta_go = {}; go_de_comp = {}; hijos = {}
    def recorrer(go, ruta):
        ruta_go[go.object_reader.path_id] = ruta
        tr = None
        for c in go.m_Component:
            pp = c.component if hasattr(c, 'component') else c
            go_de_comp[pp.path_id] = go.object_reader.path_id
            try:
                co = pp.read()
                if co.object_reader.type.name in ('Transform', 'RectTransform'): tr = co
            except Exception: pass
        if tr is None: return
        for h in tr.m_Children:
            try:
                ht = h.read(); hg = ht.m_GameObject.read()
                recorrer(hg, (ruta + '/' if ruta else '') + hg.m_Name)
            except Exception: pass
    recorrer(raiz, '')
    # PlayableDirector: pista -> GameObject atado
    atadas = {}
    for o in env.objects:
        if o.type.name != 'PlayableDirector': continue
        try: t = o.read_typetree()
        except Exception: continue
        if t.get('m_GameObject', {}).get('m_PathID') != dest[1]: continue
        for b in t.get('m_SceneBindings', []):
            k = b['key']['m_PathID']; v = b['value']['m_PathID']
            if k and v: atadas[k] = go_de_comp.get(v, v)
    pistas = []
    for k, go in atadas.items():
        o = objs.get(k)
        if os.environ.get('DEPURAR'): print('atada', k, o.type.name if o else None, go, ruta_go.get(go))
        if o is None or o.type.name != 'MonoBehaviour': continue
        try: t = o.read_typetree()
        except Exception: continue
        clips = []
        ic = (t.get('m_InfiniteClip') or {}).get('m_PathID', 0)
        if ic: clips.append((ic, 0.0, None))
        for c in t.get('m_Clips', []) or []:
            ap = (c.get('m_Asset') or {}).get('m_PathID', 0)
            if ap and ap in objs:
                try:
                    a = objs[ap].read_typetree(); cp = (a.get('m_Clip') or {}).get('m_PathID', 0)
                    if cp: clips.append((cp, c.get('m_Start', 0.0), c.get('m_Duration')))
                except Exception: pass
        base = ruta_go.get(go)
        if os.environ.get('DEPURAR'): print('   clips', clips, 'clips crudos', [(c.get('m_Start'), c.get('m_Asset')) for c in t.get('m_Clips', []) or []][:3])
        if base is None: continue
        hashes = {}
        for gpid, r in ruta_go.items():
            if base == '' or r == base or r.startswith(base + '/'):
                rel = r[len(base) + 1:] if base and r != base else ('' if r == base else r)
                hashes[zlib.crc32(rel.encode()) if rel else 0] = r
        for cp, ini, du in clips:
            if cp not in objs: continue
            m = clip_muestras.muestrear(objs[cp].read(), fps)
            if os.environ.get('DEPURAR'): print('   curvas', m['curvas'][:12], 'hashes', list(hashes.items())[:4])
            curvas = {}
            for i, n in enumerate(m['curvas']):
                h, _, comp = n.partition(':')
                try: hv = int(h)
                except ValueError: continue
                if hv not in hashes: continue
                vals = [f[i] for f in m['muestras']]
                if comp.split('.')[0] in ('pos', 'rot', 'euler', 'scale'):
                    curvas.setdefault(hashes[hv], {})[comp] = vals
                elif comp.endswith('_t23.') and min(vals) >= -0.01 and max(vals) <= 1.01:   # (MeshRenderer: la opacidad del material)
                    curvas.setdefault(hashes[hv], {})['alfa'] = vals
            if curvas:
                pistas.append({'inicio': ini, 'dur': du, 'fps': fps, 'fin_clip': m['fin'], 'atada': base, 'curvas': curvas})
    json.dump({'pistas': pistas}, open(sal, 'w'))
    for p in pistas:
        print('pista atada a', repr(p['atada']), 'inicio', p['inicio'], 'clip', round(p['fin_clip'], 2), {r: sorted(c) for r, c in p['curvas'].items()})

if __name__ == '__main__':
    main()
