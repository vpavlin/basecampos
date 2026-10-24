#!/bin/sh
RUNTIME=/usr/sbin/podman
IMAGE=ghcr.io/vpavlin/shrooms:latest
Z=:z
# Thin wrapper, with two paths — because one of them must not run inside the
# daemon container.
#
# Anything that touches the mesh authority — `init`, `invite`, `admin`,
# `keycard`, `mesh rename` — reads or writes ~/.config/shrooms, and the daemon
# container has no such directory: the unit mounts the config, the state and the
# socket, and nothing of yours. `exec` cannot add a mount, so those commands run
# in a SIBLING container with the same mounts plus the invoking user's admin
# directory. Without it `shrooms init` wrote the only key that can ever admit a
# device into a --rm container, where the next restart destroyed it, and
# `shrooms invite` reported a missing admin key on the machine that holds it.
#
# The daemon container is deliberately NOT given that mount. It has never needed
# the admin key — membership is a credential it verifies, not one it signs — and
# a process that cannot read a key cannot leak it (ADR-025).
needs_admin_key=no
case "${1:-}" in
    init|invite|admin|keycard) needs_admin_key=yes ;;
    # This subcommand only: renaming a mesh MOVES the admin key file with it.
    mesh) [ "${2:-}" = rename ] && needs_admin_key=yes ;;
esac

# Can this user see the daemon's container at all?
#
# Asked before anything else, because the honest answer is not "it is not
# running". The daemon's container belongs to root, and rootless podman is a
# separate store that cannot see root's containers — so on a Fedora box, where
# podman is what `docker` runs, every command here needs sudo and the old
# message for that was "the shrooms container is not running", on a machine
# where systemctl says it plainly is.
#
# Root goes ahead either way. Anyone else only if the runtime can already see
# the container, which is what a working docker group looks like.
#
# One inspect answers both questions asked below: empty output means the runtime
# cannot see a container of that name at all, and anything else is its running
# state. Asking twice cost a second podman inspect — a few hundred milliseconds
# — on every command the wrapper exists to make convenient.
running=$("$RUNTIME" inspect -f '{{.State.Running}}' shrooms 2>/dev/null || true)

if [ "$(id -u)" != 0 ] && [ -z "$running" ]; then
    echo "cannot see the shrooms container as this user." >&2
    echo >&2
    echo "The daemon's container belongs to root. With podman yours is a separate" >&2
    echo "store that cannot see root's at all, so this needs sudo:" >&2
    echo "  sudo shrooms $*" >&2
    exit 1
fi

if [ "$needs_admin_key" = yes ]; then
    # SUDO_USER, not $HOME: this is normally run under sudo, so $HOME is root's
    # while the admin key belongs to the person. Resolved here and mounted at
    # /root/.config/shrooms, because inside the container there is no SUDO_USER
    # and that is where the CLI's own default lands.
    admin_home=$(getent passwd "${SUDO_USER:-$(id -un)}" 2>/dev/null | cut -d: -f6)
    admin_dir=${admin_home:-$HOME}/.config/shrooms
    mkdir -p "$admin_dir"
    [ -n "${SUDO_USER:-}" ] && chown "$SUDO_USER" "$admin_dir"

    # -t as well as -i when both ends are a terminal, so the passphrase prompts
    # read from a pty and do not echo. Omitted when either end is a pipe, where
    # it would put carriage returns through the output.
    TTY=
    [ -t 0 ] && [ -t 1 ] && TTY=-t

    # --uts=host rather than --hostname: `init` defaults the device name to the
    # hostname, and that has to be this machine's rather than a container id.
    # --hostname is refused alongside host networking, which the node needs.
    #
    # A card reader too, when pcscd is running here: `keycard` and the --keycard
    # admin commands talk to it over that socket and it is not in the image.
    CARD=
    [ -S /run/pcscd/pcscd.comm ] && CARD="-v /run/pcscd:/run/pcscd$Z"

    "$RUNTIME" run --rm -i $TTY \
        --network host \
        --uts host \
        -v /etc/shrooms:/etc/shrooms$Z \
        -v /var/lib/shrooms:/var/lib/shrooms$Z \
        -v /run/shrooms:/run/shrooms$Z \
        -v "$admin_dir:/root/.config/shrooms$Z" \
        $CARD \
        "$IMAGE" "$@"
    rc=$?
    # What was just minted belongs to whoever ran sudo, not to root.
    if [ -n "${SUDO_USER:-}" ]; then chown -R "$SUDO_USER" "$admin_dir" 2>/dev/null || true; fi
    exit $rc
fi

# Everything else runs in the daemon container, which is where the mesh is. That
# one has to be up, and by here we know the answer means what it says: a user who
# could not see it at all was turned away above. The admin-key commands are not
# held to this — minting a mesh, or reading `admin show`, is worth having on a
# machine whose daemon is down.
if [ "$running" != true ]; then
    echo "the shrooms container is not running" >&2
    echo "  sudo systemctl status shrooms" >&2
    exit 1
fi
# -i so the commands that prompt work through the wrapper. `shrooms join` and
# `shrooms key rotate` both read from a terminal, and without this they get EOF
# and fail in a way that reads as a broken install rather than a missing flag.
exec "$RUNTIME" exec -i shrooms shrooms "$@"
