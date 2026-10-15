import QtQuick
import QtQuick.Controls

import Logos.Theme
import Logos.Controls

MenuItem {
    id: root

    property color highlightColor: Theme.components.menuItem.backgroundHover

    // Exposed for inspection (e.g., from tests). Read-only.
    readonly property alias labelItem: label
    readonly property alias backgroundItem: bg

    implicitHeight: Theme.components.menuItem.height

    focusPolicy: Qt.StrongFocus
    activeFocusOnTab: true

    contentItem: LogosText {
        id: label
        leftPadding: Theme.components.menuItem.paddingX
        rightPadding: Theme.components.menuItem.paddingX
        verticalAlignment: Text.AlignVCenter
        text: root.text
        color: root.enabled ? Theme.components.menuItem.text : Theme.palette.textMuted
    }

    background: Rectangle {
        id: bg
        radius: Theme.spacing.radiusSmall
        color: root.highlighted || root.visualFocus ? root.highlightColor : "transparent"
        border.width: root.visualFocus ? 1 : 0
        border.color: Theme.palette.overlayOrange
    }

    HoverHandler {
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    }
}
