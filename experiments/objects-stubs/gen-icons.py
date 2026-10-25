#!/usr/bin/env python3
"""Draws the stub apps' icons: objects_<app>/icon.png, 256x256.

Each icon is a full-bleed square: an opaque colour to every edge and corner,
with the app's letters in the middle. No rounded corners and no rounded
background are drawn here. The shell rounds every app tile when it draws it
(BasecampAppTile in basecamp; basecamp-experiments#40), so a rounding baked into
the picture would show as a second, smaller shape inside the tile.

    python3 gen-icons.py           write the five icons
    python3 gen-icons.py --check   exit 1 if an icon has a transparent corner

Needs Pillow and DejaVu Sans Bold (any Linux font package ships it).
"""
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

HERE = Path(__file__).resolve().parent
SIZE = 256
FONTS = [
    "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
    "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf",
    "/usr/share/fonts/dejavu/DejaVuSans-Bold.ttf",
]
# app -> (letters, background, font size)
ICONS = {
    "objects_wallet":      ("W",  (61, 139, 110), 118),
    "objects_chat":        ("C",  (74, 111, 209), 118),
    "objects_communities": ("Co", (138, 85, 196), 99),
    "objects_news":        ("N",  (196, 138, 42), 118),
    "objects_explorer":    ("E",  (90, 99, 112), 118),
}


def font(size):
    for f in FONTS:
        if Path(f).exists():
            return ImageFont.truetype(f, size)
    sys.exit("gen-icons.py: DejaVu Sans Bold not found; looked in " + ", ".join(FONTS))


def draw(letters, colour, size):
    im = Image.new("RGBA", (SIZE, SIZE), colour + (255,))
    d = ImageDraw.Draw(im)
    f = font(size)
    left, top, right, bottom = d.textbbox((0, 0), letters, font=f)
    x = (SIZE - (right - left)) / 2 - left
    y = (SIZE - (bottom - top)) / 2 - top
    d.text((x, y), letters, font=f, fill=(255, 255, 255, 255))
    return im


def square(path):
    im = Image.open(path).convert("RGBA")
    w, h = im.size
    return all(im.getpixel(c)[3] == 255 for c in [(0, 0), (w - 1, 0), (0, h - 1), (w - 1, h - 1)])


if __name__ == "__main__":
    if sys.argv[1:] == ["--check"]:
        bad = [a for a in ICONS if not square(HERE / a / "icon.png")]
        for a in bad:
            print(f"rounded or transparent corner: {a}/icon.png")
        sys.exit(1 if bad else 0)
    for app, (letters, colour, size) in ICONS.items():
        out = HERE / app / "icon.png"
        draw(letters, colour, size).save(out)
        print("wrote", out.relative_to(HERE))
