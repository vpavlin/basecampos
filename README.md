# Basecamp OS (experimental)


https://github.com/user-attachments/assets/a71dc6a6-16b0-4d82-9e18-de325fb97d94


Logos Basecamp as a desktop: full screen, your wallpaper, a dock, every app in its own window,
frosted glass, tiling, and Spotlight search. Built on the default Basecamp style and the
[v2 theme engine](#basecamp-theming-v2-experimental), so any theme still applies.

Spec: [docs/BASECAMP-OS-SPEC.md](docs/BASECAMP-OS-SPEC.md)

**Want to try it?** Download the test build and follow [TESTING.md](TESTING.md).

## What's in it

- **Desktop:** full screen (F11 toggles), wallpaper behind everything, a status bar with the λ,
  a search hint and a clock.
- **Dock:** Applications, Package Manager and Settings, then your installed apps; dots for running
  apps, a wider dot for the front window, the name on hover.
- **Windows:** title bar with minimise / maximise / close, drag to move, resize from any edge,
  maximise between the status bar and the dock.
- **Tiling:** the status bar toggle lays visible windows out as equal-width columns.
- **Spotlight (Ctrl+K):** search focused, Recently Closed below, same results as the workspace
  search.
- **Frosted glass:** status bar, title bars, frames, dock, Spotlight and the shell's own windows show a
  blurred wallpaper under a tint.
- **Settings → Appearance:** Layout (Window (default) / Desktop) and Wallpaper (Basecamp, Plain, your own
  image; Fit, Fill or Stretch).

## Build and run

Requires Nix with flakes enabled. The portable bundle runs real apps installed from the catalog
(it reads them from the same folder as the released Basecamp):

```sh
nix build "path:$PWD/basecamp#bin-bundle-dir" --override-input logos-design-system "path:$PWD/design-system"
env BASECAMP_LAYOUT=desktop ./result/bin/LogosBasecamp
```

Or pick **Desktop** in Settings → Appearance → Layout and restart. `./build.sh` builds the dev host
instead (no catalog apps).

**Wallpaper tip:** match your screen for a perfect fit (for example 5120 × 1440 on a 32:9 display);
keep the important part away from the bottom ~110 px (dock) and the top 30 px (status bar).

## Known limits

- Apps that run in their own process take their own keys: while one has focus, use the status bar
  hint instead of Ctrl+K.
- Apps draw their own backgrounds, so only the shell's windows, Package Manager and the chrome are
  frosted.
- Window positions aren't remembered between runs.

## Theme, layout, shell

A **theme** changes how Basecamp looks (data only). A **layout** is a built-in arrangement of the
default shell (`sidebar`, `desktop`). A **shell package** would replace the whole shell; see spec §7.

---

# Basecamp theming v2 (experimental)

This branch is the **v2 experiment**: everything in v1 (colours, corners, fonts, live switching,
Settings → Appearance) plus gradients, backgrounds, icon packs, hover glow, slow background motion,
per-control styles and density. The v1 build is on `main`.

- `design-system/`: logos-design-system `master` @ `a4f3014` + 33 commits
- `basecamp/`: logos-basecamp `master` @ `5f8cca8` + 24 commits (includes the welcome page
  changes in logos-co/logos-basecamp#510)

Spec: [docs/THEME-V2-SPEC.md](docs/THEME-V2-SPEC.md) (§11 covers per-control styles and density).

## Build

Requires Nix with flakes enabled.

```sh
./build.sh
./result/bin/LogosBasecamp
```

## Try a theme

Upload one in **Settings → Appearance**:

- `themes/aurora.theme.json`: gradients, pill controls, card shadows, per-control colours
- `themes/aurora-lucide.theme.json`: Aurora + a Lucide icon pack (bundle, icons inline)
- `themes/aurora-night.theme.json`: Aurora Lucide + an animated background image (bundle)
- v1 themes still load: Paper Serif, Hacker, Catppuccin Mocha

Density (compact, comfortable, touch) is in **Settings → Appearance** too.

## What a theme can set

| Block | What |
|---|---|
| `palette`, `radius`, `typography` | v1: colours, corner scale, font family |
| `gradients` | linear or radial fills for workspace, sidebar, primary, accent, header, card |
| `backgrounds` | an image for the workspace or onboarding, with tint and optional motion |
| `icons` | replacement SVGs for named icons (sanitised, tinted) |
| `effects.hoverGlow` | an outer glow on hover for cards and sidebar items |
| `components` | per-control sizes and colours (button, field, search, combo, list, menu, tab, card...) |
| `density` | compact, comfortable or touch |

A theme is data only: no QML, no scripts. Everything is validated at install, and anything invalid
falls back to the default for that one value.

## Docs

- [Theme v2 spec](docs/THEME-V2-SPEC.md)
- [Theme file spec v1](design-system/docs/theming/README.md)
- [Design system mobile-readiness audit](docs/DS-MOBILE-AUDIT.md)
