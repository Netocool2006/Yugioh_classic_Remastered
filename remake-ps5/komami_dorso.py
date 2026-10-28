# Dorso de las cartas boca abajo: cambia el logotipo KONAMI por KOMAMI (pedido del usuario).
# La "M" nueva no se dibuja: es la M que ya trae el propio logotipo (KONA[M]I), copiada pixel a pixel, asi tiene
# exactamente su grosor, color, antialiasing, altura e inclinacion:
#  1) la zona de N, A, M, I y (R) se rellena con el fondo que la rodea (relleno armonico, sin las letras) y de
#     cada letra blanca se saca su alfa: (min(R,G,B) - fondo) / (blanco - fondo); el fondo rojo/naranja casi no
#     tiene azul, asi que el min de canales separa bien el blanco. El (R) gris se mueve como (imagen - fondo).
#  2) la N se borra (queda solo el fondo) y en su lugar se compone la M (16 px) con el mismo blanco
#  3) la M es 2 px mas ancha que la N (14 px) y entre letras hay 1 px: para no angostarla ni que se peguen,
#     "AMI(R)" se corre 2 px a la derecha (desplazamientos enteros: nada se reescala ni se emborrona)
# Coordenadas para el dorso de 400x580 (card_ura.bmp de 200x290 mejorado con Real-ESRGAN); la mitad para el tablero
# la genera interfaz.h al cargar, de este mismo archivo. La primera vez guarda el original como
# dorso_konami_original.png al lado y siempre parte de ese (se puede volver a correr).
# Uso: python komami_dorso.py <ruta/dorso.png> [comparacion.png]
import os, shutil, sys
import numpy as np
from PIL import Image
from scipy import ndimage

ruta = sys.argv[1]
original = os.path.join(os.path.dirname(os.path.abspath(ruta)), "dorso_konami_original.png")
if not os.path.exists(original):
    shutil.copyfile(ruta, original)   # tal cual, byte a byte
fuente = Image.open(original)
if fuente.size != (400, 580):
    sys.exit("se esperaba el dorso de 400x580 y es %dx%d" % fuente.size)
modo = fuente.mode
img = np.array(fuente.convert("RGBA")).astype(np.float64)
rgb, alfa_png = img[:, :, :3], img[:, :, 3]
minc = rgb.min(axis=2)   # el blanco y el gris del logotipo tienen azul; el fondo rojo/naranja casi no

Y0, Y1 = 25, 40          # filas del logotipo (la 24 es una veta del fondo sobre la M)
LETRAS = {               # columnas (inclusive) de cada letra en el original
    "N": (49, 62), "A": (64, 79), "M": (79, 94), "I": (96, 100), "R": (100, 109),
}
CORRE = 2                # px que se corre "AMI(R)" a la derecha
N_A_M = LETRAS["M"][0] - LETRAS["N"][0]   # la M nueva empieza donde empezaba la N


def mascara(x0, x1, umbral=30):
    m = np.zeros(minc.shape, bool)
    m[Y0:Y1 + 1, x0:x1 + 1] = minc[Y0:Y1 + 1, x0:x1 + 1] > umbral
    m = ndimage.binary_dilation(m, iterations=1)
    m[:Y0] = False   # la fila de arriba es la veta amarilla sobre la M: queda como esta
    return m


def rellenar(imagen, m, vueltas=6000):
    """relleno armonico: cada pixel de la mascara es el promedio de sus 4 vecinos (borde = lo que la rodea)"""
    ys, xs = np.nonzero(m)
    y0, y1, x0, x1 = ys.min() - 2, ys.max() + 3, xs.min() - 2, xs.max() + 3
    r = imagen[y0:y1, x0:x1].copy()
    mm = m[y0:y1, x0:x1]
    r[mm] = r[~mm].mean(axis=0)
    for _ in range(vueltas):
        p = np.pad(r, ((1, 1), (1, 1), (0, 0)), mode="edge")
        r[mm] = ((p[:-2, 1:-1] + p[2:, 1:-1] + p[1:-1, :-2] + p[1:-1, 2:]) / 4.0)[mm]
    salida = imagen.copy()
    salida[y0:y1, x0:x1] = r
    return salida


# todas las letras desde la N (K y O no se tocan; la O termina en x 47-48)
zona = mascara(LETRAS["N"][0], LETRAS["R"][1])
zona[:, :LETRAS["N"][0] - 1] = False
fondo = rellenar(rgb, zona)
# letras blancas: alfa por matte (min de canales contra el fondo relleno), se componen con su blanco sobre el
# fondo nuevo (la M venia sobre una veta naranja y va sobre fondo oscuro: sumar la diferencia la teniria)
BLANCO = np.array([252.1, 252.8, 251.5])   # color medio de los pixeles llenos de las letras
fmin = fondo.min(axis=2)
alfa = np.clip((minc - fmin) / (BLANCO.min() - fmin), 0, 1) * zona
alfa[Y0, 85:89] = 0   # la veta del fondo que asoma entre las dos puntas de la M (fila 25)
# el (R) es gris y sombreado: se mueve como diferencia con el fondo relleno (corre poco y su fondo es oscuro)
capaR = (rgb - fondo) * zona[:, :, None]


def mover(c, dx):
    s = np.zeros_like(c)
    if dx >= 0:
        s[:, dx:] = c[:, :c.shape[1] - dx]
    else:
        s[:, :dx] = c[:, -dx:]
    return s


def recorte(c, x0, x1):
    s = np.zeros_like(c)
    s[:, x0:x1 + 1] = c[:, x0:x1 + 1]
    return s


# la M sola (x 78..95, su borde blando; la I empieza en 96) va donde estaba la N; "AMI" (desde el pie de la A,
# x 63, hasta la I, x 100) y el (R) (x 101..) se corren CORRE px; la N no se copia: queda borrada
alfaM = recorte(alfa, LETRAS["M"][0] - 1, LETRAS["M"][1] + 1)
alfaAMI = recorte(alfa, LETRAS["N"][1] + 1, LETRAS["I"][1])
nueva = np.maximum(mover(alfaAMI, CORRE), mover(alfaM, -N_A_M))[:, :, None]
res = fondo * (1 - nueva) + BLANCO * nueva + mover(recorte(capaR, LETRAS["I"][1] + 1, rgb.shape[1] - 1), CORRE)
fuera = np.dstack([np.clip(np.round(res), 0, 255), alfa_png]).astype(np.uint8)
Image.fromarray(fuera, "RGBA").convert(modo).save(ruta)
print("KOMAMI ->", ruta, "(original en %s)" % original)

if len(sys.argv) > 2:   # comparacion antes/despues: dorso entero, el logotipo a x4 y a x8
    a, b = Image.open(original).convert("RGB"), Image.open(ruta).convert("RGB")
    caja = (12, 18, 116, 46)
    z4 = [im.crop(caja).resize(((caja[2] - caja[0]) * 4, (caja[3] - caja[1]) * 4), Image.NEAREST) for im in (a, b)]
    caja8 = (20, 20, 114, 43)
    z8 = [im.crop(caja8).resize(((caja8[2] - caja8[0]) * 8, (caja8[3] - caja8[1]) * 8), Image.NEAREST)
          for im in (a, b)]
    W = max(a.width * 2 + 30, z4[0].width * 2 + 30, z8[0].width)
    H = a.height + 20 + z4[0].height + 20 + z8[0].height * 2 + 30
    c = Image.new("RGB", (W, H), (40, 40, 40))
    c.paste(a, (0, 0)); c.paste(b, (a.width + 30, 0))
    y = a.height + 20
    c.paste(z4[0], (0, y)); c.paste(z4[1], (z4[0].width + 30, y))
    y += z4[0].height + 20
    c.paste(z8[0], (0, y)); c.paste(z8[1], (0, y + z8[0].height + 10))
    c.save(sys.argv[2])
    print("comparacion ->", sys.argv[2])
