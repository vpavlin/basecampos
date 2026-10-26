# ADR-082: The search role keeps its own copy of the store; views keep their cards

**Status:** Accepted (experiment) · 2026-10-26 · Builds on ADR-072 (apps publish typed objects) and ADR-069 (the role-gated `shell` object). Issue #27.

## Context
After the flood fixes (§13 of the spec), one search per typing pause still blocked the UI for 100 to 300 ms with 2500 objects in the store. Measured per query in home's engine: about 65 ms in the search role (the host copied every envelope into its engine through `objects(filter)`, then the ranker split every object's words again), 10 to 15 ms for the pill counts (the host's matcher, the same splitting), and 70 to 80 ms for the feed, all of it done twice because home rebuilt its results both from the answer and straight after asking. The feed's cards were destroyed and made again on every change.

## Decision
- **The store numbers its changes.** Every change (publish, replace, retract, evict, expire, switch off, mark seen) moves a version on. `changesSince(version)` gives the envelopes changed and the ids removed since then, or a reset when it cannot tell (version 0, unknown, or older than the removals it remembers).
- **The search role keeps a copy.** New on the role-gated `shell`: `objectsSince(version, filter)`, for the `search` holder only, returning `{ version, reset, objects, removed, ids? }` (`ids`: what a narrowing filter allows, decided by the host). `basecamp_search` applies it to its own index and ranks that. `objects(filter)` stays, and `basecamp_search` falls back to it on a shell without `objectsSince`, so any app holding the role still works either way.
- **Words are split once.** An object's title, keywords, subtitle and field text become one string of lowercased words each, so "a word starts with the token" is a single substring search for space + token. Splitting runs a few hundred objects at a time while idle, or before a query that needs it; an empty bar needs none. The host keeps the same prepared index for `counts()` and its fallback ranker. The rules and the order do not change.
- **Views rebuild once per answer, and keep cards.** Home and its widgets rebuild from `resultsChanged` only. The feed keeps a card per object id across answers, moves cards that moved, and makes at most 6 new ones per pass, a frame apart.

## Consequences
+ Broad queries stall 50 to 70 ms (median) instead of 230 to 300, at 390 and 1280, headless; the refresh while a flood lands stalls under 100 ms.
+ The holder sees nothing it did not see before: `objectsSince` returns the objects `objects()` would.
+ Unit tests pin the parity: prepared vs. envelope ranking (C++ and JS), the kept copy vs. the store through a flood, eviction, retraction, replacement, marking seen, expiry and the switch.
- Two copies of the store's searchable text in memory (the holder's and the host's), bounded by the per-app cap.
- A third-party search app that keeps a copy must apply removals and resets correctly, or it ranks stale objects; the answer check (ids must exist and pass the filter) still drops anything gone.
- The widgets view still rebuilds its items on every answer and stays slower (221 ms median for "a").
- In the search app, which has no window, a QML Timer did not fire in the headless test, so idle work goes through `Qt.callLater`.

## Evidence
`app/ObjectStore.{h,cpp}` (`changesSince`), `app/ObjectRanker.{h,cpp}` (`Prepared`), `app/ObjectSearch.{h,cpp}` (`objectsSince`, the index), `app/ShellApi.*`, `apps/basecamp_search/qml/{Main.qml,BasecampSearchRanker.js}`, `apps/basecamp_home/qml/{NewTabPage,BasecampHomeFeed,BasecampHomeWidget}.qml`; tests `object_store_test` (`testChangesSince*`), `object_search_test` (`testPreparedMatchesRanker`, `testJsIndexMatchesRanker`, `testKeptCopyNothingStaleAfterAFlood`). Basecamp source branch `keychain`, commits 146732b to 37fe093. Measurements: [SHELL-OBJECTS-SPEC §14](../SHELL-OBJECTS-SPEC.md#14-broad-query-latency-results).
