# basecampos-duet: the Lenovo IdeaPad Duet (krane, MT8183), aarch64.
#
# Built on CI (the closure); booting it is not wired yet. The Duet boots
# through depthcharge from a signed ChromeOS kernel partition (a FIT with
# Image + dtb + initrd, signed with the devkeys) - see PLAN.md. Until the
# tablet's own kernel (6.12.50-stb-cbm + camera drivers) is packaged, this
# uses nixpkgs' 6.12 mainline kernel, which has the krane device tree.
{ lib, pkgs, ... }:

{
  networking.hostName = "basecampos-duet";
  basecampos.shell.enable = true;
  basecampos.duet = {
    enable = true;
    hardware = true;
  };

  users.users.basecamp = {
    extraGroups = [ "wheel" ];
    initialPassword = "basecamp";
  };

  boot.kernelPackages = pkgs.linuxPackages_6_12;
  hardware.deviceTree.name = "mediatek/mt8183-kukui-krane-sku176.dtb";
  # No bootloader NixOS manages: depthcharge boots a signed kernel partition.
  boot.loader.grub.enable = false;
  boot.loader.generic-extlinux-compatible.enable = false;
  boot.initrd.availableKernelModules = [ "mmc_block" "sdhci" "mtk_sd" "usb_storage" "uas" ];

  fileSystems."/" = {
    device = "/dev/disk/by-label/basecampos";
    fsType = "ext4";
  };
  swapDevices = [ ];
  zramSwap.enable = true;
}
