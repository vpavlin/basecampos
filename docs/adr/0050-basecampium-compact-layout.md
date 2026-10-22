# ADR-050: Basecampium below 600 px: a phone layout, not a squeezed desktop

**Status:** Accepted (experiment) · 2026-10-22

## Context
If a Basecampium-style shell becomes the default, it has to reach phones. Its model (one app at a time, a launcher page, tabs to switch) is already how phones work; the default shell's persistent sidebar is not.

## Decision
Below **600 px** of window width Basecampium switches layout (above it nothing changes):
- the tab strip gives way to the **current tab's title, centred**; **no window buttons**;
- a **bottom bar** (60 px, 64×48 targets) with Notifications · New tab · Tabs (ADR-051);
- **Tabs** opens a **switcher**: open tabs as cards, two per row, current outlined, ✕ in a 44 px corner, a dot when the app has unread notifications;
- the New tab page tightens (margins 16, four tiles per row at 390, full-width search);
- Basecampium's minimum window size drops to 320×480 (the default shell keeps 800×600).

## Consequences
+ The shell adapts with a width switch, not a redesign; the same tabs and keys.
+ The experiment shows the shell is the easy part: app screens are where phones break (ADR-053..058).
- No card thumbnails yet (app views are out-of-process windows; `grab()` can't capture them), no swipe.
- On a desktop window narrowed below 600 the window buttons are gone; widen it or use the OS.

## Evidence
bc-v2 `mobile-layout` (1195764, 5816ff5, 69019d9); `docs/MOBILE-LAYOUT.md`; headless 390×844 renders (`renders/mobile/`); Alisher on his screen: "i see now!!!! it only works when i shrink".
