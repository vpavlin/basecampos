import QtQuick
import QtQuick.Controls

import Logos.Theme
import Logos.Controls

ToolButton {
    id: root

    property color hoverColor: Theme.components.toolButton.backgroundHover
    property color pressedColor: Theme.components.toolButton.backgroundPressed

    // Exposed for inspection (e.g., from tests). Read-only.
    readonly property alias labelItem: label
    readonly property alias backgroundItem: bg

    focusPolicy: Qt.StrongFocus
    activeFocusOnTab: true

    contentItem: LogosText {
        id: label
        text: root.text
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
        color: root.enabled ? Theme.components.toolButton.text : Theme.palette.textMuted
    }

    background: Rectangle {
        id: bg
        implicitWidth: Theme.components.toolButton.size
        implicitHeight: Theme.components.toolButton.size
        radius: Theme.spacing.radiusSmall
        color: root.pressed
               ? root.pressedColor
               : (root.hovered || root.visualFocus ? root.hoverColor : "transparent")
        border.width: root.visualFocus ? 1 : 0
        border.color: Theme.palette.overlayOrange
    }

    HoverHandler {
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    }
}
