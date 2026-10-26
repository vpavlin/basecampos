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
    // The bar's text: the cards mark what it matches (basecamp-experiments#46).
    property string query: ""

    signal raiseRequested(string id, string action, string text)

    // The cards drawn, top to bottom: home's arrow keys step through them
    // (basecamp-experiments#48).
    function navCards() {
        const out = []
        for (let i = 0; i < cardRepeater.count; ++i) {
            const it = cardRepeater.itemAt(i)
            if (it) out.push(it)
        }
        return out
    }

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

    // The cards are kept, not rebuilt (basecamp-experiments#27, broad
    // queries): `cards` follows rows.slice(0, shown) by id, so a card whose
    // object stays keeps its item (and only redraws when its envelope
    // changed), one that moved is moved, and only new ones are made. Making
    // an ObjectCard is the expensive part (a few ms each in software
    // rendering), so at most cardsPerPass are made at once and the rest in
    // the next passes, a frame apart: a query that changes all 30 cards no
    // longer blocks the window while 30 are made.
    readonly property int cardsPerPass: 6
    ListModel { id: cards }
    onRowsChanged: feed.sync()
    onShownChanged: feed.sync()
    Component.onCompleted: feed.sync()
    Timer {
        id: nextPass
        interval: 16
        onTriggered: feed.sync()
    }
    // Whether the cards drawn are all of rows.slice(0, shown) yet.
    property bool settled: true
    function sync() {
        const want = feed.rows.slice(0, feed.shown)
        const wanted = {}
        for (let w = 0; w < want.length; ++w) wanted[want[w].id] = true
        let made = 0
        let i = 0
        for (; i < want.length; ++i) {
            const o = want[i]
            const json = JSON.stringify(o)
            let j = i
            while (j < cards.count && cards.get(j).rid !== o.id) ++j
            if (j < cards.count) {
                if (j !== i) cards.move(j, i, 1)
                if (cards.get(i).json !== json) cards.setProperty(i, "json", json)
                continue
            }
            if (made >= feed.cardsPerPass) break
            cards.insert(i, { rid: o.id, json: json })
            ++made
        }
        // Cards no longer wanted go now; ones still wanted further down wait
        // for the next pass, which moves them into place.
        for (let k = cards.count - 1; k >= i; --k)
            if (i >= want.length || wanted[cards.get(k).rid] !== true) cards.remove(k, 1)
        feed.settled = i >= want.length
        if (feed.settled) nextPass.stop()
        else nextPass.restart()
    }

    Repeater {
        id: cardRepeater
        model: cards
        delegate: ObjectCard {
            required property string rid
            required property string json
            width: feed.width
            object: JSON.parse(json)
            now: feed.now
            isNew: feed.newIds[rid] === true
            query: feed.query
            namePrefix: "home.card." + object.key
            onActionRequested: (action, text) => feed.raiseRequested(rid, action, text)
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
