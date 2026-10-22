# ADR-048: A dot on the tab; looking reads, opening dismisses

**Status:** Accepted (experiment) · 2026-10-22

## Context
The bell says *something* happened, not *where*. With several apps open as tabs, the user has to open the list to find which one. And a notification the user has already acted on (clicked it, the app opened) stayed in the list as clutter.

## Decision
- **Dot:** a tab whose app has unread notifications shows a small dot on its icon (`primary` colour, ringed in the bar colour). The current tab never shows one.
- **Looking reads:** while Basecamp is in front, the current tab's app is being looked at, so its notifications are marked read (`NotificationCenter::markAppRead`), and the dot and the badge go. In the background they stay unread until the user comes back.
- **Opening dismisses:** clicking a notification (toast, list, desktop copy) opens the sender and removes it. A Basecamp notice with nothing to open stays until closed.

## Consequences
+ The tab strip shows *where*, the bell shows *how many*; neither needs the list opened.
+ The list holds only what the user hasn't dealt with.
- The dot is one per app, not per notification; a busy app looks the same with 1 or 5.
- Default shell (sidebar) has no dot yet.
- Read is inferred from "tab current + window active"; a user who glances away with the tab open still counts as having seen it.

## Evidence
`TabArea::setAttention` / `TabLabel::setDot`, `MainContainer::onNotificationsChanged`, `NotificationCenter::unreadApps|markAppRead|activate`. Headless (sway + inspector), 2026-10-22: dot on the background receiver tab, gone once current; a post from the current tab's app read at once; toast click opens and dismisses.
