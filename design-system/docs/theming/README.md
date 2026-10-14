# Logos theme file: format v1

_For theme authors. Draft format v1, 2026-10-14. Validated by `ThemeRegistry` (`src/qml/Logos/Theme/`)._

A Basecamp theme is **one JSON file of colour values**. No code, no QML, no fonts. Basecamp reads
it as data, checks every value, and ignores anything it doesn't recognise, so a theme can't break
the app or run anything.

- **Schema:** [`basecamp-theme-1.schema.json`](basecamp-theme-1.schema.json) (JSON Schema 2020-12)
- **Every token with its default:** [`logos.dark.theme.json`](logos.dark.theme.json)
- **Complete example:** [`examples/catppuccin-mocha.theme.json`](../../examples/themes/catppuccin-mocha.theme.json)

## Minimal theme

```json
{
  "formatVersion": 1,
  "id": "yourname.sunset",
  "name": "Sunset",
  "author": "yourname",
  "mode": "dark",
  "palette": {
    "background": "#1A1423",
    "text": "#F6E7D8",
    "primary": "#FF7B54"
  }
}
```

Anything you leave out comes from Logos Dark, **token by token**, so a three-colour theme is valid.

## Fields

| Field | Required | Rule |
|---|---|---|
| `formatVersion` | yes | `1`. A higher number still loads (known keys only) and shows "made for a newer Basecamp". |
| `id` | yes | `<author>.<name>`, lowercase `a-z 0-9 _ -`, at least one dot, ≤ 64 chars. `dark` is reserved. Installing a theme with an existing id replaces it. |
| `name` | yes | Shown in Settings → Appearance. Plain text, ≤ 40 chars. |
| `author` | yes | Shown next to the name (`Sunset · yourname`). Plain text, ≤ 40 chars. |
| `mode` | yes | `"dark"` or `"light"`. Apps read it (`Theme.isDark`) to choose matching assets. |
| `palette` | no | Colour tokens (below). |
| `spacing`, `radius`, `typography` | no | Accepted and validated, **not applied yet**. Leave them out for now. |
| `$schema` | no | Point it at the schema for editor autocompletion. |

**Colours:** `#RRGGBB` or `#RRGGBBAA`, with alpha **last, as in CSS and Figma**. (Qt's own string
order is `#AARRGGBB`; Basecamp converts, so you never need to think about it.) Names like `"red"`
are rejected.

## Palette tokens (65)

Tokens are **roles**, not colours. Set what the thing *is*, and the right controls follow.

| Group | Tokens | What they paint |
|---|---|---|
| Backgrounds | `background` `backgroundSecondary` `backgroundTertiary` `backgroundElevated` `backgroundMuted` `backgroundBlack` `backgroundInset` `backgroundButton` | Window, sidebar, cards, inset wells (the sidebar's workspace hexagon is `backgroundInset`) |
| Surfaces | `surface` `surfaceRaised` `surfaceInteractiveHover` `surfaceRecessed` `surfaceContrast` | Controls and panels, plus their hover/recessed states |
| Text | `text` `textSecondary` `textSubtle` `textTertiary` `textPlaceholder` `textMuted` | From headings down to disabled hints |
| Borders | `border` `borderSecondary` `borderTertiary` `borderTertiaryMuted`* `borderSubtle` `borderHairline` `borderGlyph` `borderInteractive` `borderDark` `borderStrong` | Outlines and dividers |
| Primary | `primary` `primaryHover` `primaryPressed` `primarySoft` `primaryBorder`* `primaryForeground` | Main action colour; `primaryForeground` = text/glyphs **on** a primary fill (button label, switch handle, checkmark) |
| Accent | `accentOrange` `accentOrangeMid` `accentOrangeDeep` `accentBurntOrange` `accentYellowSoft` `accentForeground` | Selection highlights (active sidebar button, chips, badges); `accentForeground` = glyphs **on** the accent |
| Status | `success` `successHover` `successPressed` `successBorder` `error` `errorHover` `errorPressed` `errorBorder` `warning` `warningHover` `warningBorder` `info` `notification` | Notices, badges, validation |
| Brand | `brandMark` | The Basecamp logo (welcome page and sidebar) |
| States & overlays | `hover` `pressed` `disabled` `focus` `glassOverlay` `glassStrong` `overlayDark` `overlayLight` `overlayOrange` `scrim` | Interaction states, translucent layers, modal scrim |

\* **Derived:** if you set `primary` but not `primaryBorder`, Basecamp derives it (`primary` at 40%).
Likewise `borderTertiaryMuted` = `borderTertiary` at 20%. You can still set them explicitly.

⚠️ `accentOrange*` and `backgroundBlack` are *named* after the default look but are roles: in a blue
theme, `accentOrange` is your accent colour (see the Catppuccin example). A rename to role names is
proposed (ADR-010).

## How Basecamp loads it

1. **Install:** Settings → Appearance → **Upload your own theme…** → pick the `.json`. It's validated
   **before** anything is written. An invalid file shows the reasons and writes nothing. A valid
   one is copied to `<Basecamp data dir>/themes/<id>/theme.json` and selected.
   (Or drop it there by hand; developers can also point `LOGOS_THEME_PATH` at a folder of
   `<dir>/theme.json`.)
2. **Validate:** unknown keys and tokens → warning, ignored. Bad colours → dropped (that token
   falls back). Out-of-range numbers → clamped. Larger than 64 KiB, symlinked, bad `id` → rejected.
3. **Contrast check:** `text`/`background`, `text`/`surface`, `textSecondary`/`background` and
   `primary`/`background` are measured; under 3:1 is flagged.
4. **Apply:** Basecamp switches **live**: every view and the window chrome. **Apps** that use the
   Logos design system run in their own process and switch **the next time Basecamp starts**.

## Checklist before you share a theme

- [ ] Validates against `basecamp-theme-1.schema.json`
- [ ] `text` on `background` ≥ 4.5:1, `primary` on `background` ≥ 3:1
- [ ] `primaryForeground` reads on `primary`, `accentForeground` reads on `accentOrange`
- [ ] `brandMark` is visible on `backgroundSecondary` (sidebar) and `background`
- [ ] Looked at Settings, the welcome page and the sidebar in both selected and idle states
