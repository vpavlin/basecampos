# basecampos-desktop: BasecampOS installed on a real disk.
#
# Build: nix build .#disk-image → result/*.img (raw, UEFI)
# Write: dd if=result/basecampos-desktop.img of=/dev/sdX bs=4M status=progress
# The root partition grows to fill the disk on first boot; apps, accounts and
# keys persist across reboots.
#
# No ssh, no mesh, no agent: this is the image for people. The local password
# exists only for sudo (the session logs in by itself); change it with passwd.
{ lib, pkgs, modulesPath, ... }:

{
  networking.hostName = "basecampos";
  basecampos.shell.enable = true;

  users.users.basecamp = {
    extraGroups = [ "wheel" ];
    initialPassword = "basecamp";
  };

  boot.loader.systemd-boot.enable = true;
  boot.loader.efi.canTouchEfiVariables = false;
  boot.loader.timeout = 1;
  boot.growPartition = true;
  boot.initrd.availableKernelModules = [ "nvme" "xhci_pci" "ahci" "usb_storage" "sd_mod" "virtio_pci" "virtio_blk" ];

  fileSystems."/" = {
    device = "/dev/disk/by-label/nixos";
    fsType = "ext4";
    autoResize = true;
  };
  fileSystems."/boot" = {
    device = "/dev/disk/by-label/ESP";
    fsType = "vfat";
  };

  hardware.enableRedistributableFirmware = true;
  services.fwupd.enable = true;
  zramSwap.enable = true;
}
