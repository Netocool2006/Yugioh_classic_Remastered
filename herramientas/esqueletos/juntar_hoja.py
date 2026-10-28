# Junta los cuadros de hoja_clips.py en una imagen por clip (en fila, rotulados con el numero de cuadro).
# Uso: python -I juntar_hoja.py carpeta
import glob, os, re, sys
from PIL import Image, ImageDraw

carpeta = sys.argv[1]
clips = {}
for p in glob.glob(os.path.join(carpeta, "*_*.png")):
    m = re.match(r"(.+)_(\d+)\.png$", os.path.basename(p))
    if m and not m.group(1).startswith("hoja"):
        clips.setdefault(m.group(1), []).append((int(m.group(2)), p))
for clip, cs in clips.items():
    cs.sort()
    ims = [Image.open(p).convert("RGB") for _, p in cs]
    w, h = ims[0].size
    hoja = Image.new("RGB", (w * len(ims), h + 18), (20, 20, 20))
    d = ImageDraw.Draw(hoja)
    for k, im in enumerate(ims):
        hoja.paste(im, (k * w, 18))
        d.text((k * w + 4, 2), "%s %d" % (clip, k), fill=(255, 255, 0))
    hoja.save(os.path.join(carpeta, "hoja_%s.png" % clip))
    print("hoja", clip, len(ims))
