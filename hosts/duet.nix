# basecampos-duet: the Lenovo IdeaPad Duet (krane, MT8183), aarch64.
#
# Built on CI. Two ways to boot it (basecampos.duet.boot):
#   "subvol" (the agreed test): the tablet's own signed kernel (Duet agent)
#            boots straight into this system - no initrd - from a btrfs
#            subvolume on the tablet's /home partition. CI builds a rootfs
#            tarball (packages.aarch64-linux.duet-rootfs) to unpack there.
#   "usb":   a self-contained depthcharge USB image with a kernel partition
#            signed here (modules/duet/depthcharge.nix).
# The kernel is hexdump0815's stb-cbm 6.12.50 rebuilt here (pkgs/duet-kernel.nix);
# the Duet agent signs its Image + dtb into the spare kernel slot. No cameras yet.
{ config, lib, pkgs, hexkernel, ... }:

let
  bootMode = config.basecampos.duet.boot;
in
{
  imports = [ ../modules/duet/depthcharge.nix ];

  options.basecampos.duet.boot = lib.mkOption {
    type = lib.types.enum [ "subvol" "usb" ];
    default = "subvol";
  };
  options.basecampos.duet.testMode = lib.mkEnableOption ''
    the unattended first test on the tablet: a watchdog, a 20-minute test window
    that reboots back to Debian (KERN-A is tries=1), and reports + screenshots in
    /var/log/basecampos-test (readable from Debian inside the subvolume)'';

  config = lib.mkMerge [
    {
      networking.hostName = "basecampos";
      basecampos.shell.enable = true;
      basecampos.duet = {
        enable = true;
        hardware = true;
      };

      users.users.basecamp = {
        extraGroups = [ "wheel" ];
        initialPassword = "basecamp";
      };

      boot.kernelPackages = pkgs.linuxPackagesFor (pkgs.callPackage ../pkgs/duet-kernel.nix { inherit hexkernel; });
      hardware.enableRedistributableFirmware = true;
      hardware.deviceTree.name = "mediatek/mt8183-kukui-krane-sku176.dtb";
      swapDevices = [ ];
      # 3.8 GB of RAM and Basecamp alone takes ~1.3 GB.
      zramSwap = {
        enable = true;
        memoryPercent = 75;
      };
    }

    (lib.mkIf (bootMode == "subvol") {
      # The kernel mounts root itself (eMMC, btrfs built in) and runs NixOS's
      # stage 2 directly: cmdline
      #   root=LABEL=home rootflags=subvol=@basecampos rootwait init=/nix/var/nix/profiles/system/init
      # NixOS sees only its subvolume, never the Debian /home tree beside it.
      boot.initrd.enable = false;
      boot.loader.grub.enable = false;
      boot.loader.generic-extlinux-compatible.enable = false;
      fileSystems."/" = {
        device = "/dev/disk/by-label/home";
        fsType = "btrfs";
        options = [ "subvol=@basecampos" "noatime" ];
      };
      # Root is a subvolume of the tablet's /home partition (p1, Debian's /home):
      # nothing may grow, resize or mount anything else on that disk.
      assertions = [
        { assertion = !config.boot.growPartition; message = "duet subvol: boot.growPartition would repartition the tablet's p1"; }
        { assertion = lib.all (fs: !(fs.autoResize or false)) (lib.attrValues config.fileSystems);
          message = "duet subvol: no fileSystems autoResize"; }
        { assertion = lib.attrNames config.fileSystems == [ "/" ]; message = "duet subvol: mount only the @basecampos subvolume"; }
        { assertion = config.swapDevices == [ ]; message = "duet subvol: no swap devices (zram only)"; }
        { assertion = !config.services.shrooms.enable; message = "duet subvol: shrooms off (the tablet's mesh credential must not be duplicated)"; }
      ];
      # First boot from the unpacked tarball: register its store paths.
      boot.postBootCommands = ''
        if [ -f /nix-path-registration ]; then
          ${config.nix.package.out}/bin/nix-store --load-db < /nix-path-registration
          touch /etc/NIXOS
          rm -f /nix-path-registration
        fi
      '';

      system.build.duetRootfs = let
        closure = pkgs.closureInfo { rootPaths = [ config.system.build.toplevel ]; };
        toplevel = config.system.build.toplevel;
      in pkgs.runCommand "basecampos-duet-rootfs" { nativeBuildInputs = [ pkgs.zstd pkgs.gnutar ]; } ''
        mkdir -p root/nix/var/nix/profiles root/etc
        ln -s ${toplevel} root/nix/var/nix/profiles/system-1-link
        ln -s system-1-link root/nix/var/nix/profiles/system
        cp ${closure}/registration root/nix-path-registration
        touch root/etc/NIXOS
        mkdir $out
        # One stream: root/ first, then the store paths (as nix/store/...),
        # straight into zstd - an uncompressed tar of the closure would not fit
        # on a CI runner's disk.
        sed 's|^/||' ${closure}/store-paths > paths
        tar --sort=name --owner=0 --group=0 --numeric-owner -cf - -C root . -C / -T paths |
          zstd -T0 -12 -o $out/basecampos-duet-rootfs.tar.zst
        cat > $out/README.txt <<EOF
        BasecampOS for the Duet: unpack into an empty btrfs subvolume, e.g.
          btrfs subvolume create /home/@basecampos
          zstd -dc basecampos-duet-rootfs.tar.zst | tar -C /home/@basecampos -xpf -
        then boot your kernel with:
          root=LABEL=home rootflags=subvol=@basecampos rootwait init=/nix/var/nix/profiles/system/init
        System: ${toplevel}
        EOF
        sed -i 's/^        //' $out/README.txt
        du -sh $out/*
      '';
    })

    (lib.mkIf config.basecampos.duet.testMode (let
      user = config.basecampos.shell.user;
      logDir = "/var/log/basecampos-test";
      # Writes the state of the system into logDir; runs as root (journal, dmesg).
      systemReport = pkgs.writeShellScript "basecampos-test-system-report" ''
        tag=$1; d=${logDir}/$tag; mkdir -p $d
        check() { if eval "$2" >/dev/null 2>&1; then echo "PASS $1" ; else echo "FAIL $1"; fi >> ${logDir}/checks.log; }
        echo "== $tag $(date -Is)" >> ${logDir}/checks.log
        uname -a > $d/uname.txt; dmesg > $d/dmesg.txt; journalctl -b --no-pager > $d/journal.txt
        systemctl --failed --no-pager > $d/failed.txt; lsmod > $d/lsmod.txt; free -h > $d/free.txt
        top -bn1 | head -40 > $d/top.txt; ls -la /dev/input > $d/dev-input.txt
        libinput list-devices > $d/libinput.txt 2>&1
        nmcli dev > $d/nmcli.txt 2>&1; ip a > $d/ip.txt; brightnessctl -m > $d/brightness.txt 2>&1
        for b in /sys/class/power_supply/*; do echo "$b: $(cat $b/capacity 2>/dev/null) $(cat $b/status 2>/dev/null)"; done > $d/battery.txt
        ps -eo pid,pcpu,rss,etime,args | grep -iE '[L]ogosBasecamp|[l]ogos_host|[u]i-host' > $d/basecamp-ps.txt
        check "kernel $(uname -r) has its modules" "test -d /run/booted-system/kernel-modules/lib/modules/$(uname -r)"
        check "no failed units" "! systemctl --failed --no-legend | grep -q ."
        check "touch device (hid-multitouch/goodix)" "grep -qiE 'goodix|touch' $d/libinput.txt"
        check "wifi device (ath10k)" "nmcli -t -f TYPE dev | grep -q wifi"
        check "Basecamp running" "pgrep -f bin/LogosBasecamp"
        chmod -R a+rX ${logDir}
      '';
      # Runs in the Sway session as the user: outputs, inputs, tree, screenshot.
      sessionReport = pkgs.writeShellScript "basecampos-test-session-report" ''
        for t in 60 180; do
          sleep $(( t == 60 ? 60 : 120 ))
          d=${logDir}/t$t; mkdir -p $d
          swaymsg -t get_outputs > $d/outputs.json; swaymsg -t get_inputs > $d/inputs.json
          swaymsg -t get_tree > $d/tree.json
          (wpctl status || pactl info) > $d/audio.txt 2>&1
          grim ${logDir}/basecampos-t$t.png && echo "PASS screenshot t$t" >> ${logDir}/checks.log \
            || echo "FAIL screenshot t$t" >> ${logDir}/checks.log
          sudo -n ${systemReport} t$t || true
        done
      '';
    in {
      # Self-recovery while nobody is at the tablet: a hung kernel/system resets,
      # and after 20 minutes the test ends with a reboot; KERN-A has tries=1, so
      # the tablet comes back up in Debian (KERN-B).
      systemd.settings.Manager = {
        RuntimeWatchdogSec = "30s";
        RebootWatchdogSec = "2min";
      };
      systemd.tmpfiles.rules = [ "d ${logDir} 0775 ${user} users -" "d /var/lib/basecampos-test 0755 root root -" ];
      systemd.services.basecampos-test-window = {
        description = "BasecampOS test window: reboot back to Debian after 20 minutes";
        wantedBy = [ "multi-user.target" ];
        unitConfig.ConditionPathExists = "!/var/lib/basecampos-test/keep";
        script = "sleep 1200; [ -e /var/lib/basecampos-test/keep ] || systemctl reboot";
      };
      # A report even if the graphical session never comes up.
      systemd.services.basecampos-test-boot-report = {
        description = "BasecampOS test: early system report";
        wantedBy = [ "multi-user.target" ];
        path = with pkgs; [ coreutils util-linux systemd kmod procps iproute2 networkmanager brightnessctl libinput gnugrep ];
        script = "sleep 45; ${systemReport} boot";
        serviceConfig.Type = "simple";
      };
      security.sudo.extraRules = [ {
        users = [ user ];
        commands = [ { command = "${systemReport}"; options = [ "NOPASSWD" ]; } ];
      } ];
      environment.systemPackages = with pkgs; [ grim libinput ];
      basecampos.shell.swayExtraConfig = lib.mkAfter "exec ${sessionReport}\n";
    }))

    (lib.mkIf (bootMode == "usb") {
      basecampos.depthcharge.enable = true;
      boot.initrd.availableKernelModules = [ "mmc_block" "sdhci" "mtk_sd" "usb_storage" "uas" ];
      boot.kernelParams = [ "console=tty1" ];
    })
  ];
}
