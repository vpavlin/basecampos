# BasecampOS on a phone: Basecamp is the only UI, on phoc (Phosh's phone
# compositor, wlroots) instead of Sway. Taken from the Phosh project as
# stand-alone pieces, without its shell (Basecamp replaces that):
#
#   phoc     the compositor: windows maximized, display cutouts, input-method
#            relay for the OSK, fractional scaling
#   Stevia   the on-screen keyboard: unfolds by itself when a text field has
#            focus (input-method-v2), folds when it loses it
#   Calls    phone calls (ModemManager, callaudiod), in a Basecamp tab
#   feedbackd  haptics, LED and event sounds
#
# Native apps run inside Basecamp tabs only (apps/external-embedded): phoc has
# no workspaces and, without Phosh, no app switcher to get back to Basecamp.
{ config, lib, pkgs, ... }:

let
  cfg = config.basecampos.phone;
  shell = config.basecampos.shell;

  phocIni = pkgs.writeText "phoc.ini" (''
    [core]
    xwayland = false
  '' + lib.concatStrings (lib.mapAttrsToList (name: o: ''
    [output:${name}]
    ${lib.optionalString (o.scale != null) "scale = ${toString o.scale}"}
    ${lib.optionalString (o.rotate != null) "rotate = ${toString o.rotate}"}
  '') cfg.outputs));

  env = lib.concatStringsSep " " (lib.mapAttrsToList (k: v: "${k}=${lib.escapeShellArg v}") ({
    BASECAMP_LAYOUT = shell.layout;
    BASECAMP_NO_DETACH = "1";
    XKB_DEFAULT_LAYOUT = shell.keyboard.layout;
    XKB_DEFAULT_OPTIONS = shell.keyboard.options;
    QT_QPA_PLATFORM = "wayland";
    QT_LOGGING_RULES = "qt.qpa.*=false";
  } // shell.environment));

  # Started by phoc (-E) once it is up. Ends with phoc: phoc is its parent, and
  # the loop stops restarting Basecamp when that parent is gone.
  session = pkgs.writeShellScript "basecampos-phone-session" ''
    # phoc may start this through a shell: its pid is the nearest ancestor named phoc.
    phoc=$PPID
    # (named .phoc-wrapped: nixpkgs wraps the binary)
    isphoc() { case "$(cat /proc/$1/comm 2>/dev/null)" in *phoc*) return 0 ;; esac; return 1; }
    while [ "$phoc" -gt 1 ] && ! isphoc "$phoc"; do
      phoc=$(awk '{print $4}' /proc/$phoc/stat)
    done
    alive() { isphoc "$phoc"; }
    alive || { echo "basecampos-phone-session: phoc not found among my ancestors" >&2; exit 1; }
    export XDG_CURRENT_DESKTOP=Basecamp
    ${pkgs.systemd}/bin/systemctl --user import-environment WAYLAND_DISPLAY XDG_CURRENT_DESKTOP
    ${pkgs.dbus}/bin/dbus-update-activation-environment --systemd WAYLAND_DISPLAY XDG_CURRENT_DESKTOP
    # The OSK, restarted if it dies; its surface unfolds on text focus.
    ( while alive; do ${pkgs.stevia}/bin/phosh-osk-stevia; sleep 2; done ) &
    while alive; do
      overrides=""
      [ -f "$HOME/.config/basecampos/env" ] && overrides=$(grep -E '^[A-Z_]+=' "$HOME/.config/basecampos/env" | tr '\n' ' ')
      env ${env} $overrides ${lib.concatStringsSep " " ([ "${shell.package}/bin/LogosBasecamp" ] ++ shell.extraArgs)}
      sleep 2
    done
  '';
  # phoc's own output (and the session's) in the user's cache: greetd keeps none.
  start = pkgs.writeShellScript "basecampos-phone-start" ''
    log="$HOME/.cache/basecampos/phoc.log"
    mkdir -p "$(dirname "$log")"
    [ -f "$log" ] && mv -f "$log" "$log.old"
    exec ${pkgs.phoc}/bin/phoc -C ${phocIni} -E ${session} >"$log" 2>&1
  '';
in
{
  options.basecampos.phone = {
    enable = lib.mkEnableOption "BasecampOS on a phone (phoc, Stevia, Calls)";
    outputs = lib.mkOption {
      type = lib.types.attrsOf (lib.types.submodule {
        options = {
          scale = lib.mkOption { type = lib.types.nullOr (lib.types.either lib.types.int lib.types.float); default = null; };
          rotate = lib.mkOption { type = lib.types.nullOr lib.types.int; default = null; };
        };
      });
      default = { };
      example = { "DSI-1" = { scale = 2.5; }; };
      description = "phoc output settings, by output name.";
    };
  };

  config = lib.mkIf cfg.enable {
    basecampos.shell.session = "external";
    basecampos.shell.qtPlatform = "wayland";

    services.greetd = {
      enable = true;
      settings = rec {
        initial_session = { command = "${start}"; user = shell.user; };
        default_session = initial_session;
      };
    };
    environment.etc."basecampos/phoc.ini".source = phocIni;

    # Stevia only shows its surface while the GNOME screen-keyboard setting is on.
    programs.dconf = {
      enable = true;
      profiles.user.databases = [{
        settings."org/gnome/desktop/a11y/applications".screen-keyboard-enabled = true;
        # Calls and the OSK in Basecamp's colours
        settings."org/gnome/desktop/interface".color-scheme = "prefer-dark";
      }];
    };

    programs.calls.enable = true;
    programs.feedbackd.enable = true;
    networking.networkmanager.enable = true; # brings ModemManager

    # (GTK apps draw their icons from Adwaita)
    environment.systemPackages = with pkgs; [ phoc stevia grim wlr-randr adwaita-icon-theme ];
    fonts.packages = [ pkgs.noto-fonts-color-emoji ];
  };
}
