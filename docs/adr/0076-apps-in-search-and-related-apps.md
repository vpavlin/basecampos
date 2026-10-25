# ADR-076: Search finds apps too; a type filter shows the apps behind it

**Status:** Accepted (experiment) · 2026-10-25 · Builds on ADR-072. Issues #32, #36.

## Context
Home's search only found objects. Typing "wallet" should find wallet apps, installed or not, and picking "Transactions" should say which apps those come from, so the user can go to the source.

## Decision
- **An Apps block above the results** when the query matches app names (word-prefix, like object search): icon, name, and on the right **Running**, an **Open** button, or an **Install** button for catalog apps not yet installed. "All apps" opens Applications with its filters cleared.
- **Related apps under a type pill:** the installed apps that have published objects of that type, in the same rows, most objects first. Apps switched off in Settings → Search are left out. It follows the store live.
- **Through the `shell` object only,** gated to the home role per call: `shell.findApps(query)`, `shell.relatedApps(type)`, `shell.installApp(name)`, plus the existing open path. Install opens the app's install dialog over home rather than sending the user to Applications.

## Consequences
+ Search becomes the way into apps, not just into content; the catalog is reachable from the bar.
+ "Which app does this come from" is answered without leaving home.
- The catalog list must be loaded for Install rows; icons stay local-only (ADR-071), so uninstalled apps show letters.

## Evidence
`app/AppSearch.{h,cpp}`, `app/ShellApi.*` (`findApps`, `relatedApps`, `installApp`), `tests/app_search_test.cpp`. Commits 5ef9037, e0f5e7d, 1be7618, 343e7c1, a4fa5bb. Screens: [app-search-390](../basecamp-shell/app-search-390.png), [related-apps-390](../basecamp-shell/related-apps-390.png).
