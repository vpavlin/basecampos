# ADR-057: Rules reach whoever writes UI: the `mobile-ready-ui` agent skill

**Status:** Accepted (experiment) · 2026-10-22

## Context
Most new app UI will be written by agents. A rules document they never read doesn't change their output; the shell being phone-ready doesn't help if every app screen isn't.

## Decision
- The DS repo carries `skills/mobile-ready-ui/SKILL.md`: window classes, the scaffold (and when not to use it), one recipe per common layout, touch rules, the folding search (ADR-058), and a mandatory render-and-check step before "done".
- The skill is tested by use: an agent made Basecamp's Settings mobile-ready from the skill alone; its feedback (hosted screens, navigation state, pane caps, click by objectName) went back into the skill the same day.
- `docs/RESPONSIVE.md` holds the reasoning and screenshots; the skill stays a checklist.

## Consequences
+ "Mobile-ready" becomes the default output of agents, not a later fix.
- The skill has to evolve with the DS (it now says DS controls need no wrappers, after ADR-055).

## Evidence
Skill in PR #1 / #2, symlinked into `~/.claude/skills`; Settings run report (bc-settings 4fa6d85).
