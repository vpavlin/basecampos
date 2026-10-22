import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Logos.Theme
import Logos.Controls
import Logos.Icons

// Mail: the list → detail pattern (mail, chat, any inbox).
// Rules:
//  - medium/expanded (window ≥ 600): two panes. The list is fixed width
//    (320 in medium, 360 in expanded); the detail fills the rest. The selected
//    row stays highlighted and the detail always shows a message.
//  - compact (< 600): one pane. The list fills the page; tapping a row pushes
//    the detail full width, the scaffold shows a back chevron (canGoBack) and
//    goBack() returns to the list with the scroll position kept.
//  - Actions are real buttons, never hover-only: in the detail header on wide
//    windows, in a bar pinned to the bottom (thumb reach) on compact.
//  - Text lines elide; the body wraps and scrolls; reading width caps at 720.
// Lab state: --state open=1 starts with the first message open.
Item {
    id: page

    readonly property string title: compact && detailOpen ? "Message" : "Mail"
    readonly property string windowClass: parent && parent.windowClass ? parent.windowClass
                                                                        : Theme.windowClass(width)
    readonly property bool compact: windowClass === "compact"
    readonly property var labState: parent && parent.labState ? parent.labState : ({})

    property int selected: 0
    property bool detailOpen: labState.open === "1"
    readonly property bool canGoBack: compact && detailOpen
    // Compact: search folds into the top bar (scaffold); wider: our own bar.
    readonly property bool searchable: compact && !detailOpen
    readonly property string searchPlaceholder: "Search mail"
    readonly property string query: (compact ? (parent && parent.searchText ? parent.searchText : "")
                                             : search.text).toLowerCase()
    function goBack() { detailOpen = false }

    function open(index) {
        selected = index
        detailOpen = true
        if (messages.get(index).unread) messages.setProperty(index, "unread", false)
    }

    Component.onCompleted: if (detailOpen) open(0)

    ListModel {
        id: messages
        ListElement { sender: "Node Operators"; address: "ops@nodes.logos.co"; subject: "Testnet relaunch: epoch 1 starts Monday"; time: "09:42"; date: "Today, 09:42"; unread: true
            body: "Hi operators,\n\nThe v0.3 testnet relaunches on Monday. Epoch 1 starts at 03:50 CEST and runs about three and a half days. Nodes on 0.2.x will stop following the chain at the cutover.\n\nPlease upgrade to 0.3.0, keep your keys, and wipe the old chain state before you start. The node will sync from the new genesis on its own.\n\nIf your node stays at zero peers for more than ten minutes, check that the bootstrap peers are set in your config and post your logs in #node-operators." }
        ListElement { sender: "Kris Novak"; address: "kris@dappnode.example"; subject: "DAppNode package works on my box"; time: "08:15"; date: "Today, 08:15"; unread: true
            body: "Installed the Logos node package last night over the WireGuard tunnel. It synced in about forty minutes and the dashboard link opens fine.\n\nOne thing: the setup wizard asked for the service name twice. Not a blocker, just odd.\n\nWant me to try the arm64 build on the Rock 5 next?" }
        ListElement { sender: "Storage Network"; address: "noreply@storage.logos.co"; subject: "Your pin expires in 3 days"; time: "Yesterday"; date: "Yesterday, 18:03"; unread: true
            body: "The content you pinned is about to expire.\n\nCID: zDvZRwzm4fYm…k9Qe\nSize: 412 MB\nPinned by: 3 hosts\n\nRenew the pin from the Storage app to keep it available. Content with no pinners is dropped from the network after the grace period." }
        ListElement { sender: "Aria Chen"; address: "aria@logos.co"; subject: "Notes from the Basecamp design sync"; time: "Yesterday"; date: "Yesterday, 15:20"; unread: false
            body: "Thanks for joining. The short version:\n\n1. The sidebar collapses to an icon rail between 600 and 840 px.\n2. Under 600 the navigation moves into a drawer behind the menu button.\n3. Every tappable thing is at least 44 px on touch screens.\n\nNext sync is Thursday. Bring screenshots from the responsive lab." }
        ListElement { sender: "Keycard"; address: "keycard@logos.co"; subject: "New key paired"; time: "Mon"; date: "Monday, 11:02"; unread: false
            body: "A new Keycard was paired with this device.\n\nCard: Keycard Shell, serial ending 4F2A\nAccounts: 2\n\nIf this was not you, unpair the card from Settings › Keys and rotate your recovery phrase." }
        ListElement { sender: "Lambda Prize"; address: "prize@logos.co"; subject: "LP-0024 review window is open"; time: "Mon"; date: "Monday, 09:30"; unread: false
            body: "Submissions for LP-0024 (themed Basecamp) are ready for review.\n\nPlease check each one against the prize spec only, and leave a clear accept or reject with the failing criterion. Reviews close on Friday." }
        ListElement { sender: "Blend Network"; address: "noreply@blend.logos.co"; subject: "Your node joined the blend set"; time: "Sun"; date: "Sunday, 21:47"; unread: false
            body: "Your node is now an active blend provider.\n\nDeclaration ID: 6882b350…\nActive from: epoch 3\n\nKeep the node online. The heartbeat is sent every epoch; missing two in a row removes the node from the set." }
        ListElement { sender: "Marina Ivanova"; address: "marina@logos.co"; subject: "Referral program: finance questions"; time: "Sat"; date: "Saturday, 14:10"; unread: false
            body: "Before we can pay referral rewards we need three answers:\n\nWho holds the payout wallet, how often we pay, and what we do when a referred node goes offline in its first month.\n\nCan you draft a one-pager by next week?" }
        ListElement { sender: "Delivery"; address: "noreply@delivery.logos.co"; subject: "Relay limit reached on sneg"; time: "Oct 16"; date: "16 October, 22:31"; unread: false
            body: "The delivery service on host sneg stopped accepting connections.\n\nReason: too many open files (1024).\n\nRaise the open-file limit for the service and restart it. Messages sent while it was down will be retried by the senders." }
        ListElement { sender: "App Catalog"; address: "catalog@logos.co"; subject: "Docs Assistant 0.2.6 published"; time: "Oct 14"; date: "14 October, 10:05"; unread: false
            body: "Your module Docs Assistant 0.2.6 is now listed in the catalog.\n\nSigned by: xAlisher\nDownload: storage first, HTTPS fallback\n\nUsers on older versions will see the update on their next catalog refresh." }
        ListElement { sender: "Sequencer"; address: "sequencer@logos.live"; subject: "Weekly sequencer report"; time: "Oct 12"; date: "12 October, 07:00"; unread: false
            body: "Blocks produced: 100,812\nTransactions: 4,309\nUptime: 99.96%\n\nNo restarts this week. Proof generation stayed under two seconds per block." }
        ListElement { sender: "Logos Forum"; address: "forum@logos.co"; subject: "3 new replies in “Running a node at home”"; time: "Oct 9"; date: "9 October, 19:44"; unread: false
            body: "New replies since your last visit:\n\n• “What disk do I need for a year of chain data?”\n• “My router blocks the p2p port, any tips?”\n• “Is a Raspberry Pi enough?”\n\nOpen the thread to reply." }
    }

    // One inbox row: sender + time, subject, one-line preview, unread dot.
    // The whole row is the target; the selected row keeps the selected fill.
    component MessageRow: AbstractButton {
        id: row
        required property int index
        required property string sender
        required property string subject
        required property string body
        required property string time
        required property bool unread
        property bool selected: false

        hoverEnabled: true
        implicitHeight: Math.max(Theme.minTouchTarget, content.implicitHeight + 2 * Theme.spacing.medium)
        Accessible.name: sender + ", " + subject + (unread ? ", unread" : "")

        background: Rectangle {
            color: row.selected ? Theme.components.listItem.backgroundSelected
                 : row.pressed ? Theme.components.listItem.backgroundPressed
                 : row.hovered ? Theme.components.listItem.backgroundHover : "transparent"
            Rectangle {
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom; leftMargin: 36 }
                height: 1
                color: Theme.palette.borderSubtle
            }
        }

        contentItem: Item {}

        Rectangle {
            id: dot
            x: Theme.spacing.large - 2
            y: Theme.spacing.medium + 6
            width: 8; height: 8; radius: 4
            color: Theme.palette.accentOrange
            visible: row.unread
        }

        ColumnLayout {
            id: content
            anchors { left: parent.left; right: parent.right; top: parent.top
                      leftMargin: 36; rightMargin: Theme.spacing.large; topMargin: Theme.spacing.medium }
            spacing: 2
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacing.small
                LogosText {
                    Layout.fillWidth: true
                    text: row.sender
                    elide: Text.ElideRight
                    font.weight: row.unread ? Theme.typography.weightBold : Theme.typography.weightMedium
                }
                LogosText {
                    text: row.time
                    font.pixelSize: Theme.typography.secondaryText
                    color: row.unread ? Theme.palette.accentOrange : Theme.palette.textSecondary
                }
            }
            LogosText {
                Layout.fillWidth: true
                text: row.subject
                elide: Text.ElideRight
                font.weight: row.unread ? Theme.typography.weightMedium : Theme.typography.weightRegular
            }
            LogosText {
                Layout.fillWidth: true
                text: row.body.replace(/\s+/g, " ")
                elide: Text.ElideRight
                maximumLineCount: 1
                font.pixelSize: Theme.typography.secondaryText
                color: Theme.palette.textSecondary
            }
        }
    }

    // ---------------------------------------------------------------- list
    Rectangle {
        id: listPane
        visible: !page.compact || !page.detailOpen
        width: page.compact ? page.width : (page.windowClass === "expanded" ? 360 : 320)
        height: page.height
        color: page.compact ? "transparent" : Theme.palette.backgroundSecondary

        LogosSearchBar {
            id: search
            visible: !page.compact
            anchors { left: parent.left; right: parent.right; top: parent.top
                      margins: page.compact ? Theme.spacing.large : Theme.spacing.medium }
            placeholderText: "Search mail"
        }

        ListView {
            id: list
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom
                      top: page.compact ? parent.top : search.bottom
                      topMargin: page.compact ? 0 : Theme.spacing.small }
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            model: messages
            delegate: MessageRow {
                width: ListView.view.width
                readonly property string q: page.query
                readonly property bool matches: q === "" || (sender + " " + subject + " " + body).toLowerCase().indexOf(q) >= 0
                visible: matches
                height: matches ? implicitHeight : 0
                selected: !page.compact && index === page.selected
                onClicked: page.open(index)
            }
        }

        Rectangle {
            visible: !page.compact
            anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
            width: 1
            color: Theme.palette.borderSubtle
        }
    }

    // -------------------------------------------------------------- detail
    Item {
        id: detailPane
        visible: !page.compact || page.detailOpen
        x: page.compact ? 0 : listPane.width
        width: page.width - x
        height: page.height

        readonly property var msg: messages.count > 0 ? messages.get(page.selected) : null
        readonly property int margin: page.compact ? Theme.spacing.large : Theme.spacing.xlarge

        Flickable {
            id: reader
            anchors { left: parent.left; right: parent.right; top: parent.top; bottom: actionBar.visible ? actionBar.top : parent.bottom }
            contentHeight: detail.implicitHeight + 2 * detailPane.margin
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ColumnLayout {
                id: detail
                x: detailPane.margin
                y: detailPane.margin
                width: Math.min(reader.width - 2 * detailPane.margin, 720)
                spacing: Theme.spacing.large

                LogosText {
                    Layout.fillWidth: true
                    text: detailPane.msg ? detailPane.msg.subject : ""
                    wrapMode: Text.Wrap
                    font.pixelSize: page.compact ? Theme.typography.panelTitleText - 4 : Theme.typography.panelTitleText
                    font.weight: Theme.typography.weightMedium
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacing.medium
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        LogosText {
                            Layout.fillWidth: true
                            text: detailPane.msg ? detailPane.msg.sender : ""
                            elide: Text.ElideRight
                            font.weight: Theme.typography.weightMedium
                        }
                        LogosText {
                            Layout.fillWidth: true
                            text: detailPane.msg ? detailPane.msg.address : ""
                            elide: Text.ElideRight
                            font.pixelSize: Theme.typography.secondaryText
                            color: Theme.palette.textSecondary
                        }
                    }
                    LogosText {
                        text: detailPane.msg ? detailPane.msg.date : ""
                        font.pixelSize: Theme.typography.secondaryText
                        color: Theme.palette.textSecondary
                    }
                }

                // Wide windows: actions sit with the header.
                Flow {
                    visible: !page.compact
                    Layout.fillWidth: true
                    spacing: Theme.spacing.small
                    LogosButton {
                        text: "Reply"
                        variant: LogosButton.Variant.Primary
                        leadingIcon.source: LogosIcons.arrowLeft
                        leadingIcon.color: Theme.palette.accentForeground
                    }
                    LogosButton { text: "Archive"; leadingIcon.source: LogosIcons.trash }
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: Theme.palette.borderSubtle }

                LogosText {
                    Layout.fillWidth: true
                    text: detailPane.msg ? detailPane.msg.body : ""
                    wrapMode: Text.Wrap
                    lineHeight: 1.4
                    font.pixelSize: Theme.typography.subtitleText - 1
                }
            }
        }

        // Compact: actions pinned to the bottom, equal width, in thumb reach.
        Rectangle {
            id: actionBar
            visible: page.compact
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            height: actions.implicitHeight + 2 * Theme.spacing.medium
            color: Theme.palette.backgroundSecondary
            Rectangle { width: parent.width; height: 1; color: Theme.palette.borderSubtle }
            RowLayout {
                id: actions
                anchors { fill: parent; margins: Theme.spacing.medium; leftMargin: Theme.spacing.large; rightMargin: Theme.spacing.large }
                spacing: Theme.spacing.medium
                LogosButton {
                    Layout.fillWidth: true
                    text: "Reply"
                    variant: LogosButton.Variant.Primary
                    leadingIcon.source: LogosIcons.arrowLeft
                    leadingIcon.color: Theme.palette.accentForeground
                }
                LogosButton {
                    Layout.fillWidth: true
                    text: "Archive"
                    leadingIcon.source: LogosIcons.trash
                    onClicked: page.goBack()
                }
            }
        }
    }
}
