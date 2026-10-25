#!/usr/bin/env bash
# check-lse.sh <nix path> — list ELF files in its closure that use ARMv8.1 LSE
# atomics outside the __aarch64_* outline helpers. The Duet's MT8183
# (Cortex-A73/A53, ARMv8.0) has no LSE: such code dies with SIGILL there.
# Files over 100 MB (browser engines) are listed as not scanned.
set -u
export RE='^(swp|cas|ld(add|clr|eor|set|smax|smin|umax|umin)|st(add|clr|eor|set|smax|smin|umax|umin))(a|l|al)?(b|h)?$|^casp(a|l|al)?$'
scan() {
  f=$1
  head -c4 "$f" 2>/dev/null | grep -q $'\x7fELF' || return 0
  n=$(objdump -d --no-show-raw-insn "$f" 2>/dev/null | awk -v re="$RE" '
    /^[0-9a-f]+ <.*>:$/ { fn = $2; next }
    $2 ~ re && fn !~ /^<__aarch64_/ { c++ }
    END { print c + 0 }')
  [ "$n" -gt 0 ] && echo "LSE $n $f"
  return 0
}
export -f scan
nix path-info -r "$1" | while read -r p; do
  find "$p" -type f \( -perm -u+x -o -name '*.so*' \) -size +1k -size -100M 2>/dev/null
done | xargs -r -P 4 -I{} bash -c 'scan "$1"' _ {} > lse-report.txt
nix path-info -r "$1" | while read -r p; do
  find "$p" -type f -size +100M 2>/dev/null | sed 's/^/NOT-SCANNED (>100M) /'
done >> lse-report.txt
echo "files with LSE atomics outside __aarch64_ helpers: $(grep -c '^LSE' lse-report.txt)"
