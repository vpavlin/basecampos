# The Duet's own agent session inside BasecampOS: the tablet's Debian userland
# (its shrooms daemon + shrooms-agent + Claude) started in a chroot at boot.
# Nothing is copied into the image: Debian's root and its /home are mounted in
# place on the device, so the mesh credential and the Claude login stay where
# they are (and only one of the two OSes runs at a time). From the Duet
# agent's apply-agent-chroot.py (duet-kit docs/basecampos-tools).
{ config, lib, pkgs, ... }:

let
  cfg = config.basecampos.duet.agentChroot;
  start = pkgs.writeShellScript "duet-agent-chroot" ''
    log=/var/lib/duet-agent
    mkdir -p $log
    exec >>$log/start.log 2>&1
    echo "=== $(date) start"
    D=/mnt/debian
    mkdir -p $D
    modprobe tun; modprobe wireguard 2>/dev/null
    mountpoint -q $D || mount -t btrfs -o rw,noatime ${cfg.debianRoot} $D || { echo "cannot mount Debian root"; exit 1; }
    mountpoint -q $D/home || mount -t btrfs -o rw,noatime,subvolid=5 ${cfg.debianHome} $D/home || { echo "cannot mount /home top level"; exit 1; }
    for m in proc sys dev; do mountpoint -q $D/$m || mount --rbind /$m $D/$m; done
    # rslave: later host mounts (logind's /run/user/UID) show up in the chroot,
    # and the nscd-hiding mount below does not leak back.
    mountpoint -q $D/run || { mount --bind /run $D/run; mount --make-rslave $D/run; }
    # NixOS's nscd socket would make NSS in the chroot resolve the uid as our
    # user, and Debian's sudo breaks.
    mountpoint -q $D/run/nscd || { mkdir -p /run/nscd-hidden; mount --bind /run/nscd-hidden $D/run/nscd; }
    mountpoint -q $D/tmp || mount --bind /tmp $D/tmp
    mkdir -p /run/systemd/resolve
    { grep -h '^nameserver' /etc/resolv.conf 2>/dev/null; echo "nameserver 9.9.9.9"; echo "nameserver 1.1.1.1"; } > /run/systemd/resolve/stub-resolv.conf

    ${lib.optionalString cfg.importWifi ''
      # Debian's Wi-Fi profiles, so the tablet gets online (copied on the device, never into the image).
      for f in $D/etc/NetworkManager/system-connections/*.nmconnection; do
        [ -e "$f" ] || continue
        t=/etc/NetworkManager/system-connections/$(basename "$f")
        [ -e "$t" ] || { install -D -m 0600 "$f" "$t"; nmcli connection load "$t" || true; }
      done
    ''}
    for i in $(seq 1 24); do nmcli -t -f STATE general 2>/dev/null | grep -q '^connected' && break; sleep 5; done
    echo "network: $(nmcli -t -f STATE general 2>&1)"

    # Basecamp's Shrooms app runs as the session user (group users): the daemon
    # resets its dir to 0750 root:<socket_group>, so open it up after it starts.
    openSocket() { chmod 0755 /run/shrooms 2>/dev/null; chmod 0666 /run/shrooms/shrooms.sock 2>/dev/null; }
    mkdir -p /run/shrooms
    DEBPATH=/usr/local/sbin:/usr/sbin:/usr/local/bin:/usr/bin:/sbin:/bin
    shrooms() { chroot $D /usr/bin/env PATH=$DEBPATH LD_LIBRARY_PATH=/opt/shrooms/lib /opt/shrooms/bin/shrooms "$@"; }
    shrooms daemon >>$log/shrooms.log 2>&1 &
    DPID=$!
    for i in $(seq 1 30); do [ -S /run/shrooms/shrooms.sock ] && break; sleep 2; done
    openSocket

    U=${toString cfg.uid}
    for i in $(seq 1 60); do mountpoint -q /run/user/$U && break; sleep 3; done
    mkdir -p /run/user/$U; chown $U /run/user/$U
    chroot --userspec=$U:$U $D /usr/bin/env HOME=/home/${cfg.debianUser} USER=${cfg.debianUser} LOGNAME=${cfg.debianUser} \
      XDG_RUNTIME_DIR=/run/user/$U PATH=/home/${cfg.debianUser}/.local/bin:$DEBPATH \
      ${lib.escapeShellArgs cfg.agentCommand} >>$log/agent.log 2>&1 &
    APID=$!

    # Tell the session it is running inside BasecampOS.
    ( sock=/run/user/$U/shrooms-agent/agent.sock
      for i in $(seq 1 60); do [ -S $sock ] && break; sleep 3; done; sleep 40
      msg="[sent by the BasecampOS start script] ${cfg.upMessage} (kernel $(uname -r), started $(date +%T); logs in $log)"
      body=$(jq -n --arg t "$msg" --arg id "basecampos-agent-up-$(date +%s)" '{text: $t, id: $id}')
      curl -s -m 20 --unix-socket $sock -X POST -H 'Content-Type: application/json' -d "$body" \
        http://a/v1/sessions/${cfg.session}/messages >> $log/notify.log 2>&1 ) &

    # Health: every 30 s; no peer online for 3 min while the Internet answers
    # (roaming/suspend churn) restarts everything through systemd.
    ( BAD=0; while sleep 30; do
        P=$(shrooms status 2>/dev/null | sed -n 's/.*peers [0-9]* (\([0-9]*\) online.*/\1/p')
        openSocket
        N=$(nmcli -t -f STATE general 2>/dev/null | head -1)
        if ping -c1 -W3 1.1.1.1 >/dev/null 2>&1; then I=up; else I=down; fi
        echo "$(date +%T) peers_online=''${P:-?} nm=$N internet=$I" >> $log/health.log
        if [ "''${P:-0}" = 0 ] && [ $I = up ]; then BAD=$((BAD+1)); else BAD=0; fi
        if [ $BAD -ge 6 ]; then echo "$(date +%T) no peers for 3 min with Internet up: restarting" >> $log/health.log; kill $DPID; break; fi
      done ) &
    wait -n $DPID $APID
    echo "=== $(date) a child exited, restarting via systemd"
    exit 1
  '';
in
{
  options.basecampos.duet.agentChroot = {
    enable = lib.mkEnableOption "the tablet's Debian shrooms daemon + agent, run in a chroot at boot";
    debianRoot = lib.mkOption { type = lib.types.str; default = "/dev/mmcblk0p5"; description = "Debian's root partition."; };
    debianHome = lib.mkOption { type = lib.types.str; default = "/dev/mmcblk0p1"; description = "Debian's /home (mounted at its top level)."; };
    debianUser = lib.mkOption { type = lib.types.str; default = "linux"; };
    uid = lib.mkOption { type = lib.types.int; default = 1000; description = "uid of the Debian user (same as the Basecamp user's)."; };
    session = lib.mkOption { type = lib.types.str; default = "duet"; description = "The agent session told it is up."; };
    agentCommand = lib.mkOption {
      type = lib.types.listOf lib.types.str;
      default = [ "/usr/local/bin/shrooms-agent" "-claude" "/home/linux/.local/bin/claude-sonnet" ];
      description = "The agent, as run inside the chroot.";
    };
    importWifi = lib.mkOption { type = lib.types.bool; default = true; description = "Copy Debian's Wi-Fi profiles on the device at boot."; };
    upMessage = lib.mkOption {
      type = lib.types.str;
      default = "The shrooms daemon and agent are running INSIDE BasecampOS (NixOS, chroot into the Debian userland).";
    };
  };

  config = lib.mkIf cfg.enable {
    assertions = [ {
      assertion = !config.services.shrooms.enable;
      message = "duet agentChroot runs the tablet's own shrooms daemon: services.shrooms must stay off";
    } ];
    systemd.services.duet-agent-chroot = {
      description = "The Duet's Debian shrooms daemon + agent (chroot)";
      after = [ "network-online.target" "NetworkManager.service" ];
      wants = [ "network-online.target" ];
      wantedBy = [ "multi-user.target" ];
      # A live switch (duet-apply-generation) must not cut off the session doing it.
      restartIfChanged = false;
      stopIfChanged = false;
      path = with pkgs; [ bash coreutils util-linux kmod networkmanager iproute2 iputils curl jq gnugrep gnused ];
      serviceConfig = {
        ExecStart = start;
        Restart = "on-failure";
        RestartSec = 15;
      };
    };
  };
}
