import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Logos.Theme
import Logos.Icons

// A settings / navigation row, as in Basecamp's Settings and on phones:
//   [icon]  Title                     value  ›
//           subtitle
// The whole row is the touch target (≥ Theme.minTouchTarget). Put a control
// on the right with `accessory` (a switch, a badge) instead of a chevron.
//
//   LogosListRow { title: "Network"; value: "Testnet"; showChevron: true; onClicked: … }
//   LogosListRow { title: "Notifications"; accessory: LogosSwitch { checked: true } }
AbstractButton {
    id: root

    property string title: ""
    property string subtitle: ""
    // Let the subtitle wrap instead of eliding (help text, long descriptions).
    property bool subtitleWraps: false
    property string value: ""
    property url iconSource: ""
    property bool showChevron: false
    property bool showSeparator: true
    property bool destructive: false
    property Item accessory: null

    hoverEnabled: true
    implicitWidth: 320
    implicitHeight: Math.max(Theme.minTouchTarget, subtitle !== "" ? 64 : 52,
                             contentItem.implicitHeight + topPadding + bottomPadding)
    topPadding: Theme.spacing.tiny
    bottomPadding: Theme.spacing.tiny
    Accessible.name: title
    Accessible.description: subtitle

    // An accessory (a switch) owns its own taps; the row only toggles it.
    // toggle() changes `checked` but doesn't emit `toggled` (that's only for
    // user interaction), so emit it here: the app's onToggled must run
    // whether the user tapped the switch or the row.
    onClicked: {
        if (accessory && accessory.toggle !== undefined) {
            accessory.toggle()
            if (accessory.toggled !== undefined)
                accessory.toggled()
        }
    }

    onAccessoryChanged: if (accessory) accessory.parent = accessorySlot

    background: Rectangle {
        color: root.pressed ? Theme.components.listItem.backgroundPressed
             : root.hovered ? Theme.components.listItem.backgroundHover : "transparent"
        Rectangle {
            visible: root.showSeparator
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom
                      leftMargin: root.iconSource.toString() !== "" ? 52 : Theme.spacing.large }
            height: 1
            color: Theme.palette.borderSubtle
        }
    }

    contentItem: RowLayout {
        spacing: Theme.spacing.medium

        LogosIcon {
            visible: root.iconSource.toString() !== ""
            source: root.iconSource
            color: root.destructive ? Theme.palette.error : Theme.palette.textSecondary
            Layout.preferredWidth: 20
            Layout.preferredHeight: 20
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            LogosText {
                Layout.fillWidth: true
                text: root.title
                elide: Text.ElideRight
                color: root.destructive ? Theme.palette.error : Theme.palette.text
            }
            LogosText {
                Layout.fillWidth: true
                visible: root.subtitle !== ""
                text: root.subtitle
                elide: root.subtitleWraps ? Text.ElideNone : Text.ElideRight
                wrapMode: root.subtitleWraps ? Text.Wrap : Text.NoWrap
                font.pixelSize: Theme.typography.secondaryText
                color: Theme.palette.textSecondary
            }
        }
        LogosText {
            visible: root.value !== ""
            Layout.maximumWidth: root.width * 0.4
            text: root.value
            elide: Text.ElideRight
            color: Theme.palette.textSecondary
        }
        Item {
            id: accessorySlot
            visible: root.accessory !== null
            implicitWidth: root.accessory ? root.accessory.implicitWidth : 0
            implicitHeight: root.accessory ? root.accessory.implicitHeight : 0
        }
        LogosIcon {
            visible: root.showChevron
            source: LogosIcons.arrowRight
            color: Theme.palette.textSecondary
            Layout.preferredWidth: 20
            Layout.preferredHeight: 20
        }
    }
    leftPadding: Theme.spacing.large
    rightPadding: Theme.spacing.medium
}
