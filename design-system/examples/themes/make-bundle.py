#!/usr/bin/env python3
"""Pack a theme folder (theme.json + assets/: .svg, .png, .jpg) into one uploadable bundle.

Usage: make-bundle.py <theme-folder> <out.theme.json>
"""
import base64, json, pathlib, sys

src, out = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
theme = json.loads((src / "theme.json").read_text())
MIME = {".png": "image/png", ".jpg": "image/jpeg", ".jpeg": "image/jpeg"}
theme["assets"] = {}
for p in sorted((src / "assets").rglob("*")):
    rel = str(p.relative_to(src))
    if p.suffix == ".svg":
        theme["assets"][rel] = p.read_text()
    elif p.suffix in MIME:
        theme["assets"][rel] = f"data:{MIME[p.suffix]};base64," + base64.b64encode(p.read_bytes()).decode()
out.write_text(json.dumps(theme, indent=2) + "\n")
print(f"{out}: {len(theme['assets'])} assets, {out.stat().st_size // 1024} KiB")
