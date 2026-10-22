# ADR-058: In compact, search folds into an icon at the right of the navigation bar

**Status:** Accepted (experiment) · 2026-10-22

## Context
On a phone an always-on search bar costs a full row above every list (Alisher's screenshot of Settings → Apps Inspector).

## Decision
- In compact a search field never takes a row of its own: it folds into a **44 px 🔍 at the right end of the navigation bar** and unfolds **across that bar** only on tap. The bar's title hides while it's open; **✕ clears and folds**; it stays open while it holds text; Esc folds; focus goes into the field.
- DS: `LogosSearchToggle`; `LogosAppScaffold { searchEnabled: true }` hosts it and exposes `searchText`.
- Medium and expanded keep the full `LogosSearchBar`.

## Consequences
+ Lists get their first row back; search is one tap away in a predictable place.
- Search is one tap further on phones; an active query is visible only while unfolded (it stays unfolded while it has text, so it can't hide a filter).

## Evidence
DS fork a365240 (component, scaffold, lab Mail + Table), folded / unfolded renders at 390; Settings uses it at the right of the "‹ Settings" bar (bc-settings c3a4624; folded, unfolded, filtering, cleared at 390).
