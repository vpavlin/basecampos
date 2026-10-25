# The Lenovo IdeaPad Duet (krane, MT8183), from duet-kit (github.com/vpavlin/duet-kit).
#
# Basecamp is the UI (basecampos.shell.session = "sway"): duet-kit's own UI
# (waybar, launcher, quick-settings/Wi-Fi/Bluetooth/display panels, app tabs)
# is NOT used; the System app covers it. What is taken from the kit is what
# makes the hardware work, with the kit as the source of truth (the Duet agent
# keeps tuning it; `nix flake update duet-kit` picks it up):
#
#   etc/   -> udev, logind, journald, NetworkManager, tmpfiles, battery-limit
#   home/  -> a whitelist (camera graph, portals, notifications, VNC, theme)
#             linked into the user's home; the session daemons (autorotate,
#             idle-dim, keyboard-layout keeper, on-screen keyboard, F-keys) are
#             run from the Sway config with Debian paths rewritten
#
# Hardware-only parts (cros_ec battery limit, keyboard cover wakeup, CPU
# governor) sit behind `hardware`, so the session also runs in an x86 VM.
{ config, lib, pkgs, ... }:

let
  cfg = config.basecampos.duet;
  shell = config.basecampos.shell;
  home = "/home/${cfg.user}";
  kit = cfg.kit;

  python = pkgs.python3.withPackages (ps: [ ps.pygobject3 ]);
  typelibs = with pkgs; [ gobject-introspection gtk4 libadwaita gst_all_1.gstreamer gst_all_1.gst-plugins-base glib gdk-pixbuf pango harfbuzz graphene ];

  # The kit's home tree with Debian paths rewritten. Scripts in .local/bin are
  # wrapped so their GTK4/libadwaita panels find typelibs and schemas.
  homeTree = pkgs.runCommand "duet-kit-home" {
    nativeBuildInputs = [ pkgs.makeWrapper python ];
  } ''
    cp -r ${kit}/home $out
    chmod -R u+w $out
    find $out -type f -print0 | xargs -0 sed -i \
      -e 's|%h/Apps/Basecamp.AppImage|${shell.package}/bin/LogosBasecamp|g' \
      -e 's|/home/linux/Apps/Basecamp.AppImage|${shell.package}/bin/LogosBasecamp|g' \
      -e 's|/home/linux|${home}|g' \
      -e 's|/usr/local/bin/wvkbd-mobintl|${pkgs.wvkbd}/bin/wvkbd-mobintl|g' \
      -e 's|/usr/local/bin/shrooms-agent|/run/current-system/sw/bin/shrooms-agent|g' \
      -e 's|/usr/bin/pipewire|${pkgs.pipewire}/bin/pipewire|g' \
      -e 's|/usr/bin/wireplumber|${pkgs.wireplumber}/bin/wireplumber|g' \
      -e 's|/usr/bin/foot|${pkgs.foot}/bin/foot|g' \
      -e 's|/usr/bin/python3|${python}/bin/python3|g' \
      -e 's|/usr/bin/brave-browser|${pkgs.brave}/bin/brave|g' \
      -e 's|/usr/bin/wayvnc|${pkgs.wayvnc}/bin/wayvnc|g' \
      -e 's|/usr/bin/waybar|${pkgs.waybar}/bin/waybar|g'
    # The `czech` layer exists only in the Duet's patched wvkbd (the patch is not in
    # duet-kit yet); stock wvkbd exits on an unknown layer, so use its own layers.
    sed -i -e 's| --landscape-layers czech,landscapespecial||' $out/.local/bin/osk
    # foot 1.2x renamed [colors] (Debian's foot still reads it).
    sed -i -e 's|^\[colors\]$|[colors-dark]|' $out/.config/foot/foot.ini
    # The Debian unit trusts a self-signed LAN repo; that bundle
    # is not part of the kit, and a missing CA file breaks every download.
    sed -i -e '/basecamp-ca-bundle.pem/d' $out/.config/systemd/user/basecamp.service
    ${lib.optionalString (shell.layout != "sidebar") ''
      sed -i -e '/^\[Service\]/a Environment=BASECAMP_LAYOUT=${shell.layout}' $out/.config/systemd/user/basecamp.service
    ''}

    for f in $out/.local/bin/*; do
      if head -1 "$f" | grep -q python3; then
        sed -i -e '1s|.*|#!${python}/bin/python3|' "$f"
        wrapProgram "$f" \
          --prefix GI_TYPELIB_PATH : "${lib.makeSearchPathOutput "out" "lib/girepository-1.0" typelibs}" \
          --prefix XDG_DATA_DIRS : "${pkgs.gtk4}/share/gsettings-schemas/${pkgs.gtk4.name}:${pkgs.gsettings-desktop-schemas}/share/gsettings-schemas/${pkgs.gsettings-desktop-schemas.name}"
      else
        sed -i -e '1s|^#!/bin/sh|#!${pkgs.bash}/bin/sh|; 1s|^#!/bin/bash|#!${pkgs.bash}/bin/bash|' "$f"
      fi
    done
  '';

  # Files the kit's own scripts rewrite at runtime: copied once, then the user's.
  mutable = [ ".config/xdg-desktop-portal-wlr/config" ];

  # Every file under home/, as a path relative to it.
  listFiles = dir: prefix: lib.concatLists (lib.mapAttrsToList (name: type:
    if type == "directory" then listFiles "${dir}/${name}" "${prefix}${name}/"
    else [ "${prefix}${name}" ]) (builtins.readDir dir));
  # duet-kit's home files that are not UI: the camera graph, portals (screen
  # sharing), notifications, VNC, theme and terminal settings.
  keep = f: builtins.any (p: lib.hasPrefix p f) [
    ".config/systemd/user/camera-" ".config/systemd/user/screen-share.service"
    ".config/libcamera/" ".config/xdg-desktop-portal" ".config/mako/"
    ".config/gtk-3.0/" ".config/gtk-4.0/" ".config/foot/" ".config/wayvnc/"
    # Keeps the camera WirePlumber video-only: if it enumerates the sound card
    # too, it holds the device and PulseAudio gets no working profile (no sink).
    ".config/wireplumber/"
  ];
  homeFiles = builtins.filter keep (listFiles "${kit}/home" "");
  bin = "${homeTree}/.local/bin";

  # `systemctl --user enable` on Debian: link each unit into its WantedBy's .wants.
  unitDir = ".config/systemd/user";
  enableLinks = lib.concatLists (map (f:
    let
      unit = lib.removePrefix "${unitDir}/" f;
      wanted = builtins.match ".*\nWantedBy=([^\n]+).*" (builtins.readFile "${kit}/home/${f}");
    in lib.optional (lib.hasPrefix "${unitDir}/" f && !(lib.hasInfix "/" unit) && wanted != null)
      "${unitDir}/${builtins.head wanted}.wants/${unit} ${homeTree}/${f}")
    homeFiles);

  # Runs as the user before the session: tmpfiles can't do this (it creates
  # the parent directories as root, then refuses the "unsafe path transition"
  # from the user's .config into them).
  installHome = pkgs.writeShellScript "duet-home" ''
    set -eu
    cd "$HOME"
    link() { mkdir -p "$(dirname "$1")"; ln -sfn "$2" "$1"; }
    ${lib.concatMapStrings (f:
      if builtins.elem f mutable then ''
        [ -e ${lib.escapeShellArg f} ] && [ ! -L ${lib.escapeShellArg f} ] || {
          mkdir -p "$(dirname ${lib.escapeShellArg f})"; rm -f ${lib.escapeShellArg f}
          install -m 0644 ${homeTree}/${lib.escapeShellArg f} ${lib.escapeShellArg f}; }
      '' else ''
        link ${lib.escapeShellArg f} ${homeTree}/${lib.escapeShellArg f}
      '') homeFiles}
    ${lib.concatMapStrings (l: "link ${l}\n") enableLinks}
  '';

  tablerIcons = pkgs.fetchurl {
    url = "https://cdn.jsdelivr.net/npm/@tabler/icons-webfont@3.49.0/dist/fonts/tabler-icons.ttf";
    hash = "sha256-0pFnbcTCizhlUnU78WzyvvDpPlVD+OC1xdcsDaQa4bw=";
  };
  tablerFont = pkgs.runCommand "tabler-icons-font" { } ''
    install -Dm644 ${tablerIcons} $out/share/fonts/truetype/tabler-icons.ttf
  '';

  batteryLimit = pkgs.writeScript "battery-limit" (
    builtins.replaceStrings [ "#!/usr/bin/env python3" ] [ "#!${pkgs.python3}/bin/python3" ]
      (builtins.readFile "${kit}/usr/local/sbin/battery-limit"));
in
{
  options.basecampos.duet = {
    enable = lib.mkEnableOption "the Duet tablet session from duet-kit";
    kit = lib.mkOption {
      type = lib.types.path;
      description = "A duet-kit checkout (flake input).";
    };
    user = lib.mkOption {
      type = lib.types.str;
      default = shell.user;
    };
    hardware = lib.mkOption {
      type = lib.types.bool;
      default = false;
      description = "Parts that only make sense on the tablet: EC charge limit, keyboard-cover wakeup rule, CPU governor.";
    };
  };

  config = lib.mkIf cfg.enable {
    basecampos.shell.session = "sway";

    # The tablet's Sway rules (panel, touch, keyboard cover) and the kit's
    # session daemons, on top of BasecampOS's own Sway config.
    basecampos.shell.swayExtraConfig = ''
      output DSI-1 transform 270 scale 1.2
      input type:touch map_to_output DSI-1
      input type:tablet_tool map_to_output DSI-1
      input type:keyboard {
          xkb_layout cz,us
          xkb_options grp:alt_shift_toggle
      }
      input type:touchpad {
          tap enabled
          natural_scroll enabled
      }

      # on-screen keyboard: shown when the keyboard cover is folded away / detached
      exec ${bin}/osk
      bindswitch --reload --locked tablet:on exec pkill -USR2 wvkbd-mobintl
      bindswitch --reload --locked tablet:off exec ${bin}/osk
      exec ${bin}/autorotate
      exec ${bin}/kbd-layout-keeper
      exec ${bin}/idle-dim
      exec ${pkgs.mako}/bin/mako
      # The panel's backlight comes up at its minimum (1/1023): near black.
      exec ${pkgs.brightnessctl}/bin/brightnessctl -q -c backlight set 70%
      # ...and the speakers at 0% on a fresh profile (PulseAudio restores it after).
      exec sh -c 'sleep 5; [ -e ~/.local/state/basecampos-volume-set ] || { ${pkgs.pulseaudio}/bin/pactl set-sink-volume @DEFAULT_SINK@ 50% && mkdir -p ~/.local/state && touch ~/.local/state/basecampos-volume-set; }' 

      # projector mirror (wl-mirror, started by the System app) never takes focus
      for_window [app_id="at.yrlf.wl_mirror"] no_focus, border none
      no_focus [app_id="at.yrlf.wl_mirror"]

      bindsym Print exec ${bin}/screenshot
      # keyboard top row (Chromebook layout; the keys send plain F1-F10 and F13)
      bindsym F1 exec wtype -M alt -k Left -m alt
      bindsym F2 exec wtype -M alt -k Right -m alt
      bindsym F3 exec wtype -M ctrl -k r -m ctrl
      bindsym F4 fullscreen toggle
      bindsym F5 workspace next_on_output
      bindsym F6 exec brightnessctl set 10%-
      bindsym F7 exec brightnessctl set +10%
      bindsym F8 exec pactl set-sink-mute @DEFAULT_SINK@ toggle
      bindsym F9 exec pactl set-sink-volume @DEFAULT_SINK@ -5%
      bindsym F10 exec pactl set-sink-volume @DEFAULT_SINK@ +5%
      bindsym F13 exec systemctl suspend
      mode "fkey" {
          bindsym Mod4+Mod1+Shift+Control+F12 nop
      }
    '' + lib.concatMapStrings (n: "bindsym $mod+F${toString n} exec ${bin}/fkey ${toString n}\n") (lib.range 1 10);

    # Brave with the flags the tablet's Mali GPU and camera need (duet-kit's brave.service).
    basecampos.shell.externalApps = lib.mkForce [
      {
        id = "browser"; appId = "brave-browser"; name = "Browser";
        command = [ "${pkgs.brave}/bin/brave" "--ozone-platform=wayland" "--use-gl=angle" "--use-angle=gles"
          "--password-store=basic" "--enable-features=WebRtcPipeWireCamera,WebRTCPipeWireCapturer" ];
        embedCommand = [ "${pkgs.brave}/bin/brave" "--ozone-platform=wayland" "--disable-gpu" "--password-store=basic"
          "--user-data-dir=~/.config/BraveSoftware/Brave-Basecamp" ];
      }
      { id = "terminal"; appId = "foot"; name = "Terminal"; command = [ "${pkgs.foot}/bin/foot" ];
        embedCommand = [ "${pkgs.lxqt.qterminal}/bin/qterminal" ]; }
    ];

    # Audio as on the device: PulseAudio for sound, PipeWire only as the
    # camera/screen-capture graph (the kit's camera-pipewire user units).
    services.pipewire.enable = lib.mkForce false;
    services.pulseaudio = {
      enable = true;
      package = pkgs.pulseaudioFull;
    };
    # The MT8183's speakers/headset only switch on through ALSA UCM profiles
    # (alsa-ucm-conf has mt8183_mt6358_ts3a227_max98357); without them PulseAudio
    # sees no usable output (silent, volume reads 0%). Debian finds them in /usr/share.
    # The card's mixer must be set up before PulseAudio probes it (with
    # defaults, the Mic PCM fails hw_params and PulseAudio drops the whole
    # profile: only auto_null). Debian restores /var/lib/alsa/asound.state at
    # boot; so do we, seeded with Debian's state (duet/duet, run 9).
    # (seeded by the tmpfiles rule below; also brings alsa-utils' udev rule,
    # which restores it whenever the card appears)
    hardware.alsa.enablePersistence = true;
    environment.sessionVariables.ALSA_CONFIG_UCM2 = "${pkgs.alsa-ucm-conf}/share/alsa/ucm2";
    systemd.user.services.pulseaudio.environment.ALSA_CONFIG_UCM2 = "${pkgs.alsa-ucm-conf}/share/alsa/ucm2";
    # The tablet's shrooms daemon + agent (Debian's, in a chroot, or ours) serve
    # on the mesh interfaces; NixOS's firewall would drop the agent's port 7387
    # (run 7: ping worked, a2a timed out).
    networking.firewall.trustedInterfaces = [ "shrooms+" ];

    hardware.bluetooth = {
      enable = true;
      powerOnBoot = true;
    };
    services.blueman.enable = true;
    services.upower.enable = true;
    hardware.sensor.iio.enable = true;
    services.gvfs.enable = true;
    services.resolved.enable = true;

    xdg.portal = {
      enable = true;
      wlr.enable = true;
      extraPortals = [ pkgs.xdg-desktop-portal-gtk ];
    };

    fonts.packages = with pkgs; [ font-awesome symbola noto-fonts-color-emoji tablerFont ];

    environment.systemPackages = with pkgs; [
      swaybg swayidle swaylock waybar foot fuzzel wvkbd mako libnotify
      brightnessctl networkmanagerapplet wlr-randr grim wtype wl-mirror wayvnc
      pulseaudio pavucontrol alsa-utils bluez blueman iw libinput evtest ffmpeg htop
      portfolio-filemanager loupe brave python
      pipewire wireplumber libcamera
      iio-sensor-proxy
    ];

    users.users.${cfg.user}.extraGroups = [ "video" "input" "audio" "networkmanager" "bluetooth" ];

    systemd.services.duet-home = {
      description = "Link duet-kit's home files for ${cfg.user}";
      wantedBy = [ "multi-user.target" ];
      before = [ "display-manager.service" ];
      path = [ pkgs.coreutils ];
      serviceConfig = {
        Type = "oneshot";
        User = cfg.user;
        RemainAfterExit = true;
        ExecStart = installHome;
      };
    };

    systemd.tmpfiles.rules = [ ]
      ++ lib.optionals cfg.hardware [
        # schedutil on both clusters
        "w /sys/devices/system/cpu/cpufreq/policy0/scaling_governor - - - - schedutil"
        "w /sys/devices/system/cpu/cpufreq/policy4/scaling_governor - - - - schedutil"
        # owned by the user so the quick-settings slider can write it
        "C /etc/default/battery-limit 0644 ${cfg.user} users - ${kit}/etc/default/battery-limit"
        # Debian's mixer state, restored at boot (see hardware.alsa above); only if none yet
        "C /var/lib/alsa/asound.state 0644 root root - ${./asound.state}"
      ];

    # Wi-Fi power save off (else the tablet stops answering ARP); DNS via resolved.
    environment.etc."NetworkManager/conf.d/50-wifi-powersave-off.conf".source =
      "${kit}/etc/NetworkManager/conf.d/50-wifi-powersave-off.conf";
    environment.etc."NetworkManager/conf.d/50-dns-resolved.conf".source =
      "${kit}/etc/NetworkManager/conf.d/50-dns-resolved.conf";
    environment.etc."systemd/journald.conf.d/50-duet.conf".source =
      "${kit}/etc/systemd/journald.conf.d/50-duet.conf";
    environment.etc."systemd/logind.conf.d/50-duet-powerkey.conf".source =
      "${kit}/etc/systemd/logind.conf.d/50-duet-powerkey.conf";

    services.udev.extraRules = lib.concatStringsSep "\n" ([
      (builtins.readFile "${kit}/etc/udev/rules.d/80-hide-chromeos-parts.rules")
      (builtins.replaceStrings [ "/bin/chgrp" "/bin/chmod" ] [ "${pkgs.coreutils}/bin/chgrp" "${pkgs.coreutils}/bin/chmod" ]
        (builtins.readFile "${kit}/etc/udev/rules.d/90-backlight.rules"))
    ] ++ lib.optional cfg.hardware
      # suspend aborts unless the keyboard cover's USB wakeup is off
      (builtins.readFile "${kit}/etc/udev/rules.d/80-hammer-no-wakeup.rules"));

    # Keeps the battery between LOWER and UPPER on AC via the ChromeOS EC.
    systemd.services.battery-limit = lib.mkIf cfg.hardware {
      description = "Limit Duet battery charge while on AC";
      wantedBy = [ "multi-user.target" ];
      serviceConfig = {
        ExecStart = "${batteryLimit}";
        ExecStopPost = "${batteryLimit} --normal";
        Restart = "always";
        RestartSec = 5;
      };
    };

    hardware.enableRedistributableFirmware = lib.mkIf cfg.hardware true;
    services.fwupd.enable = lib.mkDefault false;
  };
}
