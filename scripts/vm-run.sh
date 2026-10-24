#!/usr/bin/env bash
# Boot basecampos-vm detached. Disk persists in vm/ (shrooms identity lives there).
# Console log: logs/vm-console.log. Stop: scripts/vm-ssh.sh sudo poweroff
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
mkdir -p "$root/vm" "$root/logs"
export NIX_DISK_IMAGE="${DISK:-$root/vm/basecampos-vm.qcow2}"
export QEMU_OPTS="-serial file:$root/logs/vm-console.log -monitor unix:$root/vm/monitor.sock,server,nowait"
export QEMU_KERNEL_PARAMS="console=ttyS0"
# virgl: Nix's QEMU looks for Mesa in /run/opengl-driver, which only NixOS has.
if [ ! -e /run/opengl-driver ]; then
  mesa=$(nix build --accept-flake-config --no-link --print-out-paths "$root#host-mesa")
  export GBM_BACKENDS_PATH=$mesa/lib/gbm LIBGL_DRIVERS_PATH=$mesa/lib/dri \
    __EGL_VENDOR_LIBRARY_DIRS=$mesa/share/glvnd/egl_vendor.d LD_LIBRARY_PATH=$mesa/lib
fi
cd "$root/vm"
nohup "$(ls "$root/${RESULT:-result-vm}"/bin/run-*-vm)" > "$root/logs/vm-qemu.log" 2>&1 &
echo $! > "$root/vm/qemu.pid"
echo "started pid $!; ssh: scripts/vm-ssh.sh; screen: VNC 127.0.0.1:5907"
