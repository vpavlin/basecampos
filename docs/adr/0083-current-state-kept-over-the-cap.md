# ADR-083: Current state is kept when an app is over its cap

**Status:** Accepted (experiment) · 2026-10-26 · Builds on ADR-072 (apps publish typed objects) and ADR-082 (the search role keeps a copy). Issue #63.

## Context
The store keeps at most 500 objects per app and evicted the oldest by `time` when an app went over. Age was the only rule, so an app that publishes many events (transactions, messages, posts) pushed out the objects that say what is true now: an account and its balance, a community the user belongs to, a contact. The Wallet widget then lost the balances it exists to show. The stubs could not show it either: their Flood topped up to the cap on purpose, and the rate limit (20 requests per 10 s) stopped anything faster.

## Decision
- **A publisher marks current state.** New optional envelope field `state` (bool, default false). `true` means the object stays true until the app changes or retracts it (account, asset, community membership, contact); anything unmarked is an event. Only `true` is stored, so a publisher that never sets it gets exactly the envelopes and eviction it had.
- **Over the cap, events go first.** When an app is over 500, its oldest events by `time` are evicted; current state is never evicted to make room for events.
- **Current state has its own cap: 50 per app**, inside the 500. Past 50 the oldest current-state object goes; one batch with more than 50 answers `failed`, as a batch over 500 does. The flag is no way round the 500, and with at most 50 state there are always events to evict.
- **The flag is per publish.** Republishing the same `key` replaces the object, flag included; `retract` removes it either way. Every eviction is still recorded for `changesSince`, so a kept copy (ADR-082) follows it.
- **A publish over the rate is refused before its objects are validated**, and counted only once valid, as before. A flood sends many requests at once; there is no point checking objects that will be refused.
- **The stubs flood past the cap.** "Flood past the cap" sends 2000 events, newer than anything else the app published, in about 40 requests at once with no client-side wait. The store's own limits answer: about half the requests are refused by the rate limit and the rest are evicted down to 500. Stub Wallet publishes accounts, assets and summaries as current state, Stub Communities its communities, Stub Chat its contacts and conversations.

## Consequences
+ A busy app keeps what its widgets are for: after a flood past the cap the Wallet widget shows every account and asset, and Transactions shows the newest events.
+ The cap still bounds memory per app (500), and the 50 bounds what an app can pin.
+ Headless, after all five stubs flood past the cap: broad queries stall 48 to 68 ms (median) at 390 and 1280, within the #27 budget; the search app's answers equal the host's (32 of 32).
- Five apps flooding at the same instant stall the UI once for 216 to 322 ms (one app: under 100 ms).
- Views do not put current state first: after posts flood past the cap the Communities widget's 200 newest results hold none of the kept communities.
- An app that forgets the flag on one republish turns its account into an event that can be evicted; the spec says to publish current state with the flag every time.
- What counts as current state is the publisher's word; a misbehaving app can still fill its 50 with noise, only its own.
- Views do not use the flag yet; the envelope carries it if one wants to.

## Evidence
`app/ObjectStore.{h,cpp}` (`kMaxStatePerApp`, `validate`, `evict`, `allow`); tests `object_store_test` (`testState*`, `testFlaglessPublisherUnchangedBesideState`, `testOverTheRateRefusedBeforeValidation`) and `object_search_test` (`testKeptCopyNothingStaleAfterAFlood`, now with current state through the flood). Stubs: `experiments/objects-stubs/common/{StubApp,ObjectPublisher}.qml`. Basecamp source branch `keychain`, commits 491f357 and a99c10e. Spec and measurements: [SHELL-OBJECTS-SPEC](../SHELL-OBJECTS-SPEC.md) §3, §5, §8, [§15](../SHELL-OBJECTS-SPEC.md#15-current-state-over-the-cap-results).
