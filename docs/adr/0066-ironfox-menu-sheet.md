# ADR-066: The compact menu is a bottom sheet; menus fit their context

**Status:** Accepted (experiment) · 2026-10-22 · Builds on ADR-065.

## Context
The one bar's ⋮ opened a bare QMenu: a light desktop menu with cramped rows, greyed items that didn't apply, and no theme. IronFox 158 (Firefox for Android) moved its menus into bottom sheets of grouped cards, leaves out what doesn't apply, opens the tab list by swiping the toolbar up, and gives the tab list its own menu and search.

## Decision
- **⋮ opens a bottom sheet**, its own popup window over the shell (app views are out-of-process windows and would cover an in-window overlay): scrim, handle, a card of tab actions, the shell's apps as three tiles, and the destructive action alone in the theme's error colour.
- **Menus leave out what doesn't apply** rather than greying it out; labels name their target ("Close Settings", "Close all 3 tabs").
- **Swipe up on the bar's pill** opens the tab switcher.
- **The switcher has a header:** the count, or a tab search when there are more than 8 tabs, and a ⋮ (Reopen closed tab, Close all tabs in red).
- Other dropdowns (long-press on the tab count, the switcher's ⋮) use one themed style with 50 px rows and icons.

## Consequences
+ Touch-size, themed menus; the shell apps are one tap from any tab; destructive actions are visibly separate.
- Popups can't be seen in headless tests; a test hook (`BASECAMP_SHELL_MENU_SHOT`) saves them. The scrim's translucency and the gestures need a touchscreen.
- A destructive item inside a QMenu is a widget action (QMenu can't colour one item); the DS should grow a destructive menu item.

## Evidence
`bc-v2` branch `mobile-ironfox`; spec [MOBILE-IRONFOX.md](../MOBILE-IRONFOX.md).
