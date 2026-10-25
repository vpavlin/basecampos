# ADR-065: Compact Basecampium has one bar, and New tab is the home page

**Status:** Accepted (experiment) · 2026-10-22 · Supersedes the two-bar layout of ADR-050 / ADR-051. The "New tab" name is replaced by "Basecamp" (ADR-073).

## Context
The compact layout had a title bar on top and a bottom bar (🔔 · + · tabs). Exploring Mull (Firefox for Android) showed a tighter model: one toolbar holds the page identity, the search, the tab count and a menu, and "New tab" opens a home page that is not itself a tab. Ours spent a whole bar on a title, and every + left an empty "New tab" behind in the count and the switcher.

## Decision
- **One bar** at the bottom: `[ pill ] [🔔] [tabs] [⋮]`. No top bar below 600 px.
- **Pill:** the current app's icon and title; tap → home; swipe sideways → previous / next tab. On home it is the **search field**; the New tab page hides its own search (`searchInBar`, `barQuery`) and Enter opens the first result.
- **New tab = home, transient:** not counted, not listed in the switcher, reused instead of duplicated, closed when another tab is shown. Opening an app from it still converts it in place (`m_pendingNewTab`).
- **⋮ menu:** New tab, Reopen closed tab, Close tab, Close all tabs | Applications, Settings, Package Manager. **Long-press / right-click the tab count:** New tab, Close tab, Close all tabs. Popup windows, opening upward.
- **Esc / Back:** close the switcher, or leave home for the last tab (`TabArea::handleBack`).
- Above 600 px: unchanged.

## Consequences
+ The app gets the full screen above one 60 px bar; the bar carries what used to need two.
+ No orphan New tabs; the count means open apps.
+ Shell apps (Settings, Applications, Package Manager) reachable from any tab, not only from home.
- No title bar: the pill is the only place the current app's name shows.
- Menus can't be seen in headless tests (popup needs a real input event); verified through their signals.
- Not taken from Mull: top / bottom setting, hide-on-scroll, a page-actions row, the tab sheet over content (app views are out-of-process).

## Evidence
`bc-v2` branch `mobile-onebar` (from `mobile-apps`); spec [MOBILE-ONE-BAR.md](../MOBILE-ONE-BAR.md).
