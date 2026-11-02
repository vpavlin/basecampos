# A remote desktop inside a Basecamp tab (FreeCAD on another machine, driven
# from the tablet): wlvncc, a Wayland VNC viewer, on the tab's compositor.
#
# Where to connect stays on the device, not in the image: ~/.config/<name>/
#   target  one line: "<address> <port>" (a mesh address)
#   pass    the VNC password, first line (mode 600; copied by hand)
# Without them the tab says so and exits. wlvncc fits the remote framebuffer to
# the tab; size the remote display to the tab's aspect.
#
# Patched for the tab's compositor (Qt 6.9): it offers wl_seat 4, wlvncc asked
# for 5 (a fatal protocol error) and waited for pointer frames, which only 5
# sends; and Qt holds on to shm buffers longer than wlvncc assumed, which then
# drew into buffers still on screen (garbled moving parts). It has no touch
# support: its launcher passes fingers on as the mouse.
{ lib, writeShellScript, coreutils, wlvncc, name }:

let
  wlvncc' = wlvncc.overrideAttrs (o: { patches = (o.patches or [ ]) ++ [ ./patches/wlvncc-qt-compositor.patch ]; });
in
writeShellScript "remote-desktop-${name}" ''
  PATH=${lib.makeBinPath [ coreutils ]}:$PATH
  dir="$HOME/.config/${name}"
  log="$HOME/.cache/basecampos/${name}.log"
  mkdir -p "$(dirname "$log")"
  if [ ! -r "$dir/target" ] || [ ! -r "$dir/pass" ]; then
    echo "$(date -Is) $dir/target or $dir/pass missing" >>"$log"
    exit 1
  fi
  read -r address port <"$dir/target"
  # -s: the tab's compositor offers no EGL; -d: the tab is the frame.
  # zlib/hextile: in a tab, Tight and ZRLE frames came out garbled (in the VM,
  # at any frame rate; clean on plain sway with the same binary), raw, hextile
  # and zlib clean. Lossless, more bytes than Tight's JPEG.
  exec ${wlvncc'}/bin/wlvncc -s -d -a ${name} -e zlib,hextile,copyrect -c 6 \
    -A "head -n1 '$dir/pass'" "$address" ''${port:-5900} >>"$log" 2>&1
''
