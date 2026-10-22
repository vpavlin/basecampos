# Basecampium on a phone: compact layout (experiment)

Status: experiment, 2026-10-22, `mobile-layout` branch (on top of `notifications`).
Question: if a Basecampium-style shell became the default, how far is it from a phone?
Decisions: [ADR-050 to ADR-059](adr/).

## TL;DR

- Basecampium adapts with a **width switch, not a redesign**: below **600 px** the tab strip gives
  way to **one bottom bar** (current tab / search · Notifications · Tabs · ⋮) and a **tab
  switcher** of cards. No window buttons, no title bar. New tab is the home page, not a tab.
  Updated after Mull: [MOBILE-ONE-BAR.md](MOBILE-ONE-BAR.md) (ADR-065).
  Above 600 px nothing changes.
- **Notifications** carry over: full-width banners at the top, the list as a **sheet above the
  bottom bar**, a dot on the Tabs button and on the card of an app with unread notifications.
- **What it does not show:** that apps work on a phone. App screens are still laid out for a
  desktop (the receiver's header runs off the right edge at 390 px). That gap is in the design
  system and the apps ([DS-MOBILE-AUDIT.md](DS-MOBILE-AUDIT.md)), not the shell.

## Why Basecampium and not the default shell

| | Basecampium | Default shell |
|---|---|---|
| Navigation model | one app full-screen, switch between them (= a phone) | persistent sidebar + workspace |
| Home | New tab page = launcher, reused as is | needs a new screen |
| Port | strip → bottom bar + switcher; same tabs, same keys | sidebar → drawer, layout backbone changes |

## Anatomy (below 600 px)

> Superseded by the one bar: [MOBILE-ONE-BAR.md](MOBILE-ONE-BAR.md). Kept for the record.

```
┌───────────────────────────────┐
│          receiver_ui          │  title bar: the current tab's title, centred; no window buttons
├───────────────────────────────┤
│ [banner, full width]          │  toasts become banners
│                               │
│          app / New tab        │
│                               │
│ ┌ Notifications ───── Clear ┐ │  list = sheet above the bottom bar
│ └───────────────────────────┘ │
├───────────────────────────────┤
│    🔔²        +       [2]•    │  Notifications (badge) · New tab · Tabs (count, dot)
└───────────────────────────────┘
```

- Order: the primary action (**+**, a New tab = the launcher) in the middle where the thumb
  rests; **Tabs** on the right, where phone browsers keep it; **🔔** on the left, opening the list
  as a sheet above the bar.
- **Tabs**: a grid of cards, two per row: title, large app icon, ✕ in a 44 px corner, current
  tab outlined in `primary`, dot when that app has unread notifications. Tap opens.
- Touch targets: bottom buttons 64×48, cards 150 high, close corners 44.
- New tab page: margins 16, tiles 56 on an 84 pitch (four per row at 390), search full width.
- Basecampium's minimum window size drops to 320×480 (the default shell stays at 800×600).

## Verified

Headless (sway, window 390×844, inspector-driven), 2026-10-22: compact switch at < 600 px; New tab
page and an app in compact; banner; switcher with count, current outline and unread dot; tap a
card → that tab, its notifications read, dot gone; bell → sheet above the bottom bar. Renders in
the MMORPG workspace (`renders/mobile/`).

## Next

1. Thumbnails on cards (app views are out-of-process windows; `grab()` can't capture them).
2. Swipe between tabs; swipe a card to close.
3. Banners push content down instead of covering it (they cover the switcher header today).
4. Safe areas; soft keyboard.
5. The design-system token layer from the audit (control sizes, hover replacements), then one
   app made compact end to end.
6. Android build of Basecamp (Qt for Android) only after 1-5.
