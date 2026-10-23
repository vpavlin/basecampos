import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Logos.Theme
import Logos.Controls
import Basecamp.Icons

// Basecamp's home page (the `home` role, ADR-067): what a New tab shows.
// Search, then the shell's own apps, a divider and the installed apps. Picking a
// tile asks the shell to turn the current New tab into that app. It reads only
// `shell`, the role-gated object the host injects.
Rectangle {
    id: root

    signal openSystemApp(string key)
    signal launchApp(string name)

    function focusSearch() { search.forceActiveFocus() }
    function clearSearch() { search.text = "" }

    // Basecampium's compact layout puts the search in its one bar
    // (docs/MOBILE-LAYOUT.md §8): this page then shows the results only.
    property bool searchInBar: false
    property string barQuery: ""
    readonly property string query: (searchInBar ? barQuery : search.text).trim().toLowerCase()
    // Phone width (Basecampium's compact layout): tighter margins and tiles.
    readonly property bool compact: width < 600
    readonly property int sideMargin: compact ? 32 : 96
    // Tiles the size of the Applications grid's (80), so an app looks the same in both.
    readonly property int tileSize: 80
    readonly property int pitch: compact ? 108 : 124

    readonly property var systemApps: [
        { key: "sys:applications", name: qsTr("Applications"),    icon: BasecampIcons.dashboard },
        { key: "sys:settings",     name: qsTr("Settings"),        icon: BasecampIcons.settings },
        { key: "sys:packages",     name: qsTr("Package Manager"), icon: BasecampIcons.modules }
    ]
    readonly property var apps: {
        // Apps that fill a role are already in the row above (or are this page).
        const roles = shell.roles || ({})
        const roleApps = Object.keys(roles).map(r => roles[r])
        const list = (shell.installedApps || []).filter(a => roleApps.indexOf(a.name) < 0)
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

    // The search keeps focus, and Qt's own cursor blinks forever: each blink
    // redraws the whole New tab, so an idle window never stops drawing
    // (~5% CPU in software rendering, basecamp-theming#20). This cursor
    // blinks for a few seconds after the last edit, then stays solid. A
    // cursor delegate also stops TextInput's built-in blink timer.
    Component {
        id: calmCursor
        Rectangle {
            id: caret
            property bool blinking: true
            readonly property Item input: parent
            width: 1
            color: input ? input.color : Theme.palette.text
            visible: input ? input.cursorVisible : false
            onVisibleChanged: { blinking = true; calm.restart() }
            Component.onCompleted: calm.start()

            SequentialAnimation {
                running: caret.visible && caret.blinking
                loops: Animation.Infinite
                alwaysRunToEnd: true            // stop on the visible half
                PauseAnimation { duration: 600 }
                PropertyAction { target: caret; property: "opacity"; value: 0 }
                PauseAnimation { duration: 600 }
                PropertyAction { target: caret; property: "opacity"; value: 1 }
            }
            Timer { id: calm; interval: 10000; onTriggered: caret.blinking = false }
            Connections {
                target: caret.input
                function onTextChanged() { caret.blinking = true; calm.restart() }
                function onCursorPositionChanged() { caret.blinking = true; calm.restart() }
            }
        }
    }
    Component.onCompleted: search.textInput.cursorDelegate = calmCursor

    ColumnLayout {
        anchors.top: parent.top
        anchors.topMargin: root.compact ? 24 : Math.max(32, parent.height * 0.08)
        anchors.bottom: systemRow.top
        anchors.bottomMargin: Theme.spacing.xlarge
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(parent.width - root.sideMargin, 880)
        spacing: root.compact ? Theme.spacing.xlarge : Theme.spacing.xlarge * 2

        // Who made this shell, above the search.
        Column {
            objectName: "newTab.brand"
            Layout.alignment: Qt.AlignHCenter
            spacing: 2
            LogosText {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Basecampium"
                font.pixelSize: root.compact ? 28 : 40
                font.weight: Font.Bold
                color: Theme.palette.textSecondary
            }
        }

        LogosSearchBar {
            id: search
            objectName: "newTab.search"
            visible: !root.searchInBar
            focus: true
            // From the first icon's left edge to the last icon's right edge
            // (icons, not their wider labels).
            Layout.alignment: Qt.AlignHCenter
            // Never narrower than a few tiles: with no apps the grid is empty.
            Layout.preferredWidth: root.compact ? parent.width
                                   : Math.max(grid.width - (root.pitch - root.tileSize), 4 * root.pitch)
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
                    loading: (shell.loadingModules || []).indexOf(modelData.name) >= 0
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
            width: Math.min(root.width - root.sideMargin, 480)
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
