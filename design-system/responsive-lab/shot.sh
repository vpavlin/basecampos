#!/usr/bin/env bash
# Render one lab screen and print its report (see main.cpp).
# Usage: shot.sh <page> <width> <height> <out.png> [--density touch] [--state k=v]
#
# Renders on a Wayland compositor with mesa's llvmpipe (CPU OpenGL), because
# LogosIcon tints through MultiEffect, which the software backend can't draw.
#   LAB_BIN       the built LogosResponsiveLab
#   LAB_WAYLAND   "<XDG_RUNTIME_DIR>:<WAYLAND_DISPLAY>" of a (headless) sway with
#                 `for_window [title="Logos Responsive Lab"] floating enable`
#   LAB_MESA      a mesa store path (EGL vendor json, dri drivers)
# Without LAB_WAYLAND it falls back to offscreen software (icons blank).
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
BIN="${LAB_BIN:-$HERE/../result-lab/bin/LogosResponsiveLab}"
page=$1 w=$2 h=$3 out=$4; shift 4
# Each shot gets its own settings, so nothing (density, last page) carries over.
cfg="$(mktemp -d)"; trap 'rm -rf "$cfg"' EXIT
envs=(HOME="$HOME" XDG_CONFIG_HOME="$cfg" PATH=/usr/bin:/bin LANG=C.UTF-8 QT_FORCE_STDERR_LOGGING=1
      LOGOS_LAB_QML="${LOGOS_LAB_QML:-$HERE/qml}")
if [ -n "${LAB_WAYLAND:-}" ] && [ -n "${LAB_MESA:-}" ]; then
  envs+=(QT_QPA_PLATFORM=wayland XDG_RUNTIME_DIR="${LAB_WAYLAND%%:*}" WAYLAND_DISPLAY="${LAB_WAYLAND##*:}"
         __EGL_VENDOR_LIBRARY_FILENAMES="$LAB_MESA/share/glvnd/egl_vendor.d/50_mesa.json"
         LIBGL_DRIVERS_PATH="$LAB_MESA/lib/dri" GALLIUM_DRIVER=llvmpipe GBM_BACKENDS_PATH="$LAB_MESA/lib/gbm")
else
  envs+=(QT_QPA_PLATFORM=offscreen)
fi
env -i "${envs[@]}" timeout 60 "$BIN" --shot "$out" --width "$w" --height "$h" --page "$page" "$@" \
  2> >(grep -v '^Detected locale\|^Qt depends\|^If this causes\|^for more information\|^libEGL warning\|^$' >&2)
