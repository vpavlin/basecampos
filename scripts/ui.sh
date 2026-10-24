#!/usr/bin/env bash
# ui.sh <command> [json|file] — one QML inspector command against the VM's Basecamp.
root=$(cd "$(dirname "$0")/.." && pwd)
export UI_PORT=13768
if ! (exec 3<>/dev/tcp/127.0.0.1/$UI_PORT) 2>/dev/null; then
  ssh -p 2222 -i ~/.ssh/basecampos_vm_ed25519 -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
    -o LogLevel=ERROR -o ExitOnForwardFailure=yes -f -N -L $UI_PORT:127.0.0.1:3768 basecamp@127.0.0.1
fi
node=$(command -v node || echo "$(nix build --no-link --print-out-paths nixpkgs#nodejs 2>/dev/null)/bin/node")
exec "$node" "$root/scripts/ui.mjs" "$@"
