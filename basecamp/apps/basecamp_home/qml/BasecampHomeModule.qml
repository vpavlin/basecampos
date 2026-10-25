import QtQuick
import QtQuick.Controls

import Logos.Controls
import Logos.Icons
import Logos.Theme
import Basecamp.Objects

import "BasecampHomeModules.js" as M

// One home module (SHELL-OBJECTS-SPEC §7): a saved filter and the objects it
// finds, drawn as one-line rows (small), cards (medium, large) or a sideways
// strip of cards (row). Its own live results from `shell.search`, narrowed by
// the bar's text and the pill; hidden while it finds nothing.
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

    signal changeRequested(var change)
    signal removeRequested()
    // The envelopes it now shows (NewTabPage notes the unseen and marks them).
    signal rowsShown(var rows)
    signal raiseRequested(string id, string action, string text)
    // Its results are about to go (it stopped being live): what home noted
    // from them must be marked first, while they still hold it.
    signal releasing()

    readonly property var filter: M.effectiveFilter(mod.module.filter, mod.pill)
    readonly property string query: M.joinQuery(mod.module.query, mod.barQuery)
    readonly property string size: mod.module.size || "medium"
    property var results: null
    property var rows: []
    property int total: 0
    // id -> place, so rows keep their order for the whole visit
    // (M.stableOrder); reset by a new visit, query or filter.
    property var order: null
    readonly property int newCount: M.newCount(mod.rows, mod.newIds)

    objectName: "home.module." + mod.module.name
    visible: mod.filter !== null && mod.rows.length > 0
    implicitHeight: visible ? content.implicitHeight + 2 * Theme.spacing.medium : 0
    radius: Theme.spacing.radiusXlarge
    color: Theme.palette.backgroundElevated
    border.width: 1
    border.color: Theme.palette.borderHairline

    // The cards it draws, as the lines they are on, top to bottom: home's
    // arrow keys step through them (basecamp-experiments#48). One card per
    // line in a small or medium module; two side by side in a large one with
    // two columns; a strip is one line of the cards it has made.
    function navGroups() {
        const out = []
        const live = it => it && it.visible
        if (!mod.visible) return out
        if (mod.size === "small") {
            for (let i = 0; i < smallRepeater.count; ++i)
                if (live(smallRepeater.itemAt(i))) out.push([smallRepeater.itemAt(i)])
        } else if (cards.visible) {
            let line = []
            for (let i = 0; i < cardRepeater.count; ++i) {
                const it = cardRepeater.itemAt(i)
                if (!live(it)) continue
                line.push(it)
                if (line.length === cards.columns) { out.push(line); line = [] }
            }
            if (line.length > 0) out.push(line)
        } else if (strip.visible) {
            const line = []
            for (let i = 0; i < strip.count; ++i)
                if (live(strip.itemAtIndex(i))) line.push(strip.itemAtIndex(i))
            if (line.length > 0) out.push(line)
        }
        return out
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
        mod.rows = st.ids.slice(0, M.shownCount(mod.size)).map(id => byId[id])
        mod.rowsShown(mod.rows)
    }

    onQueryChanged: refresh()
    onFilterChanged: refresh()
    onLiveChanged: refresh()
    onSizeChanged: rebuild()
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

        // Name, how many, what is new, and the module's menu.
        Item {
            width: parent.width
            height: Theme.minTouchTarget

            LogosText {
                id: title
                textFormat: Text.PlainText
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                width: Math.min(implicitWidth, parent.width - menuButton.width - newBadge.width
                                - countText.width - 3 * Theme.spacing.small)
                text: mod.module.name + (mod.module.pinned ? "  ·  " + qsTr("Pinned") : "")
                elide: Text.ElideRight
                font.pixelSize: Theme.typography.subtitleText
                font.weight: Theme.typography.weightMedium
            }
            LogosText {
                id: countText
                anchors.left: title.right
                anchors.leftMargin: Theme.spacing.small
                anchors.verticalCenter: parent.verticalCenter
                text: mod.total > mod.rows.length ? qsTr("%1 of %2").arg(mod.rows.length).arg(mod.total)
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
                // Only modules that notify count what is new.
                visible: mod.module.notify === true && mod.newCount > 0
                width: visible ? implicitWidth : 0
                text: qsTr("%1 new").arg(mod.newCount)
                color: Theme.palette.accentOrange
            }
            LogosIconButton {
                id: menuButton
                objectName: mod.objectName + ".menu"
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                size: Theme.minTouchTarget
                flat: true
                iconSource: LogosIcons.more
                Accessible.name: qsTr("%1 options").arg(mod.module.name)
                onClicked: menu.popupUnder(menuButton)
            }
        }

        // small: one-line rows.
        Column {
            visible: mod.size === "small"
            width: parent.width
            spacing: Theme.spacing.tiny
            Repeater {
                id: smallRepeater
                model: mod.size === "small" ? mod.rows : []
                delegate: ObjectCard {
                    required property var modelData
                    width: parent.width
                    object: modelData
                    now: mod.now
                    compact: true
                    isNew: mod.newIds[modelData.id] === true
                    query: mod.query
                    namePrefix: "home.card." + modelData.key
                    onActionRequested: (action, text) => mod.raiseRequested(modelData.id, action, text)
                }
            }
        }

        // medium, large: cards, two side by side in a large module that has
        // the room.
        Flow {
            id: cards
            visible: mod.size === "medium" || mod.size === "large"
            readonly property int columns: mod.size === "large" && width >= 600 ? 2 : 1
            width: parent.width
            spacing: Theme.spacing.small
            Repeater {
                id: cardRepeater
                model: cards.visible ? mod.rows : []
                delegate: ObjectCard {
                    required property var modelData
                    width: (cards.width - (cards.columns - 1) * cards.spacing) / cards.columns
                    object: modelData
                    now: mod.now
                    isNew: mod.newIds[modelData.id] === true
                    query: mod.query
                    namePrefix: "home.card." + modelData.key
                    onActionRequested: (action, text) => mod.raiseRequested(modelData.id, action, text)
                }
            }
        }

        // row: a strip of cards that scrolls sideways.
        ListView {
            id: strip
            visible: mod.size === "row"
            width: parent.width
            height: visible ? stripHeight : 0
            readonly property real cardWidth: Math.min(300, width - Theme.spacing.xlarge)
            property real stripHeight: 160
            orientation: ListView.Horizontal
            spacing: Theme.spacing.small
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            // Left and Right go through home's keys (navKey), which move the focus.
            keyNavigationEnabled: false
            model: visible ? mod.rows : []
            delegate: ObjectCard {
                required property var modelData
                required property int index
                width: strip.cardWidth
                object: modelData
                now: mod.now
                isNew: mod.newIds[modelData.id] === true
                query: mod.query
                namePrefix: "home.card." + modelData.key
                onActionRequested: (action, text) => mod.raiseRequested(modelData.id, action, text)
                // Focused from the keyboard: scrolled into the strip.
                onActiveFocusChanged: if (activeFocus) strip.positionViewAtIndex(index, ListView.Contain)
                // The strip is as tall as its tallest card.
                onImplicitHeightChanged: if (implicitHeight > strip.stripHeight) strip.stripHeight = implicitHeight
                Component.onCompleted: if (implicitHeight > strip.stripHeight) strip.stripHeight = implicitHeight
            }
        }
    }

    LogosMenu {
        id: menu
        LogosMenuItem {
            objectName: mod.objectName + ".menu.size"
            text: qsTr("Size: %1 → %2").arg(sizeName(mod.size)).arg(sizeName(M.nextSize(mod.size)))
            onTriggered: mod.changeRequested({ size: M.nextSize(mod.size) })
        }
        LogosMenuItem {
            objectName: mod.objectName + ".menu.pin"
            text: mod.module.pinned ? qsTr("Unpin") : qsTr("Pin to the top")
            onTriggered: mod.changeRequested({ pinned: !mod.module.pinned })
        }
        LogosMenuItem {
            objectName: mod.objectName + ".menu.notify"
            text: mod.module.notify ? qsTr("Stop counting new") : qsTr("Count new")
            onTriggered: mod.changeRequested({ notify: !mod.module.notify })
        }
        LogosMenuItem {
            objectName: mod.objectName + ".menu.remove"
            text: qsTr("Remove")
            onTriggered: mod.removeRequested()
        }
    }

    function sizeName(size) {
        switch (size) {
        case "small": return qsTr("Small")
        case "medium": return qsTr("Medium")
        case "large": return qsTr("Large")
        case "row": return qsTr("Row")
        }
        return size
    }
}
