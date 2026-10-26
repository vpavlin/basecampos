# Brave inside a Basecamp window ("Browser (in Basecamp)"), with its own profile
# so it never hands off to a full-screen Brave.
#
# A profile whose preferences make Brave crash at start (on the Duet, after a
# camera call: its device rankings were applied at start-up, SIGTRAP every time)
# would otherwise leave the view at "Starting…" for good. If Brave dies within
# 15 s, Preferences, Secure Preferences and Local State are set aside
# (*.bak-<time>; history, bookmarks and logins stay) and it starts once more.
{ lib, writeShellScript, coreutils, brave, extraFlags ? [ ] }:

writeShellScript "brave-in-basecamp" ''
  PATH=${lib.makeBinPath [ coreutils ]}:$PATH
  profile="$HOME/.config/BraveSoftware/Brave-Basecamp"
  log="$HOME/.cache/basecampos/brave-in-basecamp.log"
  mkdir -p "$(dirname "$log")"
  run() {
    ${brave}/bin/brave --ozone-platform=wayland --disable-gpu --user-data-dir="$profile" \
      ${lib.escapeShellArgs extraFlags} "$@" >>"$log" 2>&1
  }
  start=$(date +%s)
  run "$@"
  rc=$?
  if [ "$rc" -ne 0 ] && [ $(( $(date +%s) - start )) -lt 15 ]; then
    ts=$(date +%Y%m%d-%H%M%S)
    for f in "Default/Preferences" "Default/Secure Preferences" "Local State"; do
      [ -e "$profile/$f" ] && mv "$profile/$f" "$profile/$f.bak-$ts"
    done
    echo "$(date -Is) brave exited $rc at start: preferences set aside (.bak-$ts), starting again" >>"$log"
    run "$@"
  fi
''
