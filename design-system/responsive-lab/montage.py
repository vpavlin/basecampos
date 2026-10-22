#!/usr/bin/env python3
"""Put shots side by side at their real relative widths: montage.py out.png a.png b.png ..."""
import sys
from PIL import Image
out, files = sys.argv[1], sys.argv[2:]
ims = [Image.open(f).convert("RGB") for f in files]
h = max(i.height for i in ims)
gap = 24
canvas = Image.new("RGB", (sum(i.width for i in ims) + gap * (len(ims) - 1), h), (255, 255, 255))
x = 0
for im in ims:
    canvas.paste(im, (x, 0))
    x += im.width + gap
scale = min(1.0, 2400 / canvas.width)
if scale < 1:
    canvas = canvas.resize((int(canvas.width * scale), int(canvas.height * scale)))
canvas.save(out)
