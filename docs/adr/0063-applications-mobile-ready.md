# ADR-063: Applications: a catalog that works from phone to desktop

**Status:** Accepted (experiment) · 2026-10-22

## Context
Second real screen made mobile-ready by an agent following `mobile-ready-ui`. Before: at 390 the search and toolbar ran off the edge (250 items past it) with a 200 px sidebar on a phone; at 700 the view toggle was cut off; the Add Application dialog's ✕ and Install sat off-screen at 390.

## Decision
- **Catalog pattern** (RESPONSIVE §3.6): grid columns from a minimum cell width (104 / 140 / 180 → 3 / 3 / 5 across); on phones the categories sidebar becomes a **picker** on the left of the page's own compact bar, then 🔍 (`LogosSearchToggle`), then ⋯ (Reload, Manage Repositories, Show unavailable, grid / list).
- **Touch path for actions:** tap opens; **long-press** opens the actions menu anchored under the tile / row; list rows show a visible 44 px ⋯. The right-click handler accepts mouse / touchpad only, so a tap no longer also pops the menu.
- **Add Application dialog:** a full-screen page on phones (✕ 44 px, one scroll, Uninstall then Install / Update / Launch full width and last); unchanged at 700 / 1280.
- **Routing:** `applicationsInFront()` decides by the current tab (Basecampium), the active window (desktop layout) or the section (classic). Inside Applications a tap opens the dialog without filtering the grid; from elsewhere Applications still comes forward filtered to the app.
- **Bugs fixed on the way:** clearing a search left the search-ranked order (a pre-existing bug: the proxy sorted by an empty column; now falls back to catalog order, with a unit test); a width loop in the compact picker froze Basecamp opening Settings at 390 (Settings' engine keeps a hidden Applications copy) — found by the headless skill, bisected to `934094f`, fixed.
- `Theme.compactWindow` bound while the view is shown (restores on hide); `AppCheckMenuItem` replaced by `LogosMenuItem { checkable: true }`.

## Consequences
+ 0 overflow, 0 targets < 44 at 390 (comfortable density) for the view and the dialog; desktop unchanged.
- Long-press verified with QtTest touch events, not a finger; tiles have no visible ⋯ (long-press only), a deliberate launcher-grid choice.

## Evidence
bc-v2 `mobile-apps`: 934094f, a0b50ca, a402b77, b1466d3, 0500112 (Settings compactWindow), c60429a (unit-test build: 30 / 30 pass); before / after montages at 390 / 700 / 1280.
