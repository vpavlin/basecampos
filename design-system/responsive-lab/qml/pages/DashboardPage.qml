import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Logos.Theme
import Logos.Controls
import Logos.Icons

// Dashboard: a node at a glance. Rule: the grid follows the width it actually
// gets, not the window class. Every card has a minimum width and the column
// count is derived from it, so text never gets squeezed:
//   stat cards   4 across if 4 × minStat fit, else 2 × 2, else 1 column
//                (never 3 + 1 — an orphan card reads as a mistake)
//   Sync | Quick actions  side by side 2:1 when Sync gets ≥ minSync and
//                actions ≥ minActions, else stacked
//   quick actions  a row of 3 when each button gets ≥ minButton, else one
//                full-width button per line
// Content stops growing at 1200 and centres on very wide windows.
Flickable {
    id: page
    readonly property string title: "Dashboard"
    readonly property string windowClass: Theme.windowClass(width)

    contentHeight: column.implicitHeight + 2 * margin
    clip: true
    boundsBehavior: Flickable.StopAtBounds

    readonly property int margin: Theme.pageMargin(width)
    readonly property int gap: Theme.spacing.medium
    readonly property int maxContent: 1200
    readonly property real contentWidth_: Math.min(width - 2 * margin, maxContent)

    // Minimum widths — the only layout inputs.
    readonly property int minStat: 150
    readonly property int minSync: 400
    readonly property int minActions: 220
    readonly property int minButton: 140

    function fits(n, min, avail) { return n * min + (n - 1) * gap <= avail }

    readonly property int statColumns: fits(4, minStat, contentWidth_) ? 4
                                     : fits(2, minStat, contentWidth_) ? 2 : 1
    readonly property bool sideBySide: (contentWidth_ - gap) * 2 / 3 >= minSync
                                       && (contentWidth_ - gap) / 3 >= minActions

    component Card: LogosFrame {
        default property alias body: inner.data
        property string heading
        Layout.fillWidth: true
        padding: Theme.spacing.large
        radius: Theme.spacing.radiusLarge  // same surface as LogosStatCard
        backgroundColor: Theme.palette.surfaceRaised
        borderColor: "transparent"
        ColumnLayout {
            width: parent.width
            spacing: Theme.spacing.medium
            LogosText {
                visible: text !== ""
                text: heading
                font.pixelSize: Theme.typography.subtitleText
                font.weight: Font.DemiBold
            }
            ColumnLayout { id: inner; Layout.fillWidth: true; spacing: Theme.spacing.medium }
        }
    }

    component Figure: ColumnLayout {
        property string label
        property string value
        spacing: 2
        Layout.fillWidth: true
        Layout.preferredWidth: 1
        LogosText {
            Layout.fillWidth: true
            text: label
            font.pixelSize: Theme.typography.secondaryText
            color: Theme.palette.textSecondary
            elide: Text.ElideRight
        }
        LogosText {
            Layout.fillWidth: true
            text: value
            font.pixelSize: Theme.typography.subtitleText
            font.weight: Theme.typography.weightMedium
            elide: Text.ElideRight
        }
    }

    readonly property var events: [
        { t: "Block proposed",        s: "Slot 4,812,227 · reward 12.5 LGS", w: "2m" },
        { t: "Peer connected",        s: "12D3KooW…x9Fq via /ip4/65.21.8.14",   w: "6m" },
        { t: "Reward claimed",        s: "Voucher 0x4f1c…a2 · 38.2 LGS",        w: "18m" },
        { t: "Epoch 14 started",      s: "Aged stake 7.69 B · leader eligible",  w: "41m" },
        { t: "Peer disconnected",     s: "12D3KooW…Lm2p timed out",              w: "1h" },
        { t: "Blend heartbeat sent",  s: "Session 212 · 3 messages mixed",       w: "2h" },
        { t: "Snapshot written",      s: "Ledger state at height 1,284,000",     w: "3h" },
        { t: "Node started",          s: "logos-blockchain-node 0.3.0",          w: "1d" },
    ]

    ColumnLayout {
        id: column
        x: Math.max(page.margin, (page.width - width) / 2)
        y: page.margin
        width: page.contentWidth_
        spacing: Theme.spacing.xlarge

        // ── Stat cards ────────────────────────────────────────────
        GridLayout {
            Layout.fillWidth: true
            columns: page.statColumns
            columnSpacing: page.gap
            rowSpacing: page.gap
            uniformCellWidths: true

            LogosStatCard {
                Layout.fillWidth: true; Layout.minimumWidth: page.minStat
                label: "Height"; value: "1,284,503"; caption: "Synced"
                valueFontSizeMode: Text.HorizontalFit
            }
            LogosStatCard {
                Layout.fillWidth: true; Layout.minimumWidth: page.minStat
                label: "Peers"; value: "24"; caption: "8 inbound · 16 out"
            }
            LogosStatCard {
                Layout.fillWidth: true; Layout.minimumWidth: page.minStat
                label: "Uptime"; value: "6d 14h"; caption: "Since last restart"
            }
            LogosStatCard {
                Layout.fillWidth: true; Layout.minimumWidth: page.minStat
                label: "Disk"; value: "71%"; caption: "52 / 73 GB"
                severity: LogosStatCard.Warning
            }
        }

        // ── Sync + Quick actions ──────────────────────────────────
        GridLayout {
            Layout.fillWidth: true
            columns: page.sideBySide ? 2 : 1
            columnSpacing: page.gap
            rowSpacing: page.gap

            Card {
                heading: "Sync"
                Layout.fillWidth: !page.sideBySide
                Layout.preferredWidth: page.sideBySide ? (page.contentWidth_ - page.gap) * 2 / 3 : -1
                Layout.alignment: Qt.AlignTop
                Layout.fillHeight: page.sideBySide
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacing.small
                    LogosText {
                        text: "98.6%"
                        font.pixelSize: Theme.typography.titleText
                        font.weight: Theme.typography.weightBold
                    }
                    LogosText {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignBottom
                        Layout.bottomMargin: 4
                        text: "catching up · ~4 min left"
                        color: Theme.palette.textSecondary
                        elide: Text.ElideRight
                    }
                }
                LogosProgressBar { Layout.fillWidth: true; from: 0; to: 1; value: 0.986 }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacing.large
                    Figure { label: "Local height"; value: "1,284,503" }
                    Figure { label: "Network tip"; value: "1,302,611" }
                }
                LogosText {
                    Layout.fillWidth: true
                    text: "Last block 4 s ago · 19 of 24 peers at tip"
                    font.pixelSize: Theme.typography.secondaryText
                    color: Theme.palette.textTertiary
                    elide: Text.ElideRight
                }
            }

            Card {
                id: actions
                heading: "Quick actions"
                Layout.fillWidth: !page.sideBySide
                Layout.preferredWidth: page.sideBySide ? (page.contentWidth_ - page.gap) / 3 : -1
                Layout.alignment: Qt.AlignTop
                Layout.fillHeight: page.sideBySide
                readonly property bool rowOfButtons:
                    page.fits(3, page.minButton, actions.availableWidth)

                GridLayout {
                    Layout.fillWidth: true
                    columns: actions.rowOfButtons ? 3 : 1
                    columnSpacing: Theme.spacing.small
                    rowSpacing: Theme.spacing.small
                    uniformCellWidths: true
                    LogosButton {
                        Layout.fillWidth: true; text: "Claim rewards"
                        variant: LogosButton.Variant.Primary
                    }
                    LogosButton { Layout.fillWidth: true; text: "Restart node" }
                    LogosButton { Layout.fillWidth: true; text: "Open logs" }
                }
            }
        }
        // ── Recent events ─────────────────────────────────────────
        Card {
            heading: "Recent events"
            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: -Theme.spacing.large
                Layout.rightMargin: -Theme.spacing.large
                spacing: 0
                Repeater {
                    model: page.events
                    LogosListRow {
                        required property var modelData
                        required property int index
                        Layout.fillWidth: true
                        title: modelData.t
                        subtitle: modelData.s
                        value: modelData.w
                        showSeparator: index < page.events.length - 1
                    }
                }
            }
        }
    }
}
