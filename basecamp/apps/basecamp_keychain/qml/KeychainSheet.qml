import QtQuick

import Logos.Controls
import Logos.Theme

// A card over the rest of the Keychain, on a scrim that takes every click
// and the wheel (nothing behind it can be reached while it is up). Centred,
// at most `maxWidth` wide; on a phone (`fullScreen`) it is the whole window.
//
// Content goes in `content` (a Column the card sizes to); it scrolls when it
// is taller than the window.
Item {
    id: sheet

    property int maxWidth: 480
    // The whole window below 600 px wide, when the caller wants it (the
    // request view on a phone).
    property bool fullScreenWhenCompact: false
    readonly property bool fullScreen: fullScreenWhenCompact && width < 600
    readonly property int gutter: width < 600 ? 16 : Theme.spacing.large
    default property alias content: column.data

    visible: false

    Rectangle {
        anchors.fill: parent
        color: sheet.fullScreen ? Theme.palette.background : "#99000000"
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
            onWheel: wheel => wheel.accepted = true
        }
    }

    Rectangle {
        id: card
        readonly property int pad: sheet.fullScreen ? sheet.gutter : Theme.spacing.large
        width: sheet.fullScreen ? sheet.width : Math.min(sheet.maxWidth, sheet.width - 2 * sheet.gutter)
        height: sheet.fullScreen ? sheet.height
                                 : Math.min(column.implicitHeight + 2 * pad, sheet.height - 2 * sheet.gutter)
        anchors.centerIn: parent
        radius: sheet.fullScreen ? 0 : Theme.spacing.radiusLarge
        color: sheet.fullScreen ? Theme.palette.background : Theme.palette.backgroundSecondary
        border.width: sheet.fullScreen ? 0 : 1
        border.color: Theme.palette.border

        Flickable {
            anchors.fill: parent
            anchors.margins: card.pad
            contentWidth: width
            contentHeight: column.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            Column {
                id: column
                width: parent.width
                spacing: Theme.spacing.medium
            }
        }
    }
}
