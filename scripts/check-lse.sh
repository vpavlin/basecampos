#!/usr/bin/env bash
# check-lse.sh <nix path> — list ELF files in its closure that use ARMv8.1 LSE
# atomics outside the __aarch64_* outline helpers. The Duet's MT8183
# (Cortex-A73/A53, ARMv8.0) has no LSE: such code dies with SIGILL there.
set -u
paths=$(nix path-info -r "$1")
total=0 bad=0
re='^(swp|cas|ld(add|clr|eor|set|smax|smin|umax|umin)|st(add|clr|eor|set|smax|smin|umax|umin))(a|l|al)?(b|h)?$|^casp(a|l|al)?$'
for p in $paths; do
  find "$p" -type f \( -perm -u+x -o -name '*.so*' \) -size +1k 2>/dev/null | while read -r f; do
    head -c4 "$f" 2>/dev/null | grep -q $'\x7fELF' || continue
    n=$(objdump -d --no-show-raw-insn "$f" 2>/dev/null | awk -v re="$re" '
      /^[0-9a-f]+ <.*>:$/ { fn = $2; next }
      $2 ~ re && fn !~ /^<__aarch64_/ { c++ }
      END { print c + 0 }')
    if [ "$n" -gt 0 ]; then echo "LSE $n $f"; fi
  done
done | tee lse-report.txt
count=$(wc -l < lse-report.txt)
echo "files with LSE atomics outside __aarch64_ helpers: $count"
