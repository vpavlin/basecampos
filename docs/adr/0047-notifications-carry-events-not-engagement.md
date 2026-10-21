# ADR-047: Notifications carry events, never engagement

**Status:** Proposed · 2026-10-21

## Context
The dark-pattern register already bans absence notices. Every platform that added notifications saw them turn into re-engagement and marketing.

## Decision
Notifications report something that happened (a station appeared, a sync failed, an app stopped). They must not be used for: re-engagement ("you haven't opened X", streaks), marketing or ratings prompts, manufactured urgency, or repeating the same event instead of using `tag`. Enforced by review now and by catalog checks later; violating apps can be switched off by default or delisted.

## Consequences
+ Keeps the channel worth reading; matches the dark-pattern register.
- Needs an owner for review and a catalog rule; not machine-checkable in general.

## Evidence
NOTIFICATIONS-SPEC §8; Basecamp dark-pattern register (Notion).
