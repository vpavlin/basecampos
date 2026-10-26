import QtQuick
import QtQuick.Controls

import Logos.Controls
import Logos.Icons
import Logos.Theme
import Basecamp.Objects

import "BasecampHomeModules.js" as M

// One home module (SHELL-OBJECTS-SPEC §7): a saved filter and the objects it
// finds. Its own live results from `shell.search`, narrowed by the bar's
// text and the pill; hidden while it finds nothing, except in edit mode.
// Home's grid gives it its place and size (its block of cells), and what it
// draws fits the block (basecamp-experiments#53): as many items as fit
// whole under the header, in 1 to 3 columns by its width, one-line rows when
// it is two rows tall, a one-line summary when not even one fits.
// A long press on its header asks for edit mode (basecamp-experiments#51);
// in edit mode its cards are inert and home's grid moves it and holds its
// controls (#54).
//
// (File names here start with BasecampHome: two ui_qml apps shipping a file
// of the same name collide, spec §11.)
Rectangle {
    id: mod

    // A module as BasecampHomeModules.js keeps it.
    property var module: ({})
    property string barQuery: ""
    property var pill: null
    property real now: Date.now()
    // id -> true: what this visit shows as new (NewTabPage owns it).
    property var newIds: ({})
    // Bumped at each visit: the rows may take their new order then.
    property int visit: 0
    // Whether the modules show (home's grid view). Off, it holds no results:
    // each one ranks the whole store on every keystroke and every store
    // change, for nothing in the feed view (basecamp-experiments#27).
    property bool live: false
    // Home's edit mode (#51): shown even when empty, its cards inert.
    property bool editing: false
    // Its block's height in rows (0 = not on the grid yet): two rows draw
    // one-line rows.
    property int cellRows: 0
    // Whether it counts what is new (edit mode's switch may say otherwise
    // than the saved module until Done).
    property bool notify: mod.module.notify === true
    // Room the header leaves on its right (edit mode's controls sit there).
    property real headerEnd: 0

    // The envelopes it now shows (NewTabPage notes the unseen and marks them).
    signal rowsShown(var rows)
    signal raiseRequested(string id, string action, string text)
    // Its results are about to go (it stopped being live): what home noted
    // from them must be marked first, while they still hold it.
    signal releasing()
    // A long press on the header: home's edit mode.
    signal editRequested()

    readonly property var filter: M.effectiveFilter(mod.module.filter, mod.pill)
    readonly property string query: M.joinQuery(mod.module.query, mod.barQuery)
    property var results: null
    // The items it built (as many as could fit, M.candidateCount) and how
    // many objects it found in all.
    property var rows: []
    property int total: 0
    // id -> place, so rows keep their order for the whole visit
    // (M.stableOrder); reset by a new visit, query or filter.
    property var order: null
    readonly property int newCount: M.newCount(mod.shownRows, mod.newIds)

    // ── fitting the block (#53) ─────────────────────────────────────────
    readonly property bool compact: M.compactRows(mod.cellRows)
    readonly property int itemSpacing: Theme.spacing.small
    readonly property int columns: M.itemColumns(content.width, mod.itemSpacing)
    // The px under the header for items.
    readonly property real available: mod.height - 2 * Theme.spacing.medium - header.height - content.spacing
    readonly property int candidates: M.candidateCount(mod.available, mod.columns, mod.itemSpacing)
    // The items' heights as drawn at this width (measure()).
    property var itemHeights: []
    // How many show: the ones that fit whole.
    readonly property int shown: Math.min(mod.rows.length,
                                          M.fitCount(mod.itemHeights, mod.columns, mod.available, mod.itemSpacing))
    readonly property var shownRows: mod.rows.slice(0, mod.shown)
    // Not even one fits (or none measured yet with none built): a line
    // that says what there is.
    readonly property bool summarised: mod.hasContent && mod.shown === 0
                                       && (mod.rows.length === 0 || mod.itemHeights.length > 0)

    objectName: "home.module." + mod.module.name
    // Something to show; home's grid takes the cells of one without
    // (basecamp-experiments#50).
    readonly property bool hasContent: mod.filter !== null && mod.total > 0
    visible: mod.hasContent || mod.editing
    // Home's grid sizes it by its block; what does not fit is not drawn.
    clip: true
    radius: Theme.spacing.radiusXlarge
    color: Theme.palette.backgroundElevated
    border.width: 1
    border.color: Theme.palette.borderHairline

    // The cards it shows, as the lines they are on, top to bottom: home's
    // arrow keys step through them (basecamp-experiments#48). `columns` to a
    // line.
    function navGroups() {
        const out = []
        if (!mod.visible) return out
        let line = []
        for (let i = 0; i < itemRepeater.count; ++i) {
            const it = itemRepeater.itemAt(i)
            if (!it || !it.visible) continue
            line.push(it)
            if (line.length === mod.columns) { out.push(line); line = [] }
        }
        if (line.length > 0) out.push(line)
        return out
    }

    // The heights of the items built, at their current width; then what
    // shows is reported (home notes the unseen among them).
    function measure() {
        const out = []
        for (let i = 0; i < itemRepeater.count; ++i) {
            const it = itemRepeater.itemAt(i)
            out.push(it ? it.implicitHeight : 0)
        }
        if (JSON.stringify(out) !== JSON.stringify(mod.itemHeights)) mod.itemHeights = out
        mod.rowsShown(mod.shownRows)
    }

    function refresh() {
        mod.order = null
        if (!mod.live) {
            if (mod.results) {
                mod.releasing()
                mod.results.destroy()
                mod.results = null
            }
            mod.rows = []
            mod.total = 0
            return
        }
        if (mod.filter === null) {
            mod.rows = []
            mod.total = 0
            return
        }
        if (mod.results) mod.results.update(mod.query, mod.filter)
        else mod.results = shell.search(mod.query, mod.filter)
        mod.rebuild()
    }

    function rebuild() {
        if (!mod.results) return
        const byId = {}
        for (let i = 0; i < mod.results.count; ++i) {
            const e = mod.results.get(i)
            byId[e.id] = e
        }
        const st = M.stableOrder(mod.order, mod.results.ids.filter(id => byId[id] !== undefined))
        mod.order = st.map
        mod.total = st.ids.length
        mod.rows = st.ids.slice(0, mod.candidates).map(id => byId[id])
        // What shows is known once they are measured.
        Qt.callLater(mod.measure)
    }

    onQueryChanged: refresh()
    onFilterChanged: refresh()
    onLiveChanged: refresh()
    // A taller or wider block builds more (or fewer) of them.
    onCandidatesChanged: rebuild()
    onShownChanged: mod.rowsShown(mod.shownRows)
    // Items wrap differently at another width or in the other form.
    onColumnsChanged: Qt.callLater(mod.measure)
    onCompactChanged: Qt.callLater(mod.measure)
    onWidthChanged: Qt.callLater(mod.measure)
    onVisitChanged: { mod.order = null; rebuild() }
    Component.onCompleted: refresh()
    // A module edit rebuilds every delegate: free this one's results now
    // (JS-owned, and the GC is lazy), or they keep following the store and
    // count against home's live results (ObjectSearch::kMaxLivePerView).
    Component.onDestruction: if (mod.results) mod.results.destroy()

    Connections {
        target: mod.results
        function onResultsChanged() { mod.rebuild() }
    }

    Column {
        id: content
        x: Theme.spacing.medium
        y: Theme.spacing.medium
        width: mod.width - 2 * Theme.spacing.medium
        spacing: Theme.spacing.small

        // Name, how many show of how many, what is new.
        Item {
            id: header
            width: parent.width
            height: Theme.minTouchTarget
            // What the name and the count may take. In edit mode the count
            // gives way when the name would not fit beside it.
            readonly property real room: width - mod.headerEnd - newBadge.width - 3 * Theme.spacing.small
            readonly property bool showCount: !mod.editing
                                              || title.implicitWidth + countText.implicitWidth <= header.room

            // Long press (500 ms) for edit mode. It lets the page take a
            // drag, so the header still scrolls.
            MouseArea {
                objectName: mod.objectName + ".header"
                anchors.fill: parent
                enabled: !mod.editing
                pressAndHoldInterval: 500
                onPressAndHold: mod.editRequested()
            }

            LogosText {
                id: title
                textFormat: Text.PlainText
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                width: Math.max(0, Math.min(implicitWidth, header.room - (header.showCount ? countText.width : 0)))
                text: mod.module.name
                elide: Text.ElideRight
                font.pixelSize: Theme.typography.subtitleText
                font.weight: Theme.typography.weightMedium
            }
            LogosText {
                id: countText
                objectName: mod.objectName + ".count"
                anchors.left: title.right
                anchors.leftMargin: Theme.spacing.small
                anchors.verticalCenter: parent.verticalCenter
                visible: header.showCount
                // "4 of 12": what shows of what it found.
                text: mod.shown > 0 && mod.total > mod.shown ? qsTr("%1 of %2").arg(mod.shown).arg(mod.total)
                                                               : String(mod.total)
                color: Theme.palette.textTertiary
                font.pixelSize: Theme.typography.secondaryText
            }
            LogosBadge {
                id: newBadge
                objectName: mod.objectName + ".new"
                anchors.left: countText.right
                anchors.leftMargin: Theme.spacing.small
                anchors.verticalCenter: parent.verticalCenter
                // Only modules that count new do; edit mode has its switch.
                visible: mod.notify && mod.newCount > 0 && !mod.editing
                width: visible ? implicitWidth : 0
                text: qsTr("%1 new").arg(mod.newCount)
                color: Theme.palette.accentOrange
            }
        }

        // Edit mode keeps an empty module on the grid; it says why it is empty.
        LogosText {
            objectName: mod.objectName + ".empty"
            visible: mod.editing && !mod.hasContent
            width: parent.width
            wrapMode: Text.Wrap
            text: qsTr("Nothing to show here right now.")
            color: Theme.palette.textTertiary
            font.pixelSize: Theme.typography.secondaryText
        }

        // Too small for one item: what there is, in one line.
        LogosText {
            objectName: mod.objectName + ".summary"
            visible: mod.summarised
            width: parent.width
            elide: Text.ElideRight
            text: mod.summaryText()
            color: Theme.palette.textSecondary
            font.pixelSize: Theme.typography.secondaryText
        }

        // The items: one-line rows at two rows tall, else item cards;
        // `columns` side by side. Each is built, measured and shown only if
        // it fits whole (`shown`); Flow leaves out the hidden ones.
        Flow {
            id: items
            visible: mod.shown > 0
            enabled: !mod.editing
            width: parent.width
            spacing: mod.itemSpacing
            // Whole pixels, so `columns` of them always fit on one line.
            readonly property real itemWidth: Math.floor((width - (mod.columns - 1) * spacing) / mod.columns)
            Repeater {
                id: itemRepeater
                model: mod.rows
                onItemAdded: Qt.callLater(mod.measure)
                onItemRemoved: Qt.callLater(mod.measure)
                delegate: ObjectCard {
                    required property var modelData
                    required property int index
                    width: items.itemWidth
                    visible: index < mod.shown
                    object: modelData
                    now: mod.now
                    compact: mod.compact
                    isNew: mod.newIds[modelData.id] === true
                    query: mod.query
                    namePrefix: "home.card." + modelData.key
                    onActionRequested: (action, text) => mod.raiseRequested(modelData.id, action, text)
                    onImplicitHeightChanged: Qt.callLater(mod.measure)
                }
            }
        }
    }

    // "12 messages, 3 new": the type's name when it finds one type, else
    // "items"; what is new when it counts new.
    function summaryText() {
        const types = M.strings((mod.module.filter || {}).types)
        const what = types.length === 1 ? ObjectCardFormat.typeLabel(types[0], true).toLowerCase() : qsTr("items")
        const base = qsTr("%1 %2").arg(mod.total).arg(what)
        let fresh = 0
        for (const e of mod.rows)
            if (e && (e.seen !== true || mod.newIds[e.id] === true)) ++fresh
        return mod.notify && fresh > 0 ? qsTr("%1, %2 new").arg(base).arg(fresh) : base
    }
}
