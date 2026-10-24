# basecampos-live: a bootable ISO (USB stick or VM) that starts Basecamp.
#
# Build: nix build .#iso   → result/iso/basecampos-*.iso
# Everything is in RAM: apps, accounts and keys are gone at power-off.
# No shrooms, no ssh, no agent: this image is for people, not the lab.
{ lib, pkgs, modulesPath, ... }:

{
  imports = [ "${modulesPath}/installer/cd-dvd/iso-image.nix" ];

  networking.hostName = "basecampos";
  basecampos.shell.enable = true;

  image.baseName = lib.mkForce "basecampos-live";
  isoImage = {
    volumeID = "BASECAMPOS";
    makeEfiBootable = true;
    makeUsbBootable = true;
    squashfsCompression = "zstd -Xcompression-level 15";
    appendToMenuLabel = " Live";
  };

  # Live media: no disk to protect, so the user may administer it.
  users.users.basecamp.extraGroups = [ "wheel" ];
  security.sudo.wheelNeedsPassword = false;

  boot.supportedFilesystems.zfs = lib.mkForce false;
  hardware.enableRedistributableFirmware = true;
}
