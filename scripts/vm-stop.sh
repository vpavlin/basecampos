#!/usr/bin/env bash
# Clean shutdown of basecampos-vm; waits until QEMU has exited.
root=$(cd "$(dirname "$0")/.." && pwd)
pid=$(cat "$root/vm/qemu.pid" 2>/dev/null) || { echo "not running"; exit 0; }
kill -0 "$pid" 2>/dev/null || { echo "not running"; exit 0; }
"$root/scripts/vm-ssh.sh" 'sudo systemctl poweroff' 2>/dev/null
for _ in $(seq 60); do kill -0 "$pid" 2>/dev/null || { echo stopped; exit 0; }; sleep 2; done
echo "still running after 120s; killing"; kill "$pid"
