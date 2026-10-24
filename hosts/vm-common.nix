# What every dev VM on the dev host shares: ssh with the dev key, passwordless sudo,
# QEMU with virgl on the dev host's render node, VNC. Ports/displays per VM below.
{ lib, ... }:

{
  services.openssh = {
    enable = true;
    settings = {
      PasswordAuthentication = false;
      PermitRootLogin = "no";
    };
  };
  users.users.basecamp.openssh.authorizedKeys.keys = [
    "ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIDQRq7mVUnDod0anH4c2ZTUcD8U9Nr/4FMj5I3mp2m+f basecampos-vm-access"
  ];
  users.users.basecamp.extraGroups = [ "wheel" ];
  security.sudo.wheelNeedsPassword = false;

  virtualisation.vmVariant = {
    # The VM's /nix/store is the host's, shared over 9p and owned by the host
    # user; logrotate refuses configs not owned by root.
    systemd.services.logrotate-checkconf.enable = false;

    virtualisation = {
      memorySize = 4096; # the dev host limit (2026-10-24, after an OOM): at most 4 GB
      cores = 4;
      diskSize = 40 * 1024;
      graphics = true;
      forwardPorts = [ { from = "host"; host.address = "127.0.0.1"; host.port = 2222; guest.port = 22; } ];
      # virgl on the dev host's render node: Basecamp's translucent Qt Quick layers
      # (dock, glass) render black on software GL.
      qemu.options = [
        "-vga none"
        "-display egl-headless,rendernode=/dev/dri/renderD128"
        "-vnc 127.0.0.1:7"
      ];
    };
  };
}
