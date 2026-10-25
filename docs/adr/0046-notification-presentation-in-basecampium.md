# ADR-046: Toasts and a bell list over the tabs, in a layer that never hides

**Status:** Accepted (experiment) · 2026-10-21 · The bell list is superseded by ADR-075 (notifications are feed items); toasts stay.

## Context
Basecampium needs somewhere to show notifications without covering the page or stealing clicks, and it must work under X11 and Wayland.

## Decision
A translucent `QQuickWidget` stacked on top, sized to its content, top-right under the bar: up to 3 toasts (info/success hide after 6 s, warning/error stay), and a list behind a bell with an unread badge. Each item shows the sender's icon and name; no colour stripe; text-only ✕ and "Clear all"; list items a shade apart from the panel. The current tab's own calm toasts are skipped. A click outside the list closes it and still reaches its target. The layer is never hidden and never zero height.

## Consequences
+ Works the same headless (Xvfb), under sway and on GNOME Wayland.
+ Clicking a notification opens its sender; an action button runs its action.
- Default shell not done yet. Desktop (OS) notifications: ADR-049. Tab dots and dismiss-on-open: ADR-048.
- The "never zero height" rule is easy to break: a view that does not render does no layout, and the layer silently stops showing new toasts (hit twice while building).

## Evidence
`NotificationLayer.qml`, `MainContainer::setupNotifications`, `TabArea` `BellButton`; Alisher's review on his screen (button overlap, ✕ box, item background, stripe, icon, outside click) all applied.
