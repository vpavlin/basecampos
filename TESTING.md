# Testing Basecamp customisation

Thanks for trying this. This test build of Logos Basecamp adds **themes**, **density** and a second
**layout** (Desktop: full screen, a dock, every app in its own window). It is a prototype, not an
official Logos release, and the builds are **not signed**.

What we want to learn: **do themes and layouts feel right, and what is missing?**

## 1. Download

From the [test release](https://github.com/xAlisher/basecamp-theming/releases):

| What | File |
|---|---|
| Basecamp, Linux x86_64 | `basecamp-os-x86_64.AppImage` |
| Basecamp, macOS (Apple Silicon) | `basecamp-os-macos-arm64.zip` |
| Extra themes (optional) | the `.theme.json` files in the same release |

## 2. Run it

**Quit your regular Basecamp first.** Never run it and the test build at the same time.

**Linux:**

```sh
chmod +x basecamp-os-x86_64.AppImage
```

**macOS:** unzip `basecamp-os-macos-arm64.zip` and leave `LogosBasecamp.app` in Downloads.
**Don't move it to Applications:** it has the same name as your regular Basecamp and would
replace it.

The build is **not signed by an Apple developer**, so the first time macOS says it can't verify
the app. This is expected for a test build. To open it anyway:

- **macOS 15 or later:** try to open it once, then go to **System Settings → Privacy & Security**,
  scroll down and click **Open Anyway**.
- **macOS 14 or earlier:** right-click `LogosBasecamp.app` → **Open** → **Open**.
- **Any version, from Terminal:**

```sh
xattr -dr com.apple.quarantine ~/Downloads/LogosBasecamp.app
```

Then pick one of two ways:

### A. Separate (recommended)

The test build keeps its apps, keys and logs in a folder of its own. Your regular Basecamp is not
changed.

```sh
./basecamp-os-x86_64.AppImage --user-dir ~/basecamp-test
```

macOS:

```sh
open -n ~/Downloads/LogosBasecamp.app --args --user-dir ~/basecamp-test
```

It starts as a fresh Basecamp with no apps: open **Applications** and install a few official apps
(for example Blockchain, Docs, Chat). To bring over the apps you already have (without their data
or keys), run this once before starting:

```sh
mkdir -p ~/basecamp-test
cp -a ~/.local/share/Logos/LogosBasecamp/plugins ~/.local/share/Logos/LogosBasecamp/modules ~/.local/share/Logos/LogosBasecamp/lib ~/basecamp-test/
```

(macOS: the same with `~/Library/Application\ Support/Logos/LogosBasecamp` in place of
`~/.local/share/Logos/LogosBasecamp`.)

### B. On your real data

The test build opens your real Basecamp: your apps, node, wallets and keys, as they are. Use this
if you want to see themes and layouts on your own setup.

- **Back up first.** The test build is newer than the released Basecamp (0.3.2), and going back
  to 0.3.2 after it has used your data is not tested:

  ```sh
  cp -a ~/.local/share/Logos/LogosBasecamp ~/LogosBasecamp-backup
  ```

  macOS:

  ```sh
  cp -a ~/Library/Application\ Support/Logos/LogosBasecamp ~/LogosBasecamp-backup
  ```

  (This copies everything, including any large chain data; check you have the space.)
- Start it without `--user-dir`: `./basecamp-os-x86_64.AppImage` on Linux, or
  `open -n ~/Downloads/LogosBasecamp.app` on macOS.

- `logos://` links open the test build while you use it; your regular Basecamp takes them back
  the next time it starts.
- To undo: quit the test build, delete the Basecamp data folder (above) and put
  `~/LogosBasecamp-backup` back in its place.

## 3. Themes

Basecamp comes with two themes: **Dark** (default) and **Light**.

- Switch in **Settings → Appearance → Theme**. Basecamp changes right away; installed apps change
  the next time you start Basecamp.
- Install more: download a theme file from the release or from the
  [themes folder](https://github.com/xAlisher/basecamp-theming/tree/basecamp-os-theme/themes),
  then **Upload your own theme…** (right under the Theme list), and pick it from the list.

| Theme file | What it shows |
|---|---|
| `aurora.theme.json` | gradients and a hover glow |
| `aurora-lucide.theme.json` | Aurora with its own icon set |
| `aurora-night.theme.json` | a background image, icons, gradients |
| `catppuccin-mocha.theme.json` | a community colour palette |
| `hacker.theme.json` | a monospace font (JetBrains Mono if installed; Settings says when it falls back) |
| `paper-serif.theme.json` | a light theme with a serif font |

**Try:** open the same official apps under two or three themes. Is text always readable? Does
anything keep the old colours, look broken or clash?

## 4. Density

**Settings → Appearance → Density:** Compact, Comfortable or Touch (or "Theme default"). It changes
the size of controls and spacing. **Try:** each setting on your screen; which would you keep?

## 5. Layouts

**Settings → Appearance → Layout:**

- **Window (default):** Basecamp as you know it, with the sidebar.
- **Desktop:** full screen, wallpaper, a dock, every app in its own window.

Pick one, then **Restart now**. Basecamp closes and opens in the new layout (this can take up to
about 10 seconds).

**Wallpaper** is part of the Desktop layout: after Basecamp restarts in Desktop, Settings →
Appearance shows **Wallpaper** (Basecamp, Plain, or your own image; Fit, Fill or Stretch).

**Tip:** Desktop takes the whole screen. Move Basecamp to a workspace (virtual desktop) of its
own, so it can stay full screen while you switch to other apps with your system's workspace
shortcut. On GNOME, for example, Super+Shift+Page Down moves the focused window to the next
workspace; most desktops have a similar shortcut or let you drag the window in the overview.

**Try in Desktop:**

- open apps from the dock; drag windows by the title bar, resize from any edge, minimise,
  maximise, close;
- **tiling:** the toggle in the status bar (top right) lays out every visible window as an
  equal-width, full-height column, in the order the windows were opened. Open, close or minimise
  a window and the columns re-arrange by themselves. Try it with two, three and four apps;
- **Ctrl+K** opens Spotlight: type an app name and click the result;
- **F11** toggles full screen.

## 6. Make your own theme

A theme is a single JSON file: colours, fonts, corner radius, and optionally gradients, a
background image and icons. Start from one of the files in the
[themes folder](https://github.com/xAlisher/basecamp-theming/tree/basecamp-os-theme/themes), change
it, and upload it.

- Theme file format: [THEME-V2-SPEC.md](docs/THEME-V2-SPEC.md)
- Basecamp checks every file on upload and explains what it rejected; it also raises contrast if a
  background would make text hard to read.

Share what you make: attach it to an issue (see below).

## 7. Known limits

- Window positions are not remembered between runs (Desktop).
- Tiling is simple on purpose:
  - columns only, all the same width (no rows, no grid, no custom split), so many windows on
    a small screen get narrow;
  - you can't reorder columns by dragging; the order is the order windows were opened;
  - a tiled window you move or resize snaps back the next time a window opens, closes or is
    minimised;
  - turning tiling off leaves windows where they are (it doesn't restore earlier positions);
  - tiling is off again after a restart.
- In Desktop, while an app window has focus, Ctrl+K goes to that app; click the search hint in the
  status bar instead.
- Apps draw their own backgrounds, so only Basecamp's own windows and the frames around apps are
  frosted.
- Restarting can take up to about 10 seconds.

## 8. Send feedback

Open an issue at https://github.com/xAlisher/basecamp-theming/issues with:

- your OS and screen size,
- what you tried, what you expected, what happened,
- a screenshot if it is visual.

Short answers to "what feels right, what is missing" are the most useful thing you can send.

## Remove it

Delete the AppImage (Linux) or `LogosBasecamp.app` from Downloads (macOS). With option A, also
delete `~/basecamp-test`. Themes you uploaded are in the `themes` folder inside the Basecamp data
folder (`~/.local/share/Logos/LogosBasecamp` on Linux, `~/Library/Application Support/Logos/LogosBasecamp`
on macOS).
