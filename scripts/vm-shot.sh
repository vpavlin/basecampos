#!/usr/bin/env bash
# Screenshot the VM's Wayland session to $1 (default: logs/shot.png).
root=$(cd "$(dirname "$0")/.." && pwd)
out=${1:-$root/logs/shot.png}
"$root/scripts/vm-ssh.sh" 'XDG_RUNTIME_DIR=/run/user/$(id -u) WAYLAND_DISPLAY=wayland-0 grim -' > "$out" && echo "$out"
