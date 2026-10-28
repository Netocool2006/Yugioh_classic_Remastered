#!/usr/bin/env python3
# Rama classic-ps5-efectos: prueba de los sprites de Master Duel que pide la interfaz (pedido del usuario: «a las cartas
# activas le dejas esta barra amarilla horrible encima»: el sprite md_brillo_marco era la MASCARA llena de
# EfDuelHighlight00_01_01 dibujada como imagen, un rectangulo amarillo solido; las pruebas de siempre no lo veian).
# FALLA si:
#  1. un sprite que el codigo pide por nombre (spriteMD / spriteRitmoMD / tamSpriteMD / tamSpriteRitmoMD / luzMundoMD /
#     luzSueloMD / luzPantallaMD / uvEfMD) no esta en el atlas que el TITULO lleva (md_ui/md_ui_atlas.txt o
#     md_ritmo/md_efectos_atlas.txt), o falta su textura en el titulo, o el sprite se sale de la textura;
#  2. un sprite que se dibuja como IMAGEN (spriteMD: tapa lo de abajo) es una mascara: grande (128x128 o mas), casi la
#     mitad opaca y de un solo color (asi se ve como un bloque de color solido encima de la carta).
# Uso: python3 prueba_sprites_md.py [src] [assets del titulo]
#   (por defecto: el src de la rama y /compartido/ps5/PPSA77002/assets, el que arma sincronizar.sh)
import os, re, sys

src = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
assets = sys.argv[2] if len(sys.argv) > 2 else '/compartido/ps5/PPSA77002/assets'
ATLAS = {   # funcion -> (atlas, textura, se dibuja tapando: IMAGEN)
    'spriteMD': ('md_ui/md_ui_atlas.txt', 'md_ui/md_ui.png', True),
    'spriteRitmoMD': ('md_ui/md_ui_atlas.txt', 'md_ui/md_ui.png', True),
    'tamSpriteMD': ('md_ui/md_ui_atlas.txt', 'md_ui/md_ui.png', False),
    'tamSpriteRitmoMD': ('md_ui/md_ui_atlas.txt', 'md_ui/md_ui.png', False),
    'luzMundoMD': ('md_ritmo/md_efectos_atlas.txt', 'md_ritmo/md_efectos.png', False),
    'luzSueloMD': ('md_ritmo/md_efectos_atlas.txt', 'md_ritmo/md_efectos.png', False),
    'luzPantallaMD': ('md_ritmo/md_efectos_atlas.txt', 'md_ritmo/md_efectos.png', False),
    'uvEfMD': ('md_ritmo/md_efectos_atlas.txt', 'md_ritmo/md_efectos.png', False),
}
fallas, oks = [], 0

def leer_atlas(ruta):
    d = {}
    for l in open(ruta, encoding='utf-8'):
        c = l.rstrip('\n').split('\t')
        if l.startswith('#') or len(c) < 5:
            continue
        d[c[0]] = tuple(int(float(x)) for x in c[1:5])
    return d

def es_mascara(img, caja):
    x, y, w, h = caja
    reg = img.crop((x, y, x + w, y + h)).convert('RGBA')
    px = list(reg.getdata())
    opacos = [p for p in px if p[3] > 230]
    if w * h < 128 * 128 or len(opacos) < 0.45 * len(px):   # (los iconos chicos de un color, como la aguja, son dibujos)
        return False
    r = [p[0] for p in opacos]; g = [p[1] for p in opacos]; b = [p[2] for p in opacos]
    rango = max(max(r) - min(r), max(g) - min(g), max(b) - min(b))
    return rango < 12   # (un solo color en todo lo opaco: es una mascara, no un dibujo)

pedidos = {}   # (funcion, nombre) -> archivo:linea
for f in sorted(os.listdir(src)):
    if not f.endswith('.h') and not f.endswith('.cpp'):
        continue
    for n, l in enumerate(open(os.path.join(src, f), encoding='utf-8', errors='replace'), 1):
        if l.lstrip().startswith('//'):
            continue
        for m in re.finditer(r'\b(' + '|'.join(ATLAS) + r')\("([A-Za-z0-9_]+)"', l):
            pedidos.setdefault((m.group(1), m.group(2)), '%s:%d' % (f, n))

try:
    from PIL import Image
except ImportError:
    Image = None
cache = {}
for (fn, nombre), donde in sorted(pedidos.items()):
    atlas, tex, tapa = ATLAS[fn]
    ra, rt = os.path.join(assets, atlas), os.path.join(assets, tex)
    if not os.path.isfile(ra) or not os.path.isfile(rt):
        fallas.append('FALLA %s("%s") (%s): el titulo no lleva %s / %s' % (fn, nombre, donde, atlas, tex))
        continue
    if ra not in cache:
        cache[ra] = leer_atlas(ra)
    if nombre not in cache[ra]:
        fallas.append('FALLA %s("%s") (%s): el sprite no esta en %s del titulo' % (fn, nombre, donde, atlas))
        continue
    if Image:
        if rt not in cache:
            cache[rt] = Image.open(rt)
        img = cache[rt]
        x, y, w, h = cache[ra][nombre]
        if x < 0 or y < 0 or x + w > img.width or y + h > img.height:
            fallas.append('FALLA %s("%s") (%s): el sprite se sale de %s' % (fn, nombre, donde, tex))
            continue
        if tapa and es_mascara(img, (x, y, w, h)):
            fallas.append('FALLA %s("%s") (%s): es una MASCARA de un solo color dibujada como imagen (se ve como un bloque solido)' % (fn, nombre, donde))
            continue
    oks += 1
    print('ok  %s("%s") %s' % (fn, nombre, donde))
# 3. (rama classic-ps5-efectos, 2a vuelta) las pistas 3D de los CardRunEffect de MD (md_cardrun_ui.h) estan en el titulo
pistas = set()
for pc in (os.path.join(src, 'md_cardrun_ui.h'), os.path.join(src, 'md_efectos_ui.h')):
    if not os.path.isfile(pc): continue
    for l in open(pc, encoding='utf-8'):
        if l.lstrip().startswith('//'): continue
        for m in re.finditer(r'"(ef\d{5}[a-z_]*)"', l): pistas.add(m.group(1))
for n in sorted(pistas):
    b = os.path.join(assets, 'md_ritmo', 'efectos', n)
    if not os.path.isfile(b + '.mdp') or not os.path.isfile(b + '.png'):
        fallas.append('FALLA pista "%s": el titulo no lleva md_ritmo/efectos/%s.mdp / .png' % (n, n)); continue
    if open(b + '.mdp', 'rb').read(4) not in (b'MDP1', b'MDP2'):
        fallas.append('FALLA pista "%s": %s.mdp no es una pista MDP1 / MDP2' % (n, n)); continue
    oks += 1
    print('ok  pista "%s"' % n)
for x in fallas:
    print(x)
print('prueba_sprites_md: %d sprites ok, %d fallas' % (oks, len(fallas)))
sys.exit(1 if fallas or not pedidos else 0)
