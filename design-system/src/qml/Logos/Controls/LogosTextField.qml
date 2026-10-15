import QtQuick
import QtQuick.Controls

import Logos.Theme
import Logos.Controls

Control {
    id: root

    // --- Public API ---
    property alias text: input.text
    property string placeholderText: ""
    property color placeholderTextColor: Theme.components.field.placeholder
    property int echoMode: TextInput.Normal
    property alias validator: input.validator
    property alias readOnly: input.readOnly

    /** Expose the inner TextInput for advanced use (cursorPosition, select, etc.) */
    readonly property alias textInput: input

    // Exposed for inspection (e.g., from tests). Read-only.
    readonly property alias placeholderItem: placeholder
    readonly property alias backgroundItem: bg

    implicitWidth: 200
    implicitHeight: Theme.components.field.height
    leftPadding: Theme.components.field.paddingX
    rightPadding: Theme.components.field.paddingX
    clip: true

    focusPolicy: Qt.StrongFocus
    activeFocusOnTab: true
    // Focus handed to the field (forceActiveFocus, tab, key navigation) lands on
    // the wrapper control; the editor is where it must end up.
    onActiveFocusChanged: if (activeFocus)
        input.forceActiveFocus()

    background: Rectangle {
        id: bg
        radius: Theme.components.field.radius
        color: Theme.components.field.background
        border.width: Theme.components.field.borderWidth
        border.color: {
            if (input.validator && input.text.length > 0 && !input.acceptableInput)
                return Theme.palette.error
            if (input.activeFocus)
                return Theme.components.field.borderFocus
            return Theme.components.field.border
        }

        LogosSurface {
            anchors.fill: parent
            anchors.margins: bg.border.width
            radius: Math.max(0, bg.radius - bg.border.width)
            slot: Theme.components.field.gradient
            visible: hasGradient
        }
    }

    contentItem: Item {
        id: contentRow
        property alias input: input
        clip: true

        LogosText {
            id: placeholder
            anchors.fill: parent
            verticalAlignment: Text.AlignVCenter
            text: root.placeholderText
            color: root.placeholderTextColor
            font.pixelSize: Theme.typography.secondaryText
            visible: input.text.length === 0
        }

        TextInput {
            id: input
            clip: true
            anchors.fill: parent
            verticalAlignment: Text.AlignVCenter
            font.family: Theme.typography.family
            font.pixelSize: Theme.typography.secondaryText
            color: input.validator && input.text.length > 0 && !input.acceptableInput ? Theme.palette.error : Theme.components.field.text
            echoMode: root.echoMode
            enabled: root.enabled
            activeFocusOnTab: true
        }
    }
}
