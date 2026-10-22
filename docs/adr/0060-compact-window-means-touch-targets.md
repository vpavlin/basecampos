# ADR-060: A compact window means touch-size targets, per engine and per session

**Status:** Accepted (experiment) · 2026-10-22

## Context
ADR-055 made every DS control reach 44 px at `density: "touch"`. Three real screens (Settings, Applications, Package Manager) then hit the same gap: a phone-width window at the default density still got 32–40 px combos, tabs, menu rows and paginator, so every app wrapped DS controls or bound private properties (`_d.fullMinWidth`). Alisher had parked the question; "fix all issues" reopened it.

## Decision
- `Theme.compactWindow` (bool, **session-only, never persisted**) and `Theme.touch = density === "touch" || compactWindow`.
- Every DS touch check uses `Theme.touch`. `_size()` **floors** tappables at 44 when `touch`, but only density **scales** (a compact window never makes things bigger than 44; touch density still does).
- `LogosAppScaffold` sets `compactWindow` from its own width. Each QML engine has its own `Theme`, so a hosted screen sets it at its root:
  `Binding { target: Theme; property: "compactWindow"; value: root.visible && root.width > 0 && Theme.windowClass(root.width) === "compact" }` + `Component.onDestruction: Theme.compactWindow = false`.
- The paginator compacts by itself under `touch`; `forceCompact` is the public override (apps must not touch `_d`).
- The lab treats any width under 600 as strict for small targets, at every density.

## Consequences
+ Apps stop wrapping DS controls at phone width; Package Manager dropped its paginator / tab / row bindings.
+ Desktop stays pixel-identical (diffed).
- A screen that forgets the Binding at its root falls back to desktop sizes on phones; the skill makes it the first line of a hosted screen.

## Evidence
DS fork PR #2 (eb433c2, merged 3f5efbe): 24 / 24 lab checks incl. 390 comfortable; 760 unit tests (ThemeCompactWindow suite). Package Manager 2b6de2d: 0 small targets at 390 comfortable.
