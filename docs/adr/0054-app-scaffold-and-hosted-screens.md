# ADR-054: `LogosAppScaffold` frames apps; screens hosted by a shell use list → detail instead

**Status:** Accepted (experiment) · 2026-10-22

## Context
Every app was hand-building its navigation. Settings then showed the limit: inside a shell that already draws a title and back chrome, a scaffold adds a second bar and a ☰ hiding six entries.

## Decision
- **`LogosAppScaffold`** (DS): sidebar with labels when expanded, an 80 px icon rail when medium, a top bar with ☰ opening a dimmed drawer when compact; **‹ replaces ☰** when a page has pushed a detail (`canGoBack` / `backRequested`); `actions`, `navModel`, badges.
- **Hosted screens** (a Basecamp tab, Settings) don't nest a scaffold: compact = **the list is the page**; a tap pushes the detail under a 44 px "‹ Parent" row; Esc and the phone Back key do what ‹ does.
- Navigation state: narrowing with a detail open keeps it open; back closes the deepest level first; one way back (the inner back hides when the host shows one).

## Consequences
+ Apps get phone/tablet/desktop navigation for free; hosted screens don't double their chrome.
- Two patterns to choose between; the skill says when (ADR-057).

## Evidence
PR #1 (scaffold), Settings run (bc-settings 4fa6d85), the agent's report that §2 was wrong for hosted screens, folded into the skill (DS ec86a2b).
