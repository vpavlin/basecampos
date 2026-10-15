import QtQuick
import QtQuick.Controls

import Logos.Theme
import Logos.Controls

// List / menu row delegate. Inherits from Qt's `ItemDelegate` so it
// gets the standard `text` / `pressed` / `hovered` / `highlighted`
// surface — the customisations are background coloring, an optional
// rounded corner, and theme-driven typography.
//
//     LogosItemDelegate {
//         text: "Settings"
//         font.pixelSize: Theme.typography.subtitleText
//         font.weight: Theme.typography.weightMedium
//         textColor: Theme.palette.textTertiary
//     }
//
// Public extension knobs:
//     hoverColor / pressedColor / highlightColor   surface colors per state
//                  (hoverColor also used for keyboard visualFocus;
//                   highlightColor is for highlighted/selected only)
//     textColor                                    label color when enabled
//                                                  (disabled falls back to textMuted)
//     radius                                       background rounding (default 0)
//
// Read-only inspection aliases (tests / tooling):
//     backgroundItem    the background Rectangle (state-coloured surface)
//     labelItem         the LogosText content item (responds to `text` /
//                       `font` / `textColor`)
ItemDelegate {
    id: root

    property color hoverColor: Theme.components.listItem.backgroundHover
    property color pressedColor: Theme.components.listItem.backgroundPressed
    property color highlightColor: Theme.components.listItem.backgroundSelected
    property color textColor: Theme.components.listItem.text
    property real radius: 0

    // Exposed for inspection (e.g., from tests). Read-only.
    readonly property alias backgroundItem: bg
    readonly property alias labelItem: label

    implicitHeight: Theme.components.listItem.height

    leftPadding: Theme.components.listItem.paddingX
    rightPadding: Theme.components.listItem.paddingX

    font.family: Theme.typography.family
    font.pixelSize: Theme.typography.primaryText
    font.weight: Theme.typography.weightRegular

    focusPolicy: Qt.StrongFocus
    activeFocusOnTab: true

    contentItem: LogosText {
        id: label
        text: root.text
        font: root.font
        verticalAlignment: Text.AlignVCenter
        color: root.enabled ? root.textColor : Theme.palette.textMuted
        elide: Text.ElideRight
    }

    background: Rectangle {
        id: bg
        radius: root.radius
        // highlighted = selected/current row; visualFocus matches hover so
        // Tab focus does not look like selection.
        color: root.pressed
               ? root.pressedColor
               : (root.highlighted
                    ? root.highlightColor
                    : (root.hovered || root.visualFocus
                        ? root.hoverColor : "transparent"))
    }

    HoverHandler {
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    }
}
