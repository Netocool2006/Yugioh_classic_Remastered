# Pone una cuadricula en coordenadas glTF (cada 0.05, rotulada cada 0.1) sobre las vistas de vistas_medida.py.
# Uso: python -I cuadricula.py prefijo   (lee prefijo_medida.json y prefijo_{frente,lado,arriba}.png)
import json, sys
from PIL import Image, ImageDraw

pref = sys.argv[1]
info = json.load(open(pref + "_medida.json"))
for nombre, m in info.items():
    img = Image.open("%s_%s.png" % (pref, nombre)).convert("RGB")
    W = img.size[0]
    d = ImageDraw.Draw(img)
    S = m["S"]
    (eh, h0, sh), (ev, v0, sv) = m["h"], m["v"]

    def u_de(h):   # coordenada -> pixel horizontal
        return ((h - h0) / (sh * S) + 0.5) * W

    def v_de(v):
        return (0.5 - (v - v0) / (sv * S)) * W

    k0 = int((min(h0 - S, v0 - S)) / 0.05) - 2
    k1 = int((max(h0 + S, v0 + S)) / 0.05) + 2
    for k in range(k0, k1):
        x = k * 0.05
        fuerte = k % 2 == 0
        col = (255, 255, 255) if fuerte else (170, 170, 170)
        u = u_de(x)
        if 0 <= u < W:
            d.line([(u, 0), (u, W)], fill=col, width=1)
            if fuerte:
                d.text((u + 2, 2), "%s%.1f" % (eh, x), fill=(255, 255, 0))
        v = v_de(x)
        if 0 <= v < W:
            d.line([(0, v), (W, v)], fill=col, width=1)
            if fuerte:
                d.text((2, v + 1), "%s%.1f" % (ev, x), fill=(255, 255, 0))
    img.save("%s_%s_g.png" % (pref, nombre))
print("ok", pref)
