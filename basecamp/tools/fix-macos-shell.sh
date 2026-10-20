#!/bin/bash
# macOS: make a shell library built by Nix use the Basecamp app's own Qt and
# libraries instead of /nix/store paths. Without this a shell package loads a
# second copy of Qt next to the app's and fails ("Must construct a QApplication
# before a QWidget"). Linux needs nothing: libraries are matched by soname.
#
#   tools/fix-macos-shell.sh <main_ui.dylib> <out.dylib>
set -euo pipefail
IN=$1; OUT=$2
cp "$IN" "$OUT"; chmod u+w "$OUT"
otool -L "$OUT" | tail -n +2 | awk '{print $1}' | while read -r dep; do
  case "$dep" in
    /nix/store/*/lib/*.framework/*)
      # .../lib/QtCore.framework/Versions/A/QtCore -> @rpath/QtCore.framework/Versions/A/QtCore
      install_name_tool -change "$dep" "@rpath/${dep#*/lib/}" "$OUT" ;;
    /nix/store/*/libc++*.dylib)
      install_name_tool -change "$dep" /usr/lib/libc++.1.dylib "$OUT" ;;
    /nix/store/*/lib/*.dylib)
      install_name_tool -change "$dep" "@executable_path/../lib/$(basename "$dep")" "$OUT" ;;
  esac
done
# Resolve @rpath against the app bundle (the shell lives outside it, in the
# user's plugins folder).
for rp in $(otool -l "$OUT" | awk '/LC_RPATH/{getline; getline; print $2}'); do
  case "$rp" in /nix/store/*) install_name_tool -delete_rpath "$rp" "$OUT" ;; esac
done
install_name_tool -add_rpath @executable_path/../Frameworks "$OUT" 2>/dev/null || true
install_name_tool -add_rpath @executable_path/../lib "$OUT" 2>/dev/null || true
codesign --force -s - "$OUT"
if otool -L "$OUT" | grep -q /nix/store; then echo "still links /nix/store" >&2; exit 1; fi
echo "fixed $OUT"
