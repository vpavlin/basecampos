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
        if (shownApps.length > 0)
            root.launchApp(shownApps[0].name)
        else if (shownSystem.length > 0)
            root.openSystemApp(shownSystem[0].key)
    }

    color: Theme.palette.background

    ColumnLayout {
        anchors.top: parent.top
        anchors.topMargin: Math.max(32, parent.height * 0.08)
        anchors.bottom: systemRow.top
        anchors.bottomMargin: Theme.spacing.xlarge
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(parent.width - 96, 880)
        spacing: Theme.spacing.xlarge * 2

        // Who made this shell, above the search.
        Column {
            objectName: "newTab.brand"
            Layout.alignment: Qt.AlignHCenter
            spacing: 2
            LogosText {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Basecampium"
                font.pixelSize: 40
                font.weight: Font.Bold
                color: Theme.palette.textSecondary
            }
        }

        LogosSearchBar {
            id: search
            objectName: "newTab.search"
            focus: true
            // From the first icon's left edge to the last icon's right edge
            // (icons, not their wider labels).
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: grid.width - (root.pitch - root.tileSize)
            Layout.preferredHeight: Theme.components.search.height
            Layout.maximumHeight: Theme.components.search.height
            placeholderText: qsTr("Search apps…")
            shortcutHint: "Ctrl+K"
            onSubmitted: root.openFirst()
            Keys.onReturnPressed: root.openFirst()
            Keys.onEnterPressed: root.openFirst()
        }

        // Apps scroll inside the space above the system row.
        Flickable {
            objectName: "newTab.scroll"
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentHeight: grid.height
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        Flow {
            id: grid
            objectName: "newTab.grid"
            // Whole columns only, centred under the search, so no ragged gap
            // is left on the right.
            readonly property int columns: Math.max(1, Math.floor(parent.width / root.pitch))
            width: Math.min(columns, Math.max(1, root.shownApps.length)) * root.pitch
            x: (parent.width - width) / 2
            spacing: 0

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
        }

        LogosText {
            visible: root.apps.length === 0 && root.query.length === 0
            text: qsTr("No apps yet. Find some in Applications.")
            color: Theme.palette.textSecondary
            font.pixelSize: Theme.typography.secondaryText
        }

        LogosText {
            visible: root.query.length > 0 && root.shownApps.length === 0
            text: qsTr("Nothing matches “%1”.").arg(search.text)
            color: Theme.palette.textSecondary
            font.pixelSize: Theme.typography.secondaryText
        }
    }

    // The shell's own apps: small round buttons under a subtle divider,
    // pinned to the bottom and centred. Search still finds them by name.
    Column {
        id: systemRow
        objectName: "newTab.systemRow"
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.spacing.xlarge * 1.5
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: Theme.spacing.large

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(root.width - 96, 480)
            height: 1
            color: Theme.palette.borderHairline
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Theme.spacing.large

            Repeater {
                model: root.systemApps
                delegate: AbstractButton {
                    id: sys
                    required property var modelData
                    objectName: "newTab." + modelData.key
                    readonly property bool matched: root.query.length === 0 || root.matches(modelData.name)
                    width: 40
                    height: 40
                    hoverEnabled: true
                    opacity: matched ? 1.0 : 0.35
                    onClicked: root.openSystemApp(modelData.key)
                    Accessible.name: modelData.name

                    LogosGlow {
                        anchors.fill: parent
                        radius: width / 2
                        active: sys.hovered
                    }

                    Rectangle {
                        anchors.fill: parent
                        radius: width / 2
                        color: sys.pressed ? Theme.palette.accentOrange
                             : sys.hovered ? Theme.palette.backgroundTertiary
                             : Theme.palette.backgroundSecondary
                        border.color: sys.hovered ? Theme.palette.border : Theme.palette.borderHairline
                        border.width: 1
                        Behavior on color { ColorAnimation { duration: 120 } }

                        LogosIcon {
                            anchors.centerIn: parent
                            width: 18
                            height: 18
                            source: sys.modelData.icon
                            color: sys.pressed ? Theme.palette.accentForeground
                                 : sys.hovered ? Theme.palette.text : Theme.palette.textSecondary
                            brightness: 1.0
                        }
                    }

                    ToolTip.visible: sys.hovered
                    ToolTip.delay: 300
                    ToolTip.text: modelData.name
                }
            }
        }

        LogosText {
            objectName: "newTab.credit"
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("Shell package made by Alisher")
            font.pixelSize: Theme.typography.secondaryText
            color: Theme.palette.textTertiary
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
        height: root.tileSize + 48
        hoverEnabled: true
        onClicked: tile.activated()

        Rectangle {
            anchors.fill: parent
            anchors.margins: 4
            radius: Theme.spacing.radiusLarge
            color: tile.pressed ? Theme.palette.backgroundTertiary : Theme.palette.backgroundSecondary
            opacity: tile.hovered ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: 120 } }
        }

        Item {
            id: art
            width: root.tileSize
            height: root.tileSize
            anchors.horizontalCenter: parent.horizontalCenter
            y: tile.hovered ? 8 : 12
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
