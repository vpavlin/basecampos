import QtQuick

import Logos.Controls
import Logos.Theme
import Basecamp.Objects

// Home's feed (SHELL-OBJECTS-SPEC §7): every object the bar and the pill
// leave, newest first, one card each, under an "Activity" label. Unseen
// cards carry a dot. Grows with its rows; the page around it scrolls.
Column {
    id: feed

    // Envelopes, newest first (M.feedRows).
    property var rows: []
    property real now: Date.now()
    property var newIds: ({})
    // Cards drawn at once; "Show more" adds a page.
    property int shown: 30
    // How many objects the bar and the pill match in all: a results model
    // stops at 200, so past that the feed says it shows only part.
    property int total: 0

    signal raiseRequested(string id, string action, string text)

    objectName: "home.feed"
    spacing: Theme.spacing.small

    LogosText {
        objectName: "home.feed.title"
        width: parent.width
        text: qsTr("Activity")
        font.pixelSize: Theme.typography.secondaryText
        font.weight: Font.Medium
        color: Theme.palette.textSecondary
    }

    Repeater {
        model: feed.rows.slice(0, feed.shown)
        delegate: ObjectCard {
            required property var modelData
            width: feed.width
            object: modelData
            now: feed.now
            isNew: feed.newIds[modelData.id] === true
            namePrefix: "home.card." + modelData.key
            onActionRequested: (action, text) => feed.raiseRequested(modelData.id, action, text)
        }
    }

    LogosButton {
        objectName: "home.feed.more"
        visible: feed.rows.length > feed.shown
        anchors.horizontalCenter: parent.horizontalCenter
        text: qsTr("Show more (%1)").arg(feed.rows.length - feed.shown)
        onClicked: feed.shown += 30
    }

    LogosText {
        objectName: "home.feed.capped"
        visible: feed.rows.length <= feed.shown && feed.total > feed.rows.length
        width: parent.width
        wrapMode: Text.Wrap
        horizontalAlignment: Text.AlignHCenter
        text: qsTr("Showing %1 of %2. Search or pick a pill to find the rest.")
              .arg(feed.rows.length).arg(feed.total)
        color: Theme.palette.textTertiary
        font.pixelSize: Theme.typography.secondaryText
    }
}
