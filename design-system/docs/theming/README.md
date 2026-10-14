# Logos Theme File — Specification v1

**Status:** Draft v1 · 2026-10-14 · implemented on `theming-exp` (logos-design-system + logos-basecamp 0.3.2)
**Audience:** theme authors, and developers implementing or reviewing the loader
**Schema:** [`basecamp-theme-1.schema.json`](basecamp-theme-1.schema.json) · **All tokens with defaults:** [`logos.dark.theme.json`](logos.dark.theme.json)

---

## 1. What a theme is

A Logos theme is **one UTF-8 JSON file**, `theme.json`, that describes how Basecamp looks:
colours, corner rounding, and the name of a font. It is **data only**. No code, no QML, no
scripts, no font files, no images. Basecamp reads it as data, checks every value, and ignores
what it does not understand. A theme cannot run anything, read files or reach the network.

Every value is optional except the identity fields. Anything a theme leaves out falls back to
**Logos Dark**, token by token, so a three-line theme is valid and complete.

## 2. Example

```json
{
  "$schema": "https://logos.co/schemas/basecamp-theme/1.json",
  "formatVersion": 1,
  "id": "yourname.sunset",
  "name": "Sunset",
  "author": "yourname",
  "mode": "light",

  "palette": {
    "background": "#FAF7F2",
    "surface": "#FFFDF9",
    "text": "#1F1A15",
    "textSecondary": "#57504A",
    "primary": "#C9512C",
    "primaryForeground": "#FFFDF9",
    "brandMark": "#1F1A15"
  },

  "radius": { "scale": 1 },

  "typography": {
    "family": ["Inter", "Noto Sans"],
    "monoFamily": ["JetBrains Mono", "JetBrainsMono Nerd Font"]
  }
}
```

Full working examples: [`examples/themes/`](../../examples/themes/) (Paper, Paper Serif, Hacker, Catppuccin Mocha).

## 3. Top-level fields

| Field | Required | Type | Rule |
|---|---|---|---|
| `formatVersion` | **yes** | integer ≥ 1 | `1` for this spec. A higher version still loads, reading only the keys it knows. |
| `id` | **yes** | string | `<author>.<name>`: lowercase `a–z 0–9 _ -`, at least one dot, ≤ 64 chars. Unique per theme. `dark` is reserved. Installing a file with an existing `id` replaces that theme. |
| `name` | **yes** | string | Display name, 1–40 chars, plain text. |
| `author` | **yes** | string | 1–40 chars, plain text. Shown after the name, quieter: **Paper** *by Alisher*. |
| `mode` | **yes** | `"dark"` \| `"light"` | Overall brightness. Apps read it (`Theme.isDark`) to choose matching assets. |
| `palette` | no | object | Colour tokens, §4. |
| `radius` | no | object | Corner rounding, §5. |
| `typography` | no | object | Fonts, §6. |
| `spacing` | no | object | Reserved: validated, **not applied** in v1. |
| `$schema` | no | string | Editor hint; ignored by Basecamp. |

Unknown top-level keys are ignored with a warning, never an error.

## 4. Palette

### 4.1 Colour syntax
`"#RRGGBB"` or `"#RRGGBBAA"`, hex, **alpha last** (CSS and Figma order). Anything else is
dropped with a warning, and that token falls back to the default. (Qt itself uses `#AARRGGBB`;
Basecamp converts, so authors never need to.)

### 4.2 Tokens
Tokens are **roles**, not colours: set what something *is*, and every control with that role
follows. 65 tokens:

| Group | Tokens | Paints |
|---|---|---|
| Backgrounds | `background` `backgroundSecondary` `backgroundTertiary` `backgroundElevated` `backgroundMuted` `backgroundBlack` `backgroundInset` `backgroundButton` | Window, sidebar, cards, inset wells (incl. the sidebar workspace tile) |
| Surfaces | `surface` `surfaceRaised` `surfaceInteractiveHover` `surfaceRecessed` `surfaceContrast` | Controls and panels |
| Text | `text` `textSecondary` `textSubtle` `textTertiary` `textPlaceholder` `textMuted` | Headings → hints → disabled |
| Borders | `border` `borderSecondary` `borderTertiary` `borderTertiaryMuted`† `borderSubtle` `borderHairline` `borderGlyph` `borderInteractive` `borderDark` `borderStrong` | Outlines, dividers |
| Primary | `primary` `primaryHover` `primaryPressed` `primarySoft` `primaryBorder`† `primaryForeground` | Main action; `primaryForeground` = text and glyphs **on** a primary fill |
| Accent | `accentOrange` `accentOrangeMid` `accentOrangeDeep` `accentBurntOrange` `accentYellowSoft` `accentForeground` | Selection highlights (active sidebar button, chips, badges); `accentForeground` = glyphs **on** the accent |
| Status | `success` `successHover` `successPressed` `successBorder` `error` `errorHover` `errorPressed` `errorBorder` `warning` `warningHover` `warningBorder` `info` `notification` | Notices, badges, validation |
| Brand | `brandMark` | The Basecamp logo (welcome page, sidebar) |
| States & overlays | `hover` `pressed` `disabled` `focus` `glassOverlay` `glassStrong` `overlayDark` `overlayLight` `overlayOrange` `scrim` | Interaction states, translucent layers, modal scrim |

† **Derived:** set `primary` without `primaryBorder` and Basecamp derives it (`primary` at 40 %);
`borderTertiaryMuted` = `borderTertiary` at 20 %. An explicit value always wins.

`accentOrange*` and `backgroundBlack` are named after the default look but are **roles**: in a
green theme, `accentOrange` is your green.

## 5. Radius

| Key | Type | Range | Meaning |
|---|---|---|---|
| `scale` | number | 0 – 1 | Multiplies **every** corner, including round buttons and pills. `0` = square, `1` = as designed. |
| `tiny` `small` `medium` `large` `xlarge` `pill` | integer px | 0 – 999 | Override one step (defaults 2, 4, 6, 8, 16, 999), before `scale`. |

Out-of-range values are clamped with a warning. Radio buttons and the loading spinner stay round
on purpose: their shape carries meaning.

## 6. Typography

| Key | Type | Meaning |
|---|---|---|
| `family` | string or array of strings | Body font for all text. |
| `monoFamily` | string or array of strings | Font for code, hashes, addresses. |
| `sizes`, `weights` | object | Reserved: validated, **not applied** in v1. |

- A font is named, never shipped: it must be **installed on the user's computer**.
- A list is a **fallback chain**, like CSS `font-family`: the first installed name wins. Up to 8.
- If none is installed, Basecamp uses its bundled **Public Sans** (body) or the platform monospace,
  and Settings → Appearance says which font was asked for and which one is used.
- Names may contain letters, digits, space, `.` `_` `'` `-`; ≤ 64 chars each. Anything else is
  dropped with a warning.

## 7. Processing rules (normative)

1. **Size:** a file larger than 64 KiB is rejected.
2. **Parse:** must be a JSON object; otherwise rejected.
3. **Identity:** `formatVersion`, `id`, `name`, `author`, `mode` must be valid; otherwise **rejected**
   (nothing is installed or applied).
4. **Everything else is forgiving:** unknown keys are ignored, invalid values dropped, numbers
   clamped, each with a warning. A theme never fails because of one bad colour.
5. **Resolution:** every token resolves to the theme's value, else the derived value (§4.2 †),
   else Logos Dark.
6. **Readability check:** contrast is measured for `text`/`background`, `text`/`surface`,
   `textSecondary`/`background` and `primary`/`background`; below 3:1 is flagged.
7. **Untrusted text:** `name` and `author` are displayed as plain text (escaped). Markup in them
   has no effect.
8. **Location:** installed themes live at `<Basecamp data>/themes/<id>/theme.json`. Symlinked
   files or folders are ignored. If two folders declare the same `id`, the first found wins.

## 8. Lifecycle in Basecamp

| Step | What happens |
|---|---|
| **Install** | Settings → Appearance → **Upload your own theme…** → pick the `.json`. Validated *before* anything is written. Invalid: the reasons are shown and nothing changes. Valid: copied to the themes folder and selected. |
| **Select** | Settings → Appearance → **Theme** dropdown (*Name* by *author*). The choice is remembered. |
| **Apply** | Basecamp restyles **immediately**: every view, the sidebar, the window chrome. **Apps** that use the Logos design system run in their own process and switch the **next time Basecamp starts**. |
| **Remove** | Delete the theme's folder. If it was active, Basecamp falls back to Dark. |

## 9. Versioning

- New *optional* keys or tokens may be added within v1. Older Basecamps ignore them; newer
  Basecamps fill tokens an old theme lacks from the defaults. Themes work in both directions.
- A change that alters the meaning of an existing key requires `formatVersion: 2`.

## 10. Security model (why a theme is safe to share)

- **Data, not code:** parsed by C++ against a whitelist; never evaluated by QML or JavaScript.
- **No files:** no font files, images or paths; a theme only names an installed font.
- **No network:** nothing in a theme is fetched; `name`/`author` are escaped.
- **Install is shell-only:** sandboxed apps can read the active theme but can't install one.
- **Always recoverable:** a broken or unreadable theme falls back token by token; Dark is built in
  and can't be removed.

## 11. Checklist before sharing

- [ ] Validates against `basecamp-theme-1.schema.json`
- [ ] `text` on `background` ≥ 4.5 : 1; `primary` on `background` ≥ 3 : 1
- [ ] `primaryForeground` is legible on `primary`; `accentForeground` on `accentOrange`
- [ ] `brandMark` is visible on `background` and `backgroundSecondary` (sidebar)
- [ ] If you name a font, give a fallback list and check how it looks with Public Sans
- [ ] Looked at Settings, the welcome page and the sidebar, with an item selected and idle

## 12. Not in v1 (planned or under discussion)

Spacing and type sizes · font files (only behind a sanitiser) · bundled fonts that any theme can
name · icon and image packs · per-app themes · installing themes from the package catalog
(`type: "theme"` LGX) · uninstall and preview in Settings.
