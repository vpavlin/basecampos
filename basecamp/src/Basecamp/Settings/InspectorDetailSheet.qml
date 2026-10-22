import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Logos.Controls
import Logos.Icons
import Logos.Theme

// A row's detail as a sheet over the right side of an inspector pane — the
// medium-width table (RESPONSIVE.md §3.5: three key columns + ⋯, the dropped
// fields in a sheet). Opened by the row's ⋯ or a tap on the row; closed by
// ✕, a tap on the dimmed table, or Esc.
//
//   Name                                   ✕
//   raw_name
//   Status        [LOADED]
//   Version       1.2.0          (apps)     CPU / Memory   (modules)
//   Description   …
//   [ Unload ][ Interface ]
Item {
    id: root

    // The model row (ModulesFilterProxy role object) shown; null = closed.
    property var row: null
    property bool showStats: false
    property bool interfaceEnabled: false
    property bool busy: false
    property bool locked: false
    // The host clears this outside the medium layout, which hides the sheet.
    property bool allowed: true

    readonly property bool opened: row !== null && allowed

    signal loadToggleRequested(var row)
    signal interfaceRequested(var row)

    component FieldLabel: LogosText {
        Layout.alignment: Qt.AlignTop
        font.pixelSize: Theme.typography.secondaryText
        color: Theme.palette.textTertiary
    }
    component FieldValue: LogosText {
        Layout.fillWidth: true
        font.pixelSize: Theme.typography.primaryText
        color: Theme.palette.text
        wrapMode: Text.WordWrap
    }

    function openFor(r) { row = r }
    function close() { row = null }

    visible: opened
    z: 10

    Shortcut {
        sequence: "Esc"
        context: Qt.WindowShortcut
        enabled: root.opened
        onActivated: root.close()
    }

    // Dim the table; a tap on it closes the sheet.
    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.45)
        radius: Theme.spacing.radiusXlarge
        MouseArea {
            objectName: "inspector.detailScrim"
            anchors.fill: parent
            onClicked: root.close()
        }
    }

    Rectangle {
        id: sheet
        objectName: "inspector.detailSheet"
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: Math.min(360, Math.round(parent.width * 0.6))
        color: Theme.palette.surfaceRaised
        radius: Theme.spacing.radiusXlarge
        border.color: Theme.palette.borderSubtle
        border.width: 1

        // Swallow taps so they don't reach the scrim.
        MouseArea { anchors.fill: parent }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.spacing.large
            spacing: Theme.spacing.medium

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacing.small

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    LogosText {
                        Layout.fillWidth: true
                        text: root.row ? (root.row.label || root.row.name || "") : ""
                        font.pixelSize: Theme.typography.subtitleText
                        font.weight: Theme.typography.weightMedium
                        color: Theme.palette.text
                        wrapMode: Text.WordWrap
                    }
                    LogosText {
                        Layout.fillWidth: true
                        visible: !!root.row && root.row.label !== root.row.name
                        text: root.row ? (root.row.name || "") : ""
                        font.pixelSize: Theme.typography.secondaryText
                        color: Theme.palette.textTertiary
                        wrapMode: Text.WrapAnywhere
                    }
                }
                LogosIconButton {
                    objectName: "inspector.detailClose"
                    Layout.alignment: Qt.AlignTop
                    Layout.topMargin: -Theme.spacing.small
                    Layout.rightMargin: -Theme.spacing.small
                    size: Theme.minTouchTarget
                    flat: true
                    iconSource: LogosIcons.close
                    Accessible.name: qsTr("Close")
                    onClicked: root.close()
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: Theme.spacing.medium
                rowSpacing: Theme.spacing.small

                FieldLabel { text: qsTr("Status") }
                Item {
                    Layout.fillWidth: true
                    implicitHeight: badge.implicitHeight
                    ModuleStatusBadge { id: badge; row: root.row }
                }

                FieldLabel { visible: !root.showStats; text: qsTr("Version") }
                FieldValue {
                    visible: !root.showStats
                    readonly property string v: root.row ? String(root.row.version || "") : ""
                    text: v.length > 0 ? v : "—"
                }

                FieldLabel { visible: root.showStats; text: qsTr("CPU") }
                FieldValue {
                    visible: root.showStats
                    text: root.row && root.row.isLoaded ? Number(root.row.cpu).toFixed(1) + "%" : "—"
                }
                FieldLabel { visible: root.showStats; text: qsTr("Memory") }
                FieldValue {
                    visible: root.showStats
                    text: root.row && root.row.isLoaded ? Number(root.row.memory).toFixed(1) + " MB" : "—"
                }

                FieldLabel {
                    visible: !root.showStats && descriptionValue.text.length > 0
                    text: qsTr("Description")
                }
                FieldValue {
                    id: descriptionValue
                    visible: !root.showStats && text.length > 0
                    text: root.row ? String(root.row.description || "") : ""
                    color: Theme.palette.textSecondary
                }
            }

            Item { Layout.fillHeight: true }

            ModuleRowActions {
                Layout.fillWidth: true
                visible: !!root.row && (!root.row.isMainUi || (root.interfaceEnabled && root.row.isLoaded))
                touch: true
                stretch: true
                row: root.row
                busy: root.busy
                locked: root.locked
                interfaceEnabled: root.interfaceEnabled
                onLoadToggleRequested: root.loadToggleRequested(root.row)
                onInterfaceRequested: root.interfaceRequested(root.row)
            }
        }
    }
}
