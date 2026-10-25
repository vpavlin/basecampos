# Logos Basecamp as the desktop: autologin straight into a Wayland kiosk
# (cage) running Basecamp full screen in the "desktop" layout.
#
# P0 uses cage: one app, full screen. Basecamp draws its own windows, dock and
# status bar (Alisher's DesktopArea). P2 moves to a stacking compositor.
{ config, lib, pkgs, ... }:

let
  cfg = config.basecampos.shell;
in
{
  options.basecampos.shell = {
    enable = lib.mkEnableOption "Logos Basecamp as the desktop shell";
    package = lib.mkOption {
      type = lib.types.package;
      description = "Basecamp build providing bin/LogosBasecamp.";
    };
    user = lib.mkOption {
      type = lib.types.str;
      default = "basecamp";
    };
    session = lib.mkOption {
      type = lib.types.enum [ "cage" "sway" "external" ];
      default = "cage";
      description = ''
        "cage": Basecamp is the whole session (a kiosk compositor runs nothing else).
        "sway": Basecamp is the only UI, on a bar-less Sway that also runs the
        native apps in externalApps full screen beside it (browser, terminal).
        "external": another session runs Basecamp as one of its apps (the Duet's
        Sway); this module then only provides the runtime and the setup service.
      '';
    };
    layout = lib.mkOption {
      type = lib.types.enum [ "sidebar" "desktop" ];
      default = "desktop";
    };
    extraArgs = lib.mkOption {
      type = lib.types.listOf lib.types.str;
      default = [ ];
    };
    environment = lib.mkOption {
      type = lib.types.attrsOf lib.types.str;
      default = { };
    };
    qtPlatform = lib.mkOption {
      type = lib.types.str;
      # XWayland, not native Wayland: under Qt's wayland platform the desktop
      # layout's raster-painted root (wallpaper) intermittently composes as
      # black beneath the GPU-composited QQuickWidgets (seen in cage + virgl;
      # QT_QUICK_BACKEND=software shows the wallpaper but loses the Quick content).
      default = "xcb";
    };
    swayExtraConfig = lib.mkOption {
      type = lib.types.lines;
      default = "";
      description = "Appended to the session's Sway config (device input/output rules, on-screen keyboard...).";
    };
    apps = lib.mkOption {
      type = lib.types.listOf lib.types.package;
      default = [ ];
      description = ''
        Packages containing .lgx files, installed into the user's Basecamp
        before the shell starts (again whenever this list changes). They land in
        the user's data dir like anything from the catalogue, so the user can
        upgrade or remove them.
      '';
    };
    repositories = lib.mkOption {
      type = lib.types.listOf lib.types.str;
      default = [ ];
      example = [ "https://apps.vpavlin.xyz/logos-repo.json" ];
      description = ''
        Extra package repositories (logos-repo.json URLs) for Basecamp's Package
        Manager, next to the official one. Added once per list, so a repository
        the user removes stays removed until this list changes.
      '';
    };
    externalApps = lib.mkOption {
      type = lib.types.listOf (lib.types.submodule {
        options = {
          id = lib.mkOption { type = lib.types.str; };
          appId = lib.mkOption { type = lib.types.str; description = "Wayland app_id of its window."; };
          name = lib.mkOption { type = lib.types.str; };
          command = lib.mkOption { type = lib.types.listOf lib.types.str; };
        };
      });
      default = [ ];
      description = "Native apps Basecamp may start and switch to (basecampos_system reads them).";
    };
    lgpm = lib.mkOption {
      type = lib.types.nullOr lib.types.package;
      default = null;
      description = "The portable lgpm CLI (the nix build only installs -dev variants).";
    };
  };

  config = lib.mkIf cfg.enable (lib.mkMerge [ {
    users.users.${cfg.user} = {
      isNormalUser = true;
      description = "Basecamp";
      extraGroups = [ "video" "input" "audio" "networkmanager" ];
    };

    hardware.graphics.enable = true;

    # The portable bundle — and every app installed from the catalogue, which
    # ships portable variants only — expects an FHS host: /lib64/ld-linux and
    # these libraries (the ones nix-bundle-dir skips as "system").
    programs.nix-ld = {
      enable = true;
      libraries = with pkgs; [
        glibc stdenv.cc.cc.lib alsa-lib libdrm libgbm libglvnd mesa fontconfig freetype
        fribidi harfbuzz vulkan-loader wayland libx11 libxcb zlib openssl
      ];
    };

    services.cage = lib.mkIf (cfg.session == "cage") {
      enable = true;
      user = cfg.user;
      # -d: no client-side decorations; -s: allow VT switching (escape hatch).
      extraArguments = [ "-d" "-s" ];
      program = lib.concatStringsSep " " ([ "${cfg.package}/bin/LogosBasecamp" ] ++ cfg.extraArgs);
      environment = {
        BASECAMP_LAYOUT = cfg.layout;
        XDG_SESSION_TYPE = "wayland";
        QT_QPA_PLATFORM = cfg.qtPlatform;
        # Logs land in the user's journal next to the session.
        QT_LOGGING_RULES = "qt.qpa.*=false";
      } // cfg.environment;
    };
    # Tab-separated, one app per line: id, app_id, name, argv... (root-owned:
    # the module only ever runs what is listed here).
    environment.etc."basecampos/external-apps".text = lib.concatMapStrings (a:
      lib.concatStringsSep "\t" ([ a.id a.appId a.name ] ++ a.command) + "\n") cfg.externalApps;

    assertions = [ {
      assertion = cfg.apps == [ ] || cfg.lgpm != null;
      message = "basecampos.shell.apps needs basecampos.shell.lgpm";
    } ];

    # Before the shell starts: bundled apps and extra repositories, each once per
    # configured set, so later changes the user makes in Basecamp stick.
    systemd.services.basecamp-setup = lib.mkIf (cfg.apps != [ ] || cfg.repositories != [ ]) {
      description = "Set up BasecampOS's bundled Basecamp apps and repositories";
      # Before whichever session starts Basecamp.
      wantedBy = [ "multi-user.target" ];
      before = [ "cage-tty1.service" "display-manager.service" ];
      path = [ pkgs.jq pkgs.coreutils ];
      serviceConfig = {
        Type = "oneshot";
        User = cfg.user;
        RemainAfterExit = true;
      };
      script = let
        list = lib.concatStringsSep " " cfg.apps;
        repos = builtins.toJSON cfg.repositories;
      in ''
        data=$HOME/.local/share/Logos/LogosBasecamp
        mkdir -p "$data/modules" "$data/plugins"

        marker=$data/.basecampos-apps
        if [ -n "${list}" ] && [ "$(cat "$marker" 2>/dev/null)" != "${list}" ]; then
          for pkg in ${list}; do
            for f in "$pkg"/*.lgx; do
              ${lib.optionalString (cfg.lgpm != null) "${cfg.lgpm}/bin/lgpm"} \
                --modules-dir "$data/modules" --ui-plugins-dir "$data/plugins" \
                --allow-unsigned install --file "$f"
            done
          done
          echo "${list}" > "$marker"
        fi

        # package_downloader keeps its repositories in its instance directory,
        # {module_data}/package_downloader/<id>/repositories.json, and reuses the
        # first such directory it finds (logos-module ReuseOrCreate), so on a
        # fresh profile we create it rather than wait for a random id.
        marker=$data/.basecampos-repos
        if [ '${repos}' != '[]' ] && [ "$(cat "$marker" 2>/dev/null)" != '${repos}' ]; then
          pd=$data/module_data/package_downloader
          inst=$(ls -1 "$pd" 2>/dev/null | head -1)
          [ -n "$inst" ] || inst=basecampos
          mkdir -p "$pd/$inst"
          cfgf=$pd/$inst/repositories.json
          [ -s "$cfgf" ] || echo '{"defaultDisabled":false,"repositories":[]}' > "$cfgf"
          jq --argjson add '${repos}' '
            (.repositories // []) as $have
            | .repositories = $have + [ $add[] | select(. as $u | $have | map(.url) | index($u) | not) | { url: ., enabled: true } ]
          ' "$cfgf" > "$cfgf.new" && mv "$cfgf.new" "$cfgf"
          echo '${repos}' > "$marker"
        fi
      '';
    };

    # cage restarts with the shell, so a Basecamp crash isn't a black screen.
    systemd.services."cage-tty1".serviceConfig = lib.mkIf (cfg.session == "cage") {
      Restart = lib.mkForce "always";
      RestartSec = 2;
    };

    programs.xwayland.enable = true;

    fonts.packages = with pkgs; [ inter jetbrains-mono noto-fonts noto-fonts-color-emoji ];
    fonts.fontconfig.defaultFonts = {
      sansSerif = [ "Inter" "Noto Sans" ];
      monospace = [ "JetBrains Mono" ];
    };

    security.rtkit.enable = true;
    services.pipewire = {
      enable = true;
      pulse.enable = true;
    };

    networking.networkmanager.enable = true;

    xdg.portal = {
      enable = true;
      extraPortals = [ pkgs.xdg-desktop-portal-gtk ];
      config.common.default = "*";
    };

    environment.systemPackages = [
      cfg.package
      pkgs.grim     # screenshots of the Wayland session (agents use these)
      pkgs.wlr-randr
    ];
  } (lib.mkIf (cfg.session == "sway") (let
    env = lib.concatStringsSep " " (lib.mapAttrsToList (k: v: "${k}=${lib.escapeShellArg v}") ({
      BASECAMP_LAYOUT = cfg.layout;
      QT_QPA_PLATFORM = cfg.qtPlatform;
      QT_LOGGING_RULES = "qt.qpa.*=false";
    } // cfg.environment));
    # Basecamp is the session: if it exits or crashes, it comes back.
    runBasecamp = pkgs.writeShellScript "basecamp-session" ''
      while true; do
        # ~/.config/basecampos/env (KEY=value lines) overrides the defaults, e.g.
        # QT_QPA_PLATFORM=wayland to try native Wayland without a rebuild.
        overrides=""
        [ -f "$HOME/.config/basecampos/env" ] && overrides=$(grep -E '^[A-Z_]+=' "$HOME/.config/basecampos/env" | tr '\n' ' ')
        env ${env} $overrides ${lib.concatStringsSep " " ([ "${cfg.package}/bin/LogosBasecamp" ] ++ cfg.extraArgs)}
        sleep 2
      done
    '';
    swayConfig = pkgs.writeText "basecampos-sway.conf" (''
      # BasecampOS: Basecamp is the UI. Sway only places windows: Basecamp full
      # screen on workspace 1, each native app alone on its own workspace (tiled,
      # so it fills the screen; `fullscreen` would put browsers into their own
      # full-screen mode and hide their tabs).
      set $mod Mod4
      default_border none
      default_floating_border none
      focus_on_window_activation focus
      output * bg #07090b solid_color
      seat * hide_cursor when-typing enable

      for_window [app_id="logos-basecamp"] mark basecamp, move container to workspace 1, fullscreen enable
      for_window [class="LogosBasecamp"] mark basecamp, move container to workspace 1, fullscreen enable
    '' + lib.concatImapStrings (i: a: ''
      for_window [app_id="${a.appId}"] move container to workspace ${toString (i + 1)}, workspace ${toString (i + 1)}
    '') cfg.externalApps + ''

      # Back to Basecamp from anywhere.
      bindsym $mod+Escape [con_mark="basecamp"] focus
      bindgesture swipe:3:right [con_mark="basecamp"] focus
      bindgesture swipe:3:left workspace next_on_output
      bindsym $mod+Tab workspace next_on_output
      floating_modifier $mod normal

      exec ${pkgs.systemd}/bin/systemctl --user import-environment DISPLAY WAYLAND_DISPLAY SWAYSOCK XDG_CURRENT_DESKTOP
      exec ${runBasecamp}
    '' + cfg.swayExtraConfig);
  in {
    programs.sway = {
      enable = true;
      wrapperFeatures.gtk = true;
    };
    services.greetd = {
      enable = true;
      settings = rec {
        initial_session = { command = "${config.programs.sway.package}/bin/sway --config ${swayConfig}"; user = cfg.user; };
        default_session = initial_session;
      };
    };
    environment.etc."basecampos/sway.conf".source = swayConfig;
    # What the System app drives in this session (brightness, mirroring).
    environment.systemPackages = with pkgs; [ brightnessctl wl-mirror ];
  }))
  ]);
}
