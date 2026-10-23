import QtQuick
import QtQuick.Controls

import Logos.Controls
import Logos.Theme
import Basecamp.Backend

// Basecamp's Applications as an ordinary app filling the `launcher` role
// (SHELL-ROLES-SPEC.md, ADR-067). It reads the catalog and opens, installs and
// uninstalls apps only through `shell`, the role-gated object the host injects;
// installing and uninstalling still go through the shell's own dialogs.
Rectangle {
    id: root
    objectName: "basecampApps.root"
    color: Theme.palette.background

    readonly property bool granted: typeof shell !== "undefined" && shell !== null
                                    && shell.roles.launcher === "basecamp_apps"

    Loader {
        anchors.fill: parent
        active: root.granted
        sourceComponent: Item {
            // A filter proxy is view configuration, so it belongs to whoever
            // draws the view; only the source model crosses from the host.
            AppsFilterProxy {
                id: uiAppsProxy
                sourceModel:   shell.appsModel
                typeFilter:    "ui_qml"
                excludeMainUi: true
                showUnavailable: appManagerView.showUnavailable
            }

            AppManagerView {
                id: appManagerView
                anchors.fill: parent

                appsProxy:   uiAppsProxy
                hostVariant: shell.appsModel ? shell.appsModel.hostVariant : ""
                repositories: shell.repositories
                loading:     shell.appsLoading

                onAppClicked:            (name, repositoryUrl) => shell.openApp(name, repositoryUrl, true)
                onManageAppRequested:    (name, repositoryUrl) => shell.openApp(name, repositoryUrl, false)
                onUninstallAppRequested: (name, repositoryUrl) => shell.uninstallApp(name, repositoryUrl)
                onNavigateToRepositories: shell.showRepositories()
                onRefreshRequested:      shell.refreshAppCatalog()
            }

            Connections {
                target: shell
                function onLauncherSearchRequested(query) {
                    appManagerView.applySearch(shell.takePendingSearch() || query)
                }
            }

            Component.onCompleted: {
                shell.requestCatalog()
                const pending = shell.takePendingSearch()
                if (pending) Qt.callLater(() => appManagerView.applySearch(pending))
            }
        }
    }

    Column {
        objectName: "basecampApps.notGranted"
        visible: !root.granted
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, 420)
        spacing: 8
        LogosText {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Another app is set as Applications")
            font.pixelSize: 18
            font.weight: Font.DemiBold
        }
        LogosText {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            color: Theme.palette.textSecondary
            text: qsTr("Basecamp's Applications works only while it is the default launcher. Choose it again in Settings under Default apps.")
        }
    }
}
