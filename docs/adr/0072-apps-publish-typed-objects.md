# ADR-072: Apps publish typed objects; the search role ranks; views draw by type

**Status:** Proposed · 2026-10-24 · not built · Builds on ADR-042/043 (notifications), ADR-067 to ADR-071 (roles). Spec: [SHELL-OBJECTS-SPEC.md](../SHELL-OBJECTS-SPEC.md).

## Context
The shell can show notifications from apps but nothing else: it can't find a transaction, list recent messages or offer "Join" on a community without opening each app. A home screen built from content types (Transactions, Messages, Communities, Contacts) needs apps to hand the shell what they choose to share, in a shape the shell can search and draw. The roles work gave a way to let a user-chosen app do a system job.

## Decision
- **Apps publish objects** through intents (`basecamp.objects.publish` / `retract`). An object has an app-chosen `key` (same key replaces), a **type from a shared list** (account, asset, transaction, message, post, community, contact, news, notification; `x.<app>.<name>` for an app's own), a `kind`, plain-text title, typed `fields`, `subjects`, `refs`, up to three `actions`, optional `progress` and `time`.
- **Types, not apps, organise what the user sees:** two apps publishing `transaction` fill one Transactions card.
- **Split the jobs:** the host stores and enforces; the **`search` role ranks** (query + filter → ids) and draws nothing; **views draw by type** (home, launcher, the switcher) with shared per-type cards and a generic fallback. A host ranker answers when no app holds `search`.
- **Actions go back to the publisher** (`basecamp.objects.action`), including text for reply.
- **Objects join by subject** (`logos:pk:…`, `<network>:<address>`), never by display name.
- **Revocation is real:** switching an app off in Settings → Search, or uninstalling it, deletes what it published. v0 keeps everything in memory, on the device.
- **The privacy boundary is left open**, but every type field is marked *label* or *content* now, so the decision (content in the object, pointer, or per field) can be applied without changing the types.

## Consequences
+ One search and one feed across apps, with each app deciding what it exposes; the search engine and the home screen are separately replaceable.
+ Notifications become one type; their intent and Settings switch stay.
- v0 copies personal content (message text, amounts) into the shell's memory. It must not ship outside the experiment before the privacy question is decided.
- A shared type list needs an owner, and cross-app joins need someone to say which address belongs to which contact.
- Per-object pictures (avatars, coin logos) aren't covered yet: remote URLs are out (ADR-071).

## Evidence
Spec [SHELL-OBJECTS-SPEC.md](../SHELL-OBJECTS-SPEC.md) §2 to §9; [SHELL-ROLES-SPEC.md](../SHELL-ROLES-SPEC.md) Part B; [NOTIFICATIONS-SPEC.md](../NOTIFICATIONS-SPEC.md) §2 (the envelope it generalises).
