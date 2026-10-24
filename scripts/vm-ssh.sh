#!/usr/bin/env bash
exec ssh -p 2222 -i ~/.ssh/basecampos_vm_ed25519 -o StrictHostKeyChecking=no \
  -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -o ConnectTimeout=5 basecamp@127.0.0.1 "$@"
