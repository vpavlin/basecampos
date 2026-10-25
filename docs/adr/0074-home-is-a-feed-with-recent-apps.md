# ADR-074: Home opens on the activity feed, with Recent apps; system apps are ordinary apps

**Status:** Accepted (experiment) · 2026-10-25 · Builds on ADR-070 and ADR-072. Issues #30, #34, #41.

## Context
Home opened on a grid of modules, showed a fixed row of installed apps and kept Applications, Packages and Settings behind special buttons. Once those three became role apps (ADR-067 to ADR-070) there was no reason to treat them differently. The "N new since …" header kept saying "Nothing new" when it wasn't true.

## Decision
- **The feed is the default view;** the modules grid is one tap away. Its header is a static **Activity**; unseen items keep their dot.
- **Recent apps** replaces the app row: recently closed and recently installed apps merged, newest first, at most 12. Then whichever of Applications, Packages and Settings aren't already listed, so a fresh profile shows exactly those three. Install times are recorded by the host when an app first appears in the app list (not on the first list after start), in `recently-installed.ini` next to `recently-closed.ini`; uninstalling removes the entry.
- **Applications, Packages and Settings are normal apps:** listed, opened and closed like any other, with their own icons. The special buttons and menu tiles are gone.
- **Search text:** "Search Basecamp…"; a type pill keeps its own ("Search transactions…").
- **Width:** on wide screens the search bar and the Apps block span the card column. **On phones** the filter pills sit directly above the bottom search bar, so filter and query are under the thumb.

## Consequences
+ A newcomer sees the three apps that matter first; later the row follows what they actually use.
+ The home page is about what happened (activity), not about chrome.
- Recent apps is in the host and reached only through the home role's `shell` object.
- The pills reorder by count, so on a phone the active pill can scroll out of view.

## Evidence
`app/RecentApps.h`, `app/RecentlyClosedStore.h`, `apps/basecamp_home/qml/NewTabPage.qml`, `tests/recent_apps_test.cpp`. Commits 90041e6, 0120e11, cd4d1bb, 27b5178. Screens: [home-fresh-390](../basecamp-shell/home-fresh-390.png), [home-feed-1280](../basecamp-shell/home-feed-1280.png).
