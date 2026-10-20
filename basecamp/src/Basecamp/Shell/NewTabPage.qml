import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Logos.Theme
import Logos.Controls
import Basecamp.Icons

// Basecampium's New tab: search, then the shell's own apps, a divider and the
// installed apps. Picking a tile asks the shell to turn the current New tab
// into that app.
Rectangle {
    id: root

    signal openSystemApp(string key)
    signal launchApp(string name)

    function focusSearch() { search.forceActiveFocus() }
    function clearSearch() { search.text = "" }

    readonly property string query: search.text.trim().toLowerCase()
    readonly property int tileSize: 64
    readonly property int pitch: 112

    readonly property var systemApps: [
        { key: "sys:applications", name: qsTr("Applications"),    icon: BasecampIcons.dashboard },
        { key: "sys:settings",     name: qsTr("Settings"),        icon: BasecampIcons.settings },
        { key: "sys:packages",     name: qsTr("Package Manager"), icon: BasecampIcons.modules }
    ]
    readonly property var apps: {
        const list = (backend.launcherApps || []).slice()
        list.sort((a, b) => (a.displayName || a.name).localeCompare(b.displayName || b.name))
        return list
    }

    function matches(label) {
        return root.query.length === 0 || label.toLowerCase().indexOf(root.query) >= 0
    }
    readonly property var shownSystem: systemApps.filter(a => matches(a.name))
    readonly property var shownApps: apps.filter(a => matches(a.displayName || a.name))

    function openFirst() {
        if (shownSystem.length > 0)
            root.openSystemApp(shownSystem[0].key)
        else if (shownApps.length > 0)
            root.launchApp(shownApps[0].name)
    }

    color: Theme.palette.background

    ColumnLayout {
        anchors.top: parent.top
        anchors.topMargin: Math.max(48, parent.height * 0.16)
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(parent.width - 96, 880)
        spacing: Theme.spacing.xlarge * 2

        LogosSearchBar {
            id: search
            objectName: "newTab.search"
            focus: true
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.components.search.height
            Layout.maximumHeight: Theme.components.search.height
            placeholderText: qsTr("Search apps…")
            shortcutHint: "Ctrl+K"
            onSubmitted: root.openFirst()
            Keys.onReturnPressed: root.openFirst()
            Keys.onEnterPressed: root.openFirst()
        }

        Flow {
            id: grid
            objectName: "newTab.grid"
            Layout.fillWidth: true
            spacing: 0

            Repeater {
                model: root.shownSystem
                delegate: Tile {
                    required property var modelData
                    objectName: "newTab." + modelData.key
                    label: modelData.name
                    glyph: modelData.icon
                    onActivated: root.openSystemApp(modelData.key)
                }
            }

            Item {
                visible: root.shownSystem.length > 0 && root.shownApps.length > 0
                width: 25
                height: root.tileSize + 40
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: 6
                    width: 1
                    height: root.tileSize - 4
                    color: Theme.palette.border
                }
            }

            Repeater {
                model: root.shownApps
                delegate: Tile {
                    required property var modelData
                    objectName: "newTab.app." + modelData.name
                    key: modelData.name
                    label: modelData.displayName || modelData.name
                    artwork: modelData.iconPath || ""
                    fullBleed: modelData.supportsFullBleedIcon === true
                    loading: (backend.loadingModules || []).indexOf(modelData.name) >= 0
                    onActivated: root.launchApp(modelData.name)
                }
            }
        }

        LogosText {
            visible: root.apps.length === 0 && root.query.length === 0
            text: qsTr("No apps yet. Find some in Applications.")
            color: Theme.palette.textSecondary
            font.pixelSize: Theme.typography.secondaryText
        }

        LogosText {
            visible: root.query.length > 0 && root.shownSystem.length === 0 && root.shownApps.length === 0
            text: qsTr("Nothing matches “%1”.").arg(search.text)
            color: Theme.palette.textSecondary
            font.pixelSize: Theme.typography.secondaryText
        }
    }

    component Tile: AbstractButton {
        id: tile

        property string key
        property string label
        property url glyph: ""
        property string artwork: ""
        property bool fullBleed: false
        property bool loading: false

        signal activated()

        width: root.pitch
        height: root.tileSize + 40
        hoverEnabled: true
        onClicked: tile.activated()

        Item {
            id: art
            width: root.tileSize
            height: root.tileSize
            anchors.horizontalCenter: parent.horizontalCenter
            y: tile.hovered ? 2 : 6
            Behavior on y { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }

            LogosGlow {
                anchors.fill: parent
                radius: Theme.spacing.radiusLarge
                active: tile.hovered
            }

            Rectangle {
                anchors.fill: parent
                visible: tile.artwork.length === 0
                radius: Theme.spacing.radiusLarge
                color: tile.pressed ? Theme.palette.accentOrange : Theme.palette.surfaceRaised

                LogosIcon {
                    anchors.centerIn: parent
                    width: 30
                    height: 30
                    source: tile.glyph
                    color: tile.pressed ? Theme.palette.accentForeground : Theme.palette.text
                    brightness: 1.0
                }
            }

            LogosTile {
                anchors.centerIn: parent
                visible: tile.artwork.length > 0 && !tile.loading
                label: tile.key
                source: tile.artwork
                fallbackColor: Theme.palette.surfaceRaised
                insetArtwork: !tile.fullBleed
                interactive: false
                tileSize: root.tileSize
            }

            BusyIndicator {
                anchors.centerIn: parent
                width: 28
                height: 28
                running: tile.loading
                visible: tile.loading
            }
        }

        LogosText {
            anchors.top: art.bottom
            anchors.topMargin: 8
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width - 8
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            text: tile.label
            font.pixelSize: Theme.typography.secondaryText
            color: tile.hovered ? Theme.palette.text : Theme.palette.textSecondary
        }
    }
}
