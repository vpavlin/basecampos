#!/usr/bin/env bash
# Shoot every lab screen at 390 (touch), 700 (touch) and 1280 and print a
# one-line verdict per shot. Usage: check-all.sh <outdir>
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
out=${1:-/tmp/lab-shots}; mkdir -p "$out"
fail=0
for page in mail settings form dashboard table gallery; do
  for spec in "390 844 touch" "700 844 touch" "1280 800 comfortable"; do
    set -- $spec
    png="$out/$page-$1.png"
    json=$("$HERE/shot.sh" "$page" "$1" "$2" "$png" --density "$3" 2>"$out/$page-$1.err")
    python3 - "$page" "$1" "$3" "$json" "$out/$page-$1.err" <<'PY' || fail=1
import json, sys
page, w, dens, raw, errf = sys.argv[1:6]
d = json.loads(raw)
# QML warnings (file:line: …), not the pages' own console.log/warn lines ("qml: …").
warn = [l for l in open(errf) if l.strip() and "qml" in l.lower() and not l.startswith("qml: ")]
o, s = d["overflow"], d["smallTargets"]
ok = not o and not warn and (not s or dens != "touch")
print(f"{'PASS' if ok else 'FAIL'} {page:10} {w:>5} {dens:12} overflow={len(o)} small={len(s)} warnings={len(warn)}")
for x in (o + s)[:6]:
    print("      ", x["item"][:70], x["rect"])
for l in warn[:3]:
    print("       W", l.strip()[:120])
sys.exit(0 if ok else 1)
PY
  done
done
exit $fail
