# ADR-061: Menu check marks, menu margins, table header taps, deferred backgrounds

**Status:** Accepted (experiment) · 2026-10-22

## Context
Small DS defects found by the app runs: checked menu items had no mark (apps hand-drew one), a ⋯ at the window edge could open a menu off-screen, an untitled table column's header was a 20 px tap target, and `LogosTabBar` logged an `implicitWidth` binding loop in two apps.

## Decision
- `LogosMenuItem` draws a **trailing** check (16 px, primary) when `checkable && checked`; checkable items always reserve the space; plain items unchanged.
- `LogosMenu` has default `margins` (8 px) so popups stay inside the window.
- `LogosTable`: an untitled, non-sortable header cell is **not** a tap target; a titled one is tappable across the whole cell, ≥ 44 under `touch`.
- Implementation rule for DS controls: nothing inside `background` / `contentItem` may carry an id the control binds to, and no binding may read `background` — Qt then creates it early and layouts loop. `LogosTabBar` now builds its background as an inline component.

## Consequences
+ Apps use `LogosMenuItem { checkable: true }` instead of hand-drawn rows; Applications' `AppCheckMenuItem` can go.
- The lab couldn't reproduce a menu actually leaving the window (Basic style already clamps it); margins remove one possible cause.

## Evidence
DS eb433c2: tests `test_check_mark_*`, `test_untitled_header_cell_is_not_a_tap_target`, `test_no_implicitWidth_binding_loop_at_implicit_width`; the loop warning gone from Package Manager's log.
