# ADR-068: Package Manager is an ordinary app filling the `packages` role

**Status:** Accepted (experiment) · 2026-10-23 · Step 1 of ADR-067.

## Context
Package Manager was already its own module, but the shell special-cased it by name: a fixed stack slot, a preload, a placeholder, a `sys:packages` route, and an allow-list that let only `package_manager_ui` ask for `confirm_uninstall` / `confirm_upgrade`. Another app could not replace it, and the uninstall grant went to whoever had that name.

## Decision
- **Roles live in the manifest and in the user's settings.** `IntentRegistry` reads `"roles"` from `metadata.json` (a closed set; unknown roles are reported). Claiming a role grants nothing. `ShellRoles` keeps the user's choices (`[ShellRoles]` in `LogosBasecamp.conf`) over bundled defaults (`packages` → `package_manager_ui`).
- **Grants follow the role, not the name.** `confirm_uninstall` / `confirm_upgrade` are restricted to the current `packages` holder (`restrictIntentToRole`), and fail closed when nothing holds it.
- **No fixed slot.** Every way in (⋮ sheet, New tab, sidebar, search, restored tabs, `basecamp.apps.launch`) opens the role holder as an ordinary app tab.
- **A shell page stands in** when nothing can fill the role.

## Consequences
+ A third-party package manager can take over, uninstall prompts included, by the user's choice alone.
+ The role machinery (assign, open by role, fallback) is proven on the smallest step before Settings and Applications depend on it.
- Package Manager now also shows in the app grid next to the shell's Packages button (settled in ADR-070: role apps leave the home grid).
- The fallback page isn't verified live: Package Manager is embedded in the AppImage and can't be uninstalled. The "nothing holds it" path is unit-tested only.
- Still name-based: `UIPluginManager` listens to `package_manager_ui`'s install-failure signal by name.

## Evidence
Spec [§9](../SHELL-ROLES-SPEC.md#9-step-1-built); test app `experiments/roles-test-app/` (Alt Packages: refused `confirm_uninstall`, then given the shell dialog once assigned the role); `tests/shell_roles_test.cpp` (8 cases); `app/ShellRoles.*`, `app/IntentRegistry.*`, `app/ShellIntents.*`.
