# ADR-053: Layout by window class: compact < 600 ≤ medium < 840 ≤ expanded

**Status:** Accepted (experiment) · 2026-10-22

## Context
Apps need one shared answer to "how wide is a phone": per-app pixel checks drift, and the same screen runs inside Basecampium's compact shell, a split view or a torn-off window.

## Decision
- Design-system tokens: `Theme.breakpoints` {medium 600, expanded 840, large 1200}, `Theme.windowClass(width)`, `Theme.pageMargin(width)` (16 / 20 / 40), `Theme.minTouchTarget` (44).
- The thresholds are Android's window size classes, so designers already know them.
- A screen measures **its own area's width**, never the screen.
- Between classes the layout changes; within a class it only stretches; reading width caps at ~720.

## Consequences
+ One vocabulary for apps, shells and agents ("compact", not "under 600 px").
- Two thresholds are coarse; patterns may still decide inside a class from the width they actually get (dashboard columns).

## Evidence
DS fork [PR #1](https://github.com/xAlisher/logos-design-system/pull/1); `docs/RESPONSIVE.md` §1.
