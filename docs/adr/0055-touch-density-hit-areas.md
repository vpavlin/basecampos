# ADR-055: At touch density every control has a 44 px hit area; desktop stays pixel-identical

**Status:** Accepted (experiment) · 2026-10-22

## Context
Measured in the lab, 23 of 35 control instances were under 44 px at `density: "touch"` because their sizes were literals density never touched. Apps had to wrap them.

## Decision
- At `density: "touch"` every tappable part of every DS control reaches **44 × 44** (hit area). Visuals keep their size: the checkbox box, switch track, 16 px info ring, 12 px ✕ grow a transparent target, not a bigger glyph.
- At comfortable / compact density, rendering stays **pixel-identical** (diffed before/after). One deliberate exception: the Paginator collapses to "‹ 4 / 10 ›" when it doesn't fit, where it was clipped.
- Behaviour, not only size: tooltips open on **long-press** on touch; a toast **pins on tap** (it only paused on hover); a table's checkbox cell toggles the row; touch rows show pressed / selected states.

## Consequences
+ Apps stop wrapping DS controls; the skill says so.
- Checkbox / radio / switch / slider / combo-row sizes now copy Qt's Basic style formula; an app on another Qt style could see a desktop size change.
- Long-press and tap verified with synthetic events only, not on a phone.

## Evidence
DS fork [PR #2](https://github.com/xAlisher/logos-design-system/pull/2): 18 / 18 lab checks pass; DS unit suite passes (plus `tst_LogosListRow`).
