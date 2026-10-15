# Logos Theme File: v2 proposal (gradients, backgrounds, icon packs)

**Status:** Proposal · 2026-10-15 · builds on [Theme file spec v1](../design-system/docs/theming/README.md) · not implemented
**Audience:** Basecamp and design-system developers, theme authors, reviewers

---

## TL;DR

- **The line stays where v1 drew it:** a theme is data that Basecamp validates and draws. No QML, no
  scripts, no custom controls. v2 adds **looks**, never **behaviour**.
- **Three additions, in order of risk:**
  1. **Gradients**: pure JSON, no files. Fits inside today's single-file format. Lowest risk, ship first.
  2. **Backgrounds**: raster images in named slots. Needs a theme **package** (a file plus assets).
  3. **Icon packs**: replacement SVGs for named icons. Needs an SVG sanitiser. Highest risk.
- **Graceful in both directions:** a v2 theme loads in a v1 Basecamp (colours, corners and fonts
  apply, the rest is ignored). A v1 theme is a valid v2 theme.
- **Decisions needed:** §8.

## 1. Principles

1. **Slots, not free-form.** A theme fills named slots (`gradients.sidebar`, `backgrounds.workspace`,
   `icons.settings`). It can't paint arbitrary items. Slots are the contract between theme authors
   and Basecamp developers; new slots can be added within v2 without breaking anyone.
2. **Everything falls back.** A missing, invalid or oversized asset falls back to the v1 behaviour for
   that slot only (solid token, no image, default icon). One bad file never breaks a theme.
3. **Readability is enforced, not hoped for.** Text over a gradient or image gets a contrast check at
   install time and, if needed, an automatic scrim.
4. **Brand integrity.** The Basecamp logo is never replaceable; it stays tinted by `brandMark` (v1).
   App icons belong to the app (see §8, Q4).
5. **Install-time work, cheap runtime.** Validate, sanitise, downscale and measure once, at install.
   At runtime Basecamp only reads already-checked local files.

## 2. Gradients (no files)

### 2.1 Syntax

```json
"gradients": {
  "sidebar":   { "type": "linear", "angle": 180, "stops": [["0", "#1B1033"], ["1", "#0B0B12"]] },
  "primary":   { "type": "linear", "angle": 135, "stops": [["0", "#FF6A3D"], ["1", "#C2185B"]] },
  "workspace": { "type": "radial", "center": [0.5, 0.0], "radius": 1.2,
                 "stops": [["0", "#2A1B4D"], ["1", "#0B0B12"]] }
}
```

| Key | Type | Rule |
|---|---|---|
| `type` | `"linear"` \| `"radial"` | Anything else: slot ignored. |
| `angle` | number, degrees | Linear only. `0` = left to right, `90` = top to bottom (CSS-like). Taken modulo 360. |
| `center` | `[x, y]`, 0 to 1 | Radial only, relative to the item. Default `[0.5, 0.5]`. |
| `radius` | number, 0.1 to 3 | Radial only, relative to the item's larger side. Clamped. |
| `stops` | array of `[position, colour]` | 2 to 8 stops; position 0 to 1 (string or number); colour as v1 §4.1. Sorted by position. Fewer than 2 valid stops: slot ignored. |

### 2.2 Slots

| Slot | Paints | Falls back to |
|---|---|---|
| `workspace` | Main content background, welcome page | `palette.background` |
| `sidebar` | Sidebar column | `palette.backgroundSecondary` |
| `primary` | Primary button fill (all states derive from it, see below) | `palette.primary` |
| `accent` | Selected sidebar item, chips | `palette.accentOrange` |
| `header` | Panel and dialog headers | `palette.backgroundElevated` |

- **Hover and pressed:** Basecamp derives them from the gradient (each stop lightened or darkened like
  the solid tokens). Authors can't set per-state gradients in v2.
- **Solid colour for C++ and small items:** where a slot's area can't draw a gradient (C++ chrome,
  1 px borders, small badges), Basecamp uses the **first stop**. The window edge (`MainContainer`)
  therefore matches the gradient's start.
- **Contrast:** checked against **every stop** for the text that sits on the slot (`text` on
  `workspace`, `primaryForeground` on `primary`, `accentForeground` on `accent`). Below 3:1 on any
  stop is flagged, the same as v1.

### 2.3 Implementation notes

- `Rectangle.gradient` only does vertical or horizontal. Arbitrary angles and radial use
  `QtQuick.Shapes` (`LinearGradient` / `RadialGradient` on a `ShapePath`), which the design system
  already imports (`LogosStageLane`).
- One small DS item, `LogosSurface { slot: "sidebar" }`, draws the gradient if the theme sets one,
  else the solid token. Shell sites swap `Rectangle { color: Theme.palette.backgroundSecondary }` for
  it, slot by slot. Dark stays pixel-identical because Dark sets no gradients.
- **Verify on a real display:** Shapes rendering differs from the offscreen path.

## 3. Theme packages (needed for backgrounds and icons)

Images and icons are files, so a v2 theme that uses them is a **package**: `theme.json` plus an
`assets/` folder.

```
sunset.lgtheme            (archive)
├── theme.json            (formatVersion 2)
├── preview.png           (optional, shown in Settings)
└── assets/
    ├── workspace.jpg
    └── icons/
        ├── settings.svg
        └── dashboard.svg
```

- **Container:** reuse the **LGX** archive format and extractor the package manager already has
  rather than inventing a zip reader. One extractor to harden, and the same file
  can later ship through the package catalog as `type: "theme"`.
- **Upload accepts both:** a plain `.json` (v1, or v2 with only gradients) or a package.
- **Asset references** in `theme.json` are relative paths under `assets/`: lowercase `a-z 0-9 _ - . /`,
  no `..`, no absolute paths, no URLs, no symlinks. Anything else is dropped with a warning.

### 3.1 Package limits (normative)

| Limit | Value |
|---|---|
| Package size (compressed) | 8 MiB |
| Total uncompressed | 24 MiB; compression ratio over 50:1 rejected (zip bomb) |
| Entries | 256 max; only `theme.json`, `preview.png`, `assets/**` |
| `theme.json` | 64 KiB (unchanged) |
| Raster image | PNG or JPEG only (checked by content, not extension); 4 MiB; up to 3840 × 3840 px |
| SVG icon | 32 KiB each |

Install copies the extracted, checked files into `<Basecamp data>/themes/<id>/`. Nothing is read from
the archive at runtime.

## 4. Backgrounds

```json
"backgrounds": {
  "workspace": { "image": "assets/workspace.jpg", "fit": "cover", "opacity": 1.0,
                 "tint": "#0B0B12", "tintStrength": 0.35 },
  "onboarding": { "image": "assets/tents-at-dusk.jpg", "fit": "cover" }
}
```

| Key | Type | Rule |
|---|---|---|
| `image` | asset path | PNG or JPEG in the package. |
| `fit` | `"cover"` \| `"contain"` \| `"tile"` \| `"center"` | Default `cover`. |
| `opacity` | 0 to 1 | Default 1. Below it shows the slot's gradient or solid colour. |
| `tint`, `tintStrength` | colour, 0 to 1 | A colour layer over the image, for readability and mood. Default none / 0. |

**Slots:** `workspace` (content area and welcome page) and `onboarding` (the design system already
defines an `onboardingBackdrop` image, unused in 0.3.2). The sidebar stays solid or gradient: an image
behind a narrow icon column hurts legibility for little gain.

**Readability (normative):** at install Basecamp computes the image's average luminance (after tint)
and checks `text` against it. Below 4.5:1, it raises `tintStrength` until the check passes and tells
the author in the install result. An author can't ship an unreadable workspace.

**Install-time processing:** images are decoded and **re-encoded** (re-encoding drops metadata and
anything trailing the image data), and downscaled to at most 2× the largest screen size Basecamp
targets.

## 5. Icon packs

### 5.1 What can be replaced

Only icons Basecamp and the design system own, by **stable name**:

| Set | Names (today) |
|---|---|
| Design system (`LogosIcons`) | `arrowLeft` `arrowLeftDouble` `arrowRight` `arrowRightDouble` `triangleUp` `triangleDown` `refresh` `install` `trash` `more` `search` `close` `copy` `check` `grid` `list` `pages` `warning` `info` … (the registry is the source of truth) |
| Basecamp shell | `dashboard` `settings` `apps` (`module.svg`) `workspace` `filter` |

**Not replaceable:** the Basecamp logo (brand integrity, §1.4) and app icons (they belong to each app,
§8 Q4). A missing name falls back to the default icon, per icon.

```json
"icons": {
  "style": "tinted",
  "set": {
    "settings":  "assets/icons/settings.svg",
    "dashboard": "assets/icons/dashboard.svg",
    "search":    "assets/icons/search.svg"
  }
}
```

### 5.2 Style

| `style` | Meaning |
|---|---|
| `"tinted"` (default) | Shapes only. Basecamp colours them with tokens, so idle, hover, selected and disabled states keep working. Authors should draw them **pure white** (tinting a grey icon doesn't give the exact colour). |
| `"full-color"` | Drawn as-is. States are shown by the background behind the icon, not the glyph colour. For illustrated or multicolour packs. |

### 5.3 SVG sanitiser (normative)

SVG is a document format, so every icon passes a whitelist sanitiser at install and is
**re-serialised**; the original file is never used.

- **Allowed elements:** `svg g path rect circle ellipse line polyline polygon defs linearGradient
  radialGradient stop use title`.
- **Allowed attributes:** geometry, `fill` `stroke` and related presentation attributes, `transform`,
  `opacity`, `viewBox`, `id`. `use` and gradient references only as local `#id`.
- **Removed:** `script`, `foreignObject`, `image`, `style` elements and attributes, event handlers
  (`on*`), any `href` that isn't a local `#id`, external entities and DOCTYPE (entity expansion).
- **Shape rules:** square `viewBox`, 32 KiB max, at most 2000 path commands.
- Qt's SVG renderer (SVG Tiny 1.2) runs no scripts anyway; the sanitiser removes the remaining surface
  (external references, entity expansion) and keeps the format small enough to review.

## 6. Settings → Appearance changes

- **Preview:** `preview.png` (or a generated swatch) next to the dropdown.
- **"Includes" line:** e.g. *Gradients · Background · 12 icons*, so users know what a theme changes.
- **Warnings from install** (dropped assets, auto-raised tint) are shown, as v1 does for colours.
- **Uninstall** (planned in v1, more important now that themes carry files).

## 7. Compatibility and versioning

- `formatVersion: 2` for any theme that uses `backgrounds` or `icons`. A theme with only `gradients`
  may stay `formatVersion: 1` (new optional keys are allowed within v1, spec §9).
- A **v1 Basecamp** loading a v2 `theme.json` applies colours, corners and fonts and ignores the rest
  (v1 processing rules). A v1 Basecamp can't open a package; the author can ship the bare `theme.json` too.
- **Apps in `ui-host`** pick up gradients and icons from `Logos.Theme` on restart, as with colours today.
  They need **read access** to `<Basecamp data>/themes/` for backgrounds and icons (verify against the
  sandbox, §8 Q5).

## 8. Decisions needed

1. **Order:** gradients → backgrounds → icons (recommended: lowest risk first, and gradients alone
   change the feel the most per line of code)? Or icons first because they're the most visible?
2. **Package container:** reuse LGX (recommended) or a new archive format?
3. **Gradient slots:** are the five slots in §2.2 right? Anything missing (e.g. cards, the title bar)?
4. **App icons:** themes may **not** override app icons (recommended for v2: apps own their identity,
   and a theme re-skinning "Wallet" invites confusion). Revisit with a per-app opt-in later?
5. **Sandbox:** can `ui-host` apps read the installed theme folder, or does the shell need to pass
   assets in?
6. **Full-colour icons:** allowed in v2, or tinted only until states are designed for them?

## 9. Not in v2

Custom controls or any QML · animations and motion curves · sounds · per-app themes · overriding app
icons or the Basecamp logo · font files (still only behind a sanitiser) · fetching anything from
the network.

## 10. Suggested first slice

**Gradients only**, no package format. A `LogosSurface` item in the design system plus the five shell
slots, with one example theme (e.g. a "Sunset" variant of Paper Serif or a gradient Hacker) to prove
it. It needs no new file handling, keeps Dark pixel-identical, and shows the community the visual
range of v2 before we commit to the package work.
