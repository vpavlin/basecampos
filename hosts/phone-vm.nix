# basecampos-phone-vm: BasecampOS on a phone (phoc, Stevia, Calls; modules/phone)
# in an x86 VM with a portrait 720x1440 screen at scale 2 (360x720 logical:
# Basecampium's compact phone layout). Same ssh/VNC as the other dev VMs, so
# run one at a time:
#   RESULT=result-phone-vm DISK=$PWD/vm/phone.qcow2 scripts/vm-run.sh
{ ... }:

{
  imports = [ ./vm-common.nix ];

  networking.hostName = "basecampos-phone-vm";
  basecampos.shell.enable = true;
  basecampos.phone = {
    enable = true;
    outputs."Virtual-1".scale = 2;
  };

  # The VM always has a keyboard; Stevia hides on phones with one plugged in.
  programs.dconf.profiles.user.databases = [{
    settings."mobi/phosh/osk".ignore-hw-keyboards = true;
  }];

  virtualisation.vmVariant.virtualisation.qemu.options = [
    "-device virtio-vga-gl,xres=720,yres=1440"
    "-device virtio-tablet-pci"
  ];
}
