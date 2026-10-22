# ADR-062: UI is verified headlessly with the `headless-basecamp-ui` skill

**Status:** Accepted (experiment) · 2026-10-22

## Context
Every UI claim this session was proven by running Basecamp on a private headless compositor and driving it through the QML inspector. The know-how lived in scratch scripts with hard-coded paths, and parallel agents nearly stopped each other's runs.

## Decision
- A skill, `headless-basecamp-ui` (`~/.claude/skills/headless-basecamp-ui/`), packages it: `sway-up.sh` (headless sway), `bc-run.sh` (isolated Basecamp per `RUN` + `PORT`, optional shell `.so`, fake notification daemon), `resize.sh` (by pid), `ui.mjs` (click by objectName, props, eval in any engine, keys, touch via QtTest, screenshots), `app-audit.mjs` (overflow + targets < 44 in scene coordinates).
- Rules it encodes: never the user's display; one run name and port per instance; stop by exact user dir, never `pkill -f` a pattern in your own command; Wayland + GL, not offscreen (icons go blank); click by objectName, never text; hosted screens are separate engines; an all-commands-time-out run is a blocked UI thread → check CPU, bisect builds (ptrace is blocked).

## Consequences
+ Any agent can verify a UI change the same way; `mobile-ready-ui` points to it for real app screens.
+ It found a real regression on its first cold run (Basecamp froze opening Settings at 390 in the Applications branch).
- Esc / Back can't be sent: the inspector types text only; QtTest isn't in the bundle unless added to the import path.

## Evidence
Skill files; cold-start run on a fresh compositor (start, resize, click, screenshot, audit: Settings list at 390 = 87 items, no overflow, no small targets); the freeze reproduced on two compositors and isolated to the Applications branch by comparing builds.
