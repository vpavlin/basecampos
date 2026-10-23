import QtQuick

import Basecamp.Backend

// What is left of the shell's own content views once Applications, Package
// Manager and Settings became apps filling roles (ADR-067): the handler for the
// shell's navigation intents. Each now opens whatever app fills the role.
Item {
    id: root

    // Kept for the shell's callers: both forward to the role apps.
    function applyAppSearch(query) { backend.requestLauncherSearch(query) }
    function showRepositories() { backend.requestSettingsSection("repositories") }

    // Only one instance may answer shell intents.
    property bool handlesIntents: true
    // Kept so older callers still bind; nothing is pinned any more.
    property int pinnedSection: -1
    signal repositoriesRequested()

    Connections {
        target: backend
        // Capabilities the shell itself provides *that are pure navigation* —
        // no state, no IPC, answerable on the spot. Adding the next one of
        // those is one more `case`. Anything unrecognised must still be
        // answered, or the requester waits out the full deadline for a reply
        // never coming.
        //
        // NOT EVERY SHELL INTENT REACHES HERE. `basecamp.packages.confirm_*` is
        // intercepted in C++ before this signal is emitted — see
        // kPackageConfirmIntents in MainUIBackend.cpp. Those need the
        // cascade-unload, the dependent caches and a pending dispatch id, and
        // their dialogs live in OverlayDialogs rather than this layer. So the
        // `unavailable` default below is not the whole story: check that list
        // before concluding an intent is unhandled.
        function onShellIntentRequested(requestId, intent, params, requesterName) {
            if (!root.handlesIntents)
                return
            switch (intent) {
            case "basecamp.repositories.manage":
                backend.requestSettingsSection("repositories")
                backend.respondToShellIntent(requestId, true, ({}), "")
                return
            case "basecamp.settings.open":
                backend.openRole("settings")
                backend.respondToShellIntent(requestId, true, ({}), "")
                return
            case "basecamp.apps.open":
                backend.openRole("launcher")
                backend.respondToShellIntent(requestId, true, ({}), "")
                return
            case "basecamp.packages.open":
                backend.openRole("packages")
                backend.respondToShellIntent(requestId, true, ({}), "")
                return
            }
            console.warn("ContentViews: unhandled shell intent", intent,
                         "from", requesterName)
            backend.respondToShellIntent(requestId, false, ({}), "unavailable")
        }
    }
}
