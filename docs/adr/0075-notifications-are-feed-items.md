# ADR-075: Notifications are feed items; the bell goes away

**Status:** Accepted (experiment) · 2026-10-25 · Changes the presentation of ADR-046; ADR-042/043/045/048/049 stay. Issue #31.

## Context
Basecampium showed notifications twice: toasts, and a list behind a bell. With home becoming a feed of what apps share (ADR-072), a second list behind a bell is one more place to look.

## Decision
- **Every notification is also a `notification` object** published for the app that sent it (`app/NotificationObjects`, wired in `MainUIBackend`). The same tag means the same object, updated in place. It appears in the home feed and in search like any other object.
- **The bell and its badge are removed** from the top bar (wide) and the bottom bar (phone). On wide screens the feed is on the home tab; on a phone, tapping search opens it.
- **Both switches still apply:** Settings → Notifications decides whether an app may notify; Settings → Search decides whether its objects (notifications included) reach the feed. The NotificationCenter (ADR-043), toasts, desktop notifications (ADR-049) and the tab dot (ADR-048) are unchanged.
- **Phone toasts** sit just above the bottom bar (above the pills on home), one at a time; a calm toast doesn't replace a warning. Desktop keeps up to three, top right.

## Consequences
+ One place for everything that happened; notifications become searchable.
- Dismissing a toast doesn't remove its feed card: feed items outlive toasts.
- Warning and error toasts don't time out, so on a phone one can sit over a pinned button until closed, and on desktop they can cover the right side of home.

## Evidence
`app/NotificationObjects.*`, `src/Basecamp/Shell/NotificationLayer.qml`, `src/TabArea.cpp` (no `BellButton`). Commits 0839d6a, 00f315b, 0b55f61. Headless: a stub's notifications appear as feed cards with their level badge; no bell at 390 or 1280.
