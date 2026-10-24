# What every BasecampOS machine shares.
{ lib, pkgs, ... }:

{
  system.stateVersion = lib.mkDefault "26.05";
  time.timeZone = lib.mkDefault "Europe/Prague";
  i18n.defaultLocale = lib.mkDefault "en_US.UTF-8";

  nix.settings = {
    experimental-features = [ "nix-command" "flakes" ];
    trusted-users = [ "root" "@wheel" ];
    extra-substituters = [ "https://cache.nix.logos.co/public" ];
    extra-trusted-public-keys = [ "public:l4HrXgL4nw246+LBh2SOJyhz64BoGegOYLheT/iIAPU=" ];
  };

  # A quiet boot straight into the shell.
  boot.kernelParams = [ "quiet" "loglevel=3" "udev.log_level=3" ];
  boot.consoleLogLevel = 3;
  boot.initrd.verbose = false;

  environment.systemPackages = with pkgs; [ git vim htop curl jq ];
}
