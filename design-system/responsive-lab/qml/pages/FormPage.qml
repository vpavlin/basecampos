import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Logos.Theme
import Logos.Controls
import Logos.Icons

// Form: "Run a node". Rules, by window class (Theme.windowClass):
//   expanded (>= 840): label column (200 px) beside each field; the form stops
//       at 720 px and centres; actions sit bottom-right (Cancel, Start node).
//   medium (600-839): labels above fields; short fields share a row
//       (disk limit + peers); actions bottom-right.
//   compact (< 600): labels above fields, one field per row, full-width
//       inputs; actions stack full width with the primary last.
// At every size the page scrolls, every tap target is >= 44 px, and help text
// is printed under its field (nothing hides behind hover or tooltips).
Flickable {
    id: page
    readonly property string title: "Run a node"
    readonly property string windowClass: Theme.windowClass(width)
    readonly property bool compact: windowClass === "compact"
    readonly property bool expanded: windowClass === "expanded"
    readonly property int margin: Theme.pageMargin(width)
    readonly property int labelWidth: 200
    readonly property int touch: Theme.minTouchTarget

    contentHeight: column.implicitHeight + 2 * margin
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

    // Lab: `--state scroll=end` opens the page scrolled to the actions.
    readonly property var labState: parent && parent.labState ? parent.labState : ({})
    Timer {
        running: page.labState.scroll === "end"; interval: 50
        onTriggered: page.contentY = Math.max(0, page.contentHeight - page.height)
    }

    // ── Form state ─────────────────────────────────────────────────────────
    readonly property string nameError:
        /^[a-z0-9-]{3,32}$/.test(nameField.text)
            ? "" : "Only lowercase letters, digits and dashes (3 to 32)."
    readonly property int diskGb: Math.round(diskSlider.value)

    // One labelled field. Expanded: label left (200 px), field right.
    // Otherwise: label above the field. Help and error text sit under the field.
    component FormRow: GridLayout {
        id: row
        property string label
        property string help
        property string error
        default property alias content: slot.data

        Layout.fillWidth: true
        columns: page.expanded ? 2 : 1
        columnSpacing: Theme.spacing.large
        rowSpacing: Theme.spacing.tiny

        LogosText {
            visible: row.label.length > 0 || page.expanded
            text: row.label
            font.weight: Font.DemiBold
            color: Theme.palette.text
            wrapMode: Text.WordWrap
            verticalAlignment: Text.AlignVCenter
            Layout.preferredWidth: page.expanded ? page.labelWidth : -1
            Layout.fillWidth: !page.expanded
            Layout.minimumHeight: page.expanded ? page.touch : 0
            Layout.alignment: Qt.AlignTop
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing.tiny
            Item {
                id: slot
                Layout.fillWidth: true
                implicitHeight: childrenRect.height
                // Children fill the slot's width.
                function fit() { for (var i = 0; i < children.length; ++i) children[i].width = width }
                onWidthChanged: fit()
                onChildrenChanged: fit()
            }
            LogosText {
                visible: row.error.length > 0
                text: row.error
                color: Theme.palette.error
                font.pixelSize: Theme.typography.secondaryText
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            LogosText {
                visible: row.help.length > 0
                text: row.help
                color: Theme.palette.textSecondary
                font.pixelSize: Theme.typography.secondaryText
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }
    }

    // Two short fields: side by side on medium, stacked otherwise (expanded
    // rows already carry their label beside the field).
    component Pair: GridLayout {
        Layout.fillWidth: true
        columns: page.windowClass === "medium" ? 2 : 1
        columnSpacing: Theme.spacing.xlarge
        rowSpacing: Theme.spacing.large
    }

    component Section: ColumnLayout {
        property string heading
        Layout.fillWidth: true
        spacing: Theme.spacing.large
        LogosText {
            text: parent.heading
            font.pixelSize: Theme.typography.subtitleText
            font.weight: Font.DemiBold
        }
        Rectangle {
            Layout.fillWidth: true
            Layout.topMargin: -Theme.spacing.small
            implicitHeight: 1
            color: Theme.palette.borderSecondary
        }
    }

    ColumnLayout {
        id: column
        x: Math.max(page.margin, (page.width - width) / 2)
        y: page.margin
        width: Math.min(page.width - 2 * page.margin, 720)
        spacing: Theme.spacing.xxlarge

        LogosText {
            Layout.fillWidth: true
            text: "Join the Logos testnet. Your node keeps a copy of the chain and relays blocks to peers."
            color: Theme.palette.textSecondary
            wrapMode: Text.WordWrap
        }

        // ── Node ───────────────────────────────────────────────────────────
        Section {
            heading: "Node"

            Pair {
                FormRow {
                    Layout.preferredWidth: 1
                    Layout.alignment: Qt.AlignTop
                    label: "Name"
                    error: page.nameError
                    help: "Shown to peers. You can change it later."
                    LogosTextField {
                        id: nameField
                        text: page.labState.name !== undefined
                              ? page.labState.name : "sneg node"
                        placeholderText: "e.g. sneg-node"
                        validator: RegularExpressionValidator { regularExpression: /[a-z0-9-]{3,32}/ }
                    }
                }
                FormRow {
                    Layout.preferredWidth: 1
                    Layout.alignment: Qt.AlignTop
                    label: "Network"
                    LogosComboBox {
                        implicitHeight: Math.max(Theme.components.combo.height, page.touch)
                        model: ["Testnet", "Devnet"]
                    }
                }
            }
            FormRow {
                label: "Data folder"
                help: "Where blocks and keys are stored."
                RowLayout {
                    spacing: Theme.spacing.small
                    LogosTextField {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        Layout.fillHeight: true
                        text: "~/.local/share/logos/node"
                    }
                    LogosButton {
                        text: "Browse…"
                        Layout.preferredHeight: Math.max(implicitHeight, page.touch)
                    }
                }
            }
        }

        // ── Resources ──────────────────────────────────────────────────────
        Section {
            heading: "Resources"

            LogosNotice {
                Layout.fillWidth: true
                severity: LogosNotice.Info
                title: "Disk use grows over time"
                message: "A full node stores every block since genesis, about 1.2 GB a week on testnet. "
                         + "When the limit is reached the oldest blocks are pruned and the node keeps running."
            }

            Pair {
                FormRow {
                    Layout.preferredWidth: 1
                    Layout.alignment: Qt.AlignTop
                    label: "Disk limit"
                    help: "Pruned above this size."
                    RowLayout {
                        spacing: Theme.spacing.medium
                        LogosSlider {
                            id: diskSlider
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            Layout.preferredHeight: page.touch
                            from: 20; to: 500; stepSize: 10; value: 120
                        }
                        LogosText {
                            text: page.diskGb + " GB"
                            font.weight: Font.DemiBold
                            horizontalAlignment: Text.AlignRight
                            Layout.preferredWidth: 64
                        }
                    }
                }
                FormRow {
                    Layout.preferredWidth: 1
                    Layout.alignment: Qt.AlignTop
                    label: "Peers"
                    help: "Outbound connections, 4 to 64."
                    Item {
                        implicitHeight: peers.height
                        LogosSpinBox {
                            id: peers
                            width: Math.min(parent.width, 160)
                            height: page.touch
                            from: 4; to: 64; value: 16
                        }
                    }
                }
            }

            FormRow {
                label: "Mode"
                help: "Light nodes skip block history and use about 2 GB."
                ButtonGroup { id: modeGroup }
                Flow {
                    spacing: Theme.spacing.xlarge
                    LogosRadioButton {
                        text: "Full"
                        checked: true
                        ButtonGroup.group: modeGroup
                        height: Math.max(implicitHeight, page.touch)
                    }
                    LogosRadioButton {
                        text: "Light"
                        ButtonGroup.group: modeGroup
                        height: Math.max(implicitHeight, page.touch)
                    }
                }
            }
            FormRow {
                label: page.expanded ? "Startup" : ""
                LogosCheckbox {
                    text: "Start on login"
                    checked: true
                    height: Math.max(implicitHeight, page.touch)
                }
            }
        }

        // ── Notes ──────────────────────────────────────────────────────────
        Section {
            heading: "Notes"

            FormRow {
                label: "About this node"
                help: "Private. Only kept on this device."
                LogosTextArea {
                    height: 120
                    wrapMode: TextEdit.Wrap
                    placeholderText: "Anything to remember about this node"
                    text: "Home server in the hallway cupboard. Runs on the UPS."
                }
            }
        }

        // ── Actions ────────────────────────────────────────────────────────
        // Compact: stacked full width, primary last (nearest the thumb).
        // Medium/expanded: one row, right-aligned, primary rightmost.
        GridLayout {
            Layout.fillWidth: true
            columns: page.compact ? 1 : 3
            columnSpacing: Theme.spacing.medium
            rowSpacing: Theme.spacing.medium

            Item { visible: !page.compact; Layout.fillWidth: true }
            LogosButton {
                text: "Cancel"
                Layout.fillWidth: page.compact
                Layout.preferredHeight: Math.max(implicitHeight, page.touch)
            }
            LogosButton {
                text: "Start node"
                variant: LogosButton.Variant.Primary
                Layout.fillWidth: page.compact
                Layout.preferredHeight: Math.max(implicitHeight, page.touch)
            }
        }
    }
}
