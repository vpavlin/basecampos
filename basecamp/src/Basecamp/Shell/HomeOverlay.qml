import QtQuick

import Logos.Theme
import Logos.Controls

// The desktop layout's Basecamp overlay (basecamp-experiments#43): a dimmed
// desktop with a rounded panel in the middle. The home app's own view sits
// inside the panel as a separate widget the C++ side places over it
// (HomeOverlayGeometry.h); this page only draws what is around it. The panel
// is the home page's background colour, so its corners read as the page's.
// A click outside the panel asks to close; Esc and Ctrl+K are the shell's.
// As the Activity sidebar (basecamp-experiments#44) the panel is a column on
// the right edge instead, square, with a lighter dim beside it.
Item {
    id: root

    property int panelX: 0
    property int panelY: 0
    property int panelWidth: 0
    property int panelHeight: 0
    // Nothing can fill `home`: the panel says so itself.
    property bool noHome: false
    property bool sidebar: false

    // A click outside the panel, in this view's coordinates: the shell
    // closes it, or switches to the sidebar when it lands on the top bar's
    // Activity button (the dim covers the bar).
    signal outsideClicked(int x, int y)

    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, root.sidebar ? 0.2 : 0.45)
    }

    MouseArea {
        objectName: "homeOverlay.scrim"
        anchors.fill: parent
        onClicked: mouse => root.outsideClicked(mouse.x, mouse.y)
    }

    Rectangle {
        objectName: "homeOverlay.panel"
        x: root.panelX
        y: root.panelY
        width: root.panelWidth
        height: root.panelHeight
        radius: root.sidebar ? 0 : Theme.spacing.radiusXlarge
        color: Theme.palette.background
        border.color: Theme.palette.border
        border.width: root.sidebar ? 0 : 1

        // Clicks on the panel stay on the panel.
        MouseArea { anchors.fill: parent }

        // The sidebar's one edge against the desktop.
        Rectangle {
            visible: root.sidebar
            width: 1
            height: parent.height
            color: Theme.palette.border
        }

        Column {
            visible: root.noHome
            anchors.centerIn: parent
            width: Math.min(420, parent.width - 48)
            spacing: Theme.spacing.small

            LogosText {
                objectName: "homeOverlay.noHome"
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("No home page")
                font.pixelSize: 18
                font.weight: Font.DemiBold
                color: Theme.palette.text
            }
            LogosText {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: qsTr("Basecamp's Home app isn't installed, and no other installed app is set as Home. Choose one in Settings, Default apps, or reinstall Basecamp. Your open apps are in the dock.")
                font.pixelSize: 14
                color: Theme.palette.textSecondary
            }
        }
    }
}
