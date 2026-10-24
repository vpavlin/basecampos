#!/usr/bin/env bash
# Screenshot the VM's Wayland session (cage or sway: the socket is found) to $1.
root=$(cd "$(dirname "$0")/.." && pwd)
out=${1:-$root/logs/shot.png}
"$root/scripts/vm-ssh.sh" 'export XDG_RUNTIME_DIR=/run/user/$(id -u); WAYLAND_DISPLAY=$(ls $XDG_RUNTIME_DIR | grep -E "^wayland-[0-9]+$" | head -1) grim -' > "$out" && echo "$out"
