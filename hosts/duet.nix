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

  config = lib.mkMerge [
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
        options = [ "subvol=@basecampos" "compress=zstd" "noatime" ];
      };
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

    (lib.mkIf (bootMode == "usb") {
      basecampos.depthcharge.enable = true;
      boot.initrd.availableKernelModules = [ "mmc_block" "sdhci" "mtk_sd" "usb_storage" "uas" ];
      boot.kernelParams = [ "console=tty1" ];
    })
  ];
}
