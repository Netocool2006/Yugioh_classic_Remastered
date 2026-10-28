# Rama classic-ps5-efectos: exporta un efecto de carta de Master Duel (CardRunEffect: prefab + Timeline de
# duel/timeline/card/tcg/efXXXXX, volcados con herramientas/fx_md/volcar_ef.py y timeline.py) a una PISTA 3D que el
# juego reproduce en la mesa (src/md_efectos_ui.h, reproductor "pista"): NO es un flipbook. Cada cuadro (30 por
# segundo) guarda cada particula de cada ParticleSystem del prefab con su posicion 3D en el espacio del prefab (unidades
# de MD), su tamanno, giro, color y uv, y las mallas (particulas malla y MeshRenderer) como triangulos 3D. La simulacion
# es la de herramientas/fx_md/sim_fx.py (los modulos de ParticleSystem de Unity: emision, bursts, forma, velocidad,
# fuerza, gravedad, ruido, tamanno y color en la vida, flipbook de uv) con las ventanas de las ControlTrack del Timeline.
# El juego pone de cara a SU camara los billboards y los estirados (como Unity), y proyecta lo demas: cualquier camara.
# Salida (FUERA del repo, classic-assets/md_ritmo/efectos/): <nombre>.mdp (binario) + <nombre>.png (atlas de las
# texturas de sus materiales, alfa premultiplicado).
# Uso: python -I exportar_pistas.py <carpeta_volcada> <timeline.json|-> <salida_sin_ext> [--dur s]
import sys, os, json, math, struct, argparse
import numpy as np
from PIL import Image

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'fx_md'))
import sim_fx  # noqa: E402

class _Tex:   # (sim_fx pide texturas de OpenGL: aca no hacen falta)
    def build_mipmaps(self): pass
class _Ctx:
    def texture(self, *a, **k): return _Tex()

# (la escala propia de cada nodo: Unity escala el tamanno de las particulas por eje con ella en scalingMode Local)
_ESC_LOCAL = [1.0, 1.0, 1.0]
_rec0 = sim_fx.Escena.recorrer
_RUTA = ['']
_NODO = [None]
class _Mallas(list):   # (al agregarse una MeshRenderer, se anota su nodo: ruta, matriz del padre y transform local)
    def append(self, m):
        m['ruta'], m['padre'], m['nodo'] = _NODO[0]
        super().append(m)
def _recorrer(s, n, padre, *a):
    global _ESC_LOCAL
    if not isinstance(s.mallas, _Mallas): s.mallas = _Mallas(s.mallas)
    antes = _ESC_LOCAL; _ESC_LOCAL = n.get('esc', [1.0, 1.0, 1.0])
    ruta0 = _RUTA[0]; nodo0 = _NODO[0]
    raiz = not hasattr(s, '_vista_raiz')
    s._vista_raiz = True
    _RUTA[0] = '' if raiz else ((ruta0 + '/' if ruta0 else '') + n['nombre'])
    _NODO[0] = (_RUTA[0], padre, n)
    try: _rec0(s, n, padre, *a)
    finally: _ESC_LOCAL = antes; _RUTA[0] = ruta0; _NODO[0] = nodo0
sim_fx.Escena.recorrer = _recorrer
_em0 = sim_fx.Emisor.__init__
def _emisor(s, *a, **k):
    _em0(s, *a, **k); s.escLocal = np.abs(np.array(_ESC_LOCAL, dtype=float))
sim_fx.Emisor.__init__ = _emisor

def _cargar_obj_n(ruta):
    m = _cargar_obj0(ruta)
    if m is None: return None
    # (el obj que escribe UnityPy trae la v ya dada vuelta respecto de la de Unity: los cilindros de Ef05124 caian en la franja
    # de arriba de su textura en vez del dibujo de los signos de pregunta)
    m['uv'] = np.array(m['uv'], dtype=float); m['uv'][:, 1] = 1.0 - m['uv'][:, 1]
    V = []; N = []; F = []
    for l in open(ruta):
        if l.startswith('v '): V.append(1)
        elif l.startswith('vn '): N.append([float(x) for x in l.split()[1:4]])
        elif l.startswith('f '):
            idx = [w.split('/') for w in l.split()[1:]]
            for k in range(1, len(idx) - 1): F.append([idx[0], idx[k], idx[k + 1]])
    if N:
        nrm = []
        for f in F:
            for w in f:
                ni = int(w[2]) - 1 if len(w) > 2 and w[2] else int(w[0]) - 1
                nrm.append(N[ni] if 0 <= ni < len(N) else [0.0, 1.0, 0.0])
        nrm = np.array(nrm) * np.array([-1, 1, 1])   # (como pos: de vuelta a la mano de Unity)
        if len(nrm) == len(m['pos']): m['nrm'] = nrm
    return m
_cargar_obj0 = sim_fx.cargar_obj
sim_fx.cargar_obj = _cargar_obj_n

def _nrm(M3, n):
    n2 = (M3 @ n.T).T
    l = np.linalg.norm(n2, axis=1, keepdims=True)
    return n2 / np.maximum(l, 1e-6)

FPS = 30
DT = 1.0 / 60.0

def textura_material(m, carpeta):
    """la textura del material como la ve el shader de sim_fx (canal, alfa, mascara), en RGBA premultiplicado"""
    if m.tex:
        p = os.path.join(carpeta, m.tex + '.png')
        im = Image.open(p).convert('RGBA') if os.path.exists(p) else None
    else:
        im = m.proc.convert('RGBA') if m.proc is not None else None
    if im is None:
        return None
    a = np.asarray(im).astype(np.float32) / 255.0
    if m.canal >= 0:
        v = a[..., m.canal].copy()
        a = np.dstack([np.ones_like(v), np.ones_like(v), np.ones_like(v), v])
    else:
        a = a.copy()
        a[..., 3] = a[..., m.acanal]
    if m.mask and getattr(m, 'mesc', [1, 1]) == [1, 1] and getattr(m, 'mofs', [0, 0]) == [0, 0]:
        pm = os.path.join(carpeta, m.mask + '.png')
        if os.path.exists(pm):
            mk = np.asarray(Image.open(pm).convert('RGBA').resize((a.shape[1], a.shape[0]), Image.LANCZOS)).astype(np.float32) / 255.0
            a[..., 3] *= mk[..., m.mcanal]
    a[..., :3] *= a[..., 3:4]
    return a

def _canal(d, *claves):
    for k in claves:
        v = d.get('colores', {}).get(k)
        if v is not None and sum(v[:4]) > 0.5: return int(np.argmax(v[:4]))
    return 0

def _girar(a, grados):
    if not grados: return a
    im = Image.fromarray((np.clip(a, 0, 1) * 255).astype(np.uint8), 'RGBA').rotate(grados, resample=Image.BILINEAR)
    return np.asarray(im).astype(np.float32) / 255.0

def _suave(e0, e1, x):
    t = np.clip((x - e0) / max(e1 - e0, 1e-4), 0.0, 1.0)
    return t * t * (3 - 2 * t)

def _leer(carpeta, k):
    if not k: return None
    p = os.path.join(carpeta, k + '.png')
    return np.asarray(Image.open(p).convert('RGBA')).astype(np.float32) / 255.0 if os.path.exists(p) else None

def shader_md(d, carpeta):
    """Traduccion de los Shader Graph propios de MD que pintan con una rampa de color (el gris de la textura elige entre
    dos colores HDR) o con un color de emision. Devuelve (RGBA con alfa premultiplicado, escala HDR) o None (lo hace
    textura_material como sim_fx). La escala va al tinte del material (el atlas es de 8 bits)."""
    sh = (d.get('shader') or ''); c = d.get('colores', {}); f = d.get('floats', {}); tx = d.get('texturas', {})
    def rampa(v, c0, c1, e0, e1):
        s = _suave(e0, e1, v)[..., None]
        return np.array(c0[:3])[None, None, :] * (1 - s) + np.array(c1[:3])[None, None, :] * s
    def salida(rgb, a):
        esc = max(1.0, float(rgb.max()))
        o = np.dstack([rgb / esc * a[..., None], a]).astype(np.float32)
        return o, esc
    if 'Thunder_01' in sh:   # fxs_04343_Thunder_01_b / fxs_04887_Thunder_01_b: afuera -> adentro con smoothstep
        t = _leer(carpeta, tx['_Texture']['tex'])
        if t is None: return None
        v = t[..., _canal(d, '_Texture_Channel_Selest')]
        e0 = f.get('_Color_Smoothstep', 0.0); rgb = rampa(v, c['_Color_Outside'], c['_Color_Inside'], e0, e0 + f.get('_Color_Smoothstep_Blur', 0.5))
        o, e = salida(rgb, v * _suave(e0, e0 + f.get('_Color_Smoothstep_Blur', 0.5), v)); return _girar(o, f.get('_Rotate', 0.0)), e
    if 'SimpleHDR_MASKplus' in sh:
        t = _leer(carpeta, tx['_Texture']['tex'])
        if t is None: return None
        v = t[..., _canal(d, '_Channel_Selection')]
        e0 = f.get('_Color_Smoothstep', 0.0); rgb = rampa(v, c['_Color_01'], c['_Color_02'], e0, e0 + f.get('_Color_SmoothstepBlur', 1.0))
        a0 = f.get('_Alpha_Smoothstep', 0.0); a = _suave(a0, a0 + f.get('_Alpha_SmoothstepBlur', 1.0), v) * f.get('_Alpha', 1.0)
        o, e = salida(rgb, np.clip(a, 0, 1)); return _girar(o, f.get('_Rotation', 0.0)), e
    if 'ParticleRing' in sh:   # punto suave o anillo procedural con el color de emision
        n = 128; y, x = np.mgrid[0:n, 0:n]; r = np.hypot(x - n / 2 + 0.5, y - n / 2 + 0.5) / (n / 2)
        if f.get('_RING_ON', 0) > 0.5:
            ancho = max(0.04, 0.5 * f.get('_RING_Radial', 0.2)); a = np.clip(1 - np.abs(r - (1 - ancho)) / ancho, 0, 1) ** f.get('_Alphapow', 2.0)
        else:
            a = np.clip(1 - r, 0, 1) ** f.get('_Alphapow', 2.0)
        a = np.clip(a * f.get('_Alpha', 1.0), 0, 1)
        col = c.get('_Emission_truth') if f.get('_Emission_truth_on', 0) > 0.5 and '_Emission_truth' in c else c.get('_Emission', [1, 1, 1, 1])
        return salida(np.ones((n, n, 3)) * np.array(col[:3])[None, None, :], a)
    if 'ParticleStar' in sh:   # estrella de 4 puntas procedural
        n = 128; y, x = np.mgrid[0:n, 0:n]; dx = (x - n / 2 + 0.5) / (n / 2); dy = (y - n / 2 + 0.5) / (n / 2)
        a = np.clip(np.exp(-np.abs(dx) * 40) * np.exp(-np.abs(dy) * 2.5) + np.exp(-np.abs(dy) * 40) * np.exp(-np.abs(dx) * 2.5) + np.exp(-(dx * dx + dy * dy) * 30), 0, 1) * f.get('_Alpha', 1.0)
        return salida(np.ones((n, n, 3)) * np.array(c.get('_truth_Emissive', c.get('_Emissive', [1, 1, 1]))[:3])[None, None, :], a)
    if 'Fake_ModelAlpha' in sh:   # (modelo con su textura de color x _Emissive; la luz de borde verde no: necesita normales)
        t = _leer(carpeta, tx['_Texture2D']['tex']) if '_Texture2D' in tx else None
        if t is None: return None
        # (_Emissive (1.5, 1.5, 1.5, 0): su alfa 0 deja el color de la textura; x1.5 sin HDR saturaba el rosa)
        em = np.array(c.get('_Emissive', [1, 1, 1])[:3]) if c.get('_Emissive', [1, 1, 1, 1])[3] > 0.0 else np.ones(3)
        o, e = salida(t[..., :3] * em[None, None, :], np.ones(t.shape[:2]) * f.get('_Opacity', 1.0))
        return o, e, 0, 'mezcla', ([1.0, 1.0, 1.0], (np.array(c.get('_RimLightColor', [0, 0, 0])[:3]) / e).tolist(), f.get('_RimLight_Power', 1.0), 1, 0.0, 1.0, [0, 0, 0], [0, 0, 0], 1.0)
    if 'C_particleBlendAmp' in sh or 'C_particleAddAmpSt' in sh:   # textura (un canal o su luz) x _TintColor HDR
        t = _leer(carpeta, tx['_MainTex']['tex']) if '_MainTex' in tx else None
        if t is None: return None
        if '_Channel_Selection' in c:   # (Boolean_RorAlpha 0: el alfa; 1: el canal elegido)
            v = t[..., _canal(d, '_Channel_Selection')] if f.get('Boolean_RorAlpha', 1.0) > 0.5 else t[..., 3]
        else:
            v = t[..., :3].max(axis=2) * t[..., 3]
        tin = np.array(c.get('_TintColor', [1, 1, 1, 1])[:3])
        return salida(np.ones(v.shape + (3,)) * tin[None, None, :], v)
    if 'SimpleRimLight' in sh:   # fxs_SimpleRimLight_001: solo la luz de borde, smoothstep(_Smooth, _Smooth + _Blur, 1 - N.V) x _Color
        n = 8; o = np.ones((n, n, 4), np.float32)
        e0 = f.get('_Smooth', 0.0)
        return o, 1.0, 0, None, ([0.0, 0.0, 0.0], c['_Color'][:3], 1.0, 2, e0, e0 + f.get('_Blur', 1.0), [0, 0, 0], [0, 0, 0], 1.0)
    if 'Flit_CardEf' in sh:   # Flit_CardEf: textura x _color + luz de _AddDirection (_AmbColor x _AIntensity, (N.dir)^_Attenuation) + _FresnelColor x (1 - N.V)^_FresnelPower x _FresnelContrast
        t = _leer(carpeta, tx['_Texture2D']['tex']) if '_Texture2D' in tx else None
        if t is None: return None
        base = list(c.get('_color', [1, 1, 1])[:3])
        rim = (np.array(c.get('_FresnelColor', [0, 0, 0])[:3]) * f.get('_FresnelContrast', 1.0)).tolist()
        luz = (np.array(c.get('_AmbColor', [0, 0, 0])[:3]) * f.get('_AIntensity', 0.0)).tolist()
        dr = np.array(c.get('_AddDirection', [0, 0, 0])[:3], dtype=float); dr = (dr / np.linalg.norm(dr)).tolist() if np.linalg.norm(dr) > 1e-6 else [0, 0, 0]
        o = t.copy(); o[..., :3] *= o[..., 3:4]
        return o, 1.0, 0, None, (base, rim, f.get('_FresnelPower', 1.0), 1, 0.0, 1.0, luz, dr, f.get('_Attenuation', 1.0))
    if 'fxs_AlphaStep' in sh or 'Distortion' in sh or 'UnlitRecieveShadow' in sh:   # calcomania / distorsion / sombra: no suman luz
        return np.zeros((4, 4, 4), np.float32), 1.0
    if 'TexDissolve' in sh:   # disolucion: el ruido x la mascara es el umbral; el alfa de la particula lo recorre (en el juego)
        t = _leer(carpeta, tx['_Main_Texture']['tex']) if '_Main_Texture' in tx else None
        if t is None: return None
        v = t[..., _canal(d, '_Main_Texture_Channel_Select')]
        if '_Mask_Texture' in tx:
            mk = _leer(carpeta, tx['_Mask_Texture']['tex'])
            if mk is not None:
                mk = np.asarray(Image.fromarray((mk * 255).astype(np.uint8)).resize((v.shape[1], v.shape[0]))).astype(np.float32) / 255.0
                v = v * _girar(mk, f.get('_Mask_Texture_Rotate', 0.0))[..., 0]
        em = np.array(c.get('_Emissive', [1, 1, 1])[:3]); col = np.array(c.get('_Color_02', [1, 1, 1])[:3]) * em
        o, e = salida(np.ones(v.shape + (3,)) * col[None, None, :], np.ones_like(v))
        o[..., 3] = v   # (el alfa guarda el umbral: lo usa el modo disolucion del juego)
        o[..., :3] = col[None, None, :] / e
        return o, e, 1
    if 'MASKNoiseScroll' in sh or 'UVDefTwistScroll' in sh or 'ExodiaFireShape' in sh:
        clave = '_Main_Texture' if '_Main_Texture' in tx else ('_Texture' if '_Texture' in tx else ('_MASK_Texture' if '_MASK_Texture' in tx else None))
        if clave is None: return None
        t = _leer(carpeta, tx[clave]['tex'])
        if t is None: return None
        v = t[..., _canal(d, '_Main_Texture_Channel_Select', '_Texture_Select_Channel', '_sub_MASK_Channel_Selection')]
        mk = None
        for km in ('_Mask_Texture', '_MASK_Texture'):
            if km in tx and km != clave: mk = _leer(carpeta, tx[km]['tex'])
        if mk is not None:
            mk = np.asarray(Image.fromarray((mk * 255).astype(np.uint8)).resize((v.shape[1], v.shape[0]))).astype(np.float32) / 255.0
            v = v * mk[..., 0]
        if 'UVDefTwistScroll' in sh:   # (la mascara redonda de afuera: _OUT_MASK_Scale / _OUT_MASK_Blur)
            n0, n1 = v.shape; yy, xx = np.mgrid[0:n0, 0:n1]
            r = np.hypot((xx - n1 / 2 + 0.5) / (n1 / 2), (yy - n0 / 2 + 0.5) / (n0 / 2))
            e1 = 1.0 - f.get('_OUT_MASK_Scale', 0.3); v = v * (1.0 - _suave(e1 - f.get('_OUT_MASK_Blur', 0.5), e1, r))
        if '_Color_01' in c:
            c0 = c.get('_truth_Color_01', c['_Color_01']); c1 = c.get('_truth_Color_02', c.get('_Color_02', c0))
            rgb = rampa(v, c1, c0, 0.0, 1.0)
        else:
            # (MASKNoiseScroll, la niebla verde de Ef05124: el umbral del ruido _Main_Edge1 -> _Main_Edge2; el color _Emissive
            # (el _Emissive_truth, x9.7 en verde, es para el HDR con bloom de MD: sin HDR quedaba una mancha verde chillona))
            if 'MASKNoiseScroll' in sh: v = _suave(f.get('_Main_Edge1', 0.0), f.get('_Main_Edge2', 1.0), v) * v
            em = c.get('_Emissive', c.get('_Emissive_truth', [1, 1, 1])); rgb = np.ones(v.shape + (3,)) * np.array(em[:3])[None, None, :]
            rgb = rgb / (1.0 + rgb)   # (sin HDR: lo que pasa de 1 se comprime en vez de saturar)
        return salida(rgb, v)
    return None

def empacar(texs, lado_max=512):
    """estantes simples en un atlas de 2048 de ancho; cada textura a lo sumo lado_max"""
    piezas = []
    for k, a in texs:
        h, w = a.shape[:2]
        f = min(1.0, lado_max / max(w, h))
        if f < 1.0:
            im = Image.fromarray((np.clip(a, 0, 1) * 255).astype(np.uint8), 'RGBA').resize((max(1, int(w * f)), max(1, int(h * f))), Image.LANCZOS)
            a = np.asarray(im).astype(np.float32) / 255.0
        piezas.append((k, a))
    piezas.sort(key=lambda p: -p[1].shape[0])
    W = 2048; x = y = fila = 0; lugar = {}
    for k, a in piezas:
        h, w = a.shape[:2]
        if x + w > W: x, y, fila = 0, y + fila + 2, 0
        lugar[k] = (x, y, w, h); x += w + 2; fila = max(fila, h)
    H = 1
    while H < y + fila: H *= 2
    atlas = np.zeros((H, W, 4), np.float32)
    for k, a in piezas:
        x, y, w, h = lugar[k]
        atlas[y:y + h, x:x + w] = a
    return atlas, lugar

def uvt(m, u, v):
    """uv de la malla / particula (convencion de GL de sim_fx: v hacia arriba) -> la de la textura: tiling, offset y v hacia abajo"""
    return u * m.esc[0] + m.ofs[0], 1.0 - (v * m.esc[1] + m.ofs[1])

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('carpeta'); ap.add_argument('timeline'); ap.add_argument('salida'); ap.add_argument('--dur', type=float)
    ap.add_argument('--sin', default='', help='emisores que no se exportan (separados por comas; para revisar)')
    ap.add_argument('--anim', help='animaciones_md.py: las curvas de los Transform / la opacidad de las mallas')
    ap.add_argument('--solo', default='', help='solo estos emisores / mallas (prefijos separados por comas)')
    a = ap.parse_args()
    control = {}
    if a.timeline != '-':
        for r in json.load(open(a.timeline)):
            if r['clase'] == 'ControlTrack':
                for c in r['datos'].get('m_Clips', []):
                    control.setdefault(c.get('m_DisplayName') or r['nombre'], (c['m_Start'], c['m_Duration']))
    esc = sim_fx.Escena(a.carpeta, _Ctx(), control)
    global ANIM
    ANIM = {}
    if a.anim:   # ruta -> {comp: (muestras, fps, inicio)}
        for pz in json.load(open(a.anim))['pistas']:
            for r, cs in pz['curvas'].items():
                for k, v in cs.items(): ANIM.setdefault(r, {})[k] = (v, pz['fps'], pz['inicio'])
    if a.solo:
        pre = tuple(a.solo.split(','))
        esc.emisores = [e for e in esc.emisores if e.nombre.startswith(pre)]
        esc.mallas = [m for m in esc.mallas if m['nombre'].startswith(pre)]
    if a.sin:
        fuera = set(a.sin.split(','))
        esc.emisores = [e for e in esc.emisores if e.nombre not in fuera]
        esc.mallas = [m for m in esc.mallas if m['nombre'] not in fuera]
    # duracion: el Timeline (la ultima pista) o hasta que no queda nada (tope 4 s)
    fin_tl = max([s + d for s, d in control.values()] or [0.0])
    dur = a.dur or min(max(fin_tl, 0.5), 4.0)
    # materiales usados -> atlas
    mats = []
    for e in esc.emisores:
        if e.mat not in mats: mats.append(e.mat)
    for m in esc.mallas:
        if m['mat'] not in mats: mats.append(m['mat'])
    crudo = json.load(open(os.path.join(a.carpeta, 'params.json')))['materiales']
    clave = {id(v): k for k, v in esc.mats.items()}
    texs = []; hdr = {}; modo = {}; mezcla = set(); borde = {}
    for i, m in enumerate(mats):
        r = shader_md(crudo[clave[id(m)]], a.carpeta) if id(m) in clave else None
        if r is not None:
            t, hdr[i] = r[0], r[1]
            if len(r) > 2: modo[i] = r[2]
            if len(r) > 3 and r[3] == 'mezcla': mezcla.add(i)
            if len(r) > 4 and r[4]: borde[i] = r[4]
        else:
            t = textura_material(m, a.carpeta)
        texs.append((i, t if t is not None else np.ones((4, 4, 4), np.float32)))
    atlas, lugar = empacar(texs)
    H, W = atlas.shape[:2]
    Image.fromarray((np.clip(atlas, 0, 1) * 255 + 0.5).astype(np.uint8), 'RGBA').save(a.salida + '.png')
    idx = {id(m): i for i, m in enumerate(mats)}
    global CON_BORDE
    CON_BORDE = {i for i in borde}
    cuadros = []
    t = 0.0; paso = 0; n = int(round(dur * FPS)) + 1
    for k in range(n):
        tc = k / FPS
        while t < tc - 1e-9:
            for e in esc.emisores: e.paso(t, DT)
            t += DT
        cuadros.append(cuadro(esc, tc, idx))
    # binario: MDP1, materiales, cuadros (una tira de floats por cuadro)
    with open(a.salida + '.mdp', 'wb') as f:
        f.write(b'MDP2')
        f.write(struct.pack('<iiff', len(mats), len(cuadros), float(FPS), float(dur)))
        for i, m in enumerate(mats):
            x, y, w, h = lugar[i]
            tile = (1 if (m.esc != [1, 1] or m.ofs != [0, 0]) else 0) | (2 if modo.get(i) == 1 else 0)   # (bit 2: disolucion)
            tin = np.array([hdr[i], hdr[i], hdr[i], 1.0]) if i in hdr else np.array(m.tinte, dtype=float) * np.array([m.inten, m.inten, m.inten, 1.0])
            f.write(struct.pack('<ffffiiffff', x / W, y / H, w / W, h / H, tile, 0 if i in mezcla else (1 if m.aditivo else 0), *[float(v) for v in tin]))
            b = borde.get(i)   # (MDP2: base rgb, borde rgb, potencia, modo (0 sin borde, 1 potencia, 2 smoothstep), e0, e1, luz rgb, direccion, atenuacion)
            f.write(struct.pack('<17f', *([*b[0], *b[1], b[2], float(b[3]), b[4], b[5], *b[6], *b[7], b[8]] if b else [1, 1, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1])))
        for c in cuadros:
            f.write(struct.pack('<i', len(c)))
            f.write(struct.pack('<%df' % len(c), *c))
    tam = os.path.getsize(a.salida + '.mdp')
    print(json.dumps({'pista': os.path.basename(a.salida), 'duracion': dur, 'cuadros': len(cuadros), 'materiales': len(mats), 'atlas': [W, H],
                      'bytes': tam, 'emisores': [e.nombre for e in esc.emisores], 'mallas': [m['nombre'] for m in esc.mallas]}))

CON_BORDE = set()
def _tris(out, mi, p, uv, nrm, col, mt):
    con = mi in CON_BORDE and nrm is not None
    for i in range(0, len(p) - 2, 3):
        out += [3.0 if con else 2.0, float(mi)]
        for j in range(3):
            u, v = uvt(mt, uv[i + j][0], uv[i + j][1]); out += [*map(float, p[i + j]), u, v]
            if con: out += [*map(float, nrm[i + j])]
        out += col

ANIM = {}
def _curva(r, k, t, base):
    c = ANIM.get(r, {}).get(k)
    if c is None: return base
    v, fps, ini = c
    x = (t - ini) * fps
    if x <= 0: return v[0]
    i = int(x)
    if i >= len(v) - 1: return v[-1]
    return v[i] + (v[i + 1] - v[i]) * (x - i)

def matriz_animada(m, t):
    # la malla con su Transform animado (AnimationClip de MD): M = padre * TRS(local animado)
    n = m['nodo']; r = m['ruta']
    if r not in ANIM: return m['M'], 1.0
    p = [_curva(r, 'pos.' + e, t, n['pos'][i]) for i, e in enumerate('xyz')]
    e0 = ANIM[r]
    if any(('euler.' + e) in e0 for e in 'xyz'):
        R = sim_fx.euler_mat([_curva(r, 'euler.' + e, t, 0.0) for e in 'xyz'])
    else:
        R = sim_fx.quat_mat(n['rot'])
    sc = [_curva(r, 'scale.' + e, t, n['esc'][i]) for i, e in enumerate('xyz')]
    L = np.eye(4); L[:3, :3] = R @ np.diag(sc); L[:3, 3] = p
    return m['padre'] @ L, _curva(r, 'alfa', t, 1.0)

def cuadro(esc, t, idx):
    out = []
    items = []
    for e in esc.emisores:
        if e.parts: items.append((e.orden, -float(e.M[2, 3]), 'e', e))
    for m in esc.mallas:
        if m['ventana'][0] <= t <= m['ventana'][1]: items.append(((0, 0), -float(m['M'][2, 3]), 'm', m))
    items.sort(key=lambda it: (it[0], it[1]))
    for _, _, tipo, e in items:
        if tipo == 'm':
            mt = e['mat']; M, alfa = matriz_animada(e, t); p = (M[:3, :3] @ e['malla']['pos'].T).T + M[:3, 3]; uv = e['malla']['uv']
            if alfa <= 0.002: continue
            nr = e['malla'].get('nrm')
            _tris(out, idx[id(mt)], p, uv, _nrm(M[:3, :3], nr) if nr is not None else None, [1.0, 1.0, 1.0, float(alfa)], mt)
            continue
        mt = e.mat; mi = float(idx[id(mt)])
        # scalingMode Hierarchy: Unity escala el tamanno por eje con la escala del mundo (sim_fx usa el promedio); un eje
        # en 0 (flare04 de Ef04887, hijo de Black con escala (0,0,0.88)) no se ve
        porEje = np.ones(3)
        sm = e.ps.get('scalingMode', 1)
        if sm == 0 and e.escTam > 1e-6: porEje = e.escala / e.escTam
        elif sm == 1: porEje = e.escLocal
        for q in e.parts:
            tam, c = e.tam_color(q)
            tam = tam * porEje
            if min(abs(tam[0]), abs(tam[1])) < 1e-5 and e.modo != 4: continue
            if c[3] <= 0.002: continue
            u0, v0, u1, v1 = e.uv_rect(q)
            col = [float(x) for x in c]
            if e.modo == 4 and e.malla is not None:
                rx, ry, rz = q['rot'] + q['ang']
                Rm = sim_fx.euler_mat([math.degrees(rx), math.degrees(ry), math.degrees(rz)])
                Rl = e.M[:3, :3] / np.maximum(e.escala, 1e-6) if e.alinear != 1 else np.eye(3)
                p = (Rl @ Rm @ (e.malla['pos'] * tam).T).T + q['p']
                uv = e.malla['uv'] * np.array([u1 - u0, v1 - v0]) + np.array([u0, v0])
                nr = e.malla.get('nrm')
                _tris(out, int(mi), p, uv, _nrm(Rl @ Rm @ np.diag(1.0 / np.maximum(np.abs(tam), 1e-6)), nr) if nr is not None else None, col, mt)
                continue
            sx, sy = tam[0] * 0.5, tam[1] * 0.5
            ua, va = uvt(mt, u0, v1); ub, vb = uvt(mt, u1, v0)   # (arriba-izquierda y abajo-derecha de la imagen)
            if e.modo == 1:   # estirado por la velocidad (de cara a la camara: lo arma el juego)
                v = q.get('vel_vis', q['v']); sp = float(np.linalg.norm(v))
                d = v / sp if sp > 1e-6 else np.array([0, 1.0, 0])
                largo = tam[0] * e.lenScale + sp * e.velScale
                L = d * largo
                out += [1.0, mi, *map(float, q['p']), *map(float, L), float(sx), *col, ua, va, ub, vb]
                continue
            if e.modo in (2, 3) or e.alinear in (1, 2):   # no depende de la camara: dos triangulos 3D
                if e.modo == 2: A, B = np.array([1.0, 0, 0]), np.array([0, 0, 1.0])
                elif e.modo == 3: A, B = np.array([1.0, 0, 0]), np.array([0, 1.0, 0])
                elif e.alinear == 2:
                    Rl = e.M[:3, :3] / np.maximum(e.escala, 1e-6); A, B = Rl[:, 0], Rl[:, 1]
                else: A, B = np.array([1.0, 0, 0]), np.array([0, 1.0, 0])
                rotz = q['rot'][2] + q['ang'][2]
                ca, sa = math.cos(-rotz), math.sin(-rotz)
                a2 = A * ca + B * sa; b2 = -A * sa + B * ca
                c0 = q['p']
                P = [c0 - a2 * sx + b2 * sy, c0 + a2 * sx + b2 * sy, c0 + a2 * sx - b2 * sy, c0 - a2 * sx - b2 * sy]   # (arriba-izq, arriba-der, abajo-der, abajo-izq)
                U = [(ua, va), (ub, va), (ub, vb), (ua, vb)]
                for tri in ((0, 1, 2), (0, 2, 3)):
                    out += [2.0, mi]
                    for j in tri: out += [*map(float, P[j]), *U[j]]
                    out += col
                continue
            rotz = float(q['rot'][2] + q['ang'][2])
            out += [0.0, mi, *map(float, q['p']), float(tam[0]), float(tam[1]), rotz, *col, ua, va, ub, vb]
    return out

if __name__ == '__main__':
    main()
