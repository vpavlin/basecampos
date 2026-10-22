# ADR-059: Settings: list → detail on phones, cards for inspector tables

**Status:** Accepted (experiment) · 2026-10-22

## Context
Settings was the first real screen made mobile-ready, by an agent following the skill. Before: every section cut off at 390, and at 700 the Module Inspector ran past the right edge.

## Decision
- Compact: the Sections list is the page (a one-line hint and a chevron per row); a tap opens the section full width under "‹ Settings"; the page's big title hides (the shell already shows it). Nested levels (Module Inspector → Interface) go back deepest first.
- Inspector tables: full table ≥ 760, three key columns when the pane is 600–759 wide with the rest in a **detail sheet** (a visible 44 px ⋯ per row, or a tap on the row; ✕ / tap outside / Esc close), **cards below 600** with equal-width 44 px actions.
- Panes are capped at the space left (`Layout.maximumWidth`), which fixed the 700 px overflow: table column minimums had inflated the pane, so it never switched to cards.
- Search folds per ADR-058.

## Consequences
+ Settings works at every size and looks the same or better on desktop.
- The Notifications rows are hand-built, not `LogosListRow` (it now emits `toggled` on row taps and can wrap its subtitle, but has no untinted-icon slot yet).

## Evidence
bc-settings 4fa6d85, c3a4624 (medium detail sheet, folding search with `LogosSearchToggle`); before / after montages at 390 / 700 / 1280.
