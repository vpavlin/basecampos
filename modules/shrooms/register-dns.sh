#!/bin/sh
PLACEHOLDER=PASTE-THE-NETWORK-KEY-HERE
# Point this host's systemd-resolved at the mesh resolver the daemon runs.
#
# Run by shrooms-resolved.service, which watches rather than fires once. See the
# comment in scripts/install.sh for why this is not done inside the container.
set -eu

# Beside the control socket: the unit mounts /run/shrooms into the container, so
# it is the one directory both sides can see. The daemon reads this file to know
# that registration happened after all, and reports names as working rather than
# warning about something that has already been dealt with.
MARKER=/run/shrooms/resolver-registered

# By path, not by PATH: this runs from a unit, and a service manager's idea of
# where to look for a binary is not the shell's.
SHROOMS=${SHROOMS:-/usr/local/bin/shrooms}
CONFIG=${CONFIG:-/etc/shrooms/config.toml}
INTERVAL=${INTERVAL:-30}

# What root is willing to hand to resolvectl, defined once. Both readers of the
# marker and the value freshly parsed out of the container go through it.
valid_iface() {
    case "$1" in
        ''|*[!A-Za-z0-9._-]*) return 1 ;;
    esac
}

# The interface this host was registered for, left in $iface. `read`, not
# `cat`: this is the most-executed line in the script, and a builtin costs no
# fork where a subshell and /bin/cat cost two.
marker_iface() {
    [ -f "$MARKER" ] || return 1
    read -r iface < "$MARKER" || return 1
    valid_iface "$iface"
}

revert() {
    # resolved forgets a link when its interface disappears, which is the usual
    # case — but a daemon stopped while the tun survives would leave the host
    # pointing at a resolver that has gone. The interface is read back from the
    # marker because by now there may be no container left to ask.
    if marker_iface; then
        rm -f "$MARKER"
        resolvectl revert "$iface" >/dev/null 2>&1 || true
    else
        rm -f "$MARKER"
    fi
}

# Already done, and still true. Cheap on purpose: no container is touched, so a
# tick of the watch loop costs one fork and one D-Bus call while everything is
# working — `case` rather than a pipe into grep, for the same reason.
registered() {
    marker_iface || return 1
    case $(resolvectl status "$iface" 2>/dev/null) in
        *"DNS Servers"*) ;;
        *) return 1 ;;
    esac
}

# Is there a mesh to register names for? Read off the config rather than asked
# of the daemon, so a machine sitting prepared for a week — waiting for an
# invite that has not arrived — is not running `podman exec` twice a minute to
# be told the same thing.
has_mesh() {
    [ -f "$CONFIG" ] || return 1
    ! grep -q "$PLACEHOLDER" "$CONFIG"
}

# One message per reason, not one per attempt. In watch mode the same complaint
# every 30s buries the journal and teaches everyone to ignore this unit.
last=
say() {
    [ "$1" = "$last" ] && return 0
    last=$1
    echo "$1" >&2
}

register() {
    # Compacted as it arrives, so one spelling of the document is in scope
    # rather than two. `shrooms status --json` pretty-prints, so it says
    # `"dns": {` and `"serving": true` — with spaces. Matching the compact
    # spelling found nothing, and since ${var#pattern} returns the string
    # unchanged when the pattern is absent, that failure came out of this
    # script as "the daemon is not serving names" on a machine whose names were
    # fine.
    #
    # Safe to strip blindly because the three values read below cannot contain
    # whitespace, and each is validated before it is used.
    json=$("$SHROOMS" status --json 2>/dev/null | tr -d ' \t\n') || json=
    if [ -z "$json" ]; then
        say "the daemon is not answering yet"
        return 1
    fi

    case "$json" in
        *'"dns":{'*) ;;
        *) say "no dns block in \`shrooms status --json\`; cannot register"; return 1 ;;
    esac

    # The dns object holds only scalars, so the first closing brace ends it and
    # nothing here needs a JSON parser that the host may not have.
    dns=${json#*\"dns\":\{}
    dns=${dns%%\}*}
    case "$dns" in
        *'"serving":true'*) ;;
        *) say "the daemon is not serving names yet"; return 1 ;;
    esac

    # Parameter expansion throughout, and no forked sed or grep: `#` strips the
    # shortest leading match, so each of these takes the FIRST occurrence.
    #
    # The interface is read out of the meshes array rather than the whole
    # document, so it is the primary mesh's — the one the daemon registers —
    # because it is first in that array, not because of where Go happens to
    # declare the field. Taking it from the document meant any earlier key
    # named "interface" would have pointed root's resolvectl at another link.
    addr=${dns#*\"address\":\"};   addr=${addr%%\"*}
    suffix=${dns#*\"suffix\":\"};  suffix=${suffix%%\"*}
    meshes=${json#*\"meshes\":\[}
    iface=${meshes#*\"interface\":\"}; iface=${iface%%\"*}

    # Validated before being handed to resolvectl, because this runs as root and
    # these three values came out of a container. Rejecting is the right
    # failure: names not working is recoverable, and root running an arbitrary
    # string is not.
    valid_iface "$iface" || { say "refusing odd interface: $iface"; return 1; }
    case "$addr"   in ''|*[!0-9A-Fa-f:.]*)   say "refusing odd address: $addr";    return 1 ;; esac
    case "$suffix" in ''|*[!A-Za-z0-9.-]*)   suffix=internal ;; esac

    # Both suffixes, because the resolver answers both: the configured one and
    # the legacy `.mesh`, kept answerable so a change of default does not break
    # every ssh config on the same day. Registering one leaves the other dead.
    set -- "~$suffix"
    [ "$suffix" = mesh ] || set -- "$@" "~mesh"

    resolvectl dns "$iface" "$addr"
    resolvectl domain "$iface" "$@"

    printf '%s\n' "$iface" > "$MARKER"
    last=
    echo "mesh names resolve here: $iface -> $addr, domains $*"
}

# Registration is not a thing that happens once.
#
# `shrooms join` hands the token to the waiting daemon, which re-executes itself
# into the mesh it just joined — same pid, and systemd sees no exit. So nothing
# restarts this unit at the moment names first become answerable, and a machine
# set up with `prepare` (every device after the first) ended up with a working
# mesh and no names, which is where this started. Reconciling on a loop covers
# that, a later in-place restart, and resolved forgetting a link, none of which
# announce themselves.
# How long until the next look. INTERVAL while things are moving, doubling up
# to a ceiling while they are not — because "cannot register" is a state a
# machine can sit in permanently. A daemon that failed to bind port 53 serves
# no names for its whole life, and retrying that every thirty seconds is two
# `podman exec`s a minute, forever, to be told the same thing. Reset on
# success, so a machine that recovers is prompt again.
wait_for=$INTERVAL
MAX_WAIT=${MAX_WAIT:-600}

backoff() {
    wait_for=$((wait_for * 2))
    [ $wait_for -gt $MAX_WAIT ] && wait_for=$MAX_WAIT
    return 0
}

reconcile() {
    if registered; then
        wait_for=$INTERVAL
        return 0
    fi
    # No mesh yet is not a failure and does not back off: the config gate costs
    # one grep and never wakes the container, and the moment an invite lands we
    # want to be looking.
    has_mesh || {
        say "no mesh in $CONFIG yet; nothing to register"
        wait_for=$INTERVAL
        return 0
    }
    if register; then
        wait_for=$INTERVAL
    else
        backoff
    fi
}

command -v resolvectl >/dev/null 2>&1 || exit 0

case "${1:-}" in
    --revert)
        revert
        ;;
    --watch)
        while :; do
            reconcile
            sleep "$wait_for"
        done
        ;;
    *)
        # One pass, for running it by hand. The unit uses --watch; this is what
        # you want when you are standing in front of a machine asking why names
        # are not resolving, and it is what the comments above describe.
        reconcile
        ;;
esac
