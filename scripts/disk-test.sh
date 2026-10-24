#!/usr/bin/env bash
# Boot the raw-EFI desktop image (result-disk) in QEMU via a copy-on-write
# overlay (vm/desktop.qcow2), UEFI, virgl, VNC 127.0.0.1:5908. --fresh resets it.
root=$(cd "$(dirname "$0")/.." && pwd)
img=$(ls "$root"/result-disk/*.img)
qemu=$(nix build --no-link --print-out-paths nixpkgs#qemu)/bin
[ "$1" = --fresh ] && rm -f "$root/vm/desktop.qcow2"
[ -f "$root/vm/desktop.qcow2" ] || "$qemu/qemu-img" create -q -f qcow2 -b "$img" -F raw "$root/vm/desktop.qcow2" 24G
mesa=$(nix build --accept-flake-config --no-link --print-out-paths "$root#host-mesa")
export GBM_BACKENDS_PATH=$mesa/lib/gbm LIBGL_DRIVERS_PATH=$mesa/lib/dri \
  __EGL_VENDOR_LIBRARY_DIRS=$mesa/share/glvnd/egl_vendor.d LD_LIBRARY_PATH=$mesa/lib
ovmf=$(nix build --no-link --print-out-paths nixpkgs#OVMF.fd)/FV/OVMF.fd
nohup "$qemu/qemu-system-x86_64" -enable-kvm -m 4096 -smp 4 -bios "$ovmf" \
  -drive file="$root/vm/desktop.qcow2",if=virtio \
  -vga none -device virtio-vga-gl,xres=1920,yres=1080 \
  -display egl-headless,rendernode=/dev/dri/renderD128 -vnc 127.0.0.1:8 \
  -nic user,model=virtio-net-pci > "$root/logs/disk-qemu.log" 2>&1 &
echo $! > "$root/vm/disk.pid"; echo "disk VM pid $!, VNC 127.0.0.1:5908"
