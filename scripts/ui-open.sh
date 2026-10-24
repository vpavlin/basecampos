#!/usr/bin/env bash
# ui-open.sh <app_name> — click that app's dock tile (desktop layout).
root=$(cd "$(dirname "$0")/.." && pwd)
id=$("$root/scripts/ui.sh" listInteractive | python3 -c '
import json,sys
d=json.load(sys.stdin)["elements"]
apps=[e["objectName"][12:] for e in d if (e.get("objectName") or "").startswith("sidebar.app.")]
tiles=[e["id"] for e in d if e["type"]=="QQuickMouseArea" and e["geometry"]["width"]==48 and e["geometry"]["height"]==48]
print(tiles[apps.index(sys.argv[1])] if sys.argv[1] in apps and len(tiles)==len(apps) else "")' "$1")
[ -n "$id" ] || { echo "no dock tile for $1" >&2; exit 1; }
"$root/scripts/ui.sh" click "{\"objectId\":\"$id\"}" >/dev/null && echo "opened $1"
