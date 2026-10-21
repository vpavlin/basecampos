#!/usr/bin/env bash
# Idle CPU probe: start a variant, wait 20 s, measure CPU over 10 s per process and per main-process thread.
#   idle.sh <bundle> <label> [env...]
set -u
BUNDLE=$1; LABEL=$2; shift 2
XVFB=${XVFB:-Xvfb}; DISP=:101; ROOT=${BENCH_DIR:-$HOME/basecamp-bench}/runs/idle-$LABEL; TEMPLATE=${TEMPLATE:-${BENCH_DIR:-$HOME/basecamp-bench}/template-empty}
UD=$ROOT/user; rm -rf "$ROOT"; mkdir -p "$ROOT"; cp -a "$TEMPLATE" "$UD"
env -u WAYLAND_DISPLAY DISPLAY=$DISP QT_QPA_PLATFORM=xcb LOGOS_USER_DIR="$UD" \
    XDG_CONFIG_HOME="$ROOT/config" XDG_CACHE_HOME="$ROOT/cache" XDG_RUNTIME_DIR=/run/user/$(id -u) \
    LOGOS_INSTANCE_ID=1d1e$(printf '%08x' $RANDOM) "$@" \
    setsid "$BUNDLE/bin/LogosBasecamp" --user-dir "$UD" >"$ROOT/run.log" 2>&1 </dev/null &
PID=$!; disown
sleep 20
HZ=$(getconf CLK_TCK)
pids() { for p in $(pgrep -u "$(id -u)"); do grep -qz "LOGOS_USER_DIR=$UD" /proc/$p/environ 2>/dev/null && echo $p; done; }
ticks() { local a; read -r a < "$1" 2>/dev/null || { echo 0; return; }; a=${a#*) }; set -- $a; echo $(( ${12} + ${13} )); }
declare -A A B
P=$(pids)
for p in $P; do A[$p]=$(ticks /proc/$p/stat); done
for t in /proc/$PID/task/*; do A[$t]=$(ticks $t/stat); done
sleep 10
for p in $P; do B[$p]=$(ticks /proc/$p/stat); done
for t in /proc/$PID/task/*; do B[$t]=$(ticks $t/stat); done
echo "== $LABEL: idle CPU over 10 s (% of one core)"
for p in $P; do printf '  %-6s %-28s %5s%%\n' "$p" "$(tr '\0' ' ' < /proc/$p/cmdline | cut -c1-28 | sed 's#.*/##')" $(awk "BEGIN{printf \"%.1f\", ($((B[$p]-A[$p]))) * 100 / $HZ / 10}"); done
echo "  busiest threads of the main process:"
for t in /proc/$PID/task/*; do echo "$(( B[$t]-A[$t] )) $(cat $t/comm)"; done | sort -rn | head -6 | awk -v hz=$HZ '{printf "    %-20s %.1f%%\n", $2, $1*100/hz/10}'
echo "  shell library loaded: $(grep -o '[^ ]*main_ui[^ ]*\.so' /proc/$PID/maps | sort -u | sed 's#.*/\(plugins/[^/]*\)/.*#\1#' | tr '\n' ' ')"
for p in $(pids); do kill -9 $p 2>/dev/null; done
