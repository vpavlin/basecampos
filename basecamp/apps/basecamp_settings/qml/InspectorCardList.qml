import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Logos.Controls
import Logos.Theme

// The inspectors' rows as cards, for a pane narrower than a table can use
// (docs/RESPONSIVE.md §3.5, compact: "no table, rows become cards").
//   Name                               [status]
//   raw_name
//   1.2.0 · description    or    CPU 1.2% · 34.0 MB
//   [ Unload ][ Interface ]          (44 px, equal width)
// Same model (ModulesFilterProxy) and the same actions as the table rows.
ListView {
    id: root

    property bool busy: false
    // Module Inspector: CPU / memory line, the Interface button, locked rows.
    property bool showStats: false
    property bool interfaceEnabled: false
    property var protectedModules: []
    property string emptyText: ""

    signal loadToggleRequested(var row)
    signal interfaceRequested(var row)

    clip: true
    spacing: Theme.spacing.small
    boundsBehavior: Flickable.StopAtBounds
    // Keep every row instantiated, as the tables do (UI automation finds rows
    // by rendered text).
    cacheBuffer: 20000

    LogosText {
        anchors.centerIn: parent
        width: parent.width - 2 * Theme.spacing.large
        visible: root.count === 0
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        text: root.emptyText
        color: Theme.palette.textTertiary
        font.pixelSize: Theme.typography.secondaryText
    }

    delegate: Rectangle {
        id: card
        required property var model
        readonly property var row: model

        width: ListView.view.width
        implicitHeight: col.implicitHeight + 2 * Theme.spacing.medium
        radius: Theme.spacing.radiusLarge
        color: Theme.palette.background
        border.color: Theme.palette.borderSubtle
        border.width: 1

        ColumnLayout {
            id: col
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: Theme.spacing.medium
            spacing: Theme.spacing.tiny

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacing.small

                LogosText {
                    Layout.fillWidth: true
                    text: card.row.label || card.row.name || ""
                    font.pixelSize: Theme.typography.primaryText
                    font.weight: Theme.typography.weightMedium
                    color: Theme.palette.text
                    elide: Text.ElideRight
                }
                ModuleStatusBadge { row: card.row }
            }

            // Raw module name, only when it differs from the display name.
            LogosText {
                Layout.fillWidth: true
                visible: card.row.label !== card.row.name
                text: card.row.name || ""
                font.pixelSize: Theme.typography.secondaryText
                color: Theme.palette.textTertiary
                elide: Text.ElideRight
            }

            LogosText {
                Layout.fillWidth: true
                readonly property string version: String(card.row.version || "")
                readonly property string description: String(card.row.description || "")
                visible: text.length > 0
                text: root.showStats
                      ? (card.row.isLoaded
                         ? qsTr("CPU %1% · %2 MB").arg(Number(card.row.cpu).toFixed(1))
                                                  .arg(Number(card.row.memory).toFixed(1))
                         : "")
                      : [version, description].filter(s => s.length > 0).join(" · ")
                font.pixelSize: Theme.typography.secondaryText
                color: Theme.palette.textSecondary
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }

            ModuleRowActions {
                Layout.fillWidth: true
                Layout.topMargin: Theme.spacing.small
                visible: !card.row.isMainUi || (root.interfaceEnabled && card.row.isLoaded)
                touch: true
                stretch: true
                row: card.row
                busy: root.busy
                locked: root.protectedModules.indexOf(card.row.name) !== -1
                interfaceEnabled: root.interfaceEnabled
                onLoadToggleRequested: root.loadToggleRequested(card.row)
                onInterfaceRequested: root.interfaceRequested(card.row)
            }
        }
    }
}
