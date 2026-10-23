import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Basecamp.AppManager
import Basecamp.Backend
import Logos.Theme

Item {
    id: root

    // Entry point for the shell: the welcome page's search routes a result
    // here and hands over the query that produced it.
    function applyAppSearch(query) { appManagerView.applySearch(query) }
    // Settings is an app filling the `settings` role now (ADR-067).
    function showRepositories() { backend.requestSettingsSection("repositories") }

    // The desktop layout opens App Manager and Settings as two windows, each
    // an instance of this file pinned to one section; -1 follows the backend.
    property int pinnedSection: -1
    // Only one instance may answer shell intents.
    property bool handlesIntents: true
    readonly property int section: pinnedSection >= 0 ? pinnedSection
                                                      : backend.currentActiveSectionIndex
    // Repositories live in Settings, which may be another instance.
    signal repositoriesRequested()

    // Section indices come from ShellSection (src/ShellSections.h), the single
    // source of truth shared with MainContainer's C++ switch and
    // SidebarPanel.qml's viewSections order.
    readonly property int sidebarAppManager: ShellSection.AppManager
    readonly property int sidebarSettings:   ShellSection.Settings
    readonly property int sidebarPackages:   ShellSection.PackageManager

    // The App Manager shows the catalog, which is fetched the first time
    // something needs it rather than at launch.
    readonly property bool appManagerShown:
        root.section === root.sidebarAppManager
    onAppManagerShownChanged: if (appManagerShown) backend.requestCatalog()
    Component.onCompleted: if (appManagerShown) backend.requestCatalog()

    // The App Manager's view of the catalog. Declared here rather than handed
    // over by the backend: a filter proxy is view configuration, so it belongs
    // to whoever draws the view. Only the source model crosses, as a plain
    // QAbstractItemModel*.
    AppsFilterProxy {
        id: uiAppsProxy
        sourceModel:   backend.appsModel
        typeFilter:    "ui_qml"
        excludeMainUi: true
        showUnavailable: appManagerView.showUnavailable
    }

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
                backend.setCurrentActiveSectionIndex(root.sidebarSettings)
                backend.respondToShellIntent(requestId, true, ({}), "")
                return
            case "basecamp.apps.open":
                backend.setCurrentActiveSectionIndex(root.sidebarAppManager)
                backend.respondToShellIntent(requestId, true, ({}), "")
                return
            case "basecamp.packages.open":
                backend.setCurrentActiveSectionIndex(root.sidebarPackages)
                backend.respondToShellIntent(requestId, true, ({}), "")
                return
            }
            console.warn("ContentViews: unhandled shell intent", intent,
                         "from", requesterName)
            backend.respondToShellIntent(requestId, false, ({}), "unavailable")
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.palette.background   // was "#1e1e1e": ignored the theme
        visible: !glassBg.visible
    }

    // Desktop window: frosted glass, the blurred wallpaper under a tint
    // (the backdrop image, shifted to where this window sits).
    Item {
        id: glassBg
        anchors.fill: parent
        clip: true
        readonly property var info: typeof glass !== "undefined" ? glass : null
        visible: root.pinnedSection >= 0 && info !== null

        Image {
            x: glassBg.info ? -glassBg.info.offsetX : 0
            y: glassBg.info ? -glassBg.info.offsetY : 0
            width: glassBg.info ? glassBg.info.rootWidth : 0
            height: glassBg.info ? glassBg.info.rootHeight : 0
            source: glassBg.visible ? "image://backdrop/blur?" + glassBg.info.revision : ""
            cache: false
            smooth: true
        }
        Rectangle {
            anchors.fill: parent
            color: Theme.colors.getColor(Theme.palette.background, 0.9)
        }
    }

    // Content views stack — only App Manager and Settings live here.
    // Apps (backend idx 0) is the C++ WorkspaceArea (QDockWidget-based),
    // and Modules (backend idx 2) is the sandboxed package_manager_ui
    // QQuickWidget.
    StackLayout {
        id: contentStack
        anchors.fill: parent

        // Settings left this stack: it is an app filling the `settings` role
        // now (ADR-067), so only App Manager remains.
        currentIndex: 0

        // App Manager (sidebar sidebarAppManager -> stack index 0)
        AppManagerView {
            id: appManagerView
            backgroundOpacity: root.pinnedSection >= 0 ? 0 : 1

            appsProxy:      uiAppsProxy
            hostVariant:    backend.appsModel ? backend.appsModel.hostVariant : ""
            repositories:   backend.repositories
            loading:        backend.appsLoading
            onAppClicked: function(name, repositoryUrl) {
                // Primary click — fast-path launch for installed apps.
                backend.openApp(name, repositoryUrl, ({}), true)
            }
            onManageAppRequested: function(name, repositoryUrl) {
                // Context menu's Install… / App details… — force the dialog
                // open. The modal is the install confirmation (version picker
                // + required packages), so both items land here.
                backend.openApp(name, repositoryUrl, ({}), false)
            }
            onUninstallAppRequested: function(name, repositoryUrl) {
                backend.uninstallApp(name, repositoryUrl)
            }
            onNavigateToRepositories: backend.requestSettingsSection("repositories")
            onRefreshRequested: backend.refreshAppCatalog()
        }
    }
}

