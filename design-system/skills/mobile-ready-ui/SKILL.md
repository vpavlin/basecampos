---
name: mobile-ready-ui
description: Build or change a Logos/Basecamp app's QML UI so it works from phone to desktop. Use whenever an agent writes or edits app screens with the Logos design system (import Logos.Controls / Logos.Theme) — new apps, new pages, layout fixes, "make it mobile", "responsive", "narrow window", "touch". Gives the breakpoints, the app frame, a pattern per common layout, touch rules, and a mandatory render-and-check step at 390 / 700 / 1280.
---

# Mobile-ready UI for Logos apps

Every screen you build must work at **390 px (phone), 700 px (tablet / narrow window) and
1280 px (desktop)**, and be proven so with renders. The full rules, with the reasoning, are in
the design system's `docs/RESPONSIVE.md`; this skill is the working checklist.

## 1. Decide layout by window class, never by pixels sprinkled around

```qml
readonly property string windowClass: Theme.windowClass(width)   // your own area's width
```

| Class | Width | Navigation | Columns |
|---|---|---|---|
| `compact` | < 600 | ☰ drawer, ‹ back on a detail | 1 |
| `medium` | 600–839 | icon rail | 1–2 |
| `expanded` | ≥ 840 | sidebar with labels | 2–3 |

Use `Theme.pageMargin(width)` for side margins, `Theme.minTouchTarget` (44) for hit areas.
Measure the page's own `width` (it may sit in a shell, a split, or a torn-off window).

## 2. Frame the app with `LogosAppScaffold` (unless a host already frames it)

**If a host shell already draws the title and back chrome** (a Basecamp tab, a Settings
section, a dialog), don't nest a scaffold: use the list → detail pattern instead. Compact = the
list is the page; a tap pushes the detail under a 44 px "‹ Parent" row.

Otherwise, do not hand-build a sidebar. Put pages inside `LogosAppScaffold` (`title`, `pageTitle`,
`navModel`, `currentNav`, `canGoBack`, `onNavActivated`, `onBackRequested`, `actions`). It gives
sidebar → rail → ☰ drawer, and ‹ instead of ☰ when a page has pushed a detail. 3–7 destinations,
one-word labels.

## 3. Pick the pattern, apply its rules

| Your screen is… | Rules (details: RESPONSIVE.md §3) |
|---|---|
| a list with a detail (inbox, chat, items) | 2 panes ≥ 600 (list 320/360, detail fills, first item shown); compact: list only, tap pushes the detail full width with ‹ |
| settings | one column, max 720 — centred when it's the whole page, left-aligned inside a detail pane; grouped `LogosListRow`s: chevron = goes somewhere, value = current setting, switch = on/off; destructive group last |
| a form | max 720 centred, page scrolls; expanded: 200 px label column; medium: labels above, short fields two per row; compact: one per row, full width, primary action full width and **last** |
| a dashboard | content max 1200 centred; columns from the width you get and a minimum card width (stat cards 4 / 2×2 / 1, never 3+1); side-by-side cards only if both stay usable |
| a table | expanded: full table; medium: 3 key columns + ⋯, detail in a sheet; compact: **no table**, rows become cards, tap → detail with ‹; filter + primary action on their own line, search folds (§4) |
| a catalog (icon grid + filters, no detail pane) | columns from a minimum cell width (≈ 104 compact, 140 medium, 180 expanded); a filter sidebar becomes a picker (menu) in the compact bar; tiles: tap opens, long-press for actions; list rows: a visible ⋯ |

Not listed? Combine these: a page is usually a scaffold + one pattern per region.

Navigation state rules (every list → detail, at any depth):
- Narrowing to compact with a detail open keeps the detail open, with ‹ shown.
- Back closes the deepest level first. If the host shows a back row, hide the inner one: one
  way back.
- Esc and the phone Back key do what ‹ does.

The compact bar of a hosted screen: the shell's title bar belongs to the shell, so **the first
row of the page is the page's own 44 px navigation bar**: the page title (or the primary
filter / picker) on the left, then 🔍, then ⋯ (overflow is always last, as on Android). On a
detail it reads "‹ Parent" instead of the title.

**Filters are not navigation.** A sidebar of filters (categories, types) is not a list of
destinations: below expanded it becomes pickers (dropdowns or a menu) in the toolbar or the
compact bar. Keep the rail / drawer for real destinations.

Width rules that bite in real screens:
- Cap a fill pane with `Layout.maximumWidth` = the space actually left. Wide content (table
  column minimums) otherwise pushes the pane past the window edge.
- Never switch layout on a width your own content can inflate: measure the container, not
  the content.
- A table that drops columns (medium) must show them somewhere: a ⋯ / detail sheet.
- Per-row controls that leave the row when it narrows (version pickers, secondary actions)
  move into the detail as an **action strip, last** (thumb reach).

## 4. Touch rules (no exceptions in compact or `density: "touch"`)

- Every tappable thing has a hit area ≥ 44 × 44. Visual can be smaller: wrap it in an `Item`
  of 44 with a `MouseArea` / `TapHandler`, or set the control's `implicitHeight` to
  `Theme.minTouchTarget`.
- Nothing hover-only: actions get a visible ⋯ button (`LogosIconButton` + `LogosIcons.more`,
  opening a `LogosMenu`); information in tooltips also appears as visible help text.
- No double-click: one tap opens. No horizontal scrolling of a page; long text elides or wraps.
- **Right-click menus need a touch path:** long-press, plus a visible ⋯ where rows have room.
  Open menus anchored to the item they belong to (`popup(item, x, y)`), never at the cursor
  (on touch the cursor position is stale).
- **One touch flag.** Compute it once and pass it down:
  `readonly property bool touch: Theme.windowClass(width) === "compact" || Theme.density === "touch"`.
  DS controls reach 44 at touch density by themselves; at phone width in comfortable density
  your own wrappers still use this flag.
- Help and error text sit visibly under fields. Pattern for tooltips: expose the tooltip string
  as a property on the control (`helpText`) and print it as the card's extra line or under the
  detail's action.
- Menus get `margins` so a ⋯ near the window edge doesn't open half off-screen.
- **Search folds in compact.** A search field never takes a row of its own on a phone: it
  folds into a 44 px 🔍 at the **right end of the navigation bar** (just left of ⋯ if the bar
  has one) and unfolds across that bar
  only when tapped (title hides; ✕ clears and folds; it stays open while it holds text; Esc
  folds). Use `LogosSearchToggle`; inside `LogosAppScaffold` set `searchEnabled` and read
  `searchText`. Medium and expanded keep a normal `LogosSearchBar`.

Design-system controls reach 44 on their own at `density: "touch"` (since the touch-targets
change); don't add wrappers for them. Your own tappable items (a custom row, an icon in a
`MouseArea`) still need the 44 hit area themselves.

## 5. Prove it (required before you say done)

With the design-system repo's responsive lab (`responsive-lab/`) for DS-only screens, or, for a
screen inside a real app, the app running headless with the QML inspector (resize the window to
each width, screenshot, read geometries):

```
shot.sh <page> 390 844 out-390.png --density touch
shot.sh <page> 700 844 out-700.png --density touch
shot.sh <page> 1280 800 out-1280.png
python3 montage.py all.png out-390.png out-700.png out-1280.png
```

Each shot prints `{"overflow": [...], "smallTargets": [...]}`. Done means:
1. no QML warnings on stderr;
2. `overflow` empty at all three widths;
3. `smallTargets` empty at 390 and 700 with `--density touch`;
4. you **looked** at the three PNGs (Read them) and each looks deliberate, not just squeezed;
5. pages that push a detail or scroll are shot in those states too (`--state …`).

To run and drive a real Basecamp headlessly (isolated runs, resize, click by objectName,
screenshots, touch, audit), use the **`headless-basecamp-ui`** skill.

A hosted screen (Package Manager, a `ui_qml` app) runs in **its own QML engine** with its own
`Theme` instance: set density and `Theme.compactWindow` inside that engine, not only in the
shell.

For app screens without the lab's report, run **`responsive-lab/app-audit.mjs <W> <H>
<rootObjectName>`** against the running app (QML inspector, `QML_INSPECTOR_PORT`): it walks the
screen's visible item tree in scene coordinates and prints the same `overflow` / `smallTargets`
(the inspector's `listInteractive` geometry is parent-relative and can't show overflow). Known
false positives: an inert `LogosTile` (`interactive: false`), and truncated text's long-press
tooltip at touch density. Give every tappable control an `objectName`, ignore invisible matches
(a screen can be instantiated twice), and **click by objectName, never by visible text** —
text matches hit the wrong element silently (another "Notifications" in the shell).

Attach the montage and the three report lines to the PR or reply. "Should work on mobile" is not
done.

## 6. Don't

- Don't branch on `Qt.platform.os` or screen size for layout; use the window class.
- Don't fix one width at the cost of another; re-shoot all three after every layout change.
- Don't build custom controls that duplicate a DS one to dodge a size problem; wrap the DS
  control and note the gap.
