import QtQuick
import QtQuick.Controls

import Logos.Theme
import Logos.Controls

// Sizes: 32 high with 28-wide − / + on desktop. Under Theme.touch the control
// takes the text-field height (Theme.components.field.height, >= 44) and
// − / + are Theme.minTouchTarget wide, so each is a 44 x 44 target.
SpinBox {
    id: root

    property color backgroundColor: Theme.palette.backgroundSecondary
    property color borderColor: Theme.palette.border
    property color buttonColor: Theme.palette.surface

    // Exposed for inspection (e.g., from tests). Read-only.
    readonly property alias backgroundItem: bg
    readonly property alias contentLabel: input

    readonly property bool _touch: Theme.touch
    implicitHeight: _touch ? Math.max(Theme.components.field.height, Theme.minTouchTarget) : 32
    editable: true

    focusPolicy: Qt.StrongFocus
    activeFocusOnTab: true

    background: Rectangle {
        id: bg
        color: root.backgroundColor
        border.color: root.visualFocus || input.activeFocus
                      ? Theme.palette.overlayOrange : root.borderColor
        border.width: 1
        radius: Theme.spacing.radiusSmall
    }

    contentItem: TextInput {
        id: input
        text: root.displayText
        font.family: Theme.typography.publicSans
        font.pixelSize: Theme.typography.primaryText
        color: Theme.palette.text
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
        leftPadding: Theme.spacing.small
        rightPadding: Theme.spacing.small
        readOnly: !root.editable
        validator: root.validator
        inputMethodHints: Qt.ImhFormattedNumbersOnly
        selectByMouse: true
        activeFocusOnTab: true
    }

    up.indicator: Rectangle {
        x: parent.width - width
        height: parent.height
        implicitWidth: root._touch ? Theme.minTouchTarget : 28
        color: root.up.pressed ? Theme.palette.pressed : root.buttonColor
        radius: Theme.spacing.radiusSmall

        LogosText {
            anchors.centerIn: parent
            text: "+"
            color: Theme.palette.text
        }
    }

    down.indicator: Rectangle {
        x: 0
        height: parent.height
        implicitWidth: root._touch ? Theme.minTouchTarget : 28
        color: root.down.pressed ? Theme.palette.pressed : root.buttonColor
        radius: Theme.spacing.radiusSmall

        LogosText {
            anchors.centerIn: parent
            text: "−"
            color: Theme.palette.text
        }
    }
}
