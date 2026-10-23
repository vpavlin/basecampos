import QtQuick
import QtQuick.Controls

import Logos.Controls
import Logos.Theme

// Basecamp's Settings as an ordinary app filling the `settings` role
// (SHELL-ROLES-SPEC.md, ADR-067). Everything it changes goes through `shell`,
// the role-gated object the host injects into this engine only while this app
// holds the role. Opened without the role, it says so instead of half-working.
Rectangle {
    id: root
    objectName: "basecampSettings.root"
    color: Theme.palette.background

    // Injected only while this app holds the role; if the user then gives the
    // role away, `shell` goes inert and this switches to the explanation.
    readonly property bool granted: typeof shell !== "undefined" && shell !== null
                                    && shell.roles.settings === "basecamp_settings"

    Loader {
        id: content
        anchors.fill: parent
        active: root.granted
        sourceComponent: SettingsView {
            id: settingsView

            repositories:        shell.repositories
            repositoriesLoading: shell.repositoriesLoading
            downloadSource:      shell.downloadSource
            uiModulesModel:      shell.uiModulesModel
            coreModulesModel:    shell.coreModulesModel
            modulesLoading:      shell.modulesLoading
            repositoriesAvailable: shell.repositoryManagement

            onRepositoryRefreshRequested: shell.refreshRepositories()
            onRepositoryAddRequested:     url => shell.addRepository(url)
            onRepositoryRemoveRequested:  url => shell.removeRepository(url)
            onRepositoryEnabledRequested: (url, e) => shell.setRepositoryEnabled(url, e)
            onDownloadSourceRequested:    source => shell.setDownloadSource(source)
            onRepositoriesBecameVisible: {
                Qt.callLater(shell.refreshRepositories)
                Qt.callLater(shell.refreshDownloadSource)
            }

            onAppsRefreshRequested:       shell.refreshUiModules()
            onAppLoadRequested:           name => shell.loadUiModule(name)
            onAppUnloadRequested:         name => shell.unloadUiModule(name)
            onAppsInspectorBecameVisible: Qt.callLater(shell.refreshUiModules)

            onModulesRefreshRequested:      shell.refreshCoreModules()
            onModuleLoadRequested:          name => shell.loadCoreModule(name)
            onModuleUnloadRequested:        name => shell.unloadCoreModule(name)
            onModuleInspectorBecameVisible: Qt.callLater(shell.refreshCoreModules)

            function showSection(section) {
                if (section === "repositories") showRepositories()
            }

            Connections {
                target: shell
                function onRepositoryOperationCompleted(operation, url, success, error) {
                    settingsView.reportRepositoryResult(operation, url, success, error)
                }
                function onSectionRequested(section) {
                    settingsView.showSection(shell.takePendingSection() || section)
                }
            }

            // Opened by "Manage repositories" somewhere else: land there.
            Component.onCompleted: {
                const pending = shell.takePendingSection()
                if (pending) Qt.callLater(() => settingsView.showSection(pending))
            }
        }
    }

    Column {
        objectName: "basecampSettings.notGranted"
        visible: !root.granted
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, 420)
        spacing: 8
        LogosText {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Another app is set as Settings")
            font.pixelSize: 18
            font.weight: Font.DemiBold
        }
        LogosText {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            color: Theme.palette.textSecondary
            text: qsTr("Basecamp's Settings can change preferences only while it is the default Settings app. Choose it again in your current Settings app under Default apps.")
        }
    }
}
