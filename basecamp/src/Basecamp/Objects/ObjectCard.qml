import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Logos.Controls
import Logos.Theme

import "ObjectFormat.js" as F
import "ObjectHighlight.js" as H

// ObjectCard: one object (SHELL-OBJECTS-SPEC §3, §7), drawn by its type.
//
// Shared by every view of objects (Basecamp's home today; a launcher or a
// replacement home may reuse it or draw its own). The frame is the same for
// all types: the publisher's icon (or the tile's letters, ADR-071), a heading,
// the type and time, a progress bar, the actions and a reply field. What sits
// in the middle comes from the type's own body (ObjectCardTransaction, …);
// a type without one, an app's own `x.<app>.<name>` included, gets
// ObjectCardGeneric. Everything is plain text: nothing an app sent is read as
// markup or a link. The lines that mark matches are StyledText, the
// publisher's text escaped before the mark is added (ObjectHighlight.js).
//
// Public API:
//     object        the envelope (ObjectResults' `object` role, or get(row))
//     now           ms since epoch for relative times; bind it to a clock that
//                   ticks, or times stay as they were first drawn
//     compact       one 44 px row: icon, title, a short value, time. No body,
//                   no actions (a small module, a dense list)
//     isNew         draws the "new" dot (the view decides what is new)
//     query         the search's text: the heading, title, subtitle and text
//                   mark the start of each word it matches, in the accent
//                   (basecamp-experiments#46)
//     namePrefix    objectName of the card; its controls are
//                   <namePrefix>.action.<id> (View is .action.open),
//                   <namePrefix>.reply and <namePrefix>.reply.send
//     actionRequested(action, text)
//                   "open" when the card itself is clicked, or an action id;
//                   `text` for an `input: "text"` action. The view sends it
//                   (shell.raise); the card never acts by itself.
Rectangle {
    id: card

    property var object: ({})
    property real now: Date.now()
    property bool compact: false
    property bool isNew: false
    property string query: ""
    property string namePrefix: "objectCard." + F.str(card.object.key)

    signal actionRequested(string action, string text)

    readonly property var fields: card.object && card.object.fields
                                  && typeof card.object.fields === "object" ? card.object.fields : ({})
    readonly property string type: F.str(card.object.type)
    readonly property string cardType: F.cardFor(card.type)
    readonly property bool summary: F.str(card.object.kind) === "summary"
    readonly property var actions: F.actions(card.object)
    readonly property bool hasProgress: typeof card.object.progress === "number"
                                        && isFinite(card.object.progress)
    readonly property real progress: card.hasProgress ? Math.max(0, Math.min(100, card.object.progress)) : 0
    readonly property string timeText: F.relativeTime(F.timeOf(card.object), card.now)

    // The action whose reply field is open ("" = none).
    property string replyAction: ""

    objectName: card.namePrefix
    implicitWidth: 320
    implicitHeight: card.compact ? Math.max(Theme.minTouchTarget, row.implicitHeight + 8)
                                 : column.implicitHeight + 2 * Theme.spacing.medium
    radius: Theme.spacing.radiusLarge
    color: tap.pressed ? Theme.palette.backgroundTertiary : Theme.palette.backgroundSecondary
    // Focused from the keyboard: an accent ring, drawn inside, so nothing
    // moves (basecamp-experiments#48).
    border.width: card.activeFocus ? 2 : 1
    border.color: card.activeFocus ? Theme.palette.accentOrange
                : tap.containsMouse ? Theme.palette.border : Theme.palette.borderHairline
    activeFocusOnTab: true
    Accessible.role: Accessible.Button
    Accessible.name: F.str(card.object.title)
    Accessible.focusable: true

    // Enter on the focused card: what its primary button does (the first
    // action, a reply field opened for a text one), else View. A focused
    // action button inside is not the card: the key passes on.
    function activate() {
        const a = F.primaryAction(card.actions, card.compact)
        if (a.input === "text") {
            card.replyAction = a.id
            reply.forceActiveFocus()
        } else {
            card.actionRequested(a.id, "")
        }
    }
    Keys.onReturnPressed: event => { if (card.activeFocus) card.activate(); else event.accepted = false }
    Keys.onEnterPressed: event => { if (card.activeFocus) card.activate(); else event.accepted = false }

    function sendReply() {
        const text = reply.text.trim()
        if (text.length === 0) return
        card.actionRequested(card.replyAction, text)
        reply.text = ""
        card.replyAction = ""
    }

    // Under everything: a click anywhere the controls don't take is `open`.
    MouseArea {
        id: tap
        objectName: card.namePrefix + ".open"
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: card.actionRequested("open", "")
    }

    // ── compact: one row ────────────────────────────────────────────────
    RowLayout {
        id: row
        visible: card.compact
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: Theme.spacing.small
        anchors.rightMargin: Theme.spacing.medium
        spacing: Theme.spacing.small

        ObjectCardIcon { object: card.object; size: 28 }
        LogosText {
            // Escaped, then the matches marked (ObjectHighlight.js, ADR-072).
            textFormat: Text.StyledText
            Layout.fillWidth: true
            text: H.mark(F.str(card.object.title), card.query, Theme.palette.accentOrange)
            elide: Text.ElideRight
            maximumLineCount: 1
            font.pixelSize: Theme.typography.primaryText
        }
        LogosText {
            textFormat: Text.PlainText   // publisher text, never markup (ADR-071)
            visible: text.length > 0
            Layout.maximumWidth: card.width * 0.35
            text: body.item ? body.item.trailing : ""
            elide: Text.ElideRight
            color: Theme.palette.textSecondary
            font.pixelSize: Theme.typography.secondaryText
        }
        LogosText {
            textFormat: Text.PlainText   // publisher text, never markup (ADR-071)
            text: card.timeText
            color: Theme.palette.textTertiary
            font.pixelSize: Theme.typography.secondaryText
        }
        NewDot { visible: card.isNew }
    }

    // ── full card ───────────────────────────────────────────────────────
    ColumnLayout {
        id: column
        visible: !card.compact
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: Theme.spacing.medium
        spacing: Theme.spacing.small

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing.small

            ObjectCardIcon { object: card.object; size: 32 }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                LogosText {
                    // Escaped, then the matches marked (ObjectHighlight.js, ADR-072).
                    textFormat: Text.StyledText
                    objectName: card.namePrefix + ".heading"
                    Layout.fillWidth: true
                    text: H.mark(body.item && body.item.heading.length > 0 ? body.item.heading
                                                                           : F.str(card.object.title),
                                 card.query, Theme.palette.accentOrange)
                    elide: Text.ElideRight
                    maximumLineCount: 1
                    font.pixelSize: Theme.typography.primaryText
                    font.weight: Theme.typography.weightMedium
                }
                LogosText {
                    textFormat: Text.PlainText   // publisher text, never markup (ADR-071)
                    Layout.fillWidth: true
                    text: [F.typeLabel(card.type, false), card.timeText].filter(s => s.length > 0).join(" · ")
                    elide: Text.ElideRight
                    color: Theme.palette.textTertiary
                    font.pixelSize: Theme.typography.secondaryText
                }
            }
            NewDot { visible: card.isNew; Layout.alignment: Qt.AlignTop }
        }

        Loader {
            id: body
            Layout.fillWidth: true
            Layout.preferredHeight: item ? item.implicitHeight : 0
            // Loaded for compact rows too: they show its `trailing` value.
            sourceComponent: {
                switch (card.cardType) {
                case "account": return accountBody
                case "asset": return assetBody
                case "transaction": return transactionBody
                case "message": return messageBody
                case "post": return postBody
                case "community": return communityBody
                case "contact": return contactBody
                case "news": return newsBody
                case "notification": return notificationBody
                }
                return genericBody
            }
        }

        RowLayout {
            visible: card.hasProgress && card.progress < 100
            Layout.fillWidth: true
            spacing: Theme.spacing.small
            LogosProgressBar {
                objectName: card.namePrefix + ".progress"
                Layout.fillWidth: true
                from: 0
                to: 100
                value: card.progress
            }
            LogosText {
                textFormat: Text.PlainText   // publisher text, never markup (ADR-071)
                text: Math.round(card.progress) + "%"
                color: Theme.palette.textSecondary
                font.pixelSize: Theme.typography.secondaryText
            }
        }

        // The object's own actions, then View (`open`: the publisher comes
        // forward), which every card has unless the app offers its own: a
        // target as big as the others, not only "somewhere on the card".
        Flow {
            Layout.fillWidth: true
            spacing: Theme.spacing.small
            Repeater {
                model: card.actions
                delegate: LogosButton {
                    required property var modelData
                    required property int index
                    objectName: card.namePrefix + ".action." + modelData.id
                    text: modelData.label
                    Component.onCompleted: labelItem.textFormat = Text.PlainText
                    variant: index === 0 ? LogosButton.Variant.Primary : LogosButton.Variant.Secondary
                    onClicked: {
                        if (modelData.input === "text") {
                            card.replyAction = card.replyAction === modelData.id ? "" : modelData.id
                            if (card.replyAction.length > 0) reply.forceActiveFocus()
                        } else {
                            card.actionRequested(modelData.id, "")
                        }
                    }
                }
            }
            LogosButton {
                objectName: card.namePrefix + ".action.open"
                visible: !F.offersView(card.actions)
                text: qsTr("View")
                variant: LogosButton.Variant.Secondary
                onClicked: card.actionRequested("open", "")
            }
        }

        RowLayout {
            visible: card.replyAction.length > 0
            Layout.fillWidth: true
            spacing: Theme.spacing.small
            LogosTextField {
                id: reply
                objectName: card.namePrefix + ".reply"
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(Theme.minTouchTarget, implicitHeight)
                placeholderText: qsTr("Write a reply…")
                textInput.maximumLength: 1000
                Keys.onReturnPressed: card.sendReply()
                Keys.onEnterPressed: card.sendReply()
                Keys.onEscapePressed: card.replyAction = ""
            }
            LogosButton {
                objectName: card.namePrefix + ".reply.send"
                text: qsTr("Send")
                variant: LogosButton.Variant.Primary
                enabled: reply.text.trim().length > 0
                onClicked: card.sendReply()
            }
        }
    }

    // ── the bodies, one per type ────────────────────────────────────────
    Component { id: accountBody;      ObjectCardAccount      { object: card.object; fields: card.fields; query: card.query } }
    Component { id: assetBody;        ObjectCardAsset        { object: card.object; fields: card.fields; query: card.query } }
    Component { id: transactionBody;  ObjectCardTransaction  { object: card.object; fields: card.fields; query: card.query } }
    Component { id: messageBody;      ObjectCardMessage      { object: card.object; fields: card.fields; query: card.query } }
    Component { id: postBody;         ObjectCardPost         { object: card.object; fields: card.fields; query: card.query } }
    Component { id: communityBody;    ObjectCardCommunity    { object: card.object; fields: card.fields; query: card.query } }
    Component { id: contactBody;      ObjectCardContact      { object: card.object; fields: card.fields; query: card.query } }
    Component { id: newsBody;         ObjectCardNews         { object: card.object; fields: card.fields; query: card.query } }
    Component { id: notificationBody; ObjectCardNotification { object: card.object; fields: card.fields; query: card.query } }
    Component { id: genericBody;      ObjectCardGeneric      { object: card.object; fields: card.fields; query: card.query } }

    component NewDot: Rectangle {
        objectName: card.namePrefix + ".new"
        width: 8
        height: 8
        radius: 4
        color: Theme.palette.accentOrange
        Accessible.name: qsTr("New")
    }
}
