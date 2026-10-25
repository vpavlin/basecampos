# BasecampOS — analysis & plan (draft, 2026-10-24)

## Status (2026-10-24)

Done and verified in `basecampos-vm` on the dev host (KVM + virgl):
- Boots straight into Basecamp's **desktop layout**: Alisher's `basecamp-os-theme` fork, *portable* bundle,
  autologin → cage → XWayland. Wallpaper, glass, dock and Spotlight hint all render (`docs/p1-desktop.png`).
- **System app** (`apps/system`): a `basecampos_system` core module plus a themed `System` view. It shows
  machine info, network/Wi-Fi state, volume and power. *Restart* really reboots through logind
  (`docs/p1-system-app.png`).
- Bundled `.lgx` apps are installed into the user's Basecamp at boot with the portable `lgpm`.
- **shrooms**: NixOS module (daemon container, DNS, agent user unit). The VM is on its own mesh `basecampos`
  as `basecampos-vm` (Edge) and survives reboots with its identity; the agent serves on the mesh.
- `basecampos-live` ISO builds (~2.7 GB). Boot test pending.
- Tooling: `scripts/vm-{run,stop,restart,ssh,shot}.sh`, `scripts/ui.sh` (QML inspector over ssh),
  `scripts/vm-try-env.sh` (A/B shell environment).

Findings to share back (Alisher / Basecamp team):
1. **Native Wayland + desktop layout:** the raster-painted `DesktopRoot` (wallpaper) intermittently composes
   as black beneath the QQuickWidgets (dock/glass), in a wlroots kiosk (cage) with virgl. It's not a missed
   repaint (F11 doesn't fix it) and not state (it happens on a fresh profile too). `QT_QUICK_BACKEND=software`
   shows the wallpaper but loses the Quick content. `QT_QPA_PLATFORM=xcb` renders correctly. We ship xcb for now.
2. **Nix-store builds can't install catalogue apps:** the catalogue ships portable variants only, and the nix
   `lgpm`/Basecamp want `<platform>-dev`. `lgpm --platform` doesn't help, because the `-dev` suffix is
   compiled in (`LGPM_PORTABLE_BUILD`). A distro has to ship the portable bundle + nix-ld.
3. **Portable bundles on NixOS** need nix-ld with: glibc, libstdc++, alsa, libdrm, gbm, glvnd/mesa,
   fontconfig, freetype, fribidi, harfbuzz, vulkan-loader, wayland, X11/xcb.
4. A `ui_qml` app's core dependency must be passed to the builder as a flake input (LIDL contract), or the
   build fails ("dependencies that publish no LIDL contract").

5. **`shell-roles` + desktop layout:** that branch builds and boots in BasecampOS, but the desktop dock still
   shows its built-in system group and *also* lists the new role apps (`basecamp_apps`, `basecamp_home`,
   `basecamp_settings`, `package_manager_ui`) as installed apps, so each system app shows up twice. The dock
   should open roles, not list role holders. We stay on `basecamp-os-theme` until the dock is role-aware.

**arm64 builds:** native only; no qemu-user emulation on the dev host (it ran the dev host out of memory on
2026-10-24). Options: a GitHub Actions `ubuntu-24.04-arm` runner (needs this repo published, Václav's
call) or an arm64 builder from Václav. the dev host rules: VMs ≤4 GB, nix max-jobs 2 / cores 4.

**Direction (2026-10-24): Basecamp is the only UI.** `session = "sway"`: a bar-less Sway runs Basecamp full
screen (desktop layout) on workspace 1; native apps (Brave, foot) start from Basecamp's dock through the
System core module and live alone on their own workspaces; Super+Esc / 3-finger swipe returns. The System
app absorbs the tablet panels (display, Bluetooth, battery, rotation, on-screen keyboard, mirroring). On the
Duet, duet-kit supplies hardware fixes and tablet daemons only. arm64 builds run on GitHub Actions.

**2026-10-25: BasecampOS boots on the Lenovo Duet (arm64, real hardware).** Our kernel (hexdump0815's stb-cbm
6.12.50 config + patches, built on GitHub's arm runners), no initrd, root on a btrfs subvolume of the tablet's
/home partition, signed and booted once from the spare kernel slot by the Duet agent; all checks passed (touch,
Wi-Fi device, keyboard cover, modules, no failed units, Basecamp at ~300 MB / 4.5% CPU), and the 20-minute test
window returned it to Debian. Fixed after it: zram in the kernel, default backlight, root dirs in the tarball,
the working cmdline documented. Still untested there: Wi-Fi connect, UI touch gestures, Brave, suspend, rotation,
audio, cameras (no drivers yet).

**2026-10-25, run 6: an agent session runs inside BasecampOS on the Duet** (the Duet agent's Debian shrooms
daemon + agent in a chroot, so the mesh identity and Claude login stay in place). Verified hands-on: dock → foot,
Shrooms Agents lists the fleet, System app (power, sound, brightness, rotation lock, OSK, Bluetooth), Wi-Fi
connected, sound card + ath10k present, 1.3 GiB used, Basecamp ~20% CPU. Lessons for a native in-OS agent
(not built; needs BasecampOS to mount Debian's root and /home, safe only because the two never run together):
the daemon needs iproute2 on PATH; never share /run/nscd with a Debian chroot (NSS then resolves NixOS users and
Debian sudo fails). Open: default volume came up at 0%.

**2026-10-25: native apps INSIDE Basecamp windows** (`apps/external-embedded`, launchers "Terminal (in
Basecamp)" and "Browser (in Basecamp)"). The app view is a Wayland compositor (QtWayland.Compositor ships in
the bundle and the QML sandbox allows it) on a private socket `basecamp-<id>-<n>`; `basecampos_system.openEmbedded`
starts the app's `embedCommand` (from `/etc/basecampos/embedded-apps`) on it. Verified in the VM: qterminal
takes typing and runs commands, Brave browses. Lessons:
- The WaylandOutput must be bound to the QQuickWindow the view is drawn in, and that window changes after
  creation: create the output per window (an output with a stale window stops frame callbacks: one frame, then
  nothing). A compositor finding its socket locked qFatal()s all of Basecamp: unique socket per instance.
- Offer xdg-decoration (server side): client-drawn title bars grow their shm pool while Qt holds the old one
  and libwayland rejects the next buffer (protocol error, app dies).
- Qt 6.9's compositor: wl_seat v4 / data-device v1 (foot will not start); keys go to ONE wl_keyboard per client
  (winit apps such as alacritty bind twice and never get keys). Qt/GTK clients are fine. No EGL for clients:
  Brave needs `--disable-gpu` (software; ~45 s first start in the VM) and its own profile.
- Testing: wtype and QEMU sendkey do not reach Basecamp (XWayland); xdotool on :0 and `swaymsg seat ... cursor` do.
Open: clipboard between Basecamp and embedded apps, popups/menus placement, GPU (would need Basecamp's
compositor to offer linux-dmabuf), performance on the Duet.

Next:
- ~~Boot-test the ISO~~ (done: boots under UEFI into Basecamp + System); then an installer path (disk image + `nixos-install` flow).
- Try Alisher's `shell-roles` branch (Settings/Packages/Applications as role apps). System could become
  part of the settings role instead of a separate dock app. `notifications` adds Linux desktop
  notifications, which is relevant once there's a compositor with a notification daemon.
- Preinstall a curated set of catalogue apps (wallet, chat, blockchain node).
- VM agent session: needs a Claude Code login inside the VM (owner step).
- P2: stacking compositor + real toplevels; per-module OS sandboxing.

## What exists today

**logos-co/logos-basecamp** (`src/logos-basecamp`, HEAD 572f722, 2026-10-23)
- Qt 6 Widgets + QML desktop app, built entirely with Nix flakes (cache: `cache.nix.logos.co`).
  Outputs: `app` (nix-store build), `bin-bundle-dir`, `bin-appimage`, `app-mock`, `shell-preview`.
- Process model:
  - `LogosBasecamp` = shell. Its UI is a plugin, `main_ui.so`, behind a narrow `IShellView`/`IShellHost`
    ABI; `symbol-gate.nix` keeps the shell from linking the Logos runtime.
  - **Logos Modules** (package_manager, capability, storage, waku, blockchain…) each run in their own
    `logos_host` subprocess via `liblogos_core`, token-authenticated.
  - **UI apps**: QML in-process inside a sandboxed QML engine (deny-all network, fs-restricted, no native
    plugins); C++ backends run out-of-process in `ui-host`.
  - Packages are `.lgx` (portable / dev variants), installed by the package_manager module into the user
    data dir (`~/.local/share/Logos/LogosBasecamp/{modules,plugins,lib}`).
- Isolation is **in-app only** — no OS-level sandbox for `logos_host`/`ui-host` processes.

**xAlisher/basecamp-theming** (`basecamp-os-theme` branch, `src/basecamp-theming-os`) — forks of
Basecamp 0.3.2 + logos-design-system, ~30 commits
- Data-only theme engine (`.theme.json`: palette, radii, fonts; v2: gradients, backgrounds, icon packs,
  per-control styles, density). Themes: Dark, Light, Aurora, Catppuccin, Hacker, Paper Serif.
- **Desktop layout** (`BASECAMP_LAYOUT=desktop`): full screen, wallpaper, frosted-glass status bar, dock,
  every app in its own movable/resizable window, tiling toggle, Ctrl+K Spotlight. Settings/Package
  Manager/Applications become apps.
- Implemented as a `QMdiArea` inside one top-level window (`src/DesktopArea.cpp`, ~1.1k lines + QML dock).
  The windows are **not real OS windows**.
- `docs/BASECAMP-OS-SPEC.md`: the plan is to ship it as a built-in *layout* first, and later as a signed,
  replaceable `ui_shell` package (needs signature checks, safe mode, `hostAbiVersion` fallback).
- Test builds: AppImage (x86_64) + macOS zip on the repo's releases page.

**Conclusion:** Basecamp already *is* a desktop shell; Alisher's layout gives it the look. The distro work
is mostly in session/boot integration, a runtime that is system-provided instead of an AppImage, OS-level
isolation and updates. Nix-everything makes **NixOS the natural base**.

## Architecture proposal

```
 boot (systemd-boot, A/B via NixOS generations)
  └─ greetd (autologin "basecamp" user, or a Basecamp-styled greeter later)
      └─ Wayland compositor
           Phase 1: cage (single-app kiosk)      → LogosBasecamp --layout desktop, fullscreen
           Phase 2: labwc/wayfire + LayerShellQt → Basecamp dock/status bar as layer-shell,
                                                    apps (and foreign Linux apps) as real toplevels
      └─ systemd --user units:  logos-blockchain-node, (later) per-module logos_host under
                                 systemd sandboxing (ProtectSystem, PrivateNetwork per manifest, …)
```

Base OS pieces (NixOS module `services.basecamp`):
- Basecamp from the logos-basecamp flake (or Alisher's fork until the layout lands upstream), built
  against the system Nix store — use the **dev/`app` variant** with modules preinstalled in the store,
  plus a writable user modules dir for packages installed at runtime (portable `.lgx`).
- Logos node (`logos-blockchain-node`) as a system service with its own user and data volume.
- PipeWire, NetworkManager (with a Basecamp settings panel later), fonts (Inter, JetBrains Mono for the
  Hacker theme), xdg-desktop-portal, `logos://` URL handler registered system-wide.
- Default theme + Glowing Tents wallpaper (licence to confirm — spec open question 3).
- Images: `nixos-generators` → qcow2 (dev VM), ISO (installer/live), raw-efi (USB/bare metal);
  later aarch64 (Pi 5 / Apple-silicon VMs).

## Phases

**P0 — Dev loop in a VM (days)**
1. Flake `basecampos` with `nixosConfigurations.vm` + `packages.x86_64-linux.{vm,iso,qcow2}`.
2. Package Alisher's `basecamp-os-theme` build (their `build.sh` → flake output) and run it under
   `cage` with `BASECAMP_LAYOUT=desktop` on autologin.
3. Boot with `nix run .#vm` on the dev host (KVM, virtio-gpu/virgl), serial console + ssh for agents.
4. Bake `logos-qt-mcp` into dev images so agents can drive the UI headlessly (tests already use it).

**P1 — Usable live image (1–2 weeks)**
- ISO that boots to Basecamp; preinstalled core modules + a curated app set (wallet, chat, docs, node).
- Logos node as a service; first-boot onboarding = Basecamp's onboarding (accounts/keys).
- Shell gaps an OS exposes: power menu (shutdown/reboot/suspend via logind D-Bus), Wi-Fi/network,
  display/audio volume, battery, lock screen, notifications. Ship these as a Basecamp system app
  (`basecampos_settings_ui` + a small privileged module talking D-Bus) rather than forking the shell.
- Escape hatch: Ctrl+Alt+T terminal / TTY for developers.

**P2 — Real desktop (weeks)**
- Move from cage to a stacking compositor; Basecamp's dock/status bar via LayerShellQt; tear
  `DesktopArea` windows out into real toplevels (spec §8 "real OS windows"); allow foreign apps
  (browser, terminal) as normal windows, shown in the same dock.
  Alternative to evaluate: Basecamp *as* the compositor (QtWayland Compositor) — more control, much
  more work; only if the layer-shell route can't give the right UX.
- OS-level isolation: run each `logos_host`/`ui-host` in a systemd scope or bwrap with per-module
  policy derived from the manifest — an advantage only an OS can offer over the AppImage.

**P3 — Distribution**
- Installer (calamares or a Basecamp-native one), atomic updates (NixOS generations / binary cache at
  `cache.nix.logos.co`, optionally signed updates distributed over Logos storage), rollback from the
  boot menu, Secure Boot (lanzaboote), full-disk encryption by default.
- `ui_shell` packages once Basecamp verifies signatures (spec §7) — BasecampOS then is the reference shell.

## Upstream asks (logos-basecamp)
1. Land the `desktop` layout (or at least the layout switch + `IShellHost` additions).
2. CLI flags: `--layout desktop`, `--fullscreen`, `--system-modules-dir <ro path>` (read-only store
   modules + writable user dir is exactly the NixOS split).
3. A stable, documented `IShellHost` ABI and a hook for system apps (power/network) to live in the dock.
4. Non-fatal `hostAbiVersion` mismatch + safe-mode start (a boot-to-black screen is worse in an OS than
   in an app).

## Risks / open questions
- Alisher's fork is on Basecamp 0.3.2 while upstream moves fast → rebase cost; coordinate with Alisher and
  the Basecamp team before we depend on it.
- GPU: Qt Quick + glass blur in VMs needs virgl or falls back to software rendering — test early.
- Portable `.lgx` from the catalogue vs Nix-store builds: confirm the package manager installs into a
  writable dir while the bundled modules are read-only.
- Who owns BasecampOS (Logos org repo vs personal experiment)? Wallpaper licence.

## VM access (agreed with the shrooms maintainers, 2026-10-24)
- QEMU/KVM on the dev host, **4–6 GB RAM cap**, user-mode NAT (no bridge/tap needed). The dev host reboots
  nightly at 03:15 (watchdog), so expect the VM to stop then; the disk must persist.
- The VM joins the **main mesh as its own device `basecampos-vm`**, mode `Edge`.
  - Enrolment: Václav runs `shrooms invite` (admin key with a passphrase); the VM runs `shrooms join <TOKEN>` once.
  - The image must contain **neither the token nor `/var/lib/shrooms`**. The identity is generated on first
    boot and lives on the VM's persistent disk. `/etc/shrooms/config.toml` = name + mode, no network key.
- New NixOS module `services.shrooms` (none exists yet):
  - daemon = `ghcr.io/vpavlin/shrooms:latest` via `virtualisation.oci-containers` (podman), host network,
    `NET_ADMIN`, `/dev/net/tun`, volumes `/etc/shrooms`, `/var/lib/shrooms`, `/run/shrooms`,
    cmd `daemon --socket /run/shrooms/shrooms.sock`; mirror the dev host's unit.
  - resolver oneshot: wait for `shrooms0`, `resolvectl dns shrooms0 <overlay>`, `resolvectl domain shrooms0 '~mesh'`.
  - `shrooms-agent`: `buildGoModule` (github.com/vpavlin/shrooms `./cmd/shrooms-agent`, CGO off) as a
    **user** unit with lingering, `--cages=false`, with claude-code (own login) / pi on PATH.
- The agent runs **inside** the VM and drives Basecamp locally via logos-qt-mcp; other agents
  (e.g. a reviewer agent) reach it over the mesh. Ports: agent 7387 (mesh only), metrics 9180, WG 51820/udp,
  delivery 60000/tcp.
- Security: every mesh member can drive the VM's agent, so **no secrets and no admin keys on the VM**;
  use throwaway Logos test accounts only.
