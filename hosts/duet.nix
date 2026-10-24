# basecampos-duet: the Lenovo IdeaPad Duet (krane, MT8183), aarch64.
#
# Built on CI. Boots through depthcharge from a signed ChromeOS kernel
# partition (modules/duet/depthcharge.nix): `packages.aarch64-linux.duet-image`
# is a USB image (Ctrl+U in developer mode). Until the tablet's own kernel
# (6.12.50-stb-cbm + camera drivers) is packaged, this uses nixpkgs' 6.12
# mainline kernel, which has the krane device tree; cameras won't work on it.
{ lib, pkgs, ... }:

{
  imports = [ ../modules/duet/depthcharge.nix ];
  basecampos.depthcharge.enable = true;

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
  boot.initrd.availableKernelModules = [ "mmc_block" "sdhci" "mtk_sd" "usb_storage" "uas" ];

  # The panel and console: depthcharge hands over a framebuffer-less boot.
  boot.kernelParams = [ "console=tty1" "fbcon=rotate:1" ];
  swapDevices = [ ];
  zramSwap.enable = true;
}
