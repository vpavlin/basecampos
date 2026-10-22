# Notifications: spec

Status: prototype, 2026-10-21, `notifications` branch (Basecampium). Decisions: [ADR-042 to ADR-049](adr/).

## TL;DR

- Apps tell the user something happened ("new station found", "sync failed") through one shell
  intent, **`basecamp.notifications.post`**. No new runtime API: apps already call intents from
  QML, and the shell already provides its own `basecamp.*` intents.
- Basecamp posts too. First case: **an app's process stopped unexpectedly**, with **Reopen**.
  Before this, that was silent: the tab just went dead (logos-basecamp#10).
- The shell shows a **toast** with the sender's icon and keeps a **list** behind a bell with an
  unread count, and a dot on the tab of each app with unread notifications. Clicking a
  notification opens the app that sent it and dismisses it. While Basecamp is in the background,
  notifications also go to the desktop's own notification centre.
- **Settings → Notifications** lists every app that declares the intent, with a switch.
- Plain text only, a rate limit per app, and a short list of things notifications must never be
  used for.

## 1. Prior art and gaps

| Source | What it says |
|---|---|
| Notion "Basecamp UI — Product" → Features: **Notifications**, **Alerts** | v.04, Medium, Backlog. Both pages are empty. |
| logos-basecamp#10 (2025-05) | "present error notifications clearly, including module crashes and automatic restart events". |
| logos-basecamp#319 (2026-08) | No sign that Basecamp itself is out of date; a stale install broke a trial silently. |
| Discord, Freequencies thread (2026-08-14) | App teams show their own in-app banners and ask for "proper desktop notifs". |
| Basecamp dark-pattern register | No notification type for absence ("you haven't opened X"). |
| Status Activity Center, mobile push briefs | Patterns for a list with unread state; mobile-first, different product. |

Nothing upstream defines a model, an API, permissions or UI.

## 2. Model

| Field | Type | Rules |
|---|---|---|
| `id` | string | Set by Basecamp. |
| `app` | string | Sender's module name, set by Basecamp from the intent requester. Basecamp's own: `basecamp`. An app cannot claim another app's name. |
| `title` | string | Required, plain text, max 80 characters. |
| `body` | string | Optional, plain text, max 300. |
| `level` | `info` \| `success` \| `warning` \| `error` | Default `info`. Decides how long a toast stays (§5). |
| `tag` | string | Optional, max 64. Same `app` + `tag` **replaces** the earlier one. |
| `icon` | url | Set by Basecamp: the sender's package icon; the λ logo for Basecamp. |
| `time` | ms since epoch | Set by Basecamp. |
| `read` | bool | Set by Basecamp; opening the list marks all read. |
| `action` | `{ label, launch }` | Basecamp-only for now (**Reopen**). Apps get one implicit action: clicking opens the app. |

No markup, links, images or sounds from apps. Text is rendered as plain text.

## 3. App API

Declare it in the app's `metadata.json` (copied verbatim into the install, where the intent
registry reads it). Entries are objects; a bare string is ignored with a diagnostic and the
request answers `not_declared`:

```json
"uses": [{ "intent": "basecamp.notifications.post" }]
```

Post from QML:

```qml
logos.request("basecamp.notifications.post",
              { title: qsTr("New station"), body: name, level: "info", tag: "stations" },
              function (res) { /* res.ok, res.data.id */ })
```

- The shell is the provider, so the broker shows no chooser.
- Answers: `ok` + `{ id }`; `not_declared` (missing from `uses`); `bad_request` (no title, too
  long, wrong types, unknown level); `failed` (rate limit).
- **A switched-off app still gets `ok`.** A sender must not learn the user's settings.
- Not web-reachable: a `basecamp://` link cannot post one.
- Worked example: receiver-basecamp, `notifications` branch: it posts one tagged notification
  as stations appear ("New station" / "3 new stations").

## 4. Basecamp's own notifications

| Event | Level | Text | Action |
|---|---|---|---|
| An app's process exits while the app is open | `error` | "<App> stopped unexpectedly" / "Its process exited. Reopen it to continue." | **Reopen** |
| A newer Basecamp is available (#319) | `info` | later | **Download** |
| A core module crashed or restarted (#10) | `warning` | later: liblogos does not tell the shell yet | none |

"Unexpectedly" = not closed by the user, not uninstalled, not Basecamp quitting. Basecamp marks
a process before every intentional stop; any other exit is a crash. The crashed app is unloaded,
so **Reopen** starts it fresh.

## 5. Presentation (Basecampium)

```
┌─[ λ New tab × ]─[ Receiver × ]─[+]──────── drag area ─────────[🔔 2][–][□][×]─┐
│                                                  ┌───────────────────────────┐ │
│                                                  │ [icon] receiver_ui      ✕ │ │
│                                                  │ New station               │ │
│                                                  │ Parallel Society Radio    │ │
│                                                  └───────────────────────────┘ │
```

- **Toast:** top-right under the bar, at most 3, newest on top. Card: sender icon (16 px) and
  name, title, body, optional action button, ✕ as plain text (no box). No colour stripe.
  `info` / `success` hide after 6 s (paused while hovered); `warning` / `error` stay until
  dismissed.
- **Current tab:** the sender's own `info` / `success` toasts are not shown while its tab is open
  (still listed).
- **Bell:** in the bar, with the unread count as a badge. Opens the **list**: header with a
  text-only "Clear all", newest first, each item on its own background (a shade apart from the
  panel), with time and ✕. A click anywhere outside the list closes it; the click still goes
  where it was aimed.
- **Default shell:** later (same layer; bell at the bottom of the sidebar).
- **Tab dot:** a small dot on the icon of a tab whose app has unread notifications. Looking at
  the tab (it is current while Basecamp is in front) marks that app's notifications read, so the
  dot and the badge go. A post from the app you're looking at is read at once.
- **Opening dismisses:** clicking a notification (toast, list item, or the desktop copy) opens the
  sender and removes the notification; it has done its job. A Basecamp notice with no action
  (nothing to open) stays until closed.
- **Desktop notifications** while Basecamp is not the active window: the same notification goes
  to the desktop's notification centre (Linux: `org.freedesktop.Notifications` over D-Bus, works
  on GNOME and KDE). Title as "<App>: <title>", body as plain text (escaped), the app's icon,
  urgency normal (critical for `error`). Clicking it brings Basecamp to the front (with the
  Wayland activation token when the desktop sends one) and opens the sender. Dismissed in
  Basecamp, or Basecamp comes back to the front: the desktop copies are withdrawn, since the
  list has them. Muted apps and Do not disturb never reach the desktop. macOS and Windows: not
  yet.

## 6. Settings → Notifications

- One row per app that declares `basecamp.notifications.post`: icon, name, switch.
- **Show on the desktop** (on by default, remembered as `[Notifications] desktop=`): the
  desktop hand-off above.
- Off: nothing from that app is shown or listed, and what it already posted is removed. The app is
  not told. Remembered across restarts (`[Notifications] muted=` in Basecamp's settings).
- Empty state: "No installed app sends notifications yet."
- Later: Do not disturb (no toasts, list still fills), per-app "toasts off, list only".

## 7. Limits

- 5 per app per minute; extra posts answer `failed` and are dropped. Replacing a `tag` counts.
- Last 50 in memory; the list does not survive a restart.
- Everything stays local.

## 8. What notifications must not be used for

Enforced by review now, by catalog checks later:

- Re-engagement: "you haven't opened X", streaks, "come back".
- Marketing, promotions, ratings prompts.
- Manufactured urgency (countdowns, "act now").
- Repeating the same thing instead of using `tag`.

## 9. Implementation map (`notifications` branch)

| Part | Where |
|---|---|
| Store, limits, mutes, icons | `app/NotificationCenter.{h,cpp}`, exposed as `backend.notifications` |
| Intent | `ShellIntents.h` (`kNotificationsPostIntent`, in the shell's provides, not navigation, not web-reachable); serviced in `MainUIBackend`'s shell endpoint |
| Apps for Settings | `IntentRegistry::modulesUsing`, `backend.notificationApps`, `backend.setNotificationsEnabled` |
| Crash watch | `UIPluginManager::onPluginLoaded` (watch after load), `kStoppingProp` before each `stop()`, signal `appProcessCrashed` |
| Toasts and list | `src/Basecamp/Shell/NotificationLayer.qml` in a translucent `QQuickWidget` (`MainContainer::setupNotifications`) |
| Bell | `TabArea` (`BellButton`, `setNotificationCount`) |
| Settings page | `src/Basecamp/Settings/NotificationsView.qml` |
| Tab dots | `NotificationCenter::unreadApps` / `markAppRead`; `MainContainer::onNotificationsChanged` → `TabArea::setAttention` |
| Desktop hand-off | `app/DesktopNotifier.{h,cpp}` (Linux, `BASECAMP_DBUS_NOTIFY` when Qt6::DBus is found) |
| Self-test | `BASECAMP_NOTIFY_SELFTEST=1`: a local toast after 5 s, layer geometry in the log. `BASECAMP_NOTIFY_SELFTEST_APP=<module>`: posts as that app every 15 s (20 times) |

Learned the hard way: the floating layer must never be hidden or zero height. A view that is not
rendering does no layout, so a new toast or a just-opened list never gets its size.

## 10. Verified

Headless (Xvfb, and headless sway for Wayland), 2026-10-21: receiver's "New station" toast and
list; killing receiver's process shows the crash notice and **Reopen** restarts it; closing
receiver normally posts nothing; the Settings switch removes, suppresses and is saved; an outside
click closes the list.

On Alisher's GNOME Wayland screen: the list and toasts render, and the Settings page, icons and
outside-click close were confirmed working. The crash toast on that screen is not yet confirmed.

Headless (sway + a fake `org.freedesktop.Notifications` on a private bus, inspector-driven),
2026-10-22: the dot shows on a background tab's app and goes when its tab is current; a post from
the current tab's app is read at once; in the background a post reaches the desktop (summary,
escaped body, icon, actions); clicking it there opens the app, dismisses it and withdraws the
desktop copy; returning to Basecamp withdraws the rest; clicking a toast opens the app and
dismisses it. On Alisher's GNOME Wayland: clicking the GNOME notification brings Basecamp to
the front with the sender open.

## 11. Open questions

1. Backend (C++ / core module) posts: a call through LogosAPI, or QML only?
2. Core module crashes: liblogos has to tell the shell when a module process dies or restarts.
3. Persist the list across restarts?
4. Owner upstream: the shell (Basecamp) or the platform (`logos.notifications.*`, implemented by
   every frontend including mobile)?
5. Desktop hand-off on macOS (UNUserNotificationCenter) and Windows (toast); Linux is done.
6. Unit tests for `NotificationCenter` (the unit-test target does not build on this branch today).
