# ADR-070: Applications fills `launcher`, the New tab fills `home`

**Status:** Accepted (experiment) · 2026-10-23 · Step 3 of ADR-067; builds on ADR-065 (New tab is the home page) and ADR-069.

## Context
After steps 1 and 2, Applications (AppManager) and the New tab page were the last system UI compiled into the shell. Both need the catalog and the installed-app list, which upstream intents deliberately hide from ordinary apps, plus install and uninstall, which must keep asking the user.

## Decision
- **One `ShellApi` for every role**, each member gated by its own role (settings, launcher, home), with `openRole` for any role app. It is injected into every app that *claims* a role, so a later assignment works without reloading; claiming alone still grants nothing.
- **`basecamp_apps` holds `launcher`:** the catalog model, open / install / uninstall, repositories, and a search handed over from the shell. Install and uninstall go through the shell's own dialogs.
- **`basecamp_home` holds `home`, and Basecampium hosts it inside the New tab** rather than opening it as a tab. The bar's text arrives as `shell.homeQuery`, Enter as `homeQueryAccepted`; `launch(name)` and `openRole(role)` turn the New tab into that app.
- **Apps holding a role leave the home grid:** they already sit in the shell's row of system buttons.
- **The shared tile, its menu and the install dialog stay in the shell**, which still uses them; role apps import them like the design system.
- The shell's `ContentViews` shrinks to its navigation-intent handler.

## Consequences
+ All four roles have an app; the shell keeps only the frame, the confirmation dialogs and one fallback page per role.
+ A launcher or home screen can be replaced the same way Settings was (ADR-069).
- Home is Basecampium-only: the sidebar and desktop layouts have their own welcome page, and list Basecamp's Home as an app. A "system app" manifest flag could hide role apps where the shell already has buttons.
- The home app counts as a loaded app to the host though it is never a tab.
- Not tried yet: assigning `launcher` or `home` to another app (same mechanism as `settings`), and uninstall from Applications.

## Evidence
Spec [§11](../SHELL-ROLES-SPEC.md#11-step-3-built); `apps/basecamp_apps/`, `apps/basecamp_home/`, `src/MainContainer.cpp` (`loadHomeApp`, `adoptHomeWidget`, `openRole`). Verified headless: Basecampium at 390 and 1280, sidebar shell at 1280, 0 QML errors (bar search, Enter, tile opens an app with no stray tab, search hand-off, install dialog).
