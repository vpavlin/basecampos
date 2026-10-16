# Basecamp OS (experimental)

This branch adds a **desktop layout** to the v2 build: full screen, wallpaper, a dock, apps in
movable windows, a status bar with a clock, and Spotlight (Ctrl+K). Spec:
[docs/BASECAMP-OS-SPEC.md](docs/BASECAMP-OS-SPEC.md).

Try it: `env BASECAMP_LAYOUT=desktop ./result/bin/LogosBasecamp`, or pick **Desktop** in
Settings → Appearance → Layout and restart.

To run real apps installed from the catalog (portable `linux-amd64` packages), build the portable
bundle instead; it reads apps from the same folder as the released Basecamp:

```sh
nix build "path:$PWD/basecamp#bin-bundle-dir" --override-input logos-design-system "path:$PWD/design-system"
env BASECAMP_LAYOUT=desktop ./result/bin/LogosBasecamp
```

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
