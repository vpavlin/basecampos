# Basecampium: spec

Status: draft, 2026-10-20. A Chrome-like shell for Logos Basecamp, and the first shell package
(see [SHELL-PACKAGES-SPEC.md](SHELL-PACKAGES-SPEC.md), Phase 0).

## TL;DR

- Basecamp as a browser: **one bar** with tabs and the window controls, a **New tab** page with
  search and your apps, and **no address bar**.
- Every app, including Applications, Settings and Package Manager, opens in a tab.
- Built as a separate `main_ui` plugin that reuses the default shell's plumbing and replaces only
  the frame. Basecamp itself needs one small change: load a shell from a folder, and fall back to
  the default shell if it fails.

## 1. Goals and non-goals

**Goals**

- Feel instantly familiar to anyone who uses a browser.
- Prove that a whole Basecamp UI can be swapped as a package, while apps, keys and themes stay the
  same.
- Stay small: reuse everything that isn't the frame.

**Non-goals (for the demo)**

- An address bar, history, bookmarks, profiles, multiple windows.
- Tab groups, split view, pinned-tab sync.
- Signing and the `ui_shell` package type (Phase 1-2 of the shell-packages spec).

## 2. Anatomy

```
Linux / Windows
┌─[ λ New tab  × ]─[ Blockchain × ]─[ Docs × ]─[+]──────── drag area ────────[–][□][×]─┐
│                                                                                     │
│                                 tab content                                         │
│                                                                                     │
└─────────────────────────────────────────────────────────────────────────────────────┘

macOS
┌─(● ● ●)─[ λ New tab  × ]─[ Blockchain × ]─[ Docs × ]─[+]────── drag area ──────────────┐
```

### 2.1 The bar

One row, about 40 px high, in the theme's `backgroundSecondary` (or the `header` gradient if the
theme has one). From left to right:

1. **macOS only:** the traffic lights (close, minimise, zoom).
2. **Tabs.**
3. **New tab button `+`**, right after the last tab, as in Chrome: it moves as tabs open and close.
4. **Drag area:** the empty space after the last tab.
5. **Linux / Windows only:** minimise, maximise / restore, close.

There is no separate title bar and no address bar.

### 2.2 A tab

- Icon (the app's icon; λ for New tab), title (the app's display name), close `×` on hover and on
  the active tab.
- Active tab: same colour as the content below, so the tab and its page read as one surface.
  Inactive tabs: the bar colour, title in `textSecondary`.
- **Width:** 240 px wide at most. Tabs shrink evenly as more open, down to 72 px (icon + close).
  Past that, the tab strip scrolls horizontally, with the active tab kept in view.
- **Loading:** a spinner replaces the icon while the app's view is loading.
- **Busy app:** a small dot on the icon when an inactive tab's app asks for attention (later).

## 3. New tab page

```
                              Basecampium

        ( Search apps…                                            Ctrl+K )

        [Accounts] [Blockchain] [Chat] [Docs] [Ethereum RPC] [Keystore] …
        (installed apps, A to Z, centred, scrolls if there are many)

                    ───────────────────────────────────
                          (◎ Applications) (⚙ Settings) (⬡ Package Manager)
                         Shell package made by Alisher
```

- Opens on start, from `+`, from Ctrl+T and Ctrl+K. Search has focus, so typing filters at once.
- **Header:** the shell's name above the search, large and light grey.
- **Search** is exactly as wide as the icon grid (first icon's left edge to last icon's right edge)
  and filters the apps as you type; Enter opens the first match, and search also finds the three
  system apps by name.
- **Installed apps:** a grid of whole columns centred under the search, A to Z; hover shows a
  rounded background, as the dock and welcome cards do. With many apps the grid scrolls above the
  bottom row.
- **System apps** (Applications, Settings, Package Manager): small round buttons pinned to the
  bottom under a subtle divider, names on hover. The credit line ("Shell package made by …") sits
  under them.
- **Picking an app turns this tab into the app.** If that app is already open in another tab,
  Basecampium switches to that tab and closes this New tab (Basecamp apps are single instance).

## 4. Tabs and apps

| Action | Result |
|---|---|
| Open an app (tile, search, an intent from another app) | in the current New tab, or a new tab; switches to an existing tab if the app is already open |
| Close a tab | the app is unloaded, as closing a window does in the default shell |
| Close the last tab | a fresh New tab opens; the window stays |
| Drag a tab | reorder; the `+` stays after the last tab |
| Middle-click a tab | close |
| Double-click the drag area | maximise / restore |
| Drag the drag area | move the window |
| Right-click a tab | Close, Close others, Close tabs to the right, Reload app |

- **Settings, Applications and Package Manager are tabs like any other.** Opening Settings from an
  app (the `basecamp.settings.open` intent) opens or focuses the Settings tab.
- **Overlay dialogs** (install / uninstall confirmation, intent chooser) appear over the whole
  window, not inside one tab.
- **An app that crashes** shows "*App* stopped. Reload" inside its tab; other tabs keep working.

## 5. Keyboard

| Keys | Action |
|---|---|
| Ctrl+T | New tab |
| Ctrl+W | close tab |
| Ctrl+Tab / Ctrl+Shift+Tab | next / previous tab |
| Ctrl+1 … Ctrl+8 | go to tab 1-8 |
| Ctrl+9 | go to the last tab |
| Ctrl+K | New tab with search focused (or focus search if already on New tab) |
| Ctrl+, | Settings tab |
| Ctrl+Shift+T | reopen the last closed tab |

macOS uses Cmd instead of Ctrl. Keys must work while an app (in its own process) has focus, via the
existing `ShortcutBridge`; where an app captures a key first, that key goes to the app.

## 6. Window

- Basecamp's window is already frameless (`app/window.cpp:199`); Basecampium draws the bar and
  handles move (system move from the drag area), resize from every edge, maximise, minimise, close.
- Close follows Basecamp's existing behaviour (close to tray where configured).
- Full screen (F11 / Ctrl+Cmd+F) hides the bar; moving the pointer to the top edge shows it.

## 7. State

- **Restore tabs:** on by default. The open tabs and their order are saved on quit and reopened at
  the next start (apps load as their tab is first shown, so start-up stays fast).
- Stored under `BasecampiumShell/` in Basecamp's settings, separate from the default shell's
  prefs.
- Settings → Appearance in Basecampium shows Theme and Density, and a **Basecampium** group (Restore
  tabs on start). The default shell's Layout and Wallpaper options are hidden: they don't apply.

## 8. Themes

Everything is drawn with the design system, so every theme applies: bar, tabs, New tab page, tiles.
New tokens are not needed for the demo; if a theme sets `gradients.header`, the bar uses it.

## 9. Built from

| Reused from the default shell (`src/`) | New in Basecampium |
|---|---|
| app hosting, `ShortcutBridge`, overlay and intent dialogs, Settings pages, `ThemeInstaller`, Package Manager hosting | the frame: tab bar, tab model, window controls, New tab page, tab persistence |

Basecampium is a second `main_ui` plugin built from the same tree (`shells/basecampium/`), linking
the reused sources. What it has to copy or link shows exactly what the shared shell library
(shell-packages spec §5.9) must contain.

## 10. Shipping as a shell package

Basecampium is not bundled with Basecamp. It is built from the same tree with `-DBASECAMPIUM` and
packaged by `tools/make-shell-lgx.sh` as `basecampium.lgx` (manifest `type: "ui_shell"`, signed with
`lgx sign`). The user installs it in Package Manager and switches to it in Settings → Customisation
→ Shell; see [SHELL-PACKAGES-SPEC.md](SHELL-PACKAGES-SPEC.md) §6 for the full flow and the host
changes it relies on (shell choice, fallback, safe mode). For development, `BASECAMP_SHELL=<folder>`
loads a shell from a folder without installing it.

## 11. Acceptance checklist

- [ ] After switching to Basecampium: one bar with `+` after the last tab, a New tab with
      search focused, system tiles first.
- [ ] Open three apps from tiles and search; each gets its own tab with icon and name.
- [ ] Opening an already open app switches to its tab.
- [ ] Ctrl+T / Ctrl+W / Ctrl+Tab / Ctrl+1…9 / Ctrl+K work, also while an app has focus.
- [ ] Drag to move the window; double-click to maximise; resize from the edges.
- [ ] Settings, Applications and Package Manager open as tabs; installing an app shows the
      confirmation over the whole window.
- [ ] Switch theme (Dark, Light, Aurora): bar, tabs and New tab follow at once.
- [ ] Quit and start again: the same tabs come back.
- [ ] Point `BASECAMP_SHELL` at a broken folder: the default shell starts with a message, no crash.
- [ ] Linux and macOS (traffic lights on the left).

## 12. Open questions

1. **Closing a tab:** unload the app (frees memory, stops playback) or keep it running in the
   background like a pinned tab? Receiver playing audio is the case that decides.
2. **Pinned tabs** (small, icon only, always first) for apps people keep open, like Blockchain?
3. **Several windows** (drag a tab out into its own window): later, or never?
