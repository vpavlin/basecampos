# Notifications: spec

Status: draft, 2026-10-21. Prototype on the `notifications` branch, in Basecampium first.

## TL;DR

- Apps tell the user something happened ("new station found", "sync failed") through one shell
  intent, **`basecamp.notifications.post`**. No new runtime API: apps already call intents from
  QML, and the shell already provides its own `basecamp.*` intents.
- Basecamp itself posts too. First case: **an app's process stopped unexpectedly**, with a
  **Reopen** button. Today that is silent: the tab just goes dead (logos-basecamp#10).
- The shell shows a **toast** and keeps a **list** behind a bell with an unread count. Clicking a
  notification opens the app that sent it.
- The user stays in control: plain text only, a rate limit per app, mute per app, and a short list
  of things notifications must never be used for.

## 1. Prior art and gaps

| Source | What it says |
|---|---|
| Notion "Basecamp UI — Product" → Features: **Notifications**, **Alerts** | v.04, Medium, Backlog. Both pages are empty. |
| logos-basecamp#10 (2025-05) | "present error notifications clearly, including module crashes and automatic restart events". |
| logos-basecamp#319 (2026-08) | No sign that Basecamp itself is out of date; a stale install broke a trial silently. |
| Discord, Freequencies thread (2026-08-14) | App teams show their own in-app banners and ask for "proper desktop notifs". |
| Basecamp dark-pattern register | No notification type for absence ("you haven't opened X"). |
| Status Activity Center, mobile push briefs | Patterns for a list with unread state and grouping; mobile-first, different product. |

Nothing upstream defines a model, an API, permissions or UI. This spec proposes all four, kept
as small as the prototype needs.

## 2. Model

A notification:

| Field | Type | Rules |
|---|---|---|
| `id` | string | Set by Basecamp. |
| `app` | string | The sender's module name, set by Basecamp from the intent requester. Basecamp's own: `basecamp`. An app can never claim another app's name. |
| `title` | string | Required, plain text, max 80 characters. |
| `body` | string | Optional, plain text, max 300 characters. |
| `level` | `info` \| `success` \| `warning` \| `error` | Default `info`. |
| `tag` | string | Optional, max 64. A new notification with the same `app` + `tag` **replaces** the old one ("3 new stations" instead of three toasts). |
| `time` | timestamp | Set by Basecamp. |
| `read` | bool | Set by Basecamp. |
| `action` | object | Basecamp-only for now (e.g. **Reopen** after a crash). Apps get one implicit action: clicking opens the app. |

No markup, links, images or sounds in v1. Text is rendered as plain text.

## 3. App API

Declare the intent in the app's manifest (`metadata.json`):

```json
"uses": [{ "intent": "basecamp.notifications.post" }]
```

Post from QML:

```qml
logos.request("basecamp.notifications.post",
              { title: qsTr("New station"), body: name, level: "info", tag: "stations" },
              function (res) { /* res.ok; res.data.id */ })
```

- The shell is the provider, so the broker shows no chooser.
- Errors use the existing closed set: `not_declared` (missing from `uses`), `bad_request`
  (missing title, too long, wrong types), `failed` (rate limit).
- **Muted apps still get `ok: true`.** A sender must not be able to learn the user's settings.
- C++ backends post through their QML view for now. A backend-side call is an open question (§8).

## 4. Basecamp's own notifications

| Event | Level | Text | Action |
|---|---|---|---|
| An app's process exits while the app is open | `error` | "<App> stopped unexpectedly" / "Its process exited. Reopen it to continue." | **Reopen** |
| A newer Basecamp is available (#319) | `info` | later phase | **Download** |
| A core module crashed or restarted (#10) | `warning` | later phase: liblogos does not report it to the shell yet | none |

"Unexpectedly" means: not closed by the user, not uninstalled, not Basecamp quitting. The
crashed app is unloaded, so **Reopen** starts it fresh instead of showing the dead view.

## 5. Presentation

```
┌─[ λ New tab × ]─[ Receiver × ]─[+]──────── drag area ─────────[🔔 2][–][□][×]─┐
│                                                  ┌───────────────────────────┐ │
│                                                  │ ● Receiver               × │ │
│                                                  │ New station               │ │
│                                                  │ Parallel Society Radio    │ │
│                                                  └───────────────────────────┘ │
```

- **Toast:** top-right under the bar, at most 3 stacked, newest on top. `info` and `success` hide
  after 6 s (paused while hovered); `warning` and `error` stay until dismissed. Clicking the toast
  opens the sender; its button runs the action.
- **Bell:** in the bar (Basecampium) with the unread count. Opens a **list**, newest first, with
  per-item dismiss and "Clear all". Opening the list marks everything read.
- **While you are in the sender's tab**, its `info` and `success` toasts are not shown (still
  listed). You are already looking at it.
- Default shell: same toast layer; the bell sits at the bottom of the sidebar. Later phase.
- **Desktop notifications** (freedesktop portal, macOS Notification Center) when Basecamp is not
  the active window: later phase, opt-in in Settings.

## 6. User control and limits

- **Rate limit:** 5 per app per minute; extra posts get `failed` and are dropped. Replacing a
  `tag` counts too.
- **Mute** per app (Settings → Notifications; also from a notification's menu): nothing is shown or
  listed. **Do not disturb**: no toasts, list still fills.
- **History:** last 50, in memory only in the prototype. Persisting it is an open question (§8).
- Everything stays local. Notifications never leave the machine.

## 7. What notifications must not be used for

Enforced by review now, and by the catalog's checks later:

- Re-engagement: "you haven't opened X", streaks, "come back".
- Marketing, promotions, ratings prompts.
- Anything that needs urgency to work (fake countdowns, "act now").
- Repeating the same thing instead of using `tag`.

## 8. Open questions

1. A backend (C++/core module) post: a `basecamp.notifications` call through LogosAPI, or keep it
   QML-only?
2. Core module crashes: liblogos needs to tell the shell when a module process dies or restarts.
3. Persist the list across restarts, or keep it per session?
4. Who owns this upstream: the shell (Basecamp core) or the platform (`logos.notifications.*`
   that every frontend, including mobile, implements)?
5. Desktop hand-off: Qt has no portable notification API; per-OS code or a small library.

## 9. Prototype plan (`notifications` branch)

1. Host: a `NotificationCenter` (model, post, dismiss, read, mute, rate limit) on the backend.
2. The shell intent `basecamp.notifications.post`, serviced in C++.
3. Crash watch: keep watching each app's process after it loads; post the crash notification with
   **Reopen**.
4. Basecampium: toast layer + bell + list.
5. Receiver: `uses` the intent and posts "New station" when it discovers one, tagged so a burst
   collapses into one.

## 10. Prototype status (2026-10-21, `notifications` branch)

Done and checked on a hidden display (Xvfb), Basecampium:

- Receiver (`notifications` branch of receiver-basecamp) declares the intent and posts
  "New station"; the toast shows when you are in another tab, the bell counts it, the list
  shows it with its time.
- Killing receiver's process shows "receiver_ui stopped unexpectedly" with **Reopen**; Reopen
  starts it again in a new tab. Closing receiver normally posts nothing.

Learned on the way:

- `uses` entries are objects (`{ "intent": "..." }`); a bare string is ignored with a
  diagnostic and the request answers `not_declared`.
- The floating layer must never be zero height or hidden: a view that is not rendering does
  no layout, so a new toast or a just-opened list would never get its size.

Not done: Default shell, Settings → Notifications (mute UI, Do not disturb), desktop hand-off,
update notice (#319), core module crashes, persistence, unit tests for NotificationCenter.
