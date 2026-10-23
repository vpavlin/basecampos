# ADR-071: Basecamp shows local icons only; no icon means the tile's letters

**Status:** Accepted (experiment) · 2026-10-23 · Part of step 3 (ADR-070). Upstream: logos-package-downloader#55.

## Context
The in-shell Applications loaded catalog icons straight from GitHub: the downloader resolves each version's `icon.path` against the index URL, so the tile's `Image` fetched `https://github.com/logos-co/logos-modules-release/releases/download/index/<sha>.png`. Packages themselves are fetched from the storage network first. So opening Applications told GitHub the user's IP and that they were browsing the Logos catalog, before installing anything, and the index's `sha256` for the icon was never checked. That runs against what Basecamp is for.

## Decision
- **The catalog model keeps local icons only.** `AppsModel::catalogIconUrl` accepts `file:` URLs and nothing else, so no screen gets a remote icon: not the role apps, not the shell's welcome page, not Add Application (whose own guard is now `file:`-only too).
- **The host's image provider serves local files only.** `image://basecamp/appicon/<name>` resolves on the host and refuses anything that isn't a `file:` URL; role apps can't fetch remote images anyway (sandbox).
- **No local icon means the tile's letters** (`LogosTile`'s monogram), the same thing third-party repositories already show. The role apps pass the served URL only when a local icon exists; otherwise they pass nothing, so a blank image never hides the letters.
- **The fix for catalog icons belongs upstream:** storage sources for icons like packages, fetched storage-first, checked against `sha256`, cached, handed over as `file:`. Filed as logos-package-downloader#55.

## Consequences
+ Browsing the catalog anywhere in Basecamp (Applications, the welcome page's search, Add Application) makes no network requests for icons.
- Uninstalled catalog apps show letters until #55 lands; installed apps show their real icon.
- A third-party repository that publishes `https://` icons loses them too; that's intended until icons can be fetched and verified the way packages are.

## Evidence
https://github.com/logos-co/logos-package-downloader/issues/55; spec [§11 "Catalog icons bypass the storage network"](../SHELL-ROLES-SPEC.md#11-step-3-built); `app/AppsModel.cpp` (`catalogIconUrl`), `tests/apps_model_test.cpp` (`catalogIconRejectsRemoteUrl`, `catalogRemoteIconKeepsInstalledLocalIcon`), `app/ShellApi.cpp` (`BasecampImageResponse`), `src/Basecamp/AppManager/AppGridDelegate.qml` (`iconOverride`), `apps/basecamp_apps/qml/AppGrid.qml`. Verified headless (sidebar shell, 1280, official catalog): 51 rows, 0 remote icon URLs, 7 installed with local icons, 35 uninstalled with none; the welcome page's search and Add Application show letters ([screenshot](../shell-roles/local-icons-1280.png)).
