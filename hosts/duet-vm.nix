# basecampos-duet-vm: the Duet session (duet-kit's Sway tablet + Basecamp) in an
# x86 VM on the dev host, to develop it without the tablet. Same disk/ssh/VNC as
# basecampos-vm, so run one at a time:
#   RESULT=result-duet-vm DISK=$PWD/vm/duet.qcow2 scripts/vm-run.sh
{ pkgs, ... }:

{
  imports = [ ./vm-common.nix ];

  networking.hostName = "basecampos-duet-vm";
  basecampos.shell.enable = true;
  basecampos.duet = {
    enable = true;
    hardware = false;
  };

  # The Duet's panel in landscape.
  virtualisation.vmVariant.virtualisation.qemu.options = [
    "-device virtio-vga-gl,xres=1920,yres=1200"
    "-device virtio-tablet-pci"
  ];
}
