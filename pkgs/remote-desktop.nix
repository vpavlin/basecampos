# A remote desktop inside a Basecamp tab (FreeCAD on another machine, driven
# from the tablet): wlvncc, a Wayland VNC viewer, on the tab's compositor.
#
# Where to connect stays on the device, not in the image: ~/.config/<name>/
#   target  one line: "<address> <port>" (a mesh address)
#   pass    optional: the VNC password, first line (mode 600). Without it
#           wlvncc connects without VNC auth (a server reachable only on the
#           mesh).
# Without a target the tab says so and exits. wlvncc fits the remote framebuffer to
# the tab; size the remote display to the tab's aspect.
#
# Patched for the tab's compositor (Qt 6.9): it offers wl_seat 4, wlvncc asked
# for 5 (a fatal protocol error) and waited for pointer frames, which only 5
# sends; and Qt holds on to shm buffers longer than wlvncc assumed, which then
# drew into buffers still on screen (garbled moving parts). It has no touch
# support: its launcher passes fingers on as the mouse. Without a cursor theme
# (started outside a session) it dereferenced NULL on the first pointer enter;
# patched to show no local cursor then, and the launcher names one.
#
# embedded = false: the own-workspace launcher, on sway itself (decorations
# left to sway).
{ lib, writeShellScript, coreutils, wlvncc, adwaita-icon-theme, name, embedded ? true }:

let
  wlvncc' = wlvncc.overrideAttrs (o: { patches = (o.patches or [ ]) ++ [ ./patches/wlvncc-qt-compositor.patch ]; });
in
writeShellScript "remote-desktop-${name}${lib.optionalString (!embedded) "-window"}" ''
  PATH=${lib.makeBinPath [ coreutils ]}:$PATH
  export XCURSOR_PATH="''${XCURSOR_PATH:+$XCURSOR_PATH:}${adwaita-icon-theme}/share/icons"
  export XCURSOR_THEME="''${XCURSOR_THEME:-Adwaita}" XCURSOR_SIZE="''${XCURSOR_SIZE:-24}"
  dir="$HOME/.config/${name}"
  log="$HOME/.cache/basecampos/${name}.log"
  mkdir -p "$(dirname "$log")"
  if [ ! -r "$dir/target" ]; then
    echo "$(date -Is) $dir/target missing (one line: <address> <port>)" >>"$log"
    exit 1
  fi
  read -r address port <"$dir/target"
  auth=()
  [ -r "$dir/pass" ] && auth=(-A "head -n1 '$dir/pass'")
  # -s: the tab's compositor offers no EGL (kept on sway too: wlvncc's GPU
  # path is untested on the Duet); -d: in a tab, the tab is the frame.
  # zlib/hextile: in a tab, Tight and ZRLE frames came out garbled (in the VM,
  # at any frame rate; clean on plain sway with the same binary), raw, hextile
  # and zlib clean. Lossless, more bytes than Tight's JPEG.
  exec ${wlvncc'}/bin/wlvncc -s ${lib.optionalString embedded "-d"} -a ${name} -e zlib,hextile,copyrect -c 6 \
    "''${auth[@]}" "$address" ''${port:-5900} >>"$log" 2>&1
''
