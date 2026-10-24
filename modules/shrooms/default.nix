# shrooms mesh daemon + shrooms-agent for a BasecampOS machine.
#
# Mirrors the fleet's install.sh setup (see the shrooms installer): the daemon runs from the
# ghcr.io/vpavlin/shrooms container, `shrooms` is a thin podman wrapper, and
# register-dns keeps systemd-resolved pointed at the mesh resolver.
#
# Never baked into the image: the network key, invite tokens, /var/lib/shrooms.
# On first boot `shrooms prepare` writes a key-less config and generates the
# device identity on the machine's own disk; joining is a manual, one-time
#   sudo shrooms join <TOKEN>
{ config, lib, pkgs, ... }:

let
  cfg = config.services.shrooms;

  wrapper = pkgs.writeShellScriptBin "shrooms" (
    builtins.replaceStrings
      [ "#!/bin/sh\n" "RUNTIME=/usr/sbin/podman" ]
      [ "" "RUNTIME=${pkgs.podman}/bin/podman" ]
      (builtins.readFile ./shrooms-wrapper.sh));

  registerDns = pkgs.writeShellScript "register-dns" (
    builtins.replaceStrings
      [ "#!/bin/sh\n" "/usr/local/bin/shrooms" ]
      [ "" "${wrapper}/bin/shrooms" ]
      (builtins.readFile ./register-dns.sh));

  podman = "${pkgs.podman}/bin/podman";

  # `prepare` and `join` both write the default mode (Core); the declared one
  # is put back on first boot and on every start.
  setMode = pkgs.writeShellScript "shrooms-set-mode" ''
    [ -f /etc/shrooms/config.toml ] || exit 0
    ${pkgs.gnused}/bin/sed -i -E 's/^([Mm]ode *= *)"[A-Za-z]+"/\1"${cfg.mode}"/' /etc/shrooms/config.toml
  '';
in
{
  options.services.shrooms = {
    enable = lib.mkEnableOption "the shrooms overlay mesh";
    name = lib.mkOption {
      type = lib.types.str;
      default = config.networking.hostName;
      description = "Device name on the mesh.";
    };
    mode = lib.mkOption {
      type = lib.types.enum [ "Core" "Edge" ];
      default = "Edge";
    };
    image = lib.mkOption {
      type = lib.types.str;
      default = "ghcr.io/vpavlin/shrooms:latest";
    };

    agent = {
      enable = lib.mkEnableOption "shrooms-agent (coding-agent sessions over the mesh)";
      user = lib.mkOption { type = lib.types.str; };
      package = lib.mkOption {
        type = lib.types.package;
        description = "A package providing bin/shrooms-agent.";
      };
      extraArgs = lib.mkOption {
        type = lib.types.listOf lib.types.str;
        default = [ "--cages=false" ];
      };
      path = lib.mkOption {
        type = lib.types.listOf lib.types.package;
        default = [ ];
        description = "Harnesses and tools the sessions should find (claude-code, pi, …).";
      };
    };
  };

  config = lib.mkIf cfg.enable {
    virtualisation.podman.enable = true;
    services.resolved.enable = true;
    environment.systemPackages = [ wrapper ];
    networking.firewall.trustedInterfaces = [ "shrooms+" ];
    networking.firewall.allowedUDPPorts = [ 51820 51821 ];

    systemd.tmpfiles.rules = [
      "d /etc/shrooms 0750 root root -"
      "d /var/lib/shrooms 0700 root root -"
    ];

    # First boot only: identity on this disk, config without a network key.
    systemd.services.shrooms-prepare = {
      description = "shrooms: prepare a key-less config on first boot";
      wantedBy = [ "multi-user.target" ];
      before = [ "shrooms.service" ];
      after = [ "network-online.target" ];
      wants = [ "network-online.target" ];
      unitConfig.ConditionPathExists = "!/etc/shrooms/config.toml";
      serviceConfig.Type = "oneshot";
      script = ''
        ${podman} run --rm \
          -v /etc/shrooms:/etc/shrooms -v /var/lib/shrooms:/var/lib/shrooms \
          ${cfg.image} prepare --name ${lib.escapeShellArg cfg.name} \
          --config /etc/shrooms/config.toml --state /var/lib/shrooms
        ${setMode}
        chmod 600 /etc/shrooms/config.toml
      '';
    };

    systemd.services.shrooms = {
      description = "shrooms overlay mesh";
      wantedBy = [ "multi-user.target" ];
      after = [ "network-online.target" "shrooms-prepare.service" ];
      wants = [ "network-online.target" ];
      path = [ pkgs.podman ];
      serviceConfig = {
        RuntimeDirectory = "shrooms";
        RuntimeDirectoryMode = "0750";
        ExecStart = lib.concatStringsSep " " [
          "${podman} run --rm --name shrooms --network host"
          "--cap-add NET_ADMIN --device /dev/net/tun"
          "-v /etc/shrooms:/etc/shrooms -v /var/lib/shrooms:/var/lib/shrooms -v /run/shrooms:/run/shrooms"
          "${cfg.image} daemon --socket /run/shrooms/shrooms.sock"
        ];
        # The agent user reads the control socket (the installer does this with
        # tmpfiles ACLs; here per start, since RuntimeDirectory is recreated).
        ExecStartPre = [
          "-${podman} rm -f shrooms"
          "${setMode}"
        ]
          ++ lib.optional cfg.agent.enable
            "${pkgs.acl}/bin/setfacl -m user:${cfg.agent.user}:rx,default:user:${cfg.agent.user}:rw,default:mask::rw /run/shrooms";
        ExecReload = "${podman} kill --signal HUP shrooms";
        ExecStop = "${podman} stop shrooms";
        Restart = "always";
        RestartSec = 5;
      };
    };

    systemd.services.shrooms-resolved = {
      description = "shrooms mesh names, in systemd-resolved";
      after = [ "shrooms.service" ];
      partOf = [ "shrooms.service" ];
      wantedBy = [ "shrooms.service" ];
      path = [ pkgs.systemd pkgs.coreutils pkgs.gnugrep pkgs.gnused pkgs.gawk pkgs.iproute2 pkgs.podman ];
      serviceConfig = {
        ExecStart = "${registerDns} --watch";
        ExecStop = "${registerDns} --revert";
        Restart = "always";
        RestartSec = 10;
      };
    };

    users.users = lib.mkIf cfg.agent.enable {
      ${cfg.agent.user}.linger = true;
    };

    systemd.user.services.shrooms-agent = lib.mkIf cfg.agent.enable {
      description = "shrooms-agent: coding-agent sessions over the mesh";
      wantedBy = [ "default.target" ];
      after = [ "network-online.target" ];
      unitConfig.ConditionUser = cfg.agent.user;
      path = cfg.agent.path ++ [ pkgs.bash pkgs.coreutils pkgs.git pkgs.nix ];
      serviceConfig = {
        ExecStart = lib.concatStringsSep " " ([ "${cfg.agent.package}/bin/shrooms-agent" ] ++ cfg.agent.extraArgs);
        # The agent decides at start whether there is a mesh to serve on: a user
        # unit cannot order itself after the system's shrooms.service, so wait
        # for the daemon to answer (it also re-executes itself on a join).
        ExecStartPre = "${pkgs.bash}/bin/bash -c 'for i in $(seq 90); do ${pkgs.curl}/bin/curl -sf --unix-socket /run/shrooms/shrooms.sock http://unix/status >/dev/null && exit 0; sleep 1; done; exit 0'";
        Restart = "on-failure";
        RestartSec = 10;
      };
    };
  };
}
