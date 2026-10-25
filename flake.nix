{
  description = "BasecampOS: a NixOS-based distribution with Logos Basecamp as the desktop";

  nixConfig = {
    extra-substituters = [ "https://cache.nix.logos.co/public" ];
    extra-trusted-public-keys = [ "public:l4HrXgL4nw246+LBh2SOJyhz64BoGegOYLheT/iIAPU=" ];
  };

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";

    # Alisher's Basecamp fork with themes + the desktop layout (dock, windows,
    # Spotlight). Tracked, not vendored: `nix flake update basecamp alisher-ds`.
    basecamp = {
      url = "github:xAlisher/basecamp-theming/basecamp-os-theme?dir=basecamp";
      inputs.logos-design-system.follows = "alisher-ds";
    };
    alisher-ds.url = "github:xAlisher/basecamp-theming/basecamp-os-theme?dir=design-system";

    shrooms = {
      url = "github:vpavlin/shrooms";
      flake = false;
    };

    # The Duet tablet's Sway session and fixes, maintained by the Duet agent.
    duet-kit = {
      url = "github:vpavlin/duet-kit";
      flake = false;
    };

    # The Duet's kernel: hexdump0815's mt81xx config + patches (see pkgs/duet-kernel.nix).
    hexkernel = {
      url = "github:hexdump0815/linux-mainline-mediatek-mt81xx-kernel/ce826155326213122772ffc035221356077dd8cd";
      flake = false;
    };

    # Builds BasecampOS's own Basecamp apps (apps/*).
    logos-module-builder.url = "github:logos-co/logos-module-builder";
  };

  outputs = { self, nixpkgs, basecamp, shrooms, logos-module-builder, duet-kit, hexkernel, ... }:
    let
      lib = nixpkgs.lib;
      systems = [ "x86_64-linux" "aarch64-linux" ];

      # Everything that depends on the architecture: packages, BasecampOS's own
      # Basecamp apps, and the NixOS modules every host shares.
      forSystem = system: rec {
        pkgs = nixpkgs.legacyPackages.${system};
        pkgsUnfree = import nixpkgs { inherit system; config.allowUnfree = true; };

        shrooms-agent = pkgs.callPackage ./pkgs/shrooms-agent.nix { src = shrooms; };

        # BasecampOS's own apps. apps/*/flake.nix build each one standalone; here
        # the builder is called directly so the repo needs no nested lock files.
        system-core = logos-module-builder.lib.mkLogosModule {
          src = ./apps/system/core;
          configFile = ./apps/system/core/metadata.json;
          flakeInputs = { inherit logos-module-builder; };
        };
        system-ui = logos-module-builder.lib.mkLogosQmlModule {
          src = ./apps/system/ui;
          configFile = ./apps/system/ui/metadata.json;
          # Named as in metadata.json's dependencies: the builder reads its LIDL.
          flakeInputs = { inherit logos-module-builder; basecampos_system = system-core; };
        };
        # Curated apps from the official catalogue, pinned by scripts/pin-catalog.py
        # (dependency order, url + sha256): fetched at build time, installed by lgpm.
        # One .lgx carries every platform's variant; lgpm picks this machine's.
        catalogApps = map (p: pkgs.runCommand "lgx-${p.name}-${p.version}" { } ''
          mkdir $out
          ln -s ${pkgs.fetchurl { inherit (p) url sha256; }} $out/${p.name}-${p.version}.lgx
        '') (builtins.fromJSON (builtins.readFile ./catalog/apps.json)).packages;

        # Native apps Basecamp launches (alone on their own workspace, under Sway).
        # Each gets a launcher app in Basecamp's dock and a line in
        # /etc/basecampos/external-apps for the basecampos_system module.
        externalApps = [
          rec {
            id = "browser"; appId = "brave-browser"; name = "Browser";
            command = [ "${pkgsUnfree.brave}/bin/brave" "--ozone-platform=wayland" ];
            # Inside Basecamp: software rendering (Qt's compositor offers clients no
            # EGL) and a profile of its own, so it never hands off to a running Brave.
            embedCommand = command ++ [ "--disable-gpu" "--user-data-dir=~/.config/BraveSoftware/Brave-Basecamp" ];
            icon = "${pkgsUnfree.brave}/share/icons/hicolor/256x256/apps/brave-browser.png";
          }
          {
            id = "terminal"; appId = "foot"; name = "Terminal";
            command = [ "${pkgs.foot}/bin/foot" ];
            # Inside Basecamp: qterminal. foot needs newer wl_seat/data-device versions
            # than Qt's compositor has, and winit apps (alacritty) bind the seat twice
            # while Qt sends keys to one keyboard per client: they never get any.
            embedCommand = [ "${pkgs.lxqt.qterminal}/bin/qterminal" ];
            icon = pkgs.runCommand "foot-icon.png" { } ''
              ${pkgs.librsvg}/bin/rsvg-convert -w 256 -h 256 ${pkgs.foot}/share/icons/hicolor/scalable/apps/foot.svg -o $out
            '';
          }
        ];
        launcherMeta = app: {
          name = "basecampos_${app.launcherId or app.id}";
          display_name = app.name;
          version = "0.1.0";
          type = "ui_qml";
          category = "system";
          description = "Opens ${app.name}, a native app that runs full screen beside Basecamp.";
          view = "Main.qml";
          icon = "icon.png";
          dependencies = [ "basecampos_system" ];
          nix = { packages = { build = [ ]; runtime = [ ]; }; external_libraries = [ ]; cmake = { find_packages = [ ]; }; };
        };
        # embedded = false: the app runs beside Basecamp on its own workspace;
        # embedded = true: inside the launcher's Basecamp window (the view is a
        # Wayland compositor; apps/external-embedded).
        mkExternalLauncher' = embedded: app0: let
          app = if embedded then app0 // { name = "${app0.name} (in Basecamp)"; launcherId = "${app0.id}_embedded"; }
                else app0 // { launcherId = app0.id; };
        in (logos-module-builder.lib.mkLogosQmlModule {
          src = pkgs.runCommand "launcher-${app.launcherId}-src" { } ''
            mkdir $out
            sed -e 's|@ID@|${app0.id}|g' -e 's|@NAME@|${app.name}|g' \
              ${if embedded then ./apps/external-embedded/Main.qml else ./apps/external/Main.qml} > $out/Main.qml
            cp ${app.icon} $out/icon.png
            cp ${builtins.toFile "metadata.json" (builtins.toJSON (launcherMeta app))} $out/metadata.json
          '';
          configFile = builtins.toFile "metadata.json" (builtins.toJSON (launcherMeta app));
          flakeInputs = { inherit logos-module-builder; basecampos_system = system-core; };
        }).packages.${system}.lgx-portable;
        mkExternalLauncher = mkExternalLauncher' false;

        systemApps = [
          system-core.packages.${system}.lgx-portable
          system-ui.packages.${system}.lgx-portable
        ];

        # The package manager CLI Basecamp itself pins, portable build: only that one
        # installs the catalogue's portable variants (the nix one wants "-dev").
        lgpm = basecamp.inputs.logos-package-manager.packages.${system}.cli-portable;

        common = [
          ./modules/base.nix
          ./modules/basecamp-shell.nix
          ./modules/shrooms
          ./modules/duet
          ({ ... }: {
            basecampos.duet.kit = duet-kit;
            nixpkgs.config.allowUnfreePredicate = pkg:
              builtins.elem (lib.getName pkg) [ "claude-code" "brave" "symbola" ];
            # Portable bundle: the catalogue's apps install into it (the
            # nix-store `default` build can list them but install nothing).
            # The dev VM uses the inspector build (UI automation on :3768).
            basecampos.shell.package = lib.mkDefault basecamp.packages.${system}.bin-bundle-dir;
            services.shrooms.agent.package = shrooms-agent;
            basecampos.shell.lgpm = lgpm;
            basecampos.shell.apps = systemApps ++ map mkExternalLauncher externalApps
              ++ map (mkExternalLauncher' true) (lib.filter (a: a ? embedCommand) externalApps) ++ catalogApps;
            basecampos.shell.externalApps = map (a: { inherit (a) id appId name command; embedCommand = a.embedCommand or null; }) externalApps;
            basecampos.shell.repositories = [ "https://apps.vpavlin.xyz/logos-repo.json" ];
          })
        ];

        # What both architectures publish.
        packages = {
          inherit shrooms-agent lgpm;
          basecamp = basecamp.packages.${system}.bin-bundle-dir;
          system-core = system-core.packages.${system}.lgx-portable;
          system-ui = system-ui.packages.${system}.lgx-portable;
          launcher-browser = mkExternalLauncher (builtins.head externalApps);
          launcher-terminal = mkExternalLauncher (builtins.elemAt externalApps 1);
        };
      };

      x86 = forSystem "x86_64-linux";
      arm = forSystem "aarch64-linux";
      mkHost = system: modules: nixpkgs.lib.nixosSystem {
        inherit system;
        # logosQtMcp: the dev VM's agent (QML inspector MCP); hexkernel: the Duet's kernel.
        specialArgs = { logosQtMcp = basecamp.packages.${system}.logos-qt-mcp; inherit hexkernel; };
        modules = (forSystem system).common ++ modules;
      };
    in
    {
      nixosModules = {
        basecamp-shell = ./modules/basecamp-shell.nix;
        shrooms = ./modules/shrooms;
        duet = ./modules/duet;
      };

      nixosConfigurations = {
        basecampos-vm = mkHost "x86_64-linux" [
          ./hosts/vm.nix
          { basecampos.shell.package = basecamp.packages.x86_64-linux.bin-bundle-dir-inspector; }
        ];
        basecampos-desktop = mkHost "x86_64-linux" [ ./hosts/desktop.nix ];
        basecampos-duet-vm = mkHost "x86_64-linux" [ ./hosts/duet-vm.nix ];
        basecampos-live = mkHost "x86_64-linux" [ ./hosts/live.nix ];
        # Generic arm64 desktop (UEFI): the whole aarch64 closure, built on CI.
        basecampos-arm = mkHost "aarch64-linux" [ ./hosts/desktop.nix { basecampos.shell.session = "sway"; } ];
        basecampos-duet = mkHost "aarch64-linux" [ ./hosts/duet.nix ];
        # The unattended first test on the tablet (watchdog, 20-min window, reports).
        basecampos-duet-test = mkHost "aarch64-linux" [ ./hosts/duet.nix { basecampos.duet.testMode = true; } ];
        basecampos-duet-usb = mkHost "aarch64-linux" [ ./hosts/duet.nix { basecampos.duet.boot = "usb"; } ];
      };

      packages.x86_64-linux = x86.packages // {
        basecamp = basecamp.packages.x86_64-linux.bin-bundle-dir-inspector;
        basecamp-nixstore = basecamp.packages.x86_64-linux.default;
        # QEMU's virgl on a non-NixOS host (the dev host) needs Mesa from nixpkgs.
        host-mesa = x86.pkgs.mesa;
        vm = self.nixosConfigurations.basecampos-vm.config.system.build.vm;
        duet-vm = self.nixosConfigurations.basecampos-duet-vm.config.system.build.vm;
        iso = self.nixosConfigurations.basecampos-live.config.system.build.isoImage;
        disk-image = self.nixosConfigurations.basecampos-desktop.config.system.build.images.raw-efi;
      };

      packages.aarch64-linux = arm.packages // {
        toplevel = self.nixosConfigurations.basecampos-arm.config.system.build.toplevel;
        duet-toplevel = self.nixosConfigurations.basecampos-duet.config.system.build.toplevel;
        duet-kernel = self.nixosConfigurations.basecampos-duet.config.boot.kernelPackages.kernel;
        duet-rootfs = self.nixosConfigurations.basecampos-duet.config.system.build.duetRootfs;
        duet-test-rootfs = self.nixosConfigurations.basecampos-duet-test.config.system.build.duetRootfs;
        duet-test-kernel = self.nixosConfigurations.basecampos-duet-test.config.boot.kernelPackages.kernel;
        duet-test-toplevel = self.nixosConfigurations.basecampos-duet-test.config.system.build.toplevel;
        duet-kpart = self.nixosConfigurations.basecampos-duet-usb.config.system.build.depthchargeKpart;
        duet-image = self.nixosConfigurations.basecampos-duet-usb.config.system.build.depthchargeImage;
        disk-image = self.nixosConfigurations.basecampos-arm.config.system.build.images.raw-efi;
      };

      apps.x86_64-linux.vm = {
        type = "app";
        program = "${self.packages.x86_64-linux.vm}/bin/run-basecampos-vm-vm";
      };
    };
}
