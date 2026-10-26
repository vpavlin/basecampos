# Brave inside a Basecamp window ("Browser (in Basecamp)"), with its own profile
# so it never hands off to a full-screen Brave.
#
# A profile whose preferences make Brave crash at start (on the Duet, after a
# camera call: its device rankings were applied at start-up, SIGTRAP every time)
# would otherwise leave the view at "Starting…" for good. If Brave crashes within
# 15 s, Preferences, Secure Preferences and Local State are set aside
# (*.bak-<time>; history, bookmarks and logins stay) and it starts once more.
#
# "Crashed" is read from its output, not its exit code: nixpkgs' bin/brave runs
# the real binary from a launcher script that does not pass a crash on (exit 0
# after "Trace/breakpoint trap (core dumped)", duet/duet). A quick clean exit is
# a hand-off to a Brave already running on this profile and changes nothing.
{ lib, writeShellScript, coreutils, gnugrep, brave, extraFlags ? [ ] }:

writeShellScript "brave-in-basecamp" ''
  PATH=${lib.makeBinPath [ coreutils gnugrep ]}:$PATH
  profile="$HOME/.config/BraveSoftware/Brave-Basecamp"
  log="$HOME/.cache/basecampos/brave-in-basecamp.log"
  mkdir -p "$(dirname "$log")"
  # Straight into the log (a long session's output would fill a tmpfs copy);
  # the crash check reads what this run added.
  run() {
    ${brave}/bin/brave --ozone-platform=wayland --disable-gpu --enable-logging=stderr --user-data-dir="$profile" \
      ${lib.escapeShellArgs extraFlags} "$@" >>"$log" 2>&1
  }
  from=$(( $(stat -c %s "$log" 2>/dev/null || echo 0) + 1 ))
  start=$(date +%s)
  run "$@"
  rc=$?
  crashed=no
  if [ "$rc" -ge 128 ] || tail -c +"$from" "$log" | grep -qE 'core dumped|Trace/breakpoint trap|Segmentation fault|Aborted'; then
    crashed=yes
  fi
  if [ "$crashed" = yes ] && [ $(( $(date +%s) - start )) -lt 15 ]; then
    ts=$(date +%Y%m%d-%H%M%S)
    for f in "Default/Preferences" "Default/Secure Preferences" "Local State"; do
      [ -e "$profile/$f" ] && mv "$profile/$f" "$profile/$f.bak-$ts"
    done
    echo "$(date -Is) brave crashed at start (rc $rc): preferences set aside (.bak-$ts), starting again" >>"$log"
    run "$@"
  fi
''
