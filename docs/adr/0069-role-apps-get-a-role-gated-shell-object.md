# ADR-069: Role apps talk to the shell through a role-gated `shell` object, not intents

**Status:** Accepted (experiment) · 2026-10-23 · Step 2 of ADR-067; amends its "holding a role grants restricted intents".

## Context
ADR-067 planned one restricted intent per operation. Moving Settings out of the shell showed what that costs: about 25 operations, two live models (the module inspectors), and the shell's own preferences, which live in the shell plugin rather than the host. Intents are one request, one answer, with no way to carry a live model or a signal. Settings also has to show other apps' icons and wallpaper previews, which the sandbox (rightly) won't let it read from disk.

## Decision
- **The host injects `shell` (`ShellApi`)** into the QML engine of a role app, next to `logos`. It is a narrow, named surface: everything the in-shell Settings already did, and nothing more. It is never the host's `backend`, which can install and uninstall without asking.
- **Every read and call is checked** against the current holder of the member's role, not just at injection. Give the role away and the object goes inert at once; get it back and it works without a reload.
- **The shell hands its preferences to the host** (`IShellHost::setShellPreferences`, ABI 3 → 4); the host exposes them only to the `settings` holder.
- **Images go through a host provider.** The host serves `image://basecamp/…` to role apps, and the sandbox lets `image://` through only for providers the host registered on that engine.
- **Intents stay** for one-shot asks from any app (`confirm_uninstall`, navigation). The role object carries what needs to be live.
- Settings gains **Default apps**: each role, the apps that can fill it, the one in use; a tap reassigns.

## Consequences
+ Answers ADR-067's open "subscriptions" question for role apps: live models and signals ride on the object, no new intent kind needed.
+ The shell API a Settings app needs is now written down in one header (`app/ShellApi.h`).
- Basecamp's Settings imports the shell's `Basecamp.Backend` / `Basecamp.Common` QML types. Fine for the bundled app; a third-party Settings needs them as public API or its own copies.
- Density is stored per QML engine, so changing it in Settings doesn't reach the shell until restart. Themes are process-wide and work.
- The developer inspectors (load/unload any module, call any method) ride on the `settings` role; a separate developer role is a candidate.
- Settings now starts like any app instead of instantly; not yet measured on real hardware.

## Evidence
Spec [§10](../SHELL-ROLES-SPEC.md#10-step-2-built); `app/ShellApi.*`, `app/interfaces/IShellHost.h`, `app/restricted/RestrictedUrlInterceptor.*` (`allowImageProvider`), `tests/sandbox_image_provider_test.cpp`; `apps/basecamp_settings/` (`DefaultAppsView.qml`). Verified headless at 390 and 1280: role given away and back live, theme from the app switches the whole window.
