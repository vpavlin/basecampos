#!/usr/bin/env bash
# Wait for the arm64 workflow run of a commit (default: HEAD) and print its result.
sha=${1:-$(git -C "$(dirname "$0")/.." rev-parse HEAD)}
while true; do
  r=$(curl -s "https://api.github.com/repos/vpavlin/basecampos/actions/runs?head_sha=$sha&per_page=1" |
      python3 -c 'import json,sys;r=(json.load(sys.stdin).get("workflow_runs") or [{}])[0];print(r.get("id"),r.get("status"),r.get("conclusion"))')
  case "$r" in *" completed "*) echo "$sha $r"; exit 0 ;; esac
  sleep 600
done
