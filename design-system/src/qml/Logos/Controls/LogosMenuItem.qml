import QtQuick
import QtQuick.Controls

import Logos.Theme
import Logos.Controls
import Logos.Icons

// LogosMenuItem — themed MenuItem.
//
// checkable: a checked item shows a check mark (LogosIcons.check, primary)
// at the trailing edge, where LogosComboBox has its indicator; a checkable
// item keeps that space whether checked or not, so its text never runs under
// the mark. Non-checkable items draw exactly as before.
//
// Read-only inspection aliases: labelItem, backgroundItem, indicatorItem.
MenuItem {
    id: root

    property color highlightColor: Theme.components.menuItem.backgroundHover

    // Exposed for inspection (e.g., from tests). Read-only.
    readonly property alias labelItem: label
    readonly property alias backgroundItem: bg
    readonly property alias indicatorItem: checkMark

    readonly property int _checkSize: 16

    implicitHeight: Theme.components.menuItem.height

    focusPolicy: Qt.StrongFocus
    activeFocusOnTab: true

    contentItem: LogosText {
        id: label
        leftPadding: Theme.components.menuItem.paddingX
        rightPadding: Theme.components.menuItem.paddingX
                      + (root.checkable ? root._checkSize + Theme.spacing.small : 0)
        verticalAlignment: Text.AlignVCenter
        text: root.text
        color: root.enabled ? Theme.components.menuItem.text : Theme.palette.textMuted
    }

    indicator: LogosIcon {
        id: checkMark
        width: root._checkSize
        height: root._checkSize
        x: root.width - root.rightPadding - Theme.components.menuItem.paddingX - width
        y: root.topPadding + (root.availableHeight - height) / 2
        visible: root.checkable && root.checked
        source: root.checkable ? LogosIcons.check : ""
        color: root.enabled ? Theme.palette.primary : Theme.palette.textMuted
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
