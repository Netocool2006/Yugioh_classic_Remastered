# Rama classic-ps5-efectos: arma <assets>/md_ritmo/md_efectos.png + md_efectos_atlas.txt (FUERA del repo: texturas de
# Konami) con los sprites de Master Duel que dibuja src/md_efectos_ui.h:
#  - halo_activa: el brillo de la carta que se puede activar / activa (fxp_HL_Select_trpmgc_001, shader
#    fxs_HL_Rollover_001: la mascara con forma de carta EfDuelHighlight00_01_01 y su version difusa _01_02; se dibuja
#    SOLO la luz de afuera del borde, nunca encima de la carta: el difuso menos la mascara, mas la linea del borde).
#  - las piezas de Swords of Revealing Light (duel/timeline/card/tcg/ef04354: Ef04354 / Ef04354Op):
#    espada_brillo (EfSwordsofRevealingLight02: la espada de luz; en MD es el Flash al clavarse y la que queda clavada
#    es la malla Eff_SRL_01 con EfSwordsofRevealingLight01 sin iluminar: aca la misma espada de luz en pie), espada_estela (SwordsOfRevealingLight01, la estela que cae),
#    suelo_luz (EFF_flrstr_03_TEX, ContactArea), estrella (fxt_ple_005, las chispas "particle").
# Todo con alfa premultiplicado (se dibuja sumando luz, como los fxs_ParticleColorTexture de MD).
# Uso: python -I exportar_efectos.py <md_extraido> <salida_dir>
#   (md_extraido: D:/Docker/compartido/md_extraido; salida: D:/Docker/compartido/classic-assets/md_ritmo)
import os, sys
import numpy as np
from PIL import Image

def rgba(ruta, tam=None):
    im = Image.open(ruta).convert('RGBA')
    if tam:
        im = im.resize(tam, Image.LANCZOS)
    return np.asarray(im).astype(np.float32) / 255.0

def premult(a):
    o = a.copy()
    o[..., :3] *= o[..., 3:4]
    return o

def halo(brillo):
    m = rgba(os.path.join(brillo, 'EfDuelHighlight00_01_01_1238.png'))[..., 3]     # la carta (alfa)
    d = rgba(os.path.join(brillo, 'EfDuelHighlight00_01_02_930.png'))[..., 0]      # la carta difusa (gris)
    afuera = np.clip(d * (1.0 - m) * 3.5, 0.0, 1.0)           # la luz que sale del borde
    linea = np.clip(m * (1.0 - m) * 4.0, 0.0, 1.0)            # el borde mismo (la mascara es suave ahi)
    a = np.clip(afuera + linea, 0.0, 1.0) * (1.0 - np.clip((m - 0.5) * 2.0, 0.0, 1.0))   # (adentro de la carta: nada)
    o = np.ones(m.shape + (4,), np.float32)
    o[..., 3] = a
    im = Image.fromarray((premult(o) * 255).astype(np.uint8), 'RGBA').resize((256, 256), Image.LANCZOS)
    return np.asarray(im).astype(np.float32) / 255.0

def main():
    md, sal = sys.argv[1], sys.argv[2]
    brillo = os.path.join(md, 'ritmo', 'brillo')
    sw = os.path.join(md, 'efectos', 'swords')
    piezas = [   # nombre, imagen (premultiplicada), x, y
        ('halo_activa', halo(brillo), 0, 0),
        ('espada_brillo', premult(rgba(os.path.join(sw, 'EfSwordsofRevealingLight02_84cd3bb7.png'), (256, 512))), 512, 0),
        ('espada_estela', premult(rgba(os.path.join(sw, 'SwordsOfRevealingLight01_323b6a18.png'))), 768, 0),
        ('suelo_luz', premult(rgba(os.path.join(sw, 'EFF_flrstr_03_TEX_2f27e859.png'))), 768, 128),
        ('estrella', premult(rgba(os.path.join(sw, 'fxt_ple_005_c29d490e.png'))), 896, 128),
    ]
    atlas = np.zeros((512, 1024, 4), np.float32)
    lineas = ['# sprites de Master Duel (NO van al repo): nombre x y w h en md_efectos.png (1024x512), alfa premultiplicado']
    for n, im, x, y in piezas:
        h, w = im.shape[:2]
        atlas[y:y + h, x:x + w] = im
        lineas.append('%s\t%d\t%d\t%d\t%d' % (n, x, y, w, h))
    os.makedirs(sal, exist_ok=True)
    Image.fromarray((np.clip(atlas, 0, 1) * 255).astype(np.uint8), 'RGBA').save(os.path.join(sal, 'md_efectos.png'))
    with open(os.path.join(sal, 'md_efectos_atlas.txt'), 'w', newline='\n', encoding='utf-8') as f:
        f.write('\n'.join(lineas) + '\n')
    print('md_efectos.png:', len(piezas), 'sprites en', sal)

if __name__ == '__main__':
    main()
