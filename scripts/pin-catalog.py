#!/usr/bin/env python3
"""Pin catalogue apps for the image: resolve dependencies, write catalog/apps.json.

    scripts/pin-catalog.py chat_ui blockchain_ui accounts_ui storage_ui shrooms

Reads the official Logos catalogue (the index Basecamp's package downloader
uses) and then the extra repositories in REPOS (a name found in an earlier one
wins), takes the newest version of each named package and of everything it
depends on, and records url + sha256 in dependency order, so the flake can
fetch them as fixed-output derivations and lgpm can install them in order.
Modules the Basecamp bundle already ships are left out.
"""
import json
import sys
import urllib.request

REPOS = [
    "https://raw.githubusercontent.com/logos-co/logos-modules-release/refs/heads/main/logos-repo.json",
    "https://apps.vpavlin.xyz/logos-repo.json",
]
BUNDLED = {"capability_module", "modules_state", "package_manager", "package_downloader",
           "storage_module", "package_manager_ui"}


def newest(pkg):
    # Versions are listed newest first; keep only ones with a linux-amd64 variant
    # (pure-QML apps have an empty "main" but still list their variants).
    for v in pkg["versions"]:
        m = v["manifest"]
        if "linux-amd64" in m.get("main", {}) or "variants/linux-amd64" in m.get("hashes", {}):
            return v
    return None


def fetch(url):
    with urllib.request.urlopen(url) as r:
        return json.load(r)


def main(names):
    index = {}
    for repo in REPOS:
        meta = fetch(repo)
        for p in fetch(meta["indexUrl"])["packages"]:
            index.setdefault(p["name"], dict(p, repo=meta["name"]))
    order, seen = [], set()

    def visit(name, chain):
        if name in seen or name in BUNDLED:
            return
        if name in chain:
            raise SystemExit(f"dependency cycle: {' -> '.join(chain + [name])}")
        if name not in index:
            raise SystemExit(f"{name}: not in the catalogue (needed by {chain[-1] if chain else 'you'})")
        v = newest(index[name])
        if not v:
            raise SystemExit(f"{name}: no linux-amd64 build")
        for dep in v["manifest"].get("dependencies", []):
            visit(dep["name"] if isinstance(dep, dict) else dep, chain + [name])
        seen.add(name)
        order.append({
            "name": name,
            "version": v["manifest"]["version"],
            "type": v["manifest"].get("type"),
            "repo": index[name]["repo"],
            "url": v["url"],
            "sha256": v["sha256"],
        })

    for n in names:
        visit(n, [])
    json.dump({"repositories": REPOS, "requested": names, "packages": order},
              open("catalog/apps.json", "w"), indent=2)
    for p in order:
        print(f"{p['name']:28} {p['version']:10} {p['type']:8} {p['repo']}")


if __name__ == "__main__":
    main(sys.argv[1:])
