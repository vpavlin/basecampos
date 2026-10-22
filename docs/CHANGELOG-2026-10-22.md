# Change log, 2026-10-22: notifications, phone layout, responsive design system

One day's work across four repositories. Decisions: ADR-048 to ADR-066 ([adr/](adr/)).

## Repositories and branches

| Repo | Branch | What |
|---|---|---|
| `xAlisher/basecamp-theming` | `notifications` | notification centre follow-ups (dots, dismiss on open, desktop hand-off) |
| `xAlisher/basecamp-theming` | `mobile-layout` | Basecampium phone layout, mobile-ready Settings / Applications, DS snapshot, ADRs |
| `xAlisher/logos-design-system` | `feat/responsive` (PR #1) → `feat/touch-targets` (PR #2) | window classes, scaffold, touch targets, lab, skill |
| `xAlisher/logos-package-manager-ui` | `mobile-ready` | Package Manager phone to desktop |

## Notifications (ADR-048, 049)
- Dot on the tab of an app with unread notifications; looking at the tab reads them.
- Opening a notification opens its app and dismisses it.
- Desktop notifications (Linux, D-Bus) while Basecamp is in the background; Settings → "Show on the desktop" (on). Confirmed on GNOME Wayland: a click brings Basecamp to the front with the app open.
- Fix: tapping a notification in the list closed nothing (the card was destroyed mid-handler); the list now closes first.

## Basecampium phone layout (ADR-050, 051, 052)
- Below 600 px: centred title, no window buttons, bottom bar 🔔 · + · [tabs], tab switcher of cards with unread dots.
- Notifications as full-width banners; the list as a sheet above the bottom bar.
- Minimum window 320 × 480 for Basecampium.

## Design system (ADR-053 to 058, 060, 061)
- Tokens: `Theme.windowClass` (compact < 600 ≤ medium < 840 ≤ expanded), `breakpoints`, `pageMargin`, `minTouchTarget` (44).
- `LogosAppScaffold` (sidebar / rail / ☰ drawer, ‹ back, search), `LogosListRow` (chevron / value / switch rows; row tap emits `toggled`), `LogosSearchToggle` (search folds into the bar on phones), `menu` icon.
- Touch: every control ≥ 44 at touch density; **a compact window counts as touch** (`Theme.compactWindow`, per engine, session-only); desktop pixel-identical.
- Menus: trailing check mark for checkable items, default margins. Table: untitled headers aren't tap targets. Tab bar binding loop fixed. Paginator compacts by itself (`forceCompact`). Tooltip long-press, toast tap-to-pin.
- Responsive lab (`.#responsive-lab`): six screens, `shot.sh` / `check-all.sh` (overflow + targets < 44), `app-audit.mjs` for real apps; 24 / 24 checks pass. DS unit tests: 760 pass (new suites for compact window, menu check, table header, list row).
- Docs `RESPONSIVE.md` and the agent skill `skills/mobile-ready-ui`.

## Real screens made mobile-ready by agents following the skill (ADR-059, 063, 064)
- **Settings:** list → detail on phones, inspector tables → cards, medium detail sheet, folding search; fixed a 700 px overflow that predated the work.
- **Applications:** catalog pattern, category picker, long-press menus, full-screen Add Application on phones; fixed the search-order bug (pre-existing), the tap-pops-menu touch bug, the routing that filtered the grid, and a freeze its own first commit introduced (found by the headless skill, bisected).
- **Package Manager:** filters → pickers, table → cards, details → rows with an action strip; its UI tests fixed (49 / 49) and unit tests 39 / 39.
- Basecamp's unit-test build fixed (missing DS headers): 30 / 30.

## One bar, after Mull (ADR-065)
- Compact Basecampium: one bottom bar `[ current tab · search ] [🔔] [tabs] [⋮]`, no title bar.
- New tab is the home page: not counted or listed, closes when left; the bar's pill is its search.
- ⋮ menu (tabs, reopen, close all, shell apps); long-press the tab count; swipe the pill to switch tabs; Esc / Back leaves the switcher or home.
- Spec from exploring Mull on GrapheneOS: [MOBILE-ONE-BAR.md](MOBILE-ONE-BAR.md).

## After IronFox (ADR-066)
- The ⋮ menu is a bottom sheet: tab actions, the shell apps as tiles, "Close all" alone in red.
- Menus leave out what doesn't apply; swipe up on the bar opens the tabs; the switcher has a ⋮ and, with many tabs, a search.
- The bar menu was first restyled in place (theme colours, 50 px rows, icons) for the Mull demo.
- Spec: [MOBILE-IRONFOX.md](MOBILE-IRONFOX.md).

## Tooling (ADR-056, 062)
- `headless-basecamp-ui` skill: private headless sway, isolated runs (`RUN` / `PORT`), resize by pid, `ui.mjs` driver (objectName clicks, eval in any engine, touch, screenshots), `app-audit.mjs`, fake notification daemon.
- `mobile-ready-ui` skill: rules + mandatory render-and-check at 390 / 700 / 1280; refined after each real screen.

## Still open
- Real-device checks: long-press and tap on a touchscreen; Esc / phone Back (the inspector can't send keys).
- macOS / Windows desktop notifications.
- Card thumbnails and swipe gestures in the tab switcher; banners pushing content down.
- An untinted app-icon slot on `LogosListRow`; Settings' notification rows still hand-built.
