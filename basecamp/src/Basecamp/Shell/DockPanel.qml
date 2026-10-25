import QtQuick
import QtQuick.Controls

import Logos.Theme
import Logos.Controls
import Basecamp.Icons

// The desktop layout's dock: Basecamp pinned first, then one tile per open
// app window (basecamp-experiments#42). There are no fixed entries: the
// shell's role apps (Applications, Package Manager, Settings) show up here
// like any app while they have a window. The home and search role holders
// never do. A dot marks every window; the front-most one's dot is wider. The
// C++ side sets `windows`, `hiddenApps`, `activeKey` and `homeOpen` and
// places the view; the root is sized to its content (SizeViewToRootObject),
// with room above the panel for the hover label.
Item {
    id: root

    // Open windows in opening order, home and search already left out:
    // [{ key, title }]. A key is a module name, or "sys:<name>" for the
    // shell's own page shown when nothing fills that role.
    property var windows: []
    // Never shown here, even while loading: the home and search role holders.
    property var hiddenApps: []
    property string activeKey: ""
    // The Basecamp overlay is open (basecamp-experiments#43): Basecamp's dot.
    property bool homeOpen: false

    signal openHome()
    signal launchApp(string name)

    readonly property int tileSize: 48
    readonly property int pitch: 58
    readonly property int labelRoom: 34
    readonly property int panelHeight: 72

    readonly property var installed: backend.launcherApps || []
    readonly property var loadingNames: backend.loadingModules || []

    function appRow(name) {
        for (var i = 0; i < installed.length; ++i)
            if (installed[i].name === name) return installed[i]
        return null
    }

    // One tile per window, then the apps still loading (no window yet).
    readonly property var entries: {
        var out = []
        var seen = {}
        function add(key, title, loading) {
            if (seen[key] || hiddenApps.indexOf(key) >= 0) return
            seen[key] = true
            var row = appRow(key)
            out.push({
                key: key,
                label: (row && row.displayName) || title || key,
                artwork: (row && row.iconPath) || "",
                fullBleed: !!row && row.supportsFullBleedIcon === true,
                loading: loading
            })
        }
        for (var i = 0; i < windows.length; ++i)
            add(windows[i].key, windows[i].title, loadingNames.indexOf(windows[i].key) >= 0)
        for (var j = 0; j < loadingNames.length; ++j)
            add(loadingNames[j], "", true)
        return out
    }

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

            DockItem {
                objectName: "dock.basecamp"
                key: "basecamp"
                label: qsTr("Basecamp")
                artwork: BasecampIcons.basecampTile
                fullBleed: true
                shown: root.homeOpen
                onActivated: root.openHome()
            }

            Item {
                visible: root.entries.length > 0
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
                model: root.entries
                delegate: DockItem {
                    required property var modelData
                    objectName: "dock.app." + modelData.key
                    key: modelData.key
                    label: modelData.label
                    artwork: modelData.artwork
                    fullBleed: modelData.fullBleed
                    loading: modelData.loading
                    onActivated: root.launchApp(modelData.key)
                }
            }
        }
    }

    component DockItem: AbstractButton {
        id: item

        property string key
        property string label
        property string artwork: ""
        property bool fullBleed: false
        property bool loading: false
        // Open without a window (Basecamp: its overlay is up).
        property bool shown: false

        readonly property bool open: shown || root.windows.some(function(w) { return w.key === item.key })
        readonly property bool front: shown || root.activeKey === key

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
                radius: appTile.radius
                active: item.hovered
            }

            // The app's icon, or its letters when it has none.
            BasecampAppTile {
                id: appTile
                anchors.centerIn: parent
                visible: !item.loading
                size: root.tileSize
                label: item.label
                source: item.artwork
                fullBleed: item.fullBleed
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
