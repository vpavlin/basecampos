#!/usr/bin/env bash
# Copies the shared QML (common/) into every stub app, renamed per app, and
# with --check fails if any copy has drifted.
#
# Why copies: an installed app is one folder under <profile>/plugins/, and the
# plugin sandbox refuses QML imports from outside it, so each app carries its
# own copy of the shared files.
#
# Why renamed (common/StubApp.qml -> objects_wallet/qml/WalletStubApp.qml):
# observed 2026-10-24 on the shell-roles build, two ui_qml apps that each ship
# a sibling file with the same name collide: the second app to load resolves
# `StubApp` to the FIRST app's file, the sandbox blocks it as outside its
# tree, and the view fails with "Type StubApp unavailable". A unique type name
# per app sidesteps it.
#
#   ./sync.sh                       copy common/*.qml into objects_*/qml/
#   ./sync.sh --check               exit 1 if a copy differs from common/
#   ./sync.sh --install <profile>   sync, then copy every app into <profile>/plugins/
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
declare -A prefix=(
    [objects_wallet]=Wallet [objects_chat]=Chat [objects_communities]=Communities
    [objects_news]=News [objects_explorer]=Explorer
)
apps=(objects_wallet objects_chat objects_communities objects_news objects_explorer)

render() {   # render <app> <common file> -> the app's copy on stdout
    local p="${prefix[$1]}"
    sed -e "s/\bStubApp\b/${p}StubApp/g" -e "s/\bObjectPublisher\b/${p}ObjectPublisher/g" "$2"
}

if [[ "${1:-}" == "--check" ]]; then
    rc=0
    for a in "${apps[@]}"; do
        for f in "$here"/common/*.qml; do
            out="$here/$a/qml/${prefix[$a]}$(basename "$f")"
            if ! render "$a" "$f" | cmp -s - "$out"; then
                echo "drift: ${out#$here/}"; rc=1
            fi
        done
    done
    exit $rc
fi

for a in "${apps[@]}"; do
    for f in "$here"/common/*.qml; do
        render "$a" "$f" > "$here/$a/qml/${prefix[$a]}$(basename "$f")"
    done
done

if [[ "${1:-}" == "--install" ]]; then
    profile="${2:?usage: sync.sh --install <profile dir>}"
    mkdir -p "$profile/plugins"
    for a in "${apps[@]}"; do
        rm -rf "${profile:?}/plugins/$a"
        cp -a "$here/$a" "$profile/plugins/$a"
        echo "installed $profile/plugins/$a"
    done
fi
