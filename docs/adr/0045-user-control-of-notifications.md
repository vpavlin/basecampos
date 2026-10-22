# ADR-045: The user decides per app; senders never learn the choice

**Status:** Accepted (experiment) · 2026-10-21

## Context
Notifications are the easiest channel to abuse. The user needs a simple way to stop an app, and an app must not be able to work around it or detect it.

## Decision
- Settings → Notifications lists every app that declares the intent (`IntentRegistry::modulesUsing`) with a switch; off removes what it posted and suppresses new ones; remembered in Basecamp's settings (`Notifications/muted`).
- A switched-off app is still answered `ok`.
- Limits for all apps: 5 per minute (then `failed`), plain text, title 80 / body 300, `tag` replaces instead of stacking.
- Basecamp's own notices cannot be switched off.
- A second switch, **Show on the desktop**, controls the OS hand-off (ADR-049).

## Consequences
+ One place to control it; no per-app settings screens to hunt for.
+ No oracle: an app cannot tell muted from shown.
- No Do not disturb or "list only" mode yet.
- An app the user has never opened still appears (declared, not used): honest, but may look noisy with many apps.

## Evidence
`NotificationsView.qml`; `backend.notificationApps` / `setNotificationsEnabled`; verified headless: switch off → `muted=receiver_ui` saved, list emptied.
