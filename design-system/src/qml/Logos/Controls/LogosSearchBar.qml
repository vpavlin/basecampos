import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts

import Logos.Theme
import Logos.Controls
import Logos.Icons

// Public API:
//     text             alias — read/write the current query
//     placeholderText  alias — default "Search..."
//     echoMode         alias
//     iconSource       URL of the leading icon (empty hides it; defaults to
//                      the bundled icons/search.svg)
//     shortcutHint     literal text to render in the trailing chip
//                      (e.g. "⌘ K", "Ctrl+K"). Display-only — does not
//                      bind anything. For platform-aware text, pass the
//                      `nativeText` of the consumer's own Shortcut.
//     clearable        show a ✕ while the field has text (default true).
//                      A search that filters a list is view state and should
//                      survive navigation, which only works if there is an
//                      obvious way out of it. Turn off for fields that are
//                      cleared some other way, or that must not be.
//
// Read-only inspection aliases:
//     textInput        the inner TextInput (cursor, selection, etc.)
//     fieldItem        the inner LogosTextField
//     iconItem         the leading icon container
//     clearItem        the trailing clear button (a 24 px circle; at touch
//                      density its hit area is Theme.minTouchTarget square,
//                      the circle stays 24 px and centred)
//     shortcutItem     the trailing shortcut chip
//
// Signal:
//     submitted(text)  emitted when the user presses Return/Enter
//
// Example (with consumer-owned shortcut and platform-aware chip text):
//     Shortcut {
//         id: searchShortcut
//         sequence: "Ctrl+K"
//         context: Qt.WindowShortcut
//         onActivated: {
//             searchBar.textInput.forceActiveFocus()
//             searchBar.textInput.selectAll()
//         }
//     }
//     LogosSearchBar {
//         id: searchBar
//         shortcutHint: searchShortcut.nativeText   // "⌘ K" on macOS
//         placeholderText: qsTr("Search packages...")
//         onSubmitted: function(text) { runSearch(text) }
//     }
Control {
    id: root

    property alias text: field.text
    property alias placeholderText: field.placeholderText
    property alias echoMode: field.echoMode
    property url iconSource: LogosIcons.search
    property string shortcutHint: ""
    property bool clearable: true

    // Exposed for inspection (e.g., from tests). Read-only.
    readonly property alias textInput: field.textInput
    readonly property alias fieldItem: field
    readonly property alias iconItem: iconHolder
    readonly property alias clearItem: clearButton
    readonly property alias shortcutItem: shortcutChip

    QtObject {
        id: d

        readonly property bool clearVisible:
            root.clearable && field.text.length > 0
    }

    signal submitted(string text)

    implicitWidth: 400
    implicitHeight: Theme.components.search.height

    // Match the figma spec (8/8/8/10) using existing tokens.
    leftPadding: Theme.spacing.small + 2
    rightPadding: Theme.spacing.small
    topPadding: Theme.spacing.small
    bottomPadding: Theme.spacing.small

    focusPolicy: Qt.StrongFocus
    activeFocusOnTab: true
    onActiveFocusChanged: if (activeFocus)
        field.textInput.forceActiveFocus()

    background: Rectangle {
        id: searchBg
        color: Theme.components.search.background
        radius: Theme.components.search.radius
        border.width: Theme.components.search.borderWidth
        border.color: field.textInput.activeFocus
                      ? Theme.components.search.borderFocus
                      : Theme.components.search.border

        LogosSurface {
            anchors.fill: parent
            anchors.margins: searchBg.border.width
            radius: Math.max(0, searchBg.radius - searchBg.border.width)
            slot: Theme.components.search.gradient
            visible: hasGradient
        }
    }

    contentItem: RowLayout {
        spacing: Theme.spacing.small

        Item {
            id: iconHolder

            Layout.alignment: Qt.AlignVCenter
            visible: root.iconSource.toString().length > 0
            implicitWidth: 20
            implicitHeight: 20

            Image {
                id: iconImg
                anchors.fill: parent
                source: root.iconSource
                sourceSize: Qt.size(parent.width * 2, parent.height * 2)
                fillMode: Image.PreserveAspectFit
                visible: false
            }
            MultiEffect {
                anchors.fill: iconImg
                source: iconImg
                colorization: 1.0
                colorizationColor: Theme.palette.textSecondary
            }
        }

        LogosTextField {
            id: field

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.alignment: Qt.AlignVCenter
            background: Item {}
            placeholderText: "Search..."
            activeFocusOnTab: false
            textInput.activeFocusOnTab: false
            textInput.color: Theme.components.search.text
        }

        // Round ✕ that turns accent on hover, like the sidebar's circle buttons.
        LogosAbstractButton {
            id: clearButton

            Layout.alignment: Qt.AlignVCenter
            visible: d.clearVisible
            implicitWidth: Theme.touch ? Theme.minTouchTarget : 24
            implicitHeight: implicitWidth
            focusPolicy: Qt.NoFocus
            activeFocusOnTab: false
            Accessible.name: qsTr("Clear search")
            onClicked: {
                field.text = ""
                field.textInput.forceActiveFocus()
            }

            background: Item {
                Rectangle {
                    anchors.centerIn: parent
                    width: 24
                    height: 24
                    radius: width / 2
                    color: clearButton.isActive ? Theme.palette.accentOrange
                                                : Theme.palette.surface
                }
            }

            contentItem: Item {
                LogosIcon {
                    anchors.centerIn: parent
                    width: 12
                    height: 12
                    source: LogosIcons.close
                    color: clearButton.isActive ? Theme.palette.accentForeground
                                                : Theme.palette.textSecondary
                    brightness: clearButton.isActive ? 1.0 : 0
                }
            }
        }

        Rectangle {
            id: shortcutChip

            Layout.alignment: Qt.AlignVCenter
            // Only a hint for getting here: hidden once the field has focus.
            visible: root.shortcutHint.length > 0 && !field.textInput.activeFocus
            implicitWidth: shortcutLabel.implicitWidth + Theme.spacing.medium
            implicitHeight: 20

            color: Theme.palette.surfaceRaised
            radius: Theme.spacing.radiusSmall
            border.width: 1
            border.color: Theme.palette.borderSubtle

            LogosText {
                id: shortcutLabel

                anchors.centerIn: parent
                text: root.shortcutHint
                font.pixelSize: Theme.typography.secondaryText
                font.weight: Theme.typography.weightMedium
                color: Theme.palette.textSecondary
            }
        }
    }

    // Forward the inner TextInput's `accepted` signal 
    Connections {
        target: field.textInput
        function onAccepted() { root.submitted(field.text) }
    }
}
