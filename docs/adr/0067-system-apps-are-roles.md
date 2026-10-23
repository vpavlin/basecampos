# ADR-067: Applications, Package Manager and Settings are apps that fill roles

**Status:** Proposed · 2026-10-23 · all three steps built as an experiment (Package Manager, Settings, Applications + New tab) · Builds on the shell-packages spec and upstream app-to-app intents. · Steps: ADR-068 (Package Manager), ADR-069 (Settings; the `shell` object amends "restricted intents" below), ADR-070 (Applications, New tab), ADR-071 (local icons only).

## Context
Applications and Settings are QML compiled into the shell and read host internals through the `backend` object; Package Manager is its own module but the shell special-cases it. No other app can replace them, and the shell API they need is never written down. Upstream intents already let any app launch another (`basecamp.apps.launch`) but deliberately hide which apps are installed, so a launcher can't be an ordinary app today.

## Decision
- The shell defines **roles**: `home`, `launcher`, `packages`, `settings` (later `search`). A manifest declares `"roles": [...]`; **the user assigns** each role; Basecamp ships and assigns defaults.
- The three system apps become ordinary `ui_qml` apps filling those roles. The shell opens roles, not apps; `sys:*` routes go away.
- Holding a role **grants restricted shell intents** (e.g. `basecamp.apps.list` for launcher/home, `basecamp.prefs.*` for settings). Restriction is checked against the current role holder, replacing the name-keyed allow-list.
- A missing or crashing role app falls back to a **minimal built-in page**; defaults holding a role can't be uninstalled.
- Order: Package Manager → Settings → Launcher and home.

## Consequences
+ Any part of the system UI can be replaced without writing a whole shell; the shell API gets written down by its first users.
+ The grant to see installed apps comes from a user's choice, not a self-declared name.
- System apps start like other apps (load time; preloading is an open question).
- Needs subscriptions (`apps.watch`), which intents don't have yet, and an icon provider apps can read.
- Unsigned apps can still claim a role; the user's assignment is the only gate until signatures exist.

## Evidence
Spec [SHELL-ROLES-SPEC.md](../SHELL-ROLES-SPEC.md); upstream `docs/app-to-app-intents.md`; `src/MainContainer.cpp` (`kModulesStackIndex`, `sys:*` routes).
