# ADR-077: The tab switcher is the Apps page; the bars have no menus

**Status:** Accepted (experiment) · 2026-10-25 · Replaces the menus of ADR-066; builds on ADR-065. Issue #33.

## Context
In Basecamp a "tab" is an open app, so calling it a tab borrows a browser word for no gain. The ⋮ menus on the switcher and the bottom bar held New tab, Reopen closed tab, Close tab, and tiles for Applications, Settings and Packages. Those tiles became ordinary apps (ADR-074), and the rest hid the one action people need on a phone: closing apps.

## Decision
- **Rename:** "N open apps", "Close all apps", "Search your apps", and "Apps" as the switcher's name.
- **No ⋮ menus** in the switcher header or the bottom bar. The search pill opens home; an app closes with × on its card or Close all.
- **Phone:** **Close all apps** is a button pinned at the bottom of the page, always visible. Closing one card keeps the page open; closing the last app lands on home.
- **Reopen closed app** leaves the UI (Ctrl+Shift+T stays); recently closed apps live in Recent apps instead (ADR-074).

## Consequences
+ Fewer places to look; the destructive action is visible instead of buried.
- New tab and Close this tab are no longer one tap from the bottom bar; that was accepted.
- Close all closes every app even while the page's search shows only some.

## Evidence
`src/TabArea.cpp` (switcher, no `showSheet` / `showSwitcherMenu`). Commits 0f47061, aba9ccd, dc1b9c7. Screen: [apps-page-390](../basecamp-shell/apps-page-390.png).
