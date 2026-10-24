#!/usr/bin/env bash
# Stop, boot, and wait until ssh and the shell are up.
root=$(cd "$(dirname "$0")/.." && pwd)
"$root/scripts/vm-stop.sh"
"$root/scripts/vm-run.sh"
for _ in $(seq 60); do "$root/scripts/vm-ssh.sh" true 2>/dev/null && break; sleep 5; done
for _ in $(seq 60); do "$root/scripts/vm-ssh.sh" 'pgrep -x LogosBasecamp >/dev/null || pgrep -f bin/LogosBasecamp >/dev/null' && { echo "up"; exit 0; }; sleep 2; done
echo "ssh up, Basecamp not running"; exit 1
