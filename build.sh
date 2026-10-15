#!/usr/bin/env bash
# Build Basecamp 0.3.2 + theming against the design system in this repo.
# Usage: ./build.sh [extra nix build args]   → ./result/bin/LogosBasecamp
set -euo pipefail
cd "$(dirname "$0")"
nix build "path:$PWD/basecamp#default" \
  --override-input logos-design-system "path:$PWD/design-system" \
  -o result "$@"
echo "Built: $(readlink -f result)"
