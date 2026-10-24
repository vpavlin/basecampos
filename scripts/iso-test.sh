#!/usr/bin/env bash
# Boot the live ISO in a throwaway QEMU (no disk), virgl, VNC on 127.0.0.1:5908.
root=$(cd "$(dirname "$0")/.." && pwd)
iso=$(ls "$root"/result-iso/iso/*.iso)
mesa=$(nix build --accept-flake-config --no-link --print-out-paths "$root#host-mesa")
qemu=$(nix build --no-link --print-out-paths nixpkgs#qemu)/bin/qemu-system-x86_64
export GBM_BACKENDS_PATH=$mesa/lib/gbm LIBGL_DRIVERS_PATH=$mesa/lib/dri \
  __EGL_VENDOR_LIBRARY_DIRS=$mesa/share/glvnd/egl_vendor.d LD_LIBRARY_PATH=$mesa/lib
ovmf=$(nix build --no-link --print-out-paths nixpkgs#OVMF.fd)/FV/OVMF.fd
nohup $qemu -enable-kvm -m 4096 -smp 4 -bios "$ovmf" -cdrom "$iso" -boot d \
  -vga none -device virtio-vga-gl,xres=1920,yres=1080 \
  -display egl-headless,rendernode=/dev/dri/renderD128 -vnc 127.0.0.1:8 \
  -nic user,model=virtio-net-pci > "$root/logs/iso-qemu.log" 2>&1 &
echo $! > "$root/vm/iso.pid"; echo "ISO VM pid $!, VNC 127.0.0.1:5908 (UEFI)"
