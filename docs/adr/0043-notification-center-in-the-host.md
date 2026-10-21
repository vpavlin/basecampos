# ADR-043: One NotificationCenter in the host; shells only render it

**Status:** Accepted (experiment) · 2026-10-21

## Context
Shells are replaceable packages (ADR-032/033). If each shell kept its own notifications, switching shells would lose them, and a shell could drop or forge them. Basecamp itself (crash watch, later updates) must be able to post with no shell involved.

## Decision
`NotificationCenter` lives host-side (`app/`), exposed as `backend.notifications`: the list, unread count, mute set, rate limit, tag replace and validation. The shell intent is serviced in C++ in `MainUIBackend`'s shell endpoint. A shell renders `items` and reacts to `posted` for toasts; it never stores anything.

## Consequences
+ Same notifications and the same rules in every shell; a shell package cannot bypass the rate limit or the user's switches.
+ Basecamp's own notices use the same path (`postSystem`).
- Adds to the backend surface every shell depends on (spec §5.4 of SHELL-PACKAGES: a versioned backend API).
- In-memory only for now (last 50).

## Evidence
`app/NotificationCenter.{h,cpp}`; `MainUIBackend` endpoint branch for `kNotificationsPostIntent`.
