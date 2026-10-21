# ADR-042: Apps post notifications through a shell intent, `basecamp.notifications.post`

**Status:** Accepted (experiment) · 2026-10-21

## Context
Apps run in their own process (`ui-host`) and reach Basecamp only through the view runtime. Nothing lets them tell the user that something happened; teams draw their own banners (Freequencies) and ask for "proper desktop notifs". The runtime's intent surface is frozen, and the shell already provides its own `basecamp.*` intents (settings.open, apps.launch, packages.*).

## Decision
A notification is a shell intent, `basecamp.notifications.post` `{ title, body?, level?, tag? }`, answered `{ id }`. Apps declare it in `metadata.json` (`"uses": [{ "intent": "basecamp.notifications.post" }]`) and call `logos.request(...)` from QML. The shell provides it; it is not navigation and not web-reachable.

## Consequences
+ No change to the frozen runtime or to any SDK; any `ui_qml` app can use it today.
+ The broker's existing `declaresUse` gate is the permission: undeclared apps get `not_declared`, and Settings can list exactly the apps that may notify.
+ The sender identity comes from the broker, so an app cannot post as another app.
- QML only: a C++ backend or core module has to go through its view (open question in the spec).
- Shell-specific name: another frontend (mobile) would need the same intent, or a platform-level `logos.notifications.*`.

## Evidence
NOTIFICATIONS-SPEC §3; `ShellIntents.h` `kNotificationsPostIntent`; receiver-basecamp `notifications` 7b7fb9a posts "New station"; `uses` must be objects (bare string → `not_declared`, found while testing).
