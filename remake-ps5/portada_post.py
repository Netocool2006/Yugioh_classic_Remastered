# Yugioh Classic Remaster (rama classic-portada): la portada (pantalla de titulo) y el fondo del menu principal a partir
# del arte original (no van al repo).
#   portada: <classic-assets>\sce_sys\portada_inicio_original.jpg (1500x837, logo "Yu-Gi-Oh! CLASSIC REMASTERED")
#     -> <classic-assets>\ui\portada.jpg          fondo 3840x2160 (Real-ESRGAN x4 + LANCZOS, recorte al centro a 16:9)
#                                                 con las letras de "CLASSIC REMASTERED" tapadas con su sombra oscura
#        <classic-assets>\ui\portada_letras.png   capa RGBA solo con las letras doradas (y su contorno negro)
#        <classic-assets>\ui\portada_brillo.png   glow dorado (mascara difuminada) con margen alrededor
#   menu: <classic-assets>\sce_sys\menu_principal_original.jpg (2000x1116: Yami Yugi, la puerta egipcia con 5 botones,
#         el Mago Oscuro y el Dragon Blanco)
#     -> <classic-assets>\ui\menu_fondo.png       fondo 3840x2160 sin el texto de los botones (el juego los escribe por
#                                                 idioma) y con el primer boton sin el fuego (normal, como los otros)
#        <classic-assets>\ui\menu_letras.png, menu_brillo.png   "CLASSIC REMASTERED" de la puerta, como en la portada
#        <classic-assets>\ui\menu_sel.png         el marco del boton elegido: el marco dorado encendido y su glow naranja
# Las letras van aparte para que el juego las haga palpitar (escala, brillo y destello: src/portada_ui.h); bajo ellas el
# fondo tiene la silueta oscura, asi al agrandarse no queda la letra original "doble" detras.
# Los rectangulos (px de la imagen de 3840x2160) se imprimen al final y estan copiados en src/portada_ui.h.
# Uso: python -I portada_post.py [portada|menu]   (sin argumento: las dos)
import os, subprocess, sys
sys.path.insert(0, r"D:\Yugi-Reload\herramientas\pylib")
import numpy as np
from PIL import Image
from scipy import ndimage as nd

ASSETS = r"D:\Docker\compartido\classic-assets"
UI = os.path.join(ASSETS, "ui")
ESRGAN = r"D:\Yugi-Reload\herramientas\esrgan\realesrgan-ncnn-vulkan.exe"
W, H = 3840, 2160
CONTORNO = 8                      # px del contorno oscuro de las letras que van con la capa
MARGEN_BRILLO = 90
# menu: los 5 botones (px de la imagen de 3840x2160): x0, x1 y la "y" de arriba de cada uno; miden 116 de alto
BOTON_X0, BOTON_X1, BOTON_H = 1656, 2186, 116
BOTONES_Y = (869, 1019, 1169, 1318, 1468)


def escalar(entrada):
    tmp = os.path.join(UI, "_x4.png")
    r = subprocess.run([ESRGAN, "-i", entrada, "-o", tmp, "-n", "realesrgan-x4plus", "-s", "4", "-t", "200"],
                       capture_output=True, text=True, cwd=os.path.dirname(ESRGAN))
    if r.returncode != 0 or not os.path.exists(tmp):
        print("Real-ESRGAN fallo (%s); se escala con LANCZOS" % r.stderr[-200:])
        im = Image.open(entrada).convert("RGB")
    else:
        im = Image.open(tmp).convert("RGB")
        im.load()
        os.remove(tmp)
    w = round(im.width * H / im.height)
    im = im.resize((w, H), Image.LANCZOS)
    x = (w - W) // 2
    return np.array(im.crop((x, 0, x + W, H)), dtype=np.float32)


def mascara(px, zona, alto_min, area_min, esperadas):
    x0, y0, x1, y1 = zona
    c = px[y0:y1, x0:x1]
    R, G, B = c[..., 0], c[..., 1], c[..., 2]
    m = nd.binary_opening((R > 120) & (G > 85) & (R - B > 45) & (G > B), iterations=1)
    lab, n = nd.label(m)
    letras = np.zeros_like(m)
    k = 0
    for i, s in enumerate(nd.find_objects(lab)):
        if s[0].stop - s[0].start > alto_min and (lab[s] == i + 1).sum() > area_min:
            letra = lab == i + 1
            letra = nd.binary_fill_holes(nd.binary_closing(letra, iterations=4))
            letras |= letra
            k += 1
    print("letras encontradas: %d (CLASSIC REMASTERED = %d)" % (k, esperadas))
    full = np.zeros((H, W), bool)
    full[y0:y1, x0:x1] = letras
    return full


def capa_letras(px, letras, nombre):
    """guarda <nombre>_letras.png y <nombre>_brillo.png; devuelve el fondo con la silueta oscura"""
    oscuro = px.max(axis=2) < 90   # el contorno oscuro, no el rojo del logo de al lado
    con = letras | (nd.binary_dilation(letras, iterations=CONTORNO) & oscuro)
    con = nd.binary_closing(con, iterations=2) | letras
    alfa = np.clip(nd.gaussian_filter(con.astype(np.float32), 1.5), 0, 1)
    ys, xs = np.nonzero(con)
    lx0, ly0, lx1, ly1 = xs.min() - 4, ys.min() - 4, xs.max() + 5, ys.max() + 5
    capa = np.dstack([px, alfa[..., None] * 255])[ly0:ly1, lx0:lx1]
    Image.fromarray(capa.round().astype(np.uint8), "RGBA").save(os.path.join(UI, nombre + "_letras.png"))
    # glow: la mascara de las letras difuminada, dorada
    bx0, by0, bx1, by1 = lx0 - MARGEN_BRILLO, ly0 - MARGEN_BRILLO, lx1 + MARGEN_BRILLO, ly1 + MARGEN_BRILLO
    g = nd.gaussian_filter(letras.astype(np.float32), 22)[by0:by1, bx0:bx1]
    g = np.clip(g / g.max() * 1.6, 0, 1) ** 0.8
    oro = np.empty(g.shape + (4,), np.float32)
    oro[..., 0], oro[..., 1], oro[..., 2], oro[..., 3] = 255, 196, 80, g * 255
    Image.fromarray(oro.round().astype(np.uint8), "RGBA").save(os.path.join(UI, nombre + "_brillo.png"))
    print("%s letras: x %d y %d w %d h %d" % (nombre, lx0, ly0, lx1 - lx0, ly1 - ly0))
    print("%s brillo: x %d y %d w %d h %d" % (nombre, bx0, by0, bx1 - bx0, by1 - by0))
    # fondo: la silueta de las letras en sombra oscura (lo que asoma cuando la capa se agranda)
    sombra = np.clip(nd.gaussian_filter(nd.binary_dilation(con, iterations=3).astype(np.float32), 2.5), 0, 1)[..., None]
    return px * (1 - sombra) + np.array([12, 7, 4], np.float32) * sombra


def portada():
    px = escalar(os.path.join(ASSETS, "sce_sys", "portada_inicio_original.jpg"))
    letras = mascara(px, (1340, 1270, 2545, 1425), 70, 800, 17)
    fondo = capa_letras(px, letras, "portada")
    Image.fromarray(fondo.round().astype(np.uint8)).save(os.path.join(UI, "portada.jpg"), quality=93, subsampling=0)


def borrar_texto(px, y):
    """rellena el texto del interior del boton (arriba en "y") con el morado del boton (relleno normalizado)"""
    x0, x1, y0, y1 = BOTON_X0 + 44, BOTON_X1 - 44, y + 22, y + BOTON_H - 22
    zona = px[y0:y1, x0:x1]
    lum = zona @ np.array([0.299, 0.587, 0.114], np.float32)
    texto = nd.binary_dilation(lum > 75, iterations=4)
    peso = (~texto).astype(np.float32)
    suma = np.dstack([nd.gaussian_filter(zona[..., c] * peso, 14) for c in range(3)])
    norma = nd.gaussian_filter(peso, 14)[..., None]
    relleno = suma / np.maximum(norma, 1e-3)
    m = np.clip(nd.gaussian_filter(texto.astype(np.float32), 1.5), 0, 1)[..., None]
    px[y0:y1, x0:x1] = zona * (1 - m) + relleno * m


def menu():
    px = escalar(os.path.join(ASSETS, "sce_sys", "menu_principal_original.jpg"))
    letras = mascara(px, (1440, 530, 2410, 800), 55, 500, 17)
    px = capa_letras(px, letras, "menu")
    for y in BOTONES_Y[1:]:
        borrar_texto(px, y)
    # el primer boton viene encendido (fuego): se pisa con una copia del tercero (sin texto), con borde suave
    M = 34
    fuente = px[BOTONES_Y[2] - M:BOTONES_Y[2] + BOTON_H + M, BOTON_X0 - 30:BOTON_X1 + 30].copy()
    a = np.ones(fuente.shape[:2], np.float32)
    a[:8], a[-8:], a[:, :8], a[:, -8:] = 0, 0, 0, 0
    a = np.clip(nd.gaussian_filter(a, 5) * 1.3, 0, 1)[..., None]
    dest = px[BOTONES_Y[0] - M:BOTONES_Y[0] + BOTON_H + M, BOTON_X0 - 30:BOTON_X1 + 30]
    px[BOTONES_Y[0] - M:BOTONES_Y[0] + BOTON_H + M, BOTON_X0 - 30:BOTON_X1 + 30] = dest * (1 - a) + fuente * a
    Image.fromarray(px.round().astype(np.uint8)).save(os.path.join(UI, "menu_fondo.png"), compress_level=6)
    # el marco elegido: el dorado del marco de un boton normal, encendido, y su glow naranja alrededor
    MS = 60
    b = px[BOTONES_Y[2] - MS:BOTONES_Y[2] + BOTON_H + MS, BOTON_X0 - MS:BOTON_X1 + MS]
    R, G, B = b[..., 0], b[..., 1], b[..., 2]
    marco = ((R > 140) & (G > 95) & (R - B > 50)).astype(np.float32)
    marco[:MS - 6], marco[-(MS - 6):], marco[:, :MS - 8], marco[:, -(MS - 8):] = 0, 0, 0, 0   # (solo el boton)
    nucleo = np.clip(nd.gaussian_filter(marco, 1.2) * 1.6, 0, 1)
    glow = np.clip(nd.gaussian_filter(marco, 14) * 3.2, 0, 1) ** 0.9
    alfa = np.maximum(nucleo, glow * 0.85)
    col = np.dstack([np.full_like(alfa, 255), 150 + 90 * nucleo, 40 + 150 * nucleo])
    sel = np.dstack([col, alfa * 255])
    Image.fromarray(sel.round().astype(np.uint8), "RGBA").save(os.path.join(UI, "menu_sel.png"))
    print("menu_sel: %dx%d (el boton + %d px por lado)" % (sel.shape[1], sel.shape[0], MS))


if __name__ == "__main__":
    que = sys.argv[1] if len(sys.argv) > 1 else "las dos"
    if que in ("portada", "las dos"):
        portada()
    if que in ("menu", "las dos"):
        menu()
