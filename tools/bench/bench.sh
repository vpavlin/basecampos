#!/usr/bin/env bash
# Startup time + memory benchmark for Basecamp shells, on a hidden Xvfb display.
#
#   bench.sh <bundle-dir> <label> [runs] [env...]
#
# Each run: fresh --user-dir copied from a template, start Basecamp, record
#   t_window  = seconds until the main window is mapped
#   t_settle  = seconds until the whole process tree is idle (CPU < 3% for 3 s)
#   then, 5 s after settling: PSS and RSS of the main process and of the whole tree.
# Run 1 starts with an empty QML cache ("cold"); later runs reuse it ("warm").
# Output: one TSV line per run on stdout.
set -u
BUNDLE=$1; LABEL=$2; RUNS=${3:-6}; shift 3 || shift $#
DISP=:101; XVFB=${XVFB:-Xvfb}
ROOT=${BENCH_DIR:-$HOME/basecamp-bench}/runs/$LABEL; TEMPLATE=${TEMPLATE:-${BENCH_DIR:-$HOME/basecamp-bench}/template-empty}
mkdir -p "$ROOT"

[ -S /tmp/.X11-unix/X101 ] || { setsid nohup $XVFB $DISP -screen 0 1920x1080x24 >/dev/null 2>&1 </dev/null & sleep 2; }

tree() {  # every process of this run (they all inherit LOGOS_USER_DIR)
  local p; for p in $(pgrep -u "$(id -u)"); do
    grep -qz "LOGOS_USER_DIR=$UD" /proc/$p/environ 2>/dev/null && echo "$p"; done
}
cpu_ticks() { local s=0; for p in $(tree "$1"); do
  read -r a < /proc/$p/stat 2>/dev/null || continue
  a=${a#*) }; set -- $a; s=$((s + ${12} + ${13})); done; echo $s; }
mem_kb() {  # field (Pss|Rss) summed over pids
  local f=$1; shift; local s=0
  for p in "$@"; do v=$(awk -v f="$f:" '$1==f{print $2}' /proc/$p/smaps_rollup 2>/dev/null); s=$((s + ${v:-0})); done; echo $s; }
now() { date +%s.%N; }
stop_run() {  # only processes started from this run's user dir
  for p in $(pgrep -f 'LogosBasecamp|logos_host|ui-host'); do
    grep -qz "LOGOS_USER_DIR=$1" /proc/$p/environ 2>/dev/null && kill -9 "$p" 2>/dev/null; done; }

HZ=$(getconf CLK_TCK); NCPU=$(nproc)
for i in $(seq 1 "$RUNS"); do
  UD=$ROOT/user; rm -rf "$UD"; cp -a "$TEMPLATE" "$UD"
  CACHE=$ROOT/cache; [ "$i" = 1 ] && rm -rf "$CACHE"; mkdir -p "$CACHE" "$ROOT/config"
  t0=$(now)
  env -u WAYLAND_DISPLAY DISPLAY=$DISP QT_QPA_PLATFORM=xcb LOGOS_USER_DIR="$UD" \
      XDG_CONFIG_HOME="$ROOT/config" XDG_CACHE_HOME="$CACHE" XDG_RUNTIME_DIR=/run/user/$(id -u) \
      LOGOS_INSTANCE_ID=b3c4$(printf '%08x' "$i") "$@" \
      setsid "$BUNDLE/bin/LogosBasecamp" --user-dir "$UD" >"$ROOT/run$i.log" 2>&1 </dev/null &
  PID=$!; disown
  tw=""; while [ -z "$tw" ]; do
    DISPLAY=$DISP xdotool search --onlyvisible --pid "$PID" >/dev/null 2>&1 && tw=$(echo "$(now) - $t0" | bc)
    kill -0 $PID 2>/dev/null || { echo "run $i: exited early" >&2; break; }
    awk "BEGIN{exit !($(now) - $t0 > 120)}" && { tw=timeout; break; }
    sleep 0.05
  done
  quiet=0; prev=$(cpu_ticks $PID); ts=""
  while [ -z "$ts" ]; do
    sleep 1; cur=$(cpu_ticks $PID); pct=$(( (cur - prev) * 100 / HZ ))   # % of one core
    prev=$cur; [ "$pct" -lt 3 ] && quiet=$((quiet + 1)) || quiet=0
    [ $quiet -ge 3 ] && ts=$(echo "$(now) - $t0 - 3" | bc)
    awk "BEGIN{exit !($(now) - $t0 > 180)}" && ts=timeout
  done
  sleep 5
  PIDS=$(tree $PID)
  printf '%s\t%d\t%s\t%s\t%d\t%d\t%d\t%d\t%d\n' "$LABEL" "$i" "$tw" "$ts" \
    "$(mem_kb Pss $PID)" "$(mem_kb Rss $PID)" "$(mem_kb Pss $PIDS)" "$(mem_kb Rss $PIDS)" "$(echo $PIDS | wc -w)"
  [ -n "${SHOT:-}" ] && [ "$i" = "$RUNS" ] && DISPLAY=$DISP import -window root "$ROOT/screen.png" 2>/dev/null
  stop_run "$UD"; sleep 2
done
