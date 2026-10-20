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
| Basecamp, macOS (Apple Silicon) | coming in a later test build |
| Extra themes (optional) | the `.theme.json` files in the same release |

## 2. Run it

**Quit your regular Basecamp first** (both use the same storage folder). The test build keeps its
apps, keys and logs in a folder you choose, so your regular install is not changed.

Linux:

```sh
chmod +x basecamp-os-x86_64.AppImage
./basecamp-os-x86_64.AppImage --user-dir ~/basecamp-test
```

The first start is a fresh Basecamp with no apps. Open **Applications** and install a few official
apps (for example Blockchain, Docs, Chat) so you have something to look at.

## 3. Themes

Basecamp comes with two themes: **Dark** (default) and **Light**.

- Switch in **Settings → Appearance → Theme**. Basecamp changes right away; installed apps change
  the next time you start Basecamp.
- Install more: download a theme file from the release, then **Settings → Appearance → Upload your
  own theme…**, and pick it from the Theme list.

| Theme file | What it shows |
|---|---|
| `aurora.theme.json` | gradients and a hover glow |
| `aurora-lucide.theme.json` | Aurora with its own icon set |
| `aurora-night.theme.json` | a background image, icons, gradients |
| `catppuccin-mocha.theme.json` | a community colour palette |
| `hacker.theme.json` | a monospace font |
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

**Wallpaper** is part of the Desktop layout: once Desktop is chosen, Settings → Appearance shows
**Wallpaper** (Basecamp, Plain, or your own image; Fit, Fill or Stretch).

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
background image and icons. Start from one of the files above, change it, and upload it.

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

Delete the AppImage and the `~/basecamp-test` folder. Themes you uploaded are in
`~/.local/share/Logos/LogosBasecamp/themes`.
