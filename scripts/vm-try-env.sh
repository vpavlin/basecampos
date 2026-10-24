#!/usr/bin/env bash
# vm-try-env.sh 'VAR=value ...' — restart the shell with extra environment
# (runtime drop-in, gone at reboot), wait until it answers, screenshot.
root=$(cd "$(dirname "$0")/.." && pwd)
envs=""; for kv in $1; do envs="$envs Environment=$kv"; done
"$root/scripts/vm-ssh.sh" "sudo mkdir -p /run/systemd/system/cage-tty1.service.d && printf '[Service]\n%s\n' \"$(echo $envs | tr ' ' '\n')\" | sudo tee /run/systemd/system/cage-tty1.service.d/try.conf >/dev/null && sudo systemctl daemon-reload && sudo systemctl restart cage-tty1"
for _ in $(seq 40); do sleep 3; "$root/scripts/ui.sh" listInteractive >/dev/null 2>&1 && break; done
sleep 6
"$root/scripts/vm-shot.sh" "${2:-$root/logs/shot-try.png}"
