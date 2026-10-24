import QtQuick
import QtQuick.Controls

import Logos.Theme
import Logos.Controls
import Logos.Icons
import Basecamp.Icons
import Basecamp.Objects

import "BasecampHomeModules.js" as M

// Basecamp's home page (the `home` role, ADR-067): what a New tab shows.
// The bar's text searches both your apps and what apps share with Basecamp
// (SHELL-OBJECTS-SPEC §7): pills narrow the objects by type or app; below,
// either the modules (saved filters, a grid that is one column on a phone)
// or the feed (everything, newest first). Picking a tile asks the shell to
// turn the current New tab into that app; a card's actions go to the app
// that published it. It reads only `shell`, the role-gated object the host
// injects.
//
// The modules (order, size, pin, notify, the custom ones) live in memory
// only: home has no settings store of its own yet, so a restart brings back
// the defaults.
Rectangle {
    id: root

    signal openSystemApp(string key)
    signal launchApp(string name)

    function focusSearch() { search.forceActiveFocus() }
    // The shell calls this each time it shows a New tab: a new visit.
    function clearSearch() {
        search.text = ""
        root.pill = null
        root.startVisit()
    }

    // Basecampium's compact layout puts the search in its one bar
    // (docs/MOBILE-LAYOUT.md §8): this page then shows the results only.
    property bool searchInBar: false
    property string barQuery: ""
    readonly property string rawQuery: (searchInBar ? barQuery : search.text).trim()
    readonly property string query: rawQuery.toLowerCase()
    // Phone width (Basecampium's compact layout): tighter margins and tiles.
    readonly property bool compact: width < 600
    readonly property int sideMargin: compact ? Theme.spacing.medium : Theme.spacing.xlarge * 2
    readonly property int contentWidth: Math.min(width - 2 * sideMargin, 1200)
    // Tiles the size of the Applications grid's (80), so an app looks the same in both.
    readonly property int tileSize: 80
    readonly property int pitch: compact ? 108 : 124

    readonly property var systemApps: [
        { key: "sys:applications", name: qsTr("Applications"),    icon: BasecampIcons.dashboard },
        { key: "sys:settings",     name: qsTr("Settings"),        icon: BasecampIcons.settings },
        { key: "sys:packages",     name: qsTr("Package Manager"), icon: BasecampIcons.modules }
    ]
    readonly property var apps: {
        // Apps that fill a role are already in the row below (or are this page).
        const roles = shell.roles || ({})
        const roleApps = Object.keys(roles).map(r => roles[r])
        const list = (shell.installedApps || []).filter(a => roleApps.indexOf(a.name) < 0)
        list.sort((a, b) => (a.displayName || a.name).localeCompare(b.displayName || b.name))
        return list
    }
    readonly property var appNames: {
        const names = {}
        for (const a of (shell.installedApps || [])) names[a.name] = a.displayName || a.name
        return names
    }

    function matches(label) {
        return root.query.length === 0 || label.toLowerCase().indexOf(root.query) >= 0
    }
    readonly property var shownSystem: systemApps.filter(a => matches(a.name))
    readonly property var shownApps: apps.filter(a => matches(a.displayName || a.name))

    function openFirst() {
        if (shownApps.length > 0)
            root.launchApp(shownApps[0].name)
        else if (shownSystem.length > 0)
            root.openSystemApp(shownSystem[0].key)
    }

    // ── objects ─────────────────────────────────────────────────────────

    // { type, app } narrowing every module and the feed; null = all.
    property var pill: null
    // "grid" (modules) or "feed".
    property string view: "grid"
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
    property real visitAt: 0
    property real lastVisitAt: 0
    property int visit: 0
    property real now: Date.now()

    function startVisit() {
        root.lastVisitAt = root.visitAt
        root.visitAt = Date.now()
        root.now = root.visitAt
        root.newIds = ({})
        root.toMark = []
        root.visit += 1
        if (root.view === "feed") root.noteShown(root.feedRows.slice(0, feed.shown))
    }

    // `rows` are on screen: the unseen among them are new for this visit and
    // are marked seen shortly (they keep their dot until the next visit).
    function noteShown(rows) {
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
    readonly property string barPlaceholder: !root.pill ? qsTr("Search everything…")
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

    onRawQueryChanged: refreshAll()
    onPillChanged: refreshPill()
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
        onTriggered: {
            if (root.toMark.length > 0) shell.markSeen(root.toMark)
            root.toMark = []
        }
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
        root.refreshAll()
        root.startVisit()
    }

    Flickable {
        id: scroll
        objectName: "home.scroll"
        anchors.fill: parent
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

        // Who made this shell, above the search (desktop only: on a phone
        // the bar is the shell's).
        LogosText {
            objectName: "newTab.brand"
            visible: !root.compact
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Basecampium"
            font.pixelSize: 40
            font.weight: Font.Bold
            color: Theme.palette.textSecondary
        }

        LogosSearchBar {
            id: search
            objectName: "home.bar"
            visible: !root.searchInBar
            focus: true
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(parent.width, 720)
            height: Theme.components.search.height
            // The bar names what it narrows (§7).
            placeholderText: root.barPlaceholder
            shortcutHint: "Ctrl+K"
            onSubmitted: root.openFirst()
            Keys.onReturnPressed: root.openFirst()
            Keys.onEnterPressed: root.openFirst()
        }

        // Pills: All, one per type, one per app; scrolls sideways. The view
        // toggle (modules / feed) stays at the end.
        Item {
            objectName: "home.pills"
            visible: root.allRows.length > 0 || root.pill !== null
            width: parent.width
            height: Theme.minTouchTarget

            // A Flickable over a Row, not a ListView: every pill exists, so
            // each can be found by its objectName.
            Flickable {
                id: pillList
                anchors.left: parent.left
                anchors.right: viewToggle.left
                anchors.rightMargin: Theme.spacing.small
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
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                size: Theme.minTouchTarget
                iconSource: root.view === "grid" ? LogosIcons.list : LogosIcons.grid
                Accessible.name: root.view === "grid" ? qsTr("Show the feed") : qsTr("Show the modules")
                onClicked: root.view = root.view === "grid" ? "feed" : "grid"
            }
        }

        // Your apps that match, in one row that scrolls sideways.
        ListView {
            id: appRow
            objectName: "newTab.grid"
            visible: root.pill === null && root.shownApps.length > 0
            width: parent.width
            height: visible ? root.tileSize + 48 : 0
            orientation: ListView.Horizontal
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            model: root.shownApps
            delegate: Tile {
                required property var modelData
                objectName: "newTab.app." + modelData.name
                key: modelData.name
                label: modelData.displayName || modelData.name
                artwork: modelData.iconPath || ""
                fullBleed: modelData.supportsFullBleedIcon === true
                loading: (shell.loadingModules || []).indexOf(modelData.name) >= 0
                onActivated: root.launchApp(modelData.name)
            }
        }

        LogosText {
            visible: root.apps.length === 0 && root.allRows.length === 0 && root.query.length === 0
            width: parent.width
            wrapMode: Text.Wrap
            text: qsTr("No apps yet. Find some in Applications.")
            color: Theme.palette.textSecondary
            font.pixelSize: Theme.typography.secondaryText
        }

        LogosText {
            objectName: "home.nothing"
            visible: root.query.length > 0 && root.shownApps.length === 0 && root.allRows.length === 0
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
            since: root.lastVisitAt
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

        // The shell's own apps: small round buttons under a subtle divider,
        // at the end of the page and centred. Search still finds them by name.
        Column {
            id: systemRow
            objectName: "newTab.systemRow"
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

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: Theme.spacing.large

                Repeater {
                    model: root.systemApps
                    delegate: AbstractButton {
                        id: sys
                        required property var modelData
                        objectName: "newTab." + modelData.key
                        readonly property bool matched: root.query.length === 0 || root.matches(modelData.name)
                        width: Theme.minTouchTarget
                        height: Theme.minTouchTarget
                        hoverEnabled: true
                        opacity: matched ? 1.0 : 0.35
                        onClicked: root.openSystemApp(modelData.key)
                        Accessible.name: modelData.name

                        LogosGlow {
                            anchors.fill: parent
                            radius: width / 2
                            active: sys.hovered
                        }

                        Rectangle {
                            anchors.fill: parent
                            radius: width / 2
                            color: sys.pressed ? Theme.palette.accentOrange
                                 : sys.hovered ? Theme.palette.backgroundTertiary
                                 : Theme.palette.backgroundSecondary
                            border.color: sys.hovered ? Theme.palette.border : Theme.palette.borderHairline
                            border.width: 1
                            Behavior on color { ColorAnimation { duration: 120 } }

                            LogosIcon {
                                anchors.centerIn: parent
                                width: 18
                                height: 18
                                source: sys.modelData.icon
                                color: sys.pressed ? Theme.palette.accentForeground
                                     : sys.hovered ? Theme.palette.text : Theme.palette.textSecondary
                                brightness: 1.0
                            }
                        }

                        ToolTip.visible: sys.hovered
                        ToolTip.delay: 300
                        ToolTip.text: modelData.name
                    }
                }
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

    component Tile: AbstractButton {
        id: tile

        property string key
        property string label
        property url glyph: ""
        property string artwork: ""
        property bool fullBleed: false
        property bool loading: false

        signal activated()

        width: root.pitch
        height: root.tileSize + 48
        hoverEnabled: true
        onClicked: tile.activated()

        Rectangle {
            anchors.fill: parent
            anchors.margins: 4
            radius: Theme.spacing.radiusLarge
            color: tile.pressed ? Theme.palette.backgroundTertiary : Theme.palette.backgroundSecondary
            opacity: tile.hovered ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: 120 } }
        }

        Item {
            id: art
            width: root.tileSize
            height: root.tileSize
            anchors.horizontalCenter: parent.horizontalCenter
            y: tile.hovered ? 8 : 12
            Behavior on y { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }

            LogosGlow {
                anchors.fill: parent
                radius: Theme.spacing.radiusLarge
                active: tile.hovered
            }

            Rectangle {
                anchors.fill: parent
                visible: tile.artwork.length === 0
                radius: Theme.spacing.radiusLarge
                color: tile.pressed ? Theme.palette.accentOrange : Theme.palette.surfaceRaised

                LogosIcon {
                    anchors.centerIn: parent
                    width: 30
                    height: 30
                    source: tile.glyph
                    color: tile.pressed ? Theme.palette.accentForeground : Theme.palette.text
                    brightness: 1.0
                }
            }

            LogosTile {
                anchors.centerIn: parent
                visible: tile.artwork.length > 0 && !tile.loading
                label: tile.key
                source: tile.artwork
                fallbackColor: Theme.palette.surfaceRaised
                insetArtwork: !tile.fullBleed
                interactive: false
                tileSize: root.tileSize
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
            elide: Text.ElideRight
            text: tile.label
            font.pixelSize: Theme.typography.secondaryText
            color: tile.hovered ? Theme.palette.text : Theme.palette.textSecondary
        }
    }
}
