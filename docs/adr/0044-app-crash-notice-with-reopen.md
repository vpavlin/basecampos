# ADR-044: Basecamp tells the user when an app's process stops, and offers Reopen

**Status:** Accepted (experiment) · 2026-10-21

## Context
logos-basecamp#10 asks for clear notice of module crashes. Basecamp watched an app's `ui-host` process only while it loaded; after that a crash left a dead tab and no word.

## Decision
Keep watching each view process for the app's whole life. Every intentional stop (user closes the app, uninstall teardown) marks the process first (`kStoppingProp`); quitting is excluded. Any other exit posts an `error` notification, "<App> stopped unexpectedly", with **Reopen**, and unloads the dead app so Reopen starts it fresh.

## Consequences
+ The first Basecamp-originated notification; closes the app-crash half of #10.
+ Unloading avoids a zombie tab whose view no longer has a backend.
- Core modules (`logos_host`) are not covered: liblogos does not tell the shell when one dies or restarts.
- An app that exits on purpose by itself would be reported as a crash (none known).

## Evidence
`UIPluginManager::onPluginLoaded`, `appProcessCrashed`; verified headless: killing receiver's process shows the notice, Reopen restarts it, closing receiver normally posts nothing.
