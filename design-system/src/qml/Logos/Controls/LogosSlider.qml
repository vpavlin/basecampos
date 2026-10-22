import QtQuick
import QtQuick.Controls

import Logos.Theme

// At touch density the control is at least Theme.minTouchTarget high, so a
// press anywhere along the row (the track and the space around it) moves the
// handle; the 4 px track and 18 px handle are drawn as on desktop.
Slider {
    id: root

    property color trackColor: Theme.palette.backgroundSecondary
    property color fillColor: Theme.palette.primary
    property color handleColor: Theme.palette.primary

    // Exposed for inspection (e.g., from tests). Read-only.
    readonly property alias trackItem: track
    readonly property alias fillItem: fill
    readonly property alias handleItem: handle

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitHandleWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitHandleHeight + topPadding + bottomPadding,
                             Theme.density === "touch" ? Theme.minTouchTarget : 0)

    focusPolicy: Qt.StrongFocus
    activeFocusOnTab: true

    background: Rectangle {
        id: track
        x: root.leftPadding
        y: root.topPadding + root.availableHeight / 2 - height / 2
        implicitWidth: 200
        implicitHeight: 4
        width: root.availableWidth
        height: 4
        radius: Theme.spacing.radiusTiny
        color: root.trackColor

        Rectangle {
            id: fill
            width: root.visualPosition * parent.width
            height: parent.height
            color: root.fillColor
            radius: Theme.spacing.radiusTiny
        }
    }

    handle: Rectangle {
        id: handle
        x: root.leftPadding + root.visualPosition * (root.availableWidth - width)
        y: root.topPadding + root.availableHeight / 2 - height / 2
        implicitWidth: 18
        implicitHeight: 18
        radius: width / 2 * Theme.spacing.roundness
        color: root.pressed ? Theme.palette.primaryPressed : root.handleColor
        border.color: root.visualFocus ? Theme.palette.overlayOrange : Theme.palette.border
        border.width: root.visualFocus ? 2 : 1
    }
}
