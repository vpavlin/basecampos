import QtQuick
import QtQuick.Controls

import Logos.Theme
import Logos.Controls
import Logos.Icons
import Basecamp.Icons
import Basecamp.Objects

import "BasecampHomeModules.js" as M

// Basecamp's home page (the `home` role, ADR-067): what a New tab shows.
// The bar's text searches both apps and what apps share with Basecamp
// (SHELL-OBJECTS-SPEC §7): pills narrow the objects by type or app; below,
// either the feed (everything, newest first; the default) or the modules
// (saved filters, a grid that is one column on a phone). Above them, with
// nothing typed, Recent apps: the ones closed or installed lately, newest
// first, then Applications, Packages and Settings, which are apps like any
// other (basecamp-experiments#30). With a query, the Apps block instead: the
// installed and catalog apps that match, one row each, Running or with an
// Open or Install button (basecamp-experiments#32). Under a type pill, the
// same block lists the installed apps that share that type, whatever is
// typed (basecamp-experiments#36).
// In the desktop's Activity sidebar (feedOnly, basecamp-experiments#44) only
// the search, the pills and the feed show: no Recent apps, Apps block or
// modules.
// Picking a tile asks the shell to turn the current New tab into that app; a
// card's actions go to the app that published it. It reads only `shell`, the
// role-gated object the host injects.
//
// The modules (order, size, pin, notify, the custom ones) live in memory
// only: home has no settings store of its own yet, so a restart brings back
// the defaults.
Rectangle {
    id: root

    signal launchApp(string name)

    function focusSearch() { search.forceActiveFocus() }
    // The shell calls this each time it shows a New tab: a new visit.
    function clearSearch() {
        search.text = ""
        root.pill = null
        root.startVisit()
    }
    // Esc in the search (#47): clears what was typed, the type pill stays.
    // True when there was text to clear; the desktop shell asks this before
    // Esc closes the overlay or the sidebar. Basecampium's compact bar is the
    // shell's own, with its own Esc (Back).
    // With a result focused from the keyboard (#48), Esc first takes the
    // focus back to the field, the text kept.
    function escapeSearch() {
        if (root.searchInBar)
            return false
        if (root.resultFocused()) {
            root.focusSearch()
            return true
        }
        if (search.text.length === 0)
            return false
        search.text = ""
        return true
    }

    // ── keyboard (basecamp-experiments#48) ──────────────────────────────
    // Down from the search moves into the results: Recent apps (Left and
    // Right along the row), then down the Apps block's rows and the cards
    // (the feed's, or the modules' in reading order). Up from the first
    // returns to the search. Enter does what a click on it would (a card:
    // its primary action, ObjectCard.activate). Typing goes back to the
    // search. The results take real focus (screen readers follow it) and
    // draw an accent ring while they hold it; the page scrolls to keep the
    // focused one in view. The phone layout's search is the shell's bar,
    // so none of this applies there.
    function within(item, ancestor) {
        for (let it = item; it; it = it.parent)
            if (it === ancestor) return true
        return false
    }
    function focusedItem() { return root.Window.window ? root.Window.window.activeFocusItem : null }
    function resultFocused() {
        const f = root.focusedItem()
        return f !== null && root.within(f, page) && !root.within(f, search)
    }
    // The lines under Recent apps, top to bottom, each the results side by
    // side on it: an app row, a card, or a module's cards in a strip or two
    // columns (Left and Right go along those, M.navMove).
    function navGroups() {
        const out = []
        if (appsBlock.visible)
            for (let i = 0; i < appHitRepeater.count; ++i) {
                const it = appHitRepeater.itemAt(i)
                if (it) out.push([it])
            }
        if (feed.visible)
            return out.concat(feed.navCards().map(c => [c]))
        if (grid.visible) {
            const mods = []
            for (let i = 0; i < moduleRepeater.count; ++i) {
                const m = moduleRepeater.itemAt(i)
                if (m && m.visible) mods.push(m)
            }
            mods.sort((a, b) => a.y !== b.y ? a.y - b.y : a.x - b.x)
            for (const m of mods) out.push(...m.navGroups())
        }
        return out
    }
    // The same flattened: `rows` top to bottom and left to right, `lines`
    // the line each is on.
    function navLayout() {
        const rows = [], lines = []
        const groups = root.navGroups()
        for (let g = 0; g < groups.length; ++g)
            for (const it of groups[g]) { rows.push(it); lines.push(g) }
        return { rows: rows, lines: lines }
    }
    function navRows() { return root.navLayout().rows }
    // Which result `f` is in: { where: "tile" | "row", index, stop, key }
    // (key: the card's object or the app row's app, to find it again), or
    // null. `rows` is navRows().
    function locate(f, rows) {
        if (!f) return null
        if (appRow.visible)
            for (let i = 0; i < appRow.count; ++i) {
                const t = appRow.itemAtIndex(i)
                if (t && root.within(f, t)) return { where: "tile", index: i, stop: t, key: "" }
            }
        for (let i = 0; i < rows.length; ++i)
            if (root.within(f, rows[i])) {
                const r = rows[i]
                const key = r.object && r.object.id ? "object:" + r.object.id
                          : r.hit && r.hit.name ? "app:" + r.hit.name : ""
                return { where: "row", index: i, stop: r, key: key }
            }
        return null
    }
    // The result last focused from the keyboard ({ where, index, key }) and
    // its item. The feed and the Apps block are drawn again whenever their
    // results change (a pending payment moves every few seconds), which
    // drops the focused card; the focus then goes back to the same object,
    // or the same place. Only then: when the focus leaves for somewhere else
    // (the window lost it, a click in the dock or another app) while the
    // result is still there, nothing is taken back.
    property var navAt: null
    property Item navItem: null
    function restoreNav() {
        const at = root.navAt
        const f = root.focusedItem()
        if (!at || (f && root.within(f, page))) return
        // The result is still on the page: the focus left for somewhere else
        // (the window lost it, a click in the dock or another app), so it
        // stays there and the ring goes out (#48 review). A redrawn result
        // is out of the page by now (unparented, or destroyed: null).
        const old = root.navItem
        if (old && old.visible && root.within(old, page)) {
            root.navAt = null
            root.navItem = null
            return
        }
        if (at.where === "tile") {
            if (appRow.visible && appRow.count > 0) root.focusTile(Math.min(at.index, appRow.count - 1))
            else root.focusSearch()
            return
        }
        const rows = root.navRows()
        if (rows.length === 0) { root.focusSearch(); return }
        let target = rows[Math.min(at.index, rows.length - 1)]
        for (const r of rows)
            if (at.key.length > 0 && root.locate(r, [r]).key === at.key) { target = r; break }
        target.forceActiveFocus(Qt.TabFocusReason)
    }
    function noteFocus() {
        const f = root.focusedItem()
        const at = root.locate(f, root.navRows())
        if (at) {
            root.navAt = { where: at.where, index: at.index, key: at.key }
            root.navItem = at.stop
        } else if (root.navAt && (!f || !root.within(f, page))) {
            // Its result may have been drawn again: once that is done,
            // restoreNav brings the focus back to the new one.
            Qt.callLater(root.restoreNav)
            return
        } else {
            root.navAt = null
            root.navItem = null
        }
        Qt.callLater(root.revealFocus)
    }
    function focusTile(i) {
        if (appRow.count === 0) return
        appRow.currentIndex = i
        appRow.positionViewAtIndex(i, ListView.Contain)
        const t = appRow.itemAtIndex(i)
        if (t) t.forceActiveFocus(Qt.TabFocusReason)
    }
    function navKey(event) {
        if (root.searchInBar) return
        const f = root.focusedItem()
        if (!f || !root.within(f, root)) return
        const inSearch = root.within(f, search)
        if (!inSearch && root.within(f, page) && M.typesText(event.text, event.modifiers)) {
            root.focusSearch()
            search.textInput.cursorPosition = search.text.length
            search.textInput.insert(search.textInput.cursorPosition, event.text)
            event.accepted = true
            return
        }
        if (!inSearch && event.key === Qt.Key_Backspace && root.within(f, page)) {
            root.focusSearch()
            search.textInput.cursorPosition = search.text.length
            if (search.text.length > 0)
                search.textInput.remove(search.text.length - 1, search.text.length)
            event.accepted = true
            return
        }
        if (!inSearch && event.key === Qt.Key_Escape) {
            event.accepted = root.escapeSearch()
            return
        }
        const key = { [Qt.Key_Up]: "up", [Qt.Key_Down]: "down",
                      [Qt.Key_Left]: "left", [Qt.Key_Right]: "right" }[event.key]
        const isEnter = event.key === Qt.Key_Return || event.key === Qt.Key_Enter
        if (!key && !isEnter) return

        const tiles = appRow.visible ? appRow.count : 0
        const nav = root.navLayout()
        const rows = nav.rows
        const from = inSearch ? { where: "search" } : root.locate(f, rows)
        if (!from) return
        const stop = from.stop
        if (isEnter) {
            // A card takes its own Enter; a tile or an app row opens.
            if (stop && typeof stop.activate === "function" && stop.activeFocus) {
                stop.activate()
                event.accepted = true
            }
            return
        }
        const to = M.navMove(from, key, tiles, rows.length, appRow.currentIndex, nav.lines)
        if (!to) return
        event.accepted = true
        if (to.where === "search") root.focusSearch()
        else if (to.where === "tile") root.focusTile(to.index)
        else rows[to.index].forceActiveFocus(Qt.TabFocusReason)
    }
    Keys.onPressed: event => root.navKey(event)

    // Keeps what has the focus in view, the search included.
    function revealFocus() {
        const f = root.focusedItem()
        if (!f || !root.within(f, page)) return
        // Back in the search: the page's top, as it was before.
        if (root.within(f, search)) {
            scroll.contentY = 0
            return
        }
        const item = f
        const p = item.mapToItem(scroll.contentItem, 0, 0)
        const pad = Theme.spacing.medium
        let y = scroll.contentY
        if (p.y + item.height + pad > y + scroll.height) y = p.y + item.height + pad - scroll.height
        if (p.y - pad < y) y = p.y - pad
        scroll.contentY = Math.max(0, Math.min(y, scroll.contentHeight - scroll.height))
    }
    Connections {
        target: root.Window.window
        function onActiveFocusItemChanged() { root.noteFocus() }
    }

    // Basecampium's compact layout puts the search in its one bar
    // (docs/MOBILE-LAYOUT.md §8): this page then shows the results only.
    property bool searchInBar: false
    property string barQuery: ""
    // The desktop's Activity sidebar: the search, the pills and the feed only.
    property bool feedOnly: false
    // What is typed, and what the results are for: the same text a moment
    // after the last keystroke (basecamp-experiments#27, the flood test:
    // each search ranks the whole store, so typing fast asked for one per
    // letter). Emptied at once (Esc, a new visit); Enter applies it first.
    readonly property string typedQuery: (searchInBar ? barQuery : search.text).trim()
    property string rawQuery: ""
    readonly property string query: rawQuery.toLowerCase()
    onTypedQueryChanged: {
        if (root.typedQuery.length === 0) root.applyQuery()
        else queryTimer.restart()
    }
    function applyQuery() {
        queryTimer.stop()
        root.rawQuery = root.typedQuery
    }
    Timer {
        id: queryTimer
        interval: 100
        onTriggered: root.applyQuery()
    }
    // Phone width (Basecampium's compact layout): tighter margins and tiles.
    readonly property bool compact: width < 600
    readonly property int sideMargin: compact ? Theme.spacing.medium : Theme.spacing.xlarge * 2
    readonly property int contentWidth: Math.min(width - 2 * sideMargin, 1200)
    // Tiles the size of the Applications grid's (80), so an app looks the same in both.
    readonly property int tileSize: 80
    // Art plus a label of up to two lines ("Package Manager" at 390).
    readonly property int tileHeight: tileSize + 56
    readonly property int pitch: compact ? 108 : 124

    // Every installed app you can open, by name. Applications, Packages and
    // Settings are among them; home itself and the search ranker are not.
    readonly property var hidden: {
        const roles = shell.roles || ({})
        return [roles.home || "", roles.search || ""].filter(n => n.length > 0)
    }
    readonly property var apps: {
        const list = (shell.installedApps || []).filter(a => root.hidden.indexOf(a.name) < 0)
        list.sort((a, b) => (a.displayName || a.name).localeCompare(b.displayName || b.name))
        return list
    }
    // Recent apps: closed or installed lately, newest first (the host keeps
    // both), then whichever of Applications, Packages and Settings is not
    // already there. A fresh profile shows just those three.
    readonly property int maxRecent: 12
    readonly property var recentApps: {
        const out = []
        const add = a => {
            if (a && root.hidden.indexOf(a.name) < 0 && !out.some(o => o.name === a.name))
                out.push(a)
        }
        for (const a of (shell.recentApps || [])) add(a)
        const roles = shell.roles || ({})
        for (const role of ["launcher", "packages", "settings"])
            add((shell.installedApps || []).find(a => a.name === (roles[role] || "")))
        return out.slice(0, root.maxRecent)
    }
    readonly property var appNames: {
        const names = {}
        for (const a of (shell.installedApps || [])) names[a.name] = a.displayName || a.name
        return names
    }

    // The Apps block: what shell.findApps answers for the bar's text, best
    // first ({ name, title, icon, fullBleed, state }). Asked again when the
    // text changes and when the host says the answer would differ (an app
    // opened, closed or installed, the catalog arrived).
    property var appHits: []
    readonly property int maxAppHits: 5
    function refreshApps() {
        root.appHits = root.rawQuery.length > 0 ? (shell.findApps(root.rawQuery) || []) : []
    }
    // Under a type pill: shell.relatedApps, the installed apps that publish
    // that type, in the same row shape. Asked again when the pill changes and
    // when the host says the store or the apps changed; not with the pill's
    // results, which follow the text and miss objects it does not match.
    property var relatedHits: []
    function refreshRelated() {
        root.relatedHits = root.pill && root.pill.type ? (shell.relatedApps(root.pill.type) || []) : []
    }
    // What the Apps block lists: the related apps under a type pill, the
    // apps found for the text without a pill, nothing under an app pill.
    readonly property var shownAppHits: !root.pill ? root.appHits
                                      : root.pill.type ? root.relatedHits : []
    Connections {
        target: shell
        function onAppResultsChanged() { root.refreshApps(); root.refreshRelated() }
        function onRelatedAppsChanged() { root.refreshRelated() }
    }

    function openApp(hit) {
        if (hit.state === "available") shell.installApp(hit.name)
        else if (hit.state !== "installing") root.launchApp(hit.name)
    }
    // Enter: the first app found that is installed (never an install).
    function openFirst() {
        if (root.feedOnly) return
        if (queryTimer.running) root.applyQuery()
        const hit = root.appHits.find(a => a.state === "running" || a.state === "installed")
        if (root.query.length > 0 && hit) root.launchApp(hit.name)
    }

    // ── objects ─────────────────────────────────────────────────────────

    // { type, app } narrowing every module and the feed; null = all.
    property var pill: null
    // "feed" (the default) or "grid" (modules), as the toggle left it;
    // the sidebar's feed mode always shows the feed.
    property string chosenView: "feed"
    readonly property string view: root.feedOnly ? "feed" : root.chosenView
    property var modules: M.defaultModules().map(M.sanitizeModule)
    readonly property var shownModules: M.ordered(root.modules)

    // One live result for whether anything matches: the bar's text, no filter.
    property var allResults: null
    property var allRows: []
    // The feed's own, narrowed by the pill: a results model holds at most 200
    // objects, so the feed under a pill must not be a slice of everything's.
    // Made when a pill is first picked; with none, the feed is allResults.
    property var pillResults: null
    property var pillRows: []
    // What the pills count: every object the bar's text matches, uncapped
    // (shell.counts), read again whenever the results change.
    property var counts: ({})
    readonly property var pillSet: ({ types: root.counts.types || [], apps: root.counts.apps || [] })
    readonly property var feedRows: M.feedRows(root.pill ? root.pillRows : root.allRows, root.pill)
    // How many the feed would show uncapped (the pill's count, or all).
    readonly property int feedTotal: {
        if (!root.pill) return root.counts.total || 0
        const list = root.pill.type ? root.pillSet.types : root.pillSet.apps
        const hit = list.find(p => (root.pill.type ? p.type === root.pill.type : p.app === root.pill.app))
        return hit ? hit.count : 0
    }

    // What this visit shows as new (id -> true), and the ids noted since the
    // last markSeen. A visit starts when the shell shows a New tab.
    property var newIds: ({})
    property var toMark: []
    property int visit: 0
    property real now: Date.now()
    // On screen: the shell hides home's view behind other tabs, which hides
    // its window. Nothing is marked seen while it is hidden.
    readonly property bool onScreen: Window.window !== null && Window.window.visibility !== Window.Hidden
    // Deferred: the shell shows the view before it starts the visit
    // (clearSearch), and marking first would make the hidden visit "seen".
    onOnScreenChanged: if (root.onScreen) Qt.callLater(root.noteFeedShown)
    function noteFeedShown() {
        if (root.view === "feed") root.noteShown(root.feedRows.slice(0, feed.shown))
    }

    function startVisit() {
        root.flushMarks()
        root.now = Date.now()
        root.newIds = ({})
        root.visit += 1
        if (root.view === "feed") root.noteShown(root.feedRows.slice(0, feed.shown))
    }

    // `rows` are on screen: the unseen among them are new for this visit and
    // are marked seen shortly (they keep their dot until the next visit).
    // Hidden, they are left alone, so the first look still counts them new.
    function noteShown(rows) {
        if (!root.onScreen) return
        const r = M.addUnseen(root.newIds, rows)
        if (r.added.length === 0) return
        root.newIds = r.map
        root.toMark = root.toMark.concat(r.added)
        markTimer.restart()
    }

    function raise(id, action, text) {
        if (shell.raise(id, action, text || "") === "")
            console.warn("Home: the shell refused", action, "on", id)
    }

    function pillLabel(p) {
        if (!p) return ""
        if (p.type) return ObjectCardFormat.typeLabel(p.type, true)
        return root.appNames[p.app] || p.app
    }

    // What the bar reads (§7): what it searches. The shell's bar on a phone
    // (shell.setBarPlaceholder), this page's own on a desktop.
    readonly property string barPlaceholder: !root.pill ? qsTr("Search Basecamp…")
        : root.pill.type ? qsTr("Search %1…").arg(root.pillLabel(root.pill).toLowerCase())
                         : qsTr("Search %1…").arg(root.pillLabel(root.pill))
    onBarPlaceholderChanged: shell.setBarPlaceholder(root.barPlaceholder)

    function setPill(p) {
        const same = root.pill && p && root.pill.type === p.type && root.pill.app === p.app
        root.pill = same || !p ? null : p
    }

    // The modules with the bar's text and the pill added as one, or null when
    // home would refuse it (nothing to save, a duplicate, no room).
    readonly property string customName: M.customName(root.rawQuery, root.pillLabel(root.pill))
    readonly property var withCustom: M.addModule(root.modules, root.customName, root.rawQuery, root.pill)
    function addModule() {
        if (root.withCustom) root.modules = root.withCustom
    }

    function refreshAll() {
        if (root.allResults) root.allResults.update(root.rawQuery, {})
        else root.allResults = shell.search(root.rawQuery, {})
        rebuildAll()
        refreshPill()
    }
    function rebuildAll() {
        root.allRows = root.rowsOf(root.allResults)
        root.counts = shell.counts(root.rawQuery) || ({})
        if (root.view === "feed" && !root.pill) root.noteShown(root.feedRows.slice(0, feed.shown))
    }
    function refreshPill() {
        root.refreshRelated()
        if (!root.pill) { root.pillRows = []; return }
        if (root.pillResults) root.pillResults.update(root.rawQuery, M.pillFilter(root.pill))
        else root.pillResults = shell.search(root.rawQuery, M.pillFilter(root.pill))
        rebuildPill()
    }
    function rebuildPill() {
        if (!root.pill) return
        root.pillRows = root.rowsOf(root.pillResults)
        if (root.view === "feed") root.noteShown(root.feedRows.slice(0, feed.shown))
    }
    function rowsOf(results) {
        const rows = []
        if (results)
            for (let i = 0; i < results.count; ++i) rows.push(results.get(i))
        return rows
    }

    onRawQueryChanged: { flushMarks(); refreshAll(); refreshApps() }
    onPillChanged: { flushMarks(); refreshPill() }
    onViewChanged: if (root.view === "feed") root.noteShown(root.feedRows.slice(0, feed.shown))
    Connections {
        target: root.allResults
        function onResultsChanged() { root.rebuildAll() }
    }
    Connections {
        target: root.pillResults
        function onResultsChanged() { root.rebuildPill() }
    }
    // Results are JS-owned and the GC is lazy: free them now, so they stop
    // following the store and do not count against this view's live ones.
    Component.onDestruction: {
        if (root.allResults) root.allResults.destroy()
        if (root.pillResults) root.pillResults.destroy()
    }

    Timer {
        id: markTimer
        interval: 1500
        onTriggered: root.flushMarks()
    }
    // Marks what was noted now. The host marks only ids still in this view's
    // results, so this runs before the results change under them too (new
    // text, a pill, the modules dropping theirs), or they would stay unseen
    // for good: noted, in newIds, never noted again in this visit.
    function flushMarks() {
        markTimer.stop()
        if (root.toMark.length > 0) shell.markSeen(root.toMark)
        root.toMark = []
    }
    // Relative times ("5 min") move on.
    Timer {
        interval: 30000
        running: true
        repeat: true
        onTriggered: root.now = Date.now()
    }

    color: Theme.palette.background

    // The search keeps focus, and Qt's own cursor blinks forever: each blink
    // redraws the whole New tab, so an idle window never stops drawing
    // (~5% CPU in software rendering, basecamp-theming#20). This cursor
    // blinks for a few seconds after the last edit, then stays solid. A
    // cursor delegate also stops TextInput's built-in blink timer.
    Component {
        id: calmCursor
        Rectangle {
            id: caret
            property bool blinking: true
            readonly property Item input: parent
            width: 1
            color: input ? input.color : Theme.palette.text
            visible: input ? input.cursorVisible : false
            onVisibleChanged: { blinking = true; calm.restart() }
            Component.onCompleted: calm.start()

            SequentialAnimation {
                running: caret.visible && caret.blinking
                loops: Animation.Infinite
                alwaysRunToEnd: true            // stop on the visible half
                PauseAnimation { duration: 600 }
                PropertyAction { target: caret; property: "opacity"; value: 0 }
                PauseAnimation { duration: 600 }
                PropertyAction { target: caret; property: "opacity"; value: 1 }
            }
            Timer { id: calm; interval: 10000; onTriggered: caret.blinking = false }
            Connections {
                target: caret.input
                function onTextChanged() { caret.blinking = true; calm.restart() }
                function onCursorPositionChanged() { caret.blinking = true; calm.restart() }
            }
        }
    }
    Component.onCompleted: {
        search.textInput.cursorDelegate = calmCursor
        shell.setBarPlaceholder(root.barPlaceholder)
        root.rawQuery = root.typedQuery
        root.refreshAll()
        root.refreshApps()
        root.startVisit()
    }

    // Pills: All, one per type, one per app; scrolls sideways. The view
    // toggle (modules / feed) stays at the end. On a desktop they sit under
    // the page's own search; on a phone, where the search is the shell's
    // bottom bar, they sit just above it, under the thumb with the query
    // (basecamp-experiments#34).
    readonly property bool hasPills: root.allRows.length > 0 || root.pill !== null
    readonly property bool pillsAtBottom: root.compact && root.searchInBar
    Component {
        id: pillsBar
        Item {
            objectName: "home.pills"
            height: Theme.minTouchTarget

            // A Flickable over a Row, not a ListView: every pill exists, so
            // each can be found by its objectName.
            Flickable {
                id: pillList
                anchors.left: parent.left
                anchors.right: viewToggle.left
                anchors.rightMargin: viewToggle.visible ? Theme.spacing.small : 0
                height: parent.height
                contentWidth: pillRow.width
                contentHeight: height
                flickableDirection: Flickable.HorizontalFlick
                clip: true
                boundsBehavior: Flickable.StopAtBounds

                Row {
                    id: pillRow
                    spacing: Theme.spacing.small
                    Repeater {
                        model: [{ key: "all" }]
                               .concat(root.pillSet.types.map(t => ({ key: t.type, type: t.type, count: t.count })))
                               .concat(root.pillSet.apps.length > 1
                                       ? root.pillSet.apps.map(a => ({ key: "app." + a.app, app: a.app, count: a.count }))
                                       : [])
                        delegate: LogosButton {
                            required property var modelData
                            readonly property bool selected: modelData.key === "all"
                                ? root.pill === null
                                : root.pill !== null && (root.pill.type || "") === (modelData.type || "")
                                  && (root.pill.app || "") === (modelData.app || "")
                            objectName: "home.pill." + modelData.key
                            height: Theme.minTouchTarget
                            text: modelData.key === "all" ? qsTr("All")
                                : qsTr("%1 %2").arg(root.pillLabel({ type: modelData.type || "", app: modelData.app || "" }))
                                               .arg(modelData.count)
                            variant: selected ? LogosButton.Variant.Primary : LogosButton.Variant.Secondary
                            onClicked: root.setPill(modelData.key === "all" ? null
                                                    : { type: modelData.type || "", app: modelData.app || "" })
                        }
                    }
                }
            }

            LogosIconButton {
                id: viewToggle
                objectName: "home.viewToggle"
                visible: !root.feedOnly
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                width: visible ? Theme.minTouchTarget : 0
                size: Theme.minTouchTarget
                iconSource: root.view === "grid" ? LogosIcons.list : LogosIcons.grid
                Accessible.name: root.view === "grid" ? qsTr("Show the feed") : qsTr("Show the modules")
                onClicked: root.chosenView = root.view === "grid" ? "feed" : "grid"
            }
        }
    }

    // The phone's pills, docked on the page's bottom edge (the shell's
    // search bar is just below it); the page above stops at the dock, so
    // no card hides behind the pills.
    Rectangle {
        id: pillDock
        objectName: "home.pillDock"
        visible: root.hasPills && root.pillsAtBottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: Theme.minTouchTarget + 2 * Theme.spacing.small
        color: Theme.palette.background

        Rectangle {
            width: parent.width
            height: 1
            color: Theme.palette.borderHairline
        }

        Loader {
            active: pillDock.visible
            x: root.sideMargin
            width: parent.width - 2 * root.sideMargin
            height: Theme.minTouchTarget
            anchors.verticalCenter: parent.verticalCenter
            sourceComponent: pillsBar
        }
    }

    Flickable {
        id: scroll
        objectName: "home.scroll"
        anchors.fill: parent
        anchors.bottomMargin: pillDock.visible ? pillDock.height : 0
        contentWidth: width
        contentHeight: page.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

    Column {
        id: page
        x: root.sideMargin + (root.width - 2 * root.sideMargin - root.contentWidth) / 2
        width: root.contentWidth
        topPadding: root.compact ? Theme.spacing.medium : Theme.spacing.xlarge * 2
        spacing: root.compact ? Theme.spacing.medium : Theme.spacing.large

        LogosSearchBar {
            id: search
            objectName: "home.bar"
            visible: !root.searchInBar
            focus: true
            // As wide as the feed cards below, so its edges line up with them.
            width: parent.width
            height: Theme.components.search.height
            // The bar names what it narrows (§7).
            placeholderText: root.barPlaceholder
            // In the sidebar Ctrl+K opens the overlay instead.
            shortcutHint: root.feedOnly ? "" : "Ctrl+K"
            onSubmitted: root.openFirst()
            Keys.onReturnPressed: root.openFirst()
            Keys.onEnterPressed: root.openFirst()
            Keys.onEscapePressed: event => { event.accepted = root.escapeSearch() }
        }

        Loader {
            active: root.hasPills && !root.pillsAtBottom
            visible: active
            width: parent.width
            height: active ? Theme.minTouchTarget : 0
            sourceComponent: pillsBar
        }

        LogosText {
            objectName: "home.recentApps.title"
            visible: appRow.visible && root.query.length === 0
            width: parent.width
            text: qsTr("Recent apps")
            font.pixelSize: Theme.typography.secondaryText
            font.weight: Font.Medium
            color: Theme.palette.textSecondary
        }

        // Recent apps, in one row that scrolls sideways.
        ListView {
            id: appRow
            objectName: "newTab.grid"
            visible: !root.feedOnly && root.pill === null && root.query.length === 0
                     && root.recentApps.length > 0
            width: parent.width
            height: visible ? root.tileHeight : 0
            orientation: ListView.Horizontal
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            // Left and Right go through home's keys (navKey), which move the focus.
            keyNavigationEnabled: false
            model: root.recentApps
            delegate: Tile {
                required property var modelData
                required property int index
                objectName: "newTab.app." + modelData.name
                key: modelData.name
                label: modelData.displayName || modelData.name
                artwork: modelData.iconPath || ""
                fullBleed: modelData.supportsFullBleedIcon === true
                loading: (shell.loadingModules || []).indexOf(modelData.name) >= 0
                onActivated: root.launchApp(modelData.name)
                onActiveFocusChanged: if (activeFocus) {
                    appRow.currentIndex = index
                    appRow.positionViewAtIndex(index, ListView.Contain)
                }
            }
        }

        LogosText {
            visible: !root.feedOnly && root.apps.length === 0 && root.allRows.length === 0
                     && root.query.length === 0
            width: parent.width
            wrapMode: Text.Wrap
            text: qsTr("No apps yet. Find some in Applications.")
            color: Theme.palette.textSecondary
            font.pixelSize: Theme.typography.secondaryText
        }

        // ── apps found (basecamp-experiments#32) ────────────────────────
        Column {
            id: appsBlock
            objectName: "home.apps"
            visible: !root.feedOnly && root.shownAppHits.length > 0
            // The content column's width, like the bar and the cards.
            width: parent.width

            Item {
                width: parent.width
                height: Theme.minTouchTarget

                LogosText {
                    objectName: "home.apps.title"
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Apps")
                    font.pixelSize: Theme.typography.secondaryText
                    font.weight: Font.Medium
                    color: Theme.palette.textSecondary
                }

                LogosLink {
                    objectName: "home.apps.all"
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    // The whole header's height is the tap target, text centred.
                    width: Math.max(Theme.minTouchTarget, labelItem.implicitWidth + 2 * Theme.spacing.small)
                    height: Theme.minTouchTarget
                    leftPadding: (width - labelItem.implicitWidth) / 2
                    rightPadding: leftPadding
                    topPadding: Math.max(0, (height - labelItem.implicitHeight) / 2)
                    bottomPadding: topPadding
                    text: qsTr("All apps")
                    underline: false
                    onActivated: shell.openRole("launcher")
                }
            }

            Repeater {
                id: appHitRepeater
                model: root.shownAppHits.slice(0, root.maxAppHits)
                delegate: AppRow {
                    required property var modelData
                    width: appsBlock.width
                    hit: modelData
                }
            }
        }

        LogosText {
            objectName: "home.nothing"
            // Under a pill the Apps block lists related apps, not matches,
            // so then only the pill's own results count.
            visible: root.query.length > 0
                     && (root.pill !== null || root.appHits.length === 0 || root.feedOnly)
                     && (root.pill ? root.pillRows.length === 0 : root.allRows.length === 0)
            width: parent.width
            wrapMode: Text.Wrap
            text: qsTr("Nothing matches “%1”.").arg(root.rawQuery)
            color: Theme.palette.textSecondary
            font.pixelSize: Theme.typography.secondaryText
        }

        // ── modules ─────────────────────────────────────────────────────
        Item {
            id: grid
            objectName: "home.grid"
            visible: root.view === "grid" && root.allRows.length > 0
            width: parent.width
            height: visible ? gridHeight : 0
            readonly property int gap: Theme.spacing.medium
            readonly property int columns: M.columnsFor(width, 340, gap, 3)
            readonly property real columnWidth: (width - (columns - 1) * gap) / columns
            property real gridHeight: 0
            property int shownCount: 0

            // Each visible module goes where its columns are lowest so far
            // (M.layout); a hidden one (nothing found) takes no room.
            function relayout() {
                const index = [], spans = [], heights = []
                for (let i = 0; i < moduleRepeater.count; ++i) {
                    const it = moduleRepeater.itemAt(i)
                    if (!it || !it.visible) continue
                    index.push(i)
                    spans.push(M.spanFor(it.size, grid.columns))
                    heights.push(it.implicitHeight)
                }
                const placed = M.layout(spans, heights, grid.columns, grid.gap)
                for (let k = 0; k < index.length; ++k) {
                    const it = moduleRepeater.itemAt(index[k])
                    it.x = placed.items[k].col * (grid.columnWidth + grid.gap)
                    it.y = placed.items[k].y
                }
                grid.gridHeight = placed.height
                grid.shownCount = index.length
            }
            onColumnsChanged: Qt.callLater(relayout)
            onWidthChanged: Qt.callLater(relayout)

            Repeater {
                id: moduleRepeater
                model: root.shownModules
                delegate: BasecampHomeModule {
                    required property var modelData
                    module: modelData
                    width: M.spanFor(modelData.size, grid.columns) * grid.columnWidth
                           + (M.spanFor(modelData.size, grid.columns) - 1) * grid.gap
                    barQuery: root.rawQuery
                    pill: root.pill
                    // Searching only while the modules show (#27).
                    live: root.view === "grid"
                    onReleasing: root.flushMarks()
                    now: root.now
                    newIds: root.newIds
                    visit: root.visit
                    onImplicitHeightChanged: Qt.callLater(grid.relayout)
                    onVisibleChanged: Qt.callLater(grid.relayout)
                    onRowsShown: rows => { if (root.view === "grid") root.noteShown(rows) }
                    onRaiseRequested: (id, action, text) => root.raise(id, action, text)
                    onChangeRequested: change => root.modules = M.updateModule(root.modules, modelData.id, change)
                    onRemoveRequested: root.modules = M.removeModule(root.modules, modelData.id)
                }
                onItemAdded: Qt.callLater(grid.relayout)
                onItemRemoved: Qt.callLater(grid.relayout)
            }
        }

        LogosText {
            objectName: "home.grid.empty"
            visible: grid.visible && grid.shownCount === 0
            width: parent.width
            wrapMode: Text.Wrap
            text: qsTr("None of your modules has anything here. The feed shows everything.")
            color: Theme.palette.textSecondary
            font.pixelSize: Theme.typography.secondaryText
        }

        LogosButton {
            objectName: "home.addModule"
            visible: root.view === "grid" && root.allRows.length > 0 && root.withCustom !== null
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("Add “%1” as a module").arg(root.customName)
            width: Math.min(implicitWidth, parent.width)
            onClicked: root.addModule()
        }

        // ── feed ────────────────────────────────────────────────────────
        BasecampHomeFeed {
            id: feed
            visible: root.view === "feed" && root.allRows.length > 0
            width: parent.width
            rows: visible ? root.feedRows : []
            total: root.feedTotal
            now: root.now
            newIds: root.newIds
            query: root.rawQuery
            onRaiseRequested: (id, action, text) => root.raise(id, action, text)
            onShownChanged: root.noteShown(root.feedRows.slice(0, feed.shown))
        }

        // Nothing shared yet: say what will appear here.
        LogosText {
            objectName: "home.objectsEmpty"
            visible: root.allRows.length === 0 && root.query.length === 0 && root.apps.length > 0
            width: parent.width
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Messages, payments, communities and news from your apps show up here once they share them.")
            color: Theme.palette.textTertiary
            font.pixelSize: Theme.typography.secondaryText
        }

        // The page's foot: a subtle divider and the credit, centred.
        Column {
            objectName: "newTab.footer"
            visible: !root.feedOnly
            anchors.horizontalCenter: parent.horizontalCenter
            topPadding: Theme.spacing.xlarge
            bottomPadding: Theme.spacing.xlarge * 1.5
            spacing: Theme.spacing.large

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: Math.min(root.width - 2 * root.sideMargin, 480)
                height: 1
                color: Theme.palette.borderHairline
            }

            LogosText {
                objectName: "newTab.credit"
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Shell package made by Alisher")
                font.pixelSize: Theme.typography.secondaryText
                color: Theme.palette.textTertiary
            }
        }

    }
    }

    // One app in the Apps block: icon (letters when there is no local one),
    // name, and on the right what it is: Running, or Open, or Install. The
    // whole row does what the right side says; Running switches to it.
    component AppRow: AbstractButton {
        id: appRowItem

        property var hit: ({})
        readonly property string appState: hit.state || ""

        objectName: "home.app." + (hit.name || "")
        height: 56
        hoverEnabled: true
        // Focus from the keyboard only: a click leaves it in the search.
        focusPolicy: Qt.TabFocus
        Accessible.name: hit.title || hit.name || ""
        Accessible.description: stateLabel.visible ? stateLabel.text : actionButton.text
        onClicked: root.openApp(hit)
        // Enter (#48): what a click does.
        function activate() { root.openApp(appRowItem.hit) }

        background: Rectangle {
            radius: Theme.spacing.radiusMedium
            color: appRowItem.pressed ? Theme.palette.backgroundTertiary
                 : appRowItem.hovered || appRowItem.activeFocus ? Theme.palette.backgroundSecondary
                 : "transparent"
            // The keyboard's focus ring (#48), drawn inside: nothing moves.
            border.width: appRowItem.activeFocus ? 2 : 0
            border.color: Theme.palette.accentOrange
        }

        BasecampAppTile {
            id: appIcon
            anchors.left: parent.left
            anchors.leftMargin: Theme.spacing.tiny
            anchors.verticalCenter: parent.verticalCenter
            size: 40
            label: appRowItem.hit.title || appRowItem.hit.name || ""
            source: appRowItem.hit.icon || ""
            fullBleed: appRowItem.hit.fullBleed === true
        }

        LogosText {
            anchors.left: appIcon.right
            anchors.leftMargin: Theme.spacing.medium
            anchors.right: rowEnd.left
            anchors.rightMargin: Theme.spacing.medium
            anchors.verticalCenter: parent.verticalCenter
            elide: Text.ElideRight
            // The name escaped, what the bar's text matches marked
            // (basecamp-experiments#46).
            textFormat: Text.StyledText
            text: ObjectCardFormat.mark(appRowItem.hit.title || appRowItem.hit.name || "", root.rawQuery,
                                        Theme.palette.accentOrange)
            color: Theme.palette.text
        }

        Item {
            id: rowEnd
            anchors.right: parent.right
            anchors.rightMargin: Theme.spacing.tiny
            anchors.verticalCenter: parent.verticalCenter
            width: stateLabel.visible ? stateLabel.implicitWidth : actionButton.width
            height: Theme.minTouchTarget

            LogosText {
                id: stateLabel
                objectName: "home.app.state." + (appRowItem.hit.name || "")
                visible: appRowItem.appState === "running" || appRowItem.appState === "installing"
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                text: appRowItem.appState === "running" ? qsTr("Running") : qsTr("Installing…")
                font.pixelSize: Theme.typography.secondaryText
                color: Theme.palette.textSecondary
            }

            LogosButton {
                id: actionButton
                objectName: "home.app.action." + (appRowItem.hit.name || "")
                visible: !stateLabel.visible
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                height: Theme.minTouchTarget
                width: Math.max(implicitWidth, 88)
                text: appRowItem.appState === "available" ? qsTr("Install") : qsTr("Open")
                variant: appRowItem.appState === "available" ? LogosButton.Variant.Primary
                                                           : LogosButton.Variant.Secondary
                Accessible.name: qsTr("%1 %2").arg(text).arg(appRowItem.hit.title || appRowItem.hit.name || "")
                onClicked: root.openApp(appRowItem.hit)
            }
        }
    }

    component Tile: AbstractButton {
        id: tile

        property string key
        property string label
        property string artwork: ""
        property bool fullBleed: false
        property bool loading: false

        signal activated()

        width: root.pitch
        height: root.tileHeight
        hoverEnabled: true
        // Focus from the keyboard only: a click leaves it in the search.
        focusPolicy: Qt.TabFocus
        Accessible.name: tile.label
        onClicked: tile.activated()
        // Enter (#48): what a click does.
        function activate() { tile.activated() }
        // Hovered or focused from the keyboard: lifted the same way.
        readonly property bool lit: tile.hovered || tile.activeFocus

        Rectangle {
            anchors.fill: parent
            anchors.margins: 4
            radius: Theme.spacing.radiusLarge
            color: tile.pressed ? Theme.palette.backgroundTertiary : Theme.palette.backgroundSecondary
            opacity: tile.lit ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: 120 } }
            // The keyboard's focus ring (#48), on the plate's edge.
            border.width: tile.activeFocus ? 2 : 0
            border.color: Theme.palette.accentOrange
        }

        Item {
            id: art
            width: root.tileSize
            height: root.tileSize
            anchors.horizontalCenter: parent.horizontalCenter
            y: tile.lit ? 8 : 12
            Behavior on y { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }

            LogosGlow {
                anchors.fill: parent
                radius: artTile.radius
                active: tile.lit
            }

            // The app's icon, or its letters when it has none.
            BasecampAppTile {
                id: artTile
                anchors.centerIn: parent
                visible: !tile.loading
                size: root.tileSize
                label: tile.label
                source: tile.artwork
                fullBleed: tile.fullBleed
            }

            BusyIndicator {
                anchors.centerIn: parent
                width: 28
                height: 28
                running: tile.loading
                visible: tile.loading
            }
        }

        LogosText {
            anchors.top: art.bottom
            anchors.topMargin: 8
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width - 8
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            maximumLineCount: 2
            elide: Text.ElideRight
            lineHeight: 1.1
            text: tile.label
            font.pixelSize: Theme.typography.secondaryText
            color: tile.lit ? Theme.palette.text : Theme.palette.textSecondary
        }
    }
}
