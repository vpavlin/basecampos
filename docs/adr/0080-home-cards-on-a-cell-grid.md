# ADR-080: Home's module cards sit on a cell grid, arranged in an edit mode

**Status:** Accepted (experiment) · 2026-10-25 · Extends ADR-074 (home is a feed with Recent apps). Epic #61, issues #49 to #54.

## Context
Home's modules view put the cards (Transactions, Messages, Communities, Wallet, News, custom ones) in 1 to 3 masonry columns. Height came from content and the only controls were a ⋮ menu with a size cycle and "Pin to the top". People could not place a card where they wanted it, and a card's size said nothing about how much it showed.

## Decision
- **A cell grid.** Columns by content width (1 under 600 px, 2 to 999, 3 to 1399, 4 above), fixed 120 px rows. Each card is a block of cells, 1 to all columns wide and 2 to 8 rows tall. The layout rules are pure functions in `BasecampHomeModules.js` (`place`, `resize`, `compact`, `fitTo`, `layoutFor`): no overlaps, cards pushed down then moved up.
- **Arranging only in an edit mode.** Entered by "Edit home" or a 500 ms header press. Drag to move, three handles to resize (bottom only on a phone), arrows and Shift+arrows from the keyboard. Done saves, Cancel or Esc restores, leaving home cancels.
- **Cards fit their block.** Height decides how many whole items show; 2 rows tall is one-line rows; width decides 1 to 3 inner columns. The header counts what is shown.
- **The ⋮ menu goes.** Size became resizing, pin became dragging to the top, Count new is a switch and Remove an × with Undo, both in edit mode.
- **One layout per column count,** saved by the shell for home (`home-settings.json`). A width with none borrows the widest saved one in reading order. The feed stays the default view; the Activity sidebar (feed only) has no grid.

## Consequences
+ People place and size cards directly, and a bigger card shows more instead of a preset.
+ The layout logic is unit tested (random operations never overlap, migration, per-width layouts).
+ Dragging never fights scrolling or card buttons, since it only happens in edit mode.
- Cards only move up, so holes can stay to the left of a moved or grown card.
- A layout arranged only on a phone makes wider screens derive 1-wide cards until arranged there.
- At phone width in Basecampium, Esc in edit mode also triggers the shell's Back and leaves home (found in testing; open).
- The "nothing fits" one-line summary is reachable only in theory with the current row height.

## Evidence
`apps/basecamp_home/qml/BasecampHomeModules.js`, `NewTabPage.qml` (grid, EditFrame, Handle), `BasecampHomeModule.qml`, `app/HomeSettingsStore.*`, `tests/object_cards_test.cpp` (grid*), `tests/home_settings_store_test.cpp`. Commits a60412b, e81eebb, 51cf096, 9cabb29, 3e7581b, 8d26cd5, aa2e037. Spec and results: [HOME-GRID-SPEC.md](../HOME-GRID-SPEC.md). Screens: [1280-edit](../home-grid/1280-edit.png), [1600-news-2x5](../home-grid/1600-news-2x5.png), [1600-messages-3x6](../home-grid/1600-messages-3x6.png).
