# ADR-078: One app tile everywhere; icons are square, rounded only when drawn

**Status:** Accepted (experiment) · 2026-10-25 · Complements ADR-071 (local icons) and the icon contract (full-tile icons). Issue #40.

## Context
App icons looked different depending on where they appeared. Some PNGs had their own rounded corners baked in, others were square; letter tiles sat inside a second darker square; the three system apps each had a different background. Any baked radius disagrees with the radius the shell draws, so two shapes show.

## Decision
- **Icon assets are full-bleed squares.** No icon we ship has rounded corners or a rounded background in the image (corner alpha 255). The stub icons are generated that way (`experiments/objects-stubs/gen-icons.py --check`).
- **The renderer rounds.** One shared tile (`src/Basecamp/Icons/BasecampAppTile.qml` in QML, `src/AppTileIcon` for the Qt Widgets chrome) clips every icon to the same radius ratio (16 px at an 80 px tile, following the theme's roundness), with the same size and padding per context. It is used in Recent apps, Apps rows, Applications, the switcher, tabs, the dock, window title bars, cards and Settings rows. Third-party icons are clipped the same way.
- **Letters** for apps without an icon are drawn on the same plate, from the display name.
- **System apps share one look:** one background and one glyph colour for Applications, Package Manager and Settings. Package Manager gets a host-drawn system icon, because its own package ships a transparent cube.

## Consequences
+ Every icon has the same outline; changing the radius is one place.
+ Icon authors make one square image; the shell decides the shape.
- Catalog apps that ship rounded PNGs still show a small second corner until they ship square ones.
- Letter tiles are tinted per app in Applications but neutral in home's rows.

## Evidence
`src/Basecamp/Icons/BasecampAppTile.qml`, `src/AppTileIcon.*`, `tests/app_tile_icon_test.cpp`, `app/icons/system/package_manager.svg`. Commits 9ea066b, 6d3dc9d; experiments a25b1a1. Screen: [tiles-zoom](../basecamp-shell/tiles-zoom.png) (LEZ Wallet, Applications, Chat, a letter tile: one outline).
