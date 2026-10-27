# BasecampOS changes to the Basecamp shell: notes for Alisher

BasecampOS (NixOS, the Lenovo Duet tablet first) runs your Basecamp: branch `shell-objects` of
xAlisher/basecamp-experiments. Our shell changes are commits on top of it, on branch
**`basecamp-shell`** of this repo:

    git fetch https://github.com/vpavlin/basecampos basecamp-shell
    git log --oneline <your shell-objects>..FETCH_HEAD
    git cherry-pick <the ones you want>

Diff: https://github.com/vpavlin/basecampos/compare/f46c60b...basecamp-shell

## Worth taking regardless of BasecampOS

- **`c0667f8` Tab switches no longer emit `launcherAppsChanged`.**
  - **Why:** `UIPluginManager::setCurrentVisibleApp` emitted it on every switch. Every view bound to `launcherApps` then recomputes every row (`buildAppRow`, with dependency checks per app) and rebuilds its delegates. On the Duet (MT8183) that made each tab switch take about 1 s.
  - **Safe to drop:** no row depends on the visible app.
  - **Timing log:** the commit also logs any switch step over 50 ms (`tab switch to …`).
- **Crash on SIGTERM:** Basecamp segfaults when killed with SIGTERM, for example by a session restart (coredumps in the VM, `shell-objects` build). It's harmless for us but noisy.

## Basecampium (the tabs shell)

- **`8604379` Show beside:** two apps side by side.
  - **Where:** the tab menu → Show beside / Back to tabs; a `QSplitter` holds `m_stack` | a side pane, and the tab stays in the strip.
  - **Guards:** in `contentFor`, `onCurrentChanged`, `detachTab` and `takeTab`.
  - **Compact layout:** not available there.
- **`48b5ef6` Tablet touches:**
  - **System status at the right of the bar:** `StatusCluster` shows time, battery, network and the Shrooms mesh. It reads sysfs every 10 s, so the shell needs no network library. Tapping it emits `statusClicked`, and `MainContainer` opens `$BASECAMP_STATUS_APP`, by default our `basecampos_system_ui`.
  - **Long-press on a tab:** opens the tab menu after 550 ms if the finger stays still. A touch screen has no right-click.
  - **"+  Apps":** replaces the bare "+", which people didn't recognise as the way to open apps.
- **`8838a2f` `BASECAMP_NO_DETACH`:** turns tear-off off. Under a full-screen kiosk compositor (sway) a second top-level window has nowhere to go. Made full screen too, it split the screen; floated, it hides behind.

## Basecamp Legacy (the desktop)

- **`161f1b2` Split view with remembered pairs.**
  - **Split button:** in each window's title bar.
  - **The other half:** a picker with "usually with" partners (pairs ranked by use, halving weekly, in `split-pairs.json`; closed ones are launched), then the open windows.
  - **The divider:** snaps to ⅓/½/⅔; double-click swaps the sides.
  - **Leaving split:** minimising, closing, maximising or dragging a window leaves it.
  - **The code:** `DesktopArea`, plus `MainContainer` for `launchRequested`.
- **`6f4e007`, `0462a4f`:** finger-sized hit areas, and an app opened from the dock fills the empty half.
- **`2dcdb88` Dock auto-hide:** the dock steps aside while a window covers it, and the bottom edge brings it back (hover or tap). `BASECAMP_DOCK_AUTOHIDE=0` keeps it fixed.

## Not in the shell, maybe interesting

- **Native apps inside Basecamp windows:** a `ui_qml` app that is a Wayland compositor (`QtWayland.Compositor` from your bundle) launches qterminal or Brave on its own socket.
  - **Where:** `apps/external-embedded/Main.qml` in this repo.
  - **Lessons:** PLAN.md, "native apps INSIDE Basecamp windows":
    - one output per window;
    - a unique socket per instance (a locked socket `qFatal`s the whole app);
    - server-side decorations;
    - Qt 6.9's compositor sends keys to one `wl_keyboard` per client.
- **Basecamp under XWayland:** it ignored keyboard layout switches when another keyboard (an on-screen keyboard) has a US keymap. BasecampOS now runs Basecamp natively on Wayland on the Duet. In our tests that only broke the Legacy desktop's wallpaper, which rendered black.
