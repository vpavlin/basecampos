# ADR-049: Desktop notifications while Basecamp is in the background

**Status:** Accepted (experiment) · 2026-10-22 · supersedes "desktop: later, opt-in" in ADR-046

## Context
Notifications only help if they are seen. When Basecamp is behind other windows, in-app toasts are invisible. Every desktop has a notification centre users already trust and control (Do not disturb, history).

## Decision
- When Basecamp is not the active window, each notification that reaches the list is also handed to the desktop. Linux: `org.freedesktop.Notifications` over D-Bus (GNOME, KDE, most others), in `app/DesktopNotifier`, built when Qt6::DBus is present.
- Same filter as the list: muted apps and Do not disturb never reach the desktop.
- Shape: "<App>: <title>", body as escaped plain text, the app's icon (cached PNG), urgency normal (critical for `error`), a default action.
- Clicking it brings Basecamp to the front (passing the desktop's Wayland activation token) and does what a click in the list does (ADR-048).
- Dismissed in Basecamp, or Basecamp comes back to the front: the desktop copies are withdrawn; the list has them.
- Settings → Notifications → **Show on the desktop**, on by default, remembered (`Notifications/desktop`).

## Consequences
+ Real events are seen without Basecamp in front, through the OS's own controls.
+ No duplicates once the user is back: one place of record, the list.
- On by default (the earlier spec said opt-in): chosen because the switch is one click away and the content is already user-filtered; revisit if it feels noisy.
- macOS (UNUserNotificationCenter) and Windows (toasts) not built.
- Wayland focus on click depends on the desktop sending an activation token; not yet confirmed on GNOME.

## Evidence
`app/DesktopNotifier.{h,cpp}`, `NotificationsView.qml`. Headless with a fake notification daemon on a private bus, 2026-10-22: Notify with summary/escaped body/icon; click → app opened, notification dismissed, `CloseNotification`; return to Basecamp → remaining copies closed; a real receiver "New station" also delivered. On Alisher's GNOME: pending.
