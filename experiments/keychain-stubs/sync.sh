#!/usr/bin/env bash
# Writes each Keychain stub's crypto file from common/, and with --check fails
# if a copy has drifted or the vendored TweetNaCl is not the recorded one.
#
#   common/KeychainCrypto.js  the stubs' helpers, with a marker line
#   common/nacl-fast.js       TweetNaCl-js 1.0.3, verbatim (README: source, sha256)
#
# Each app gets <Prefix>Crypto.js = KeychainCrypto.js with nacl-fast.js put in
# place of the marker. Copies, because the plugin sandbox refuses imports from
# outside an app's own folder; renamed, because two ui_qml apps shipping a file
# of the same name collide in the host (see ../objects-stubs/sync.sh).
#
#   ./sync.sh                       write keychain_*/qml/<Prefix>Crypto.js
#   ./sync.sh --check [bc-apps]     exit 1 on drift; with a Basecamp checkout
#                                   also compare its test fixture
#   ./sync.sh --install <profile>   sync, then copy both apps into <profile>/plugins/
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
declare -A prefix=([keychain_signer]=Signer [keychain_requester]=Requester)
apps=(keychain_signer keychain_requester)
nacl_sha256=6bcd37a3b20dce913f82d4b23e4e2b661058b4b953df8a3f8c45d56ac4f72447
marker='// @@TWEETNACL@@'
fixture=tests/fixtures/keychain/SignerCrypto.js   # in Basecamp (bc-apps)

render() {   # render <app> -> the app's crypto file on stdout
    local p="${prefix[$1]}"
    while IFS= read -r line || [[ -n "$line" ]]; do
        if [[ "$line" == "$marker" ]]; then
            echo "// ---- TweetNaCl-js 1.0.3 nacl-fast.js, verbatim (sha256 $nacl_sha256) ----"
            cat "$here/common/nacl-fast.js"
            echo "// ---- end of TweetNaCl-js ----"
        else
            printf '%s\n' "${line//KeychainCrypto.js/${p}Crypto.js}"
        fi
    done < "$here/common/KeychainCrypto.js"
}

check_nacl() {
    local got
    got="$(sha256sum "$here/common/nacl-fast.js" | cut -d' ' -f1)"
    if [[ "$got" != "$nacl_sha256" ]]; then
        echo "common/nacl-fast.js is not TweetNaCl-js 1.0.3 (sha256 $got)"; return 1
    fi
}

if [[ "${1:-}" == "--check" ]]; then
    rc=0
    check_nacl || rc=1
    for a in "${apps[@]}"; do
        out="$here/$a/qml/${prefix[$a]}Crypto.js"
        if ! render "$a" | cmp -s - "$out"; then echo "drift: ${out#$here/}"; rc=1; fi
    done
    if [[ -n "${2:-}" ]]; then
        if ! render keychain_signer | cmp -s - "$2/$fixture"; then echo "drift: $2/$fixture"; rc=1; fi
    fi
    exit $rc
fi

check_nacl
for a in "${apps[@]}"; do
    render "$a" > "$here/$a/qml/${prefix[$a]}Crypto.js"
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
