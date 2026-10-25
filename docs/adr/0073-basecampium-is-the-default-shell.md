# ADR-073: Basecampium is the default shell, named Basecamp; the old one stays as Basecamp Legacy

**Status:** Accepted (experiment) · 2026-10-25 · Builds on ADR-065 and ADR-070. Issue #29, epic #35.

## Context
Basecampium was only on in builds made with `BASECAMPIUM=ON`, and users saw two names for one product: "Basecampium" on the home page and "New tab" on its tab. It has become the blueprint for the main shell, but people still rely on the old shell, and safe mode needs something to fall back to.

## Decision
- **Basecampium is the default.** A fresh profile, or one with no shell saved, starts it. The bundle ships it in `plugins/shells/basecampium/`, so every build has it.
- **One rule decides the shell** (`app/interfaces/ShellChoice.h`, shared by `window.cpp` and `ShellPrefs`). The `BASECAMP_SHELL` environment variable wins. Otherwise a saved package wins. A saved `legacy`, or the `""` or `default` of older profiles, gives the old shell. Nothing saved gives Basecampium.
- **Names users see:** Basecampium is "Basecamp", and the old shell is **Basecamp Legacy** (plus "Basecamp Legacy (desktop)" for its desktop layout). The home tab is titled "Basecamp" and the "Basecampium" heading is gone. Internal names (`basecampium.*` object names) stay, because tests and tools depend on them.
- **Safe mode and the crash fallback** start Basecamp Legacy. A start that leaves its "starting" marker behind is a strike; a normal quit clears it. A shell the user chose falls back after one strike, the default after two in a row (`ShellChoice::strikesToFallBack`), so one killed or very short run doesn't move a fresh profile to Legacy for good.

## Consequences
+ One product name; the experiment shell is what people get by default, and Legacy is one choice away in Settings.
+ The bundled copy needs no package-trust prompt.
- A user-installed `basecampium` package still wins over the bundled one, so an old test install can shadow newer bundled code.
- Choosing "Basecamp Legacy (desktop)" saves `layout=desktop`, and switching back to Basecamp keeps it.

## Evidence
`app/interfaces/ShellChoice.h`, `tests/shell_choice_test.cpp`, `nix/app.nix` (bundles the shell), `apps/basecamp_settings/qml/AppearanceView.qml`. Commits 9f74b2e, 79db59b, 101df27 (local branch `basecamp-default`). Headless: a fresh profile opens "Basecamp"; Legacy and back works ([home-fresh-390](../basecamp-shell/home-fresh-390.png)).
