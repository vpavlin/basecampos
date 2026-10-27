# BasecampOS as the Duet's default boot, Debian as the fallback (Václav, 2026-10-27).
#
# depthcharge boots the highest-priority kernel slot with tries left. Debian's
# KERN-B (partition 4) stays the default with priority 5; BasecampOS's KERN-A
# (partition 2) gets one try per arming. This re-arms KERN-A at every clean
# shutdown, but only while KERN-B is healthy (priority 5, successful), so a
# crash or power loss still lands in Debian. Logic and partition numbers from
# duet/duet's keep-default.sh (verified on the tablet in a real reboot); it
# owns the slots.
#
# "Restart into Debian" (the System app) starts basecampos-boot-debian.service:
# it sets KERN-A aside and reboots, and the shutdown hook leaves it alone. Back
# into BasecampOS from Debian: duet/duet's bcos-boot.sh re-arms KERN-A.
{ config, lib, pkgs, ... }:

let
  cfg = config.basecampos.duet.keepDefault;
  state = "/var/lib/duet-boot";
  script = pkgs.writeShellScript "duet-keep-default" ''
    PATH=${lib.makeBinPath [ pkgs.coreutils pkgs.vboot_reference pkgs.systemd ]}
    disk=${cfg.disk}
    flag=${state}/force-debian
    log() { echo "duet-keep-default: $*"; }
    case "$1" in
      start)
        rm -f "$flag" ;;
      stop)
        if [ -e "$flag" ]; then log "Restart into Debian asked: KERN-A left alone"; exit 0; fi
        prio=$(cgpt show -i 4 -P "$disk"); ok=$(cgpt show -i 4 -S "$disk")
        if [ "$prio" = 5 ] && [ "$ok" = 1 ]; then
          cgpt add -i 2 -P 6 -T 1 -S 0 "$disk" && log "armed KERN-A for the next boot"
        else
          log "KERN-B not healthy (priority $prio, successful $ok): not arming KERN-A"
        fi ;;
      force-debian)
        mkdir -p ${state}; touch "$flag"
        cgpt add -i 2 -P 0 -T 0 -S 0 "$disk" && log "KERN-A set aside: the next boot is Debian" ;;
      *) echo "usage: $0 start|stop|force-debian" >&2; exit 2 ;;
    esac
  '';
in
{
  options.basecampos.duet.keepDefault = {
    enable = lib.mkEnableOption "BasecampOS as the default boot (KERN-A re-armed at clean shutdown, Debian's KERN-B the fallback)";
    disk = lib.mkOption { type = lib.types.str; default = "/dev/mmcblk0"; };
  };

  config = lib.mkIf cfg.enable {
    systemd.services.duet-keep-default = {
      description = "Keep BasecampOS the default boot (Debian stays the fallback)";
      wantedBy = [ "multi-user.target" ];
      serviceConfig = {
        Type = "oneshot";
        RemainAfterExit = true;
        StateDirectory = "duet-boot";
        ExecStart = "${script} start";
        ExecStop = "${script} stop";
      };
    };

    systemd.services.basecampos-boot-debian = {
      description = "Restart into Debian (the System app's button)";
      serviceConfig = {
        Type = "oneshot";
        StateDirectory = "duet-boot";
        ExecStart = "${script} force-debian";
        ExecStartPost = "${pkgs.systemd}/bin/systemctl reboot";
      };
    };
    # The System app sees the button only where this exists.
    environment.etc."basecampos/boot-debian".text = "basecampos-boot-debian.service\n";

    # The session user may start exactly that unit (the System app runs as it).
    security.polkit.extraConfig = ''
      polkit.addRule(function(action, subject) {
        if (action.id == "org.freedesktop.systemd1.manage-units" &&
            action.lookup("unit") == "basecampos-boot-debian.service" &&
            action.lookup("verb") == "start" &&
            subject.user == "${config.basecampos.shell.user}") {
          return polkit.Result.YES;
        }
      });
    '';
  };
}
