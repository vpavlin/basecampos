import QtQuick
import QtQuick.Controls

import Logos.Theme
import Logos.Controls
import Basecamp.Icons

// The desktop layout's dock: the shell's own apps, a separator, then the
// installed apps. A dot marks every app with a window; the front-most one's
// dot is wider. The C++ side sets `openKeys` and `activeKey` and places the
// view; the root is sized to its content (SizeViewToRootObject), with room
// above the panel for the hover label.
Item {
    id: root

    // Window keys: a module name, or "sys:applications" / "sys:packages" / "sys:settings".
    property var openKeys: []
    property string activeKey: ""

    signal openSystemApp(string key)
    signal launchApp(string name)

    readonly property int tileSize: 48
    readonly property int pitch: 58
    readonly property int labelRoom: 34
    readonly property int panelHeight: 72

    readonly property var systemApps: [
        { key: "sys:applications", name: qsTr("Applications"),    icon: BasecampIcons.dashboard },
        { key: "sys:packages",     name: qsTr("Package Manager"), icon: BasecampIcons.modules },
        { key: "sys:settings",     name: qsTr("Settings"),        icon: BasecampIcons.settings }
    ]
    readonly property var apps: backend.launcherApps || []

    width: panel.width
    height: labelRoom + panelHeight

    Rectangle {
        id: panel
        objectName: "dock.panel"
        anchors.bottom: parent.bottom
        width: row.width + 2 * Theme.spacing.medium
        height: root.panelHeight
        radius: Theme.spacing.radiusXlarge
        color: Theme.colors.getColor(Theme.palette.backgroundSecondary, 0.8)
        border.color: Theme.palette.borderHairline
        border.width: 1

        // Tiles sit in the panel's vertical centre; the running dots go in
        // the bottom padding, outside the tiles' box.
        Row {
            id: row
            anchors.centerIn: parent

            Repeater {
                model: root.systemApps
                delegate: DockItem {
                    required property var modelData
                    objectName: "dock." + modelData.key
                    key: modelData.key
                    label: modelData.name
                    glyph: modelData.icon
                    onActivated: root.openSystemApp(modelData.key)
                }
            }

            Item {
                visible: root.apps.length > 0
                width: 17
                height: root.tileSize
                Rectangle {
                    anchors.centerIn: parent
                    width: 1
                    height: parent.height - 8
                    color: Theme.palette.border
                }
            }

            Repeater {
                model: root.apps
                delegate: DockItem {
                    required property var modelData
                    objectName: "dock.app." + modelData.name
                    key: modelData.name
                    label: modelData.displayName || modelData.name
                    artwork: modelData.iconPath || ""
                    fullBleed: modelData.supportsFullBleedIcon === true
                    loading: backend.loadingModules.indexOf(modelData.name) >= 0
                    onActivated: root.launchApp(modelData.name)
                }
            }
        }
    }

    component DockItem: AbstractButton {
        id: item

        property string key
        property string label
        property url glyph: ""
        property string artwork: ""
        property bool fullBleed: false
        property bool loading: false

        readonly property bool open: root.openKeys.indexOf(key) >= 0
        readonly property bool front: root.activeKey === key

        signal activated()

        width: root.pitch
        height: root.tileSize
        hoverEnabled: true
        // Above its neighbours, so the name label isn't drawn under them.
        z: hovered ? 10 : 0
        onClicked: item.activated()

        Item {
            id: lift
            width: root.tileSize
            height: root.tileSize
            anchors.horizontalCenter: parent.horizontalCenter
            y: item.hovered ? -4 : 0
            Behavior on y { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }

            LogosGlow {
                anchors.fill: parent
                radius: Theme.spacing.radiusLarge
                active: item.hovered
            }

            // Shell apps: a glyph on a surface tile.
            Rectangle {
                anchors.fill: parent
                visible: item.artwork.length === 0
                radius: Theme.spacing.radiusLarge
                color: item.pressed ? Theme.palette.accentOrange : Theme.palette.surfaceRaised

                LogosIcon {
                    anchors.centerIn: parent
                    width: 26
                    height: 26
                    source: item.glyph
                    color: item.pressed ? Theme.palette.accentForeground : Theme.palette.text
                    brightness: 1.0
                }
            }

            // Installed apps: their own artwork, as in the sidebar.
            LogosTile {
                anchors.centerIn: parent
                visible: item.artwork.length > 0 && !item.loading
                label: item.key
                source: item.artwork
                fallbackColor: Theme.palette.surfaceRaised
                insetArtwork: !item.fullBleed
                interactive: false
                tileSize: root.tileSize
            }

            BusyIndicator {
                anchors.centerIn: parent
                width: 24
                height: 24
                running: item.loading
                visible: item.loading
            }
        }

        // Running dot; wider for the front-most window.
        Rectangle {
            objectName: "dock.dot." + item.key
            visible: item.open
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.bottom
            anchors.topMargin: 5
            width: item.front ? 14 : 5
            height: 5
            radius: 2.5
            color: Theme.palette.accentOrange
            Behavior on width { NumberAnimation { duration: 120 } }
        }

        // Name above the tile while hovered, centred on it.
        Rectangle {
            visible: item.hovered
            x: (item.width - width) / 2
            y: lift.y - height - 10
            width: nameText.implicitWidth + 2 * Theme.spacing.medium
            height: nameText.implicitHeight + Theme.spacing.small * 2
            radius: Theme.spacing.radiusSmall
            color: Theme.palette.backgroundElevated
            border.color: Theme.palette.border

            LogosText {
                id: nameText
                anchors.centerIn: parent
                text: item.label
                font.pixelSize: Theme.typography.secondaryText
                color: Theme.palette.text
            }
        }
    }
}
