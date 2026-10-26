import QtQuick
import QtQuick.Layouts

import Logos.Controls
import Logos.Theme

import "BasecampSearchRanker.js" as Ranker

// Basecamp's default `search` role app (SHELL-OBJECTS-SPEC §2, §5; ADR-072).
// It ranks and draws nothing: the shell starts it without a window, views
// (Home, Applications) ask the shell for results, the shell hands the request
// here through `shell.searchRequested`, and the answer goes back as ranked ids.
// Ranking is BasecampSearchRanker.js, the same rules as the host's fallback.
//
// Opened like an app (from Applications) it shows this status page only.
Rectangle {
    id: root
    objectName: "basecampSearch.root"
    color: Theme.palette.background

    readonly property bool granted: typeof shell !== "undefined" && shell !== null
                                    && shell.roles.search === "basecamp_search"

    // This app's copy of the store, brought up to date on each request from
    // what changed since (basecamp-experiments#27): a query no longer copies
    // every object. A shell without objectsSince gets the old path.
    property var index: Ranker.newIndex()
    function rank(query, filter) {
        const changes = typeof shell.objectsSince === "function"
                        ? shell.objectsSince(root.index.version, filter) : null
        if (Ranker.applyChanges(root.index, changes)) {
            const ids = Ranker.rankIndex(root.index, changes.ids || null, query, Date.now())
            if (root.index.waiting.length > 0) Qt.callLater(root.prepareStep)
            return ids
        }
        return Ranker.rank(shell.objects(filter), query, Date.now())
    }
    // New objects' words are split a few hundred at a time while idle, so
    // the refresh after a flood (often an empty bar, which needs no words)
    // does not split 2500 objects' words at once. A query that needs them
    // splits whatever is still waiting first. Qt.callLater, one turn of the
    // event loop per step: this app runs without a window, and a QML Timer
    // here did not fire in the headless test (the words were never ready).
    function prepareStep() {
        if (Ranker.prepareSome(root.index, 250) > 0)
            Qt.callLater(root.prepareStep)
    }
    // Losing the role (or getting it back) starts over.
    onGrantedChanged: root.index = Ranker.newIndex()

    property int answered: 0
    property string lastQuery: ""
    property int lastCount: 0

    Connections {
        target: root.granted ? shell : null
        function onSearchRequested(requestId, query, filter) {
            const started = Date.now()
            const ids = root.rank(query, filter)
            const took = Date.now() - started
            if (took > 30)
                console.info("basecamp_search: ranking took", took, "ms for", root.index.size, "objects")
            if (shell.answer(requestId, ids)) {
                root.answered += 1
                root.lastQuery = query
                root.lastCount = ids.length
            }
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 2 * Theme.spacing.large, 420)
        spacing: Theme.spacing.small

        LogosText {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Search")
            font.pixelSize: 20
            font.weight: Font.DemiBold
        }
        LogosText {
            objectName: "basecampSearch.status"
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            color: Theme.palette.textSecondary
            text: root.granted
                  ? qsTr("Ranking what apps share for Home and Applications. Nothing to see here: results show where you search.")
                  : qsTr("Another app ranks search, or none does. Choose Basecamp's again in Settings under Default apps.")
        }
        LogosText {
            objectName: "basecampSearch.counter"
            visible: root.granted
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            color: Theme.palette.textSecondary
            font.pixelSize: Theme.typography.secondaryText
            text: root.answered === 0
                  ? qsTr("No searches yet.")
                  : qsTr("%n searches answered.", "", root.answered)
                    + " " + qsTr("Last: \"%1\", %2 results.").arg(root.lastQuery).arg(root.lastCount)
        }
    }
}
