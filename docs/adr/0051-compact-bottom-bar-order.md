# ADR-051: Compact bottom bar: Notifications · New tab · Tabs

**Status:** Accepted (experiment) · 2026-10-22

## Context
The first cut had Home (λ) · New tab · Tabs, with the bell still in the top bar. Alisher: λ and + did the same thing; notifications belong down by the thumb; the order needed a reason.

## Decision
- One button for "open something": **+** (New tab = the launcher), drawn (2.4 px stroke, 26 px) because a font's "+" was too light, in the same secondary tone as its neighbours.
- **Order: 🔔 · + · [tabs]**. The primary action sits in the middle, where the thumb rests; the tab switcher sits on the right, where phone browsers keep it; notifications, used least, on the left.
- The bell (with its badge) lives in the bottom bar; its list opens as a sheet just above the bar (ADR-052).

## Consequences
+ Every bottom-bar action is one thumb reach away; nothing duplicated.
- No explicit "home" if no New tab is open: + makes one.

## Evidence
bc-v2 5816ff5, 69019d9; 390 px crop renders; Alisher's review ("leave only plus", "move notifications to bottom bar", "plus feels whiter").
