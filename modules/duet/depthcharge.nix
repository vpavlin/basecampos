# Booting on a ChromeOS device in developer mode (the Duet): depthcharge loads
# a kernel partition (GPT type "ChromeOS kernel") holding a vbutil_kernel blob.
# On arm64 that blob wraps a FIT with the kernel, the device tree and the
# initrd, and the kernel command line is signed in with it. Developer mode with
# dev_boot_signed_only=0 accepts the public ChromeOS dev keys, which ship with
# vboot_reference, so the image is signed here.
#
#   system.build.depthchargeKpart  the signed kernel partition (kpart.bin)
#   system.build.depthchargeImage  a GPT disk image: KERN-A (that kpart, set to
#                                  boot once and stay) + an ext4 root holding the
#                                  system; dd it to a USB stick, Ctrl+U boots it
#
# NixOS manages no bootloader here: a new generation needs a new kpart (no
# rollback menu either). Kernel changes go to a spare slot with tries=1 first.
{ config, lib, pkgs, modulesPath, ... }:

let
  cfg = config.basecampos.depthcharge;
  toplevel = config.system.build.toplevel;
  kernel = config.boot.kernelPackages.kernel;
  dtb = "${config.hardware.deviceTree.package}/${config.hardware.deviceTree.name}";
  initrd = "${config.system.build.initialRamdisk}/${config.system.boot.loader.initrdFile}";
  cmdline = lib.concatStringsSep " " ([
    "init=${toplevel}/init"
    "root=LABEL=${cfg.rootLabel}"
    "rootwait"
  ] ++ config.boot.kernelParams);
  devkeys = "${pkgs.vboot_reference}/share/vboot/devkeys";

  kpart = pkgs.runCommand "depthcharge-kpart" {
    nativeBuildInputs = [ pkgs.ubootTools pkgs.lz4 pkgs.vboot_reference pkgs.dtc ];
  } ''
    lz4 -9 -c ${kernel}/${config.system.boot.loader.kernelFile} > Image.lz4
    cat > kernel.its <<EOF
    /dts-v1/;
    / {
      description = "BasecampOS";
      #address-cells = <1>;
      images {
        kernel-1 {
          description = "Linux ${kernel.version}";
          data = /incbin/("Image.lz4");
          type = "kernel";
          arch = "arm64";
          os = "linux";
          compression = "lz4";
          load = <0>;
          entry = <0>;
        };
        fdt-1 {
          description = "${config.hardware.deviceTree.name}";
          data = /incbin/("${dtb}");
          type = "flat_dt";
          arch = "arm64";
          compression = "none";
        };
        ramdisk-1 {
          description = "NixOS initrd";
          data = /incbin/("${initrd}");
          type = "ramdisk";
          arch = "arm64";
          os = "linux";
          compression = "none";
        };
      };
      configurations {
        default = "conf-1";
        conf-1 {
          kernel = "kernel-1";
          fdt = "fdt-1";
          ramdisk = "ramdisk-1";
        };
      };
    };
    EOF
    mkimage -f kernel.its kernel.itb
    echo -n '${cmdline}' > cmdline
    dd if=/dev/zero of=bootloader.bin bs=512 count=1
    mkdir $out
    vbutil_kernel --pack $out/kpart.bin --version 1 --arch aarch64 \
      --keyblock ${devkeys}/kernel.keyblock --signprivate ${devkeys}/kernel_data_key.vbprivk \
      --config cmdline --bootloader bootloader.bin --vmlinuz kernel.itb
    vbutil_kernel --verify $out/kpart.bin --verbose > $out/verify.txt
    cp cmdline $out/
    test "$(stat -c %s $out/kpart.bin)" -le ${toString (cfg.kernelPartitionMiB * 1024 * 1024)}
  '';

  rootfs = pkgs.callPackage "${modulesPath}/../lib/make-ext4-fs.nix" {
    storePaths = [ toplevel ];
    volumeLabel = cfg.rootLabel;
    compressImage = false;
    populateImageCommands = ''
      mkdir -p ./files/etc
      touch ./files/etc/NIXOS
    '';
  };

  image = pkgs.runCommand "basecampos-depthcharge-image" {
    nativeBuildInputs = [ pkgs.vboot_reference pkgs.zstd ];
  } ''
    kpartSectors=$(( ${toString cfg.kernelPartitionMiB} * 2048 ))
    rootBytes=$(stat -c %s ${rootfs})
    rootSectors=$(( (rootBytes + 511) / 512 ))
    kStart=8192
    rStart=$(( kStart + kpartSectors ))
    total=$(( rStart + rootSectors + 8192 ))
    truncate -s $(( total * 512 )) disk.img
    cgpt create disk.img
    cgpt add -i 1 -b $kStart -s $kpartSectors -t kernel -l KERN-A -S 1 -T 5 -P 10 disk.img
    cgpt add -i 2 -b $rStart -s $rootSectors -t 0FC63DAF-8483-4772-8E79-3D69D8477DE4 -l ${cfg.rootLabel} disk.img
    cgpt boot -p disk.img
    dd if=${kpart}/kpart.bin of=disk.img bs=512 seek=$kStart conv=notrunc
    dd if=${rootfs} of=disk.img bs=512 seek=$rStart conv=notrunc
    cgpt show disk.img
    mkdir $out
    zstd -T0 -10 disk.img -o $out/basecampos-duet.img.zst
    cp ${kpart}/verify.txt ${kpart}/cmdline $out/
  '';
in
{
  options.basecampos.depthcharge = {
    enable = lib.mkEnableOption "a depthcharge (ChromeOS developer mode) boot image";
    rootLabel = lib.mkOption { type = lib.types.str; default = "basecampos"; };
    kernelPartitionMiB = lib.mkOption { type = lib.types.int; default = 64; };
  };

  config = lib.mkIf cfg.enable {
    boot.loader.grub.enable = false;
    boot.loader.generic-extlinux-compatible.enable = false;
    fileSystems."/" = {
      device = "/dev/disk/by-label/${cfg.rootLabel}";
      fsType = "ext4";
      autoResize = true;
    };
    boot.growPartition = true;

    # First boot from the image: register the store paths the image carries.
    boot.postBootCommands = ''
      if [ -f /nix-path-registration ]; then
        ${config.nix.package.out}/bin/nix-store --load-db < /nix-path-registration
        touch /etc/NIXOS
        ${config.nix.package.out}/bin/nix-env -p /nix/var/nix/profiles/system --set /run/current-system
        rm -f /nix-path-registration
      fi
    '';

    system.build.depthchargeKpart = kpart;
    system.build.depthchargeImage = image;
  };
}
