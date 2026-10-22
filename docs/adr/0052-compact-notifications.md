# ADR-052: Notifications in compact: banners on top, the list as a sheet above the bottom bar

**Status:** Accepted (experiment) · 2026-10-22

## Context
ADR-046's toasts (360 px, top-right) and list don't fit a phone, and the bell moved to the bottom bar (ADR-051).

## Decision
- Toasts become **full-width banners** under the title bar.
- The list becomes a **sheet just above the bottom bar** (up to 70 % of the height); ✕ grows to 16 px text with a 44 px target.
- A tap on a notification **closes the list first, then opens the sender** (dismissing the notification destroys its card mid-handler, so anything after `activate` never ran and the list stayed over the app).

## Consequences
+ The list opens where the thumb already is.
- Banners still cover the top of the content for 6 s (pushing content down is a follow-up).

## Evidence
bc-v2 1195764, 69019d9; headless repro of "notification from the list didn't open the receiver" (the list stayed on top) and the fix shot; the desktop list had the same latent bug, fixed by the same change.
