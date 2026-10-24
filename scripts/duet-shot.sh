#!/usr/bin/env bash
# duet-shot.sh [out.png] ['sway command'] — run a sway command in the Duet VM, then screenshot.
root=$(cd "$(dirname "$0")/.." && pwd)
out=${1:-$root/logs/duet.png}
"$root/scripts/vm-ssh.sh" "export XDG_RUNTIME_DIR=/run/user/1000 WAYLAND_DISPLAY=wayland-1 SWAYSOCK=\$(ls /run/user/1000/sway-ipc.*.sock | head -1); ${2:+swaymsg '$2' >/dev/null; sleep 4;} grim -" > "$out" && echo "$out"
