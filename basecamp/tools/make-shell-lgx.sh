#!/usr/bin/env bash
# Package a shell plugin as a signed shell package (.lgx, manifest type "ui_shell").
#
#   tools/make-shell-lgx.sh <main_ui library> <out.lgx> [key name] [keys dir]
#
# Needs the `lgx` tool from logos-package on PATH (or LGX=/path/to/lgx). Without
# a key the package is left unsigned (Basecamp warns about that before switching).
set -euo pipefail
LIB=$1; OUT=$2; KEY=${3:-}; KEYS=${4:-$HOME/.config/logos/keys}
LGX=${LGX:-lgx}
NAME=${SHELL_NAME:-basecampium}
case "$(uname -s)-$(uname -m)" in
  Linux-x86_64) VARIANT=linux-amd64 ;; Linux-aarch64) VARIANT=linux-arm64 ;;
  Darwin-arm64) VARIANT=darwin-arm64 ;; Darwin-x86_64) VARIANT=darwin-amd64 ;;
  *) echo "unknown platform" >&2; exit 1 ;;
esac
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
( cd "$W" && "$LGX" create "$NAME" >/dev/null )
mkdir "$W/x" && tar -xzf "$W/$NAME.lgx" -C "$W/x"
python3 - "$W/x/manifest.json" <<'PY'
import json, os, sys
p = sys.argv[1]; m = json.load(open(p))
m.update({
    "type": "ui_shell",
    "category": "shell",
    "display_name": os.environ.get("SHELL_DISPLAY_NAME", "Basecamp"),
    "author": os.environ.get("SHELL_AUTHOR", "Alisher (xAlisher)"),
    "description": os.environ.get("SHELL_DESCRIPTION",
        "Basecamp's default shell: tabs, a home page, no address bar."),
    "version": os.environ.get("SHELL_VERSION", "0.1.0"),
    "hostAbiVersion": 3,
})
json.dump(m, open(p, "w"), indent=2)
PY
# Plain entry names (no "./" prefix): lgx crashes on those.
( cd "$W/x" && tar -czf "$W/$NAME.lgx" manifest.json $(ls | grep -v '^manifest.json$') )
cp "$LIB" "$W/$(basename "$LIB")"
"$LGX" add "$W/$NAME.lgx" --variant "$VARIANT" --files "$W/$(basename "$LIB")" --main "$(basename "$LIB")" -y >/dev/null
if [ -n "$KEY" ]; then
  "$LGX" sign "$W/$NAME.lgx" --key "$KEY" --keys-dir "$KEYS" \
    --name "${SHELL_AUTHOR:-Alisher (xAlisher)}" --url "${SHELL_URL:-https://github.com/xAlisher/basecamp-theming}" >/dev/null
fi
"$LGX" verify "$W/$NAME.lgx"
cp "$W/$NAME.lgx" "$OUT"
echo "wrote $OUT"
