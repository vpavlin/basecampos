.pragma library

// Home's pills, widgets and feed (SHELL-OBJECTS-SPEC §7) as pure functions
// over envelopes, so they are unit-tested outside QML
// (tests/object_cards_test.cpp). NewTabPage.qml holds the state; nothing
// here talks to the shell.
//
// (A file name no other bundled app uses: two ui_qml apps shipping a file of
// the same name collide, spec §11.)

// The sizes widgets had before the grid: only the first layout reads them
// (migrateLayout); a widget's block is its size now (#53).
var kSizes = ["small", "medium", "large", "row"]

// The host keeps 8 live results per view (ObjectSearch::kMaxLivePerView):
// one for the pills and the feed, one per widget.
var kMaxWidgets = 7

var kMaxNameLength = 40
var kMaxQueryLength = 80

// Basecamp's starting widgets: saved filters, not data.
function defaultWidgets() {
    return [
        { id: "transactions", name: "Transactions", query: "", filter: { types: ["transaction"] },
          size: "large", pinned: false, notify: true, custom: false },
        { id: "messages", name: "Messages", query: "", filter: { types: ["message"] },
          size: "medium", pinned: false, notify: true, custom: false },
        { id: "communities", name: "Communities", query: "", filter: { types: ["community", "post"] },
          size: "medium", pinned: false, notify: false, custom: false },
        { id: "wallet", name: "Wallet", query: "", filter: { types: ["account", "asset"] },
          size: "small", pinned: false, notify: false, custom: false },
        { id: "news", name: "News", query: "", filter: { types: ["news"] },
          size: "row", pinned: false, notify: false, custom: false }
    ]
}

function str(v) { return typeof v === "string" ? v : "" }

// A list as a JS array. One that came through C++ (a widget as a model's
// modelData, an envelope from ObjectResults.get()) is a sequence object that
// Array.isArray() says no to; anything that is not a list is [].
function arr(v) {
    if (Array.isArray(v)) return v
    if (v !== null && typeof v === "object" && typeof v.length === "number")
        return Array.prototype.slice.call(v)
    return []
}

function strings(v) {
    v = arr(v)
    var out = []
    for (var i = 0; i < v.length; ++i)
        if (typeof v[i] === "string" && v[i].length > 0 && out.indexOf(v[i]) < 0) out.push(v[i])
    return out
}

// A widget as home stores it: { id, name, query, filter { types?, apps? },
// size, pinned, notify, custom }. Anything else is dropped.
function sanitizeWidget(m) {
    m = m || {}
    var filter = {}
    var f = m.filter || {}
    var types = strings(f.types)
    var apps = strings(f.apps)
    if (types.length > 0) filter.types = types
    if (apps.length > 0) filter.apps = apps
    return {
        id: str(m.id),
        name: str(m.name).trim().slice(0, kMaxNameLength),
        query: str(m.query).trim().slice(0, kMaxQueryLength),
        filter: filter,
        size: kSizes.indexOf(m.size) >= 0 ? m.size : "medium",
        pinned: m.pinned === true,
        notify: m.notify === true,
        custom: m.custom === true
    }
}

// Pinned widgets first; otherwise the saved order.
function ordered(widgets) {
    var pinned = [], rest = []
    for (var i = 0; i < widgets.length; ++i)
        (widgets[i].pinned ? pinned : rest).push(widgets[i])
    return pinned.concat(rest)
}

// "Add custom widget": the current query and pill as a new widget, or null
// when home already has kMaxWidgets or the same widget exists.
function addWidget(widgets, name, query, pill) {
    if (widgets.length >= kMaxWidgets) return null
    var m = sanitizeWidget({ name: name, query: query, filter: pillFilter(pill),
                             size: "medium", notify: true, custom: true })
    if (m.name.length === 0) return null
    for (var i = 0; i < widgets.length; ++i) {
        var o = widgets[i]
        if (o.name === m.name || (o.query === m.query
                                  && JSON.stringify(o.filter) === JSON.stringify(m.filter)))
            return null
    }
    var n = 1
    var ids = widgets.map(function (o) { return o.id })
    while (ids.indexOf("custom-" + n) >= 0) ++n
    m.id = "custom-" + n
    return widgets.concat([m])
}

function updateWidget(widgets, id, change) {
    return widgets.map(function (m) {
        return m.id === id ? sanitizeWidget(Object.assign({}, m, change)) : m
    })
}

function removeWidget(widgets, id) {
    return widgets.filter(function (m) { return m.id !== id })
}

// ── pills ───────────────────────────────────────────────────────────────
// A pill is { type, app }: "" = not narrowed that way.

function pillFilter(pill) {
    var f = {}
    if (pill && str(pill.type).length > 0) f.types = [pill.type]
    if (pill && str(pill.app).length > 0) f.apps = [pill.app]
    return f
}

// A widget's filter narrowed by the pill, or null when the two exclude each
// other (a Messages widget under the Transactions pill shows nothing).
function effectiveFilter(widgetFilter, pill) {
    widgetFilter = widgetFilter || {}
    var out = {}
    var keys = [["types", "type"], ["apps", "app"]]
    for (var i = 0; i < keys.length; ++i) {
        var m = strings(widgetFilter[keys[i][0]])
        var p = pill ? str(pill[keys[i][1]]) : ""
        if (p.length > 0) {
            if (m.length > 0 && m.indexOf(p) < 0) return null
            out[keys[i][0]] = [p]
        } else if (m.length > 0) {
            out[keys[i][0]] = m
        }
    }
    var kinds = strings(widgetFilter.kinds), subjects = strings(widgetFilter.subjects)
    if (kinds.length > 0) out.kinds = kinds
    if (subjects.length > 0) out.subjects = subjects
    if (widgetFilter.unseenOnly === true) out.unseenOnly = true
    return out
}

function joinQuery(a, b) {
    return [str(a).trim(), str(b).trim()].filter(function (s) { return s.length > 0 }).join(" ")
}

function matchesPill(envelope, pill) {
    if (!pill) return true
    if (str(pill.type).length > 0 && envelope.type !== pill.type) return false
    if (str(pill.app).length > 0 && envelope.app !== pill.app) return false
    return true
}

// The pills for these envelopes: one per type and one per app that has any,
// most objects first, then by name.
function pills(envelopes) {
    function count(key) {
        var counts = {}, order = []
        for (var i = 0; i < envelopes.length; ++i) {
            var v = str(envelopes[i][key])
            if (v.length === 0) continue
            if (!counts.hasOwnProperty(v)) { counts[v] = 0; order.push(v) }
            counts[v] += 1
        }
        order.sort(function (a, b) { return counts[b] - counts[a] || (a < b ? -1 : a > b ? 1 : 0) })
        return order.map(function (v) { var o = { count: counts[v] }; o[key] = v; return o })
    }
    return { types: count("type"), apps: count("app") }
}

// ── feed ────────────────────────────────────────────────────────────────

function timeOf(e) {
    if (typeof e.time === "number" && isFinite(e.time)) return e.time
    if (typeof e.updated === "number" && isFinite(e.updated)) return e.updated
    return 0
}

// All objects by time, newest first (§7 "Feed"), narrowed by the pill. Ties
// by id, so equal times never swap.
function feedRows(envelopes, pill) {
    var rows = envelopes.filter(function (e) { return matchesPill(e, pill) })
    rows.sort(function (a, b) {
        var d = timeOf(b) - timeOf(a)
        if (d !== 0) return d
        return a.id < b.id ? -1 : a.id > b.id ? 1 : 0
    })
    return rows
}

// The ids that are not seen yet.
function unseenIds(envelopes) {
    var out = []
    for (var i = 0; i < envelopes.length; ++i)
        if (envelopes[i].seen !== true) out.push(envelopes[i].id)
    return out
}

// What this visit shows as new: `newIds` (id -> true) plus the unseen ids
// among `envelopes`. Kept for the whole visit, so a card keeps its dot after
// home marks it seen. Returns { map, added }: `added` the ids that were not
// in it yet (home marks those seen); `map` is `newIds` itself when none were.
function addUnseen(newIds, envelopes) {
    var added = []
    for (var i = 0; i < envelopes.length; ++i) {
        var e = envelopes[i]
        if (!e || e.seen === true || typeof e.id !== "string" || e.id.length === 0) continue
        if (newIds[e.id] === true || added.indexOf(e.id) >= 0) continue
        added.push(e.id)
    }
    if (added.length === 0) return { map: newIds, added: added }
    var map = Object.assign({}, newIds)
    for (var j = 0; j < added.length; ++j) map[added[j]] = true
    return { map: map, added: added }
}

// A name for "Add widget": "Transactions: btc", "btc" or "Transactions".
function customName(query, pillLabel) {
    query = str(query).trim()
    pillLabel = str(pillLabel).trim()
    var name = query.length > 0 && pillLabel.length > 0 ? pillLabel + ": " + query
             : query.length > 0 ? query : pillLabel
    return name.slice(0, kMaxNameLength)
}

// How many of `rows` are in `newIds` (a map id -> true).
function newCount(rows, newIds) {
    var n = 0
    for (var i = 0; i < rows.length; ++i)
        if (newIds[rows[i].id] === true) ++n
    return n
}

// Keeps the order a visit started with. Marking objects seen re-ranks them
// (the unseen boost goes), and a widget's rows must not jump under the user's
// finger: ids already shown keep their places, new ids go first in rank
// order. `prev` is the map this returned last time (id -> place), or null to
// start over. Returns { ids, map }.
function stableOrder(prev, ids) {
    var fresh = [], known = []
    for (var i = 0; i < ids.length; ++i) {
        if (prev && prev.hasOwnProperty(ids[i])) known.push(ids[i])
        else fresh.push(ids[i])
    }
    known.sort(function (a, b) { return prev[a] - prev[b] })
    var out = fresh.concat(known)
    var map = {}
    for (var j = 0; j < out.length; ++j) map[out[j]] = j
    return { ids: out, map: map }
}

// ── cell grid (basecamp-experiments#49) ─────────────────────────────────

// Home's widgets on a grid of cells that scrolls down: as many columns as the
// window's width takes (columnsFor), rows of one fixed height. A layout is
// { columns, blocks: [{ id, x, y, w, h }] }, in cells: `w` columns wide
// (1..columns), `h` rows tall (kMinRows or more). Home keeps one per column
// count it was arranged at; one it has none for is derived (layoutFor).

var kRowHeight = 120
var kMinRows = 2
var kMaxColumns = 8
// A widget added since the layout was saved.
var kNewBlock = { w: 1, h: 3 }

// Columns for a page (the window) `width` px wide, not the grid inside its
// side margins: under 600 one, then one more every 400 px (2 from 600, 3
// from 1000, 4 from 1400, 5 from 1800 ... 8 from 3000), at most kMaxColumns.
function columnsFor(width) {
    if (!(width >= 600)) return 1
    return Math.min(kMaxColumns, 2 + Math.floor((width - 600) / 400))
}

function cell(v, fallback) {
    return typeof v === "number" && isFinite(v) ? Math.round(v) : fallback
}

// Lower than any grid home makes (kMaxWidgets cards of kMaxRows): a row
// further down is from a broken save, and compact lifts it anyway.
var kMaxY = 256

// A block clamped to the grid: one column to all of them, kMinRows to
// kMaxRows rows, not past its right edge, above its top or below kMaxY.
function clampBlock(b, columns) {
    var w = Math.max(1, Math.min(columns, cell(b.w, 1)))
    return {
        id: str(b.id),
        x: Math.max(0, Math.min(columns - w, cell(b.x, 0))),
        y: Math.max(0, Math.min(kMaxY, cell(b.y, 0))),
        w: w,
        h: Math.max(kMinRows, Math.min(kMaxRows, cell(b.h, kMinRows)))
    }
}

function overlaps(a, b) {
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h
}

function fits(placed, b) {
    for (var i = 0; i < placed.length; ++i)
        if (overlaps(placed[i], b)) return false
    return true
}

// The lowest bottom of the cards in `placed` that `b` overlaps: the first
// row it could fit below them. Its own y when it overlaps none.
function below(placed, b) {
    var y = b.y
    for (var i = 0; i < placed.length; ++i)
        if (overlaps(placed[i], b)) y = Math.max(y, placed[i].y + placed[i].h)
    return y
}

// The row `b` rises to from where it is: under the lowest card above it in
// its columns, else the top.
function riseTo(placed, b) {
    var y = 0
    for (var i = 0; i < placed.length; ++i) {
        var p = placed[i]
        if (p.x < b.x + b.w && b.x < p.x + p.w && p.y + p.h <= b.y) y = Math.max(y, p.y + p.h)
    }
    return y
}

// The blocks top to bottom, left to right; ties keep their order.
function readingOrder(blocks) {
    var order = blocks.map(function (b, i) { return { b: b, i: i } })
    order.sort(function (p, q) { return p.b.y - q.b.y || p.b.x - q.b.x || p.i - q.i })
    return order.map(function (p) { return p.b })
}

// `blocks` (moved, clamped) back in the order of `like`'s ids.
function inOrderOf(like, blocks) {
    var byId = {}
    for (var i = 0; i < blocks.length; ++i) byId[blocks[i].id] = blocks[i]
    return like.map(function (b) { return byId[b.id] })
}

// Every card moves up as far as it can without overlapping another, top to
// bottom and left to right. One that overlaps a card already settled moves
// down until it does not, so the result never overlaps.
function compact(layout) {
    var placed = []
    var order = readingOrder(layout.blocks)
    for (var i = 0; i < order.length; ++i) {
        var b = clampBlock(order[i], layout.columns)
        // Down past what it overlaps, a card at a time, then up in one go.
        while (!fits(placed, b)) b.y = below(placed, b)
        b.y = riseTo(placed, b)
        placed.push(b)
    }
    return { columns: layout.columns, blocks: inOrderOf(layout.blocks, placed) }
}

// `holdId` stays where it is; every other card, in reading order, takes the
// highest free place in its own columns, above the held card too, but not
// above the card before it (so the others keep their order). A card dragged
// below its neighbour thus stays there and the neighbour moves up past it
// (#51), where compact alone would lift the dragged card straight back.
function closeUpAround(layout, holdId) {
    var placed = []
    for (var h = 0; h < layout.blocks.length; ++h)
        if (layout.blocks[h].id === holdId) placed.push(clampBlock(layout.blocks[h], layout.columns))
    var order = readingOrder(layout.blocks)
    var top = 0
    for (var i = 0; i < order.length; ++i) {
        if (order[i].id === holdId) continue
        var b = clampBlock(order[i], layout.columns)
        b.y = top
        while (!fits(placed, b)) b.y += 1
        top = b.y
        placed.push(b)
    }
    return { columns: layout.columns, blocks: inOrderOf(layout.blocks, placed) }
}

// Puts `id` at `block` (clamped); the cards it now overlaps are pushed down,
// out of its way and each other's; they close up around it (closeUpAround),
// then everything compacts.
function settle(layout, id, block) {
    var columns = layout.columns
    var moved = null
    for (var i = 0; i < layout.blocks.length; ++i)
        if (layout.blocks[i].id === id) moved = clampBlock(Object.assign({}, layout.blocks[i], block), columns)
    if (!moved) return layout
    var placed = [moved]
    var order = readingOrder(layout.blocks)
    for (var j = 0; j < order.length; ++j) {
        if (order[j].id === id) continue
        var b = clampBlock(order[j], columns)
        while (!fits(placed, b)) b.y += 1
        placed.push(b)
    }
    return compact(closeUpAround({ columns: columns, blocks: inOrderOf(layout.blocks, placed) }, id))
}

// Moves a card to cell (x, y).
function place(layout, id, x, y) {
    return settle(layout, id, { x: x, y: y })
}

// Gives a card `w` x `h` cells, clamped to the grid and kMinRows. Made
// narrower, it pulls the next card in reading order up into the cell freed
// at its top right (#52), so no column is left empty beside it; compact
// alone only lifts cards, never moves them left.
function resize(layout, id, w, h) {
    var out = settle(layout, id, { w: w, h: h })
    var was = blockFor(layout, id), now = blockFor(out, id)
    if (!was || !now || now.w >= was.w) return out
    var free = { id: "", x: now.x + now.w, y: now.y, w: 1, h: 1 }
    if (free.x >= out.columns || !fits(out.blocks, free)) return out
    var order = readingOrder(out.blocks)
    var next = order[order.indexOf(now) + 1]
    if (!next || free.x + next.w > out.columns) return out
    return place(out, next.id, free.x, free.y)
}

// Lays `blocks` out in their order on `columns`: each at the first free cell
// after the one before it (same row further right, or a row below), so the
// reading order holds; widths clamped. `placed` are cards already there.
function flow(blocks, columns, placed) {
    placed = (placed || []).slice()
    var last = readingOrder(placed)
    var cy = last.length > 0 ? last[last.length - 1].y : 0
    var cx = last.length > 0 ? last[last.length - 1].x : -1
    var out = []
    for (var i = 0; i < blocks.length; ++i) {
        var b = clampBlock(blocks[i], columns)
        var found = false
        for (var y = cy; !found; ++y) {
            for (var x = y === cy ? cx + 1 : 0; x + b.w <= columns && !found; ++x) {
                b.x = x
                b.y = y
                found = fits(placed, b)
            }
        }
        placed.push(b)
        out.push(b)
        cy = b.y
        cx = b.x
    }
    return out
}

// The same cards on `columns` when that width has no layout of its own: in
// reading order, widths clamped, each after the one before, compacted.
function fitTo(layout, columns) {
    return compact({ columns: columns, blocks: flow(readingOrder(layout.blocks), columns) })
}

// The first layout, from the size and pin widgets had before the grid:
// pinned first, then the saved order; small, medium and large are 2, 3 and 4
// rows, large two columns wide; a row (one sideways strip of cards) is the
// whole width and 2 rows.
function migrateLayout(widgets, columns) {
    var kHeights = { small: 2, medium: 3, large: 4, row: 2 }
    var blocks = ordered(widgets).map(function (m) {
        return {
            id: m.id,
            x: 0, y: 0,
            w: m.size === "row" ? columns : m.size === "large" ? 2 : 1,
            h: kHeights.hasOwnProperty(m.size) ? kHeights[m.size] : kHeights.medium
        }
    })
    return compact({ columns: columns, blocks: flow(blocks, columns) })
}

// A saved layout made safe: blocks clamped, ones for widgets home no longer
// has (or twice) dropped, overlaps settled. Null when there is none.
function cleanLayout(layout, columns, ids) {
    if (!layout || typeof layout !== "object") return null
    var seen = {}
    var blocks = arr(layout.blocks).filter(function (b) {
        if (!b || ids.indexOf(b.id) < 0 || seen[b.id] === true) return false
        seen[b.id] = true
        return true
    }).map(function (b) { return clampBlock(b, columns) })
    return compact({ columns: columns, blocks: blocks })
}

// The layout home shows at `columns`: the one saved for it, else one derived
// from the widest saved (fitTo), else the migrated first one. `saved` maps a
// column count to a layout. Widgets added since get a kNewBlock at the end;
// removed ones drop out.
function layoutFor(saved, columns, widgets) {
    var ids = widgets.map(function (m) { return m.id })
    saved = saved || {}
    var base = cleanLayout(saved[columns], columns, ids)
    if (!base) {
        var widest = 0
        for (var k in saved)
            if (saved.hasOwnProperty(k) && Number(k) > widest && saved[k] && Number(k) <= kMaxColumns)
                widest = Number(k)
        var from = widest > 0 ? cleanLayout(saved[widest], widest, ids) : null
        if (!from) return migrateLayout(widgets, columns)
        base = fitTo(from, columns)
    }
    var has = base.blocks.map(function (b) { return b.id })
    var added = ordered(widgets).filter(function (m) { return has.indexOf(m.id) < 0 })
        .map(function (m) { return { id: m.id, x: 0, y: 0, w: kNewBlock.w, h: kNewBlock.h } })
    if (added.length === 0) return base
    return compact({ columns: columns, blocks: base.blocks.concat(flow(added, columns, base.blocks)) })
}

// What shows outside edit mode: the cards with nothing to show (a pill or
// the bar's text leaves them empty) give up their cells, and the rest close
// up in reading order (fitTo), so one card left is not stranded at the
// right. With none hidden, the layout as it is.
function withoutHidden(layout, hiddenIds) {
    var shown = layout.blocks.filter(function (b) { return hiddenIds.indexOf(b.id) < 0 })
    if (shown.length === layout.blocks.length) return layout
    return fitTo({ columns: layout.columns, blocks: shown }, layout.columns)
}

// Saved layouts (column count -> layout) without blocks for widgets that are
// gone, so a widget added later with a removed one's id starts fresh.
function pruneLayouts(layouts, widgets) {
    var ids = (widgets || []).map(function (m) { return m.id })
    var out = {}
    for (var k in (layouts || {})) {
        var l = layouts[k]
        // A layout read back from the saved settings has its blocks as a
        // list from C++, which Array.isArray() says no to (arr takes it).
        if (!l || typeof l !== "object" || !l.blocks || typeof l.blocks !== "object") continue
        out[k] = { columns: l.columns, blocks: arr(l.blocks).filter(function (b) {
            return b && ids.indexOf(b.id) >= 0 }) }
    }
    return out
}

// How many rows the layout takes.
function rowsOf(layout) {
    var rows = 0
    for (var i = 0; i < layout.blocks.length; ++i)
        rows = Math.max(rows, layout.blocks[i].y + layout.blocks[i].h)
    return rows
}

// The block for `id`, or null.
function blockFor(layout, id) {
    for (var i = 0; i < layout.blocks.length; ++i)
        if (layout.blocks[i].id === id) return layout.blocks[i]
    return null
}

// ── edit mode (basecamp-experiments#51) ─────────────────────────────────

// The cell a card dragged to (`x`, `y`) px (its top left, in the grid)
// lands on: the nearest, in reach of the grid. One column moves it up and
// down only. `pitchX` and `pitchY` are a cell plus the gap.
function dropCell(x, y, pitchX, pitchY, columns, w) {
    if (columns <= 1) return { x: 0, y: Math.max(0, Math.round(y / pitchY)) }
    return {
        x: Math.max(0, Math.min(columns - Math.max(1, w), Math.round(x / pitchX))),
        y: Math.max(0, Math.round(y / pitchY))
    }
}

// How far the page scrolls on one tick while a card is held at `y` px in a
// view `height` tall: up near the top, down near the bottom, faster the
// closer to the edge (at most `maxStep`), 0 elsewhere.
var kScrollEdge = 64
function autoScrollStep(y, height, maxStep) {
    var edge = Math.min(kScrollEdge, height / 4)
    if (edge <= 0) return 0
    if (y < edge) return -Math.ceil(maxStep * Math.min(1, (edge - y) / edge))
    if (y > height - edge) return Math.ceil(maxStep * Math.min(1, (y - (height - edge)) / edge))
    return 0
}

// ── resize (basecamp-experiments#52) ────────────────────────────────────

// A card is at most this many rows tall from its handles, and never
// narrower than one item card stays readable (px).
var kMaxRows = 8
var kMinCardWidth = 280

// What a card's handles allow, in cells: { minW, maxW, minH, maxH }. Width
// from the narrowest that is kMinCardWidth px wide (or the card's own width
// when it is narrower already) to the grid's right edge; height kMinRows to
// kMaxRows.
function sizeLimits(block, columns, columnWidth, gap) {
    var need = Math.max(1, Math.ceil((kMinCardWidth + gap) / (columnWidth + gap)))
    var maxW = Math.max(1, columns - block.x)
    return {
        minW: Math.min(need, block.w, maxW),
        maxW: maxW,
        minH: kMinRows,
        maxH: kMaxRows
    }
}

// The cells a card `w` x `h` px tall snaps to (the nearest), within
// `limits` (sizeLimits). `pitchX` and `pitchY` are a cell plus the gap.
function snapSize(w, h, pitchX, pitchY, gap, limits) {
    var clamp = function (v, lo, hi) { return Math.max(lo, Math.min(hi, v)) }
    return {
        w: clamp(Math.round((w + gap) / pitchX), limits.minW, limits.maxW),
        h: clamp(Math.round((h + gap) / pitchY), limits.minH, limits.maxH)
    }
}

// ── cards fit their block (basecamp-experiments#53) ────────────────────
// A widget's block decides what it draws: its height how many items show
// (whole ones only), its width how many columns of them. Two rows tall it
// draws one-line rows instead of item cards.

// An item card is at least this wide (px); a widget has 1 to 3 columns.
var kMinItemWidth = 300
var kMaxItemColumns = 3
// The shortest an item is: a one-line row is one touch target tall.
var kMinItemHeight = 44

// Columns of items in a widget whose content is `width` px wide, with
// `spacing` between them.
function itemColumns(width, spacing) {
    var n = Math.floor((width + spacing) / (kMinItemWidth + spacing))
    return Math.max(1, Math.min(kMaxItemColumns, isFinite(n) ? n : 1))
}

// Whether a block `h` rows tall draws one-line rows.
function compactRows(h) {
    return h > 0 && h <= kMinRows
}

// The most items that could fit in `available` px on `columns`: what a
// widget builds before it measures them (each is kMinItemHeight or more).
function candidateCount(available, columns, spacing) {
    if (!(available >= kMinItemHeight)) return 0
    return Math.max(1, columns) * Math.floor((available + spacing) / (kMinItemHeight + spacing))
}

// How many items fit whole in `available` px: laid out `columns` to a line
// in order, a line as tall as its tallest item, `spacing` between lines.
// `heights` are the items' heights in px; one not measured yet (0 or less)
// stops the count, so nothing is shown that might not fit.
function fitCount(heights, columns, available, spacing) {
    heights = arr(heights)
    columns = Math.max(1, columns)
    var used = 0, n = 0
    for (var start = 0; start < heights.length; start += columns) {
        var line = 0
        var end = Math.min(heights.length, start + columns)
        for (var i = start; i < end; ++i) {
            if (!(heights[i] > 0)) return n
            line = Math.max(line, heights[i])
        }
        var next = used + (start > 0 ? spacing : 0) + line
        if (next > available) return n
        used = next
        n = end
    }
    return n
}

// ── keyboard in edit mode (basecamp-experiments#54) ────────────────────

// An arrow key on a card in edit mode: `id` one step left, right, up or
// down (`dx`, `dy` of -1, 0 or 1). The others close up after each move
// (place), so a card one cell further may land back where it was (below a
// taller neighbour); then it tries the next cell, until the card's place
// changes or it reaches the edge. Up past the card above swaps the two.
// The layout itself when nothing would change.
function keyMove(layout, id, dx, dy) {
    var b = blockFor(layout, id)
    if (!b) return layout
    var steps = dx !== 0 ? layout.columns : rowsOf(layout) + 1
    for (var step = 1; step <= steps; ++step) {
        var x = b.x + dx * step, y = b.y + dy * step
        if (x < 0 || x + b.w > layout.columns || y < 0) break
        var out = place(layout, id, x, y)
        var nb = blockFor(out, id)
        if (nb && (nb.x !== b.x || nb.y !== b.y)) return out
    }
    return layout
}

// Shift and an arrow key: `id` one cell wider or narrower (`dw`), taller
// or shorter (`dh`), within `limits` (sizeLimits). A card at the grid's
// right edge grows to the left instead (a handle cannot, a key can). The
// layout itself when it is at the limit.
function keyResize(layout, id, dw, dh, limits) {
    var b = blockFor(layout, id)
    if (!b) return layout
    var h = Math.max(limits.minH, Math.min(limits.maxH, b.h + dh))
    if (dw > 0 && b.w + dw > limits.maxW && b.w + dw <= layout.columns)
        return settle(layout, id, { x: layout.columns - (b.w + dw), w: b.w + dw, h: h })
    var w = Math.max(limits.minW, Math.min(limits.maxW, b.w + dw))
    if (w === b.w && h === b.h) return layout
    return resize(layout, id, w, h)
}

// What home saves through the shell (shell.saveHomeSettings): its widgets
// and their layouts, one per column count. Only `widgets`: the shell keeps
// what home saves as a whole, so the next save drops a kOldWidgetsKey list.
function settingsOf(widgets, layouts) {
    return { version: 1, widgets: widgets, layouts: layouts }
}

// The key home's widgets were saved under while they were called modules.
// Read when `widgets` is absent (fromSettings), never written.
var kOldWidgetsKey = "modules"

// The widgets and layouts in what was saved, made safe; the defaults and no
// layouts when nothing usable was. A widget list may be empty (all removed).
// Settings from before the rename keep their list under kOldWidgetsKey.
function fromSettings(settings) {
    var out = { widgets: defaultWidgets().map(sanitizeWidget), layouts: {} }
    if (!settings || typeof settings !== "object" || settings.version !== 1) return out
    var saved = settings.widgets
    if (saved === undefined) saved = settings[kOldWidgetsKey]
    if (saved !== undefined && saved !== null && typeof saved === "object") {
        var ids = []
        out.widgets = arr(saved).map(sanitizeWidget).filter(function (m) {
            if (m.id.length === 0 || m.name.length === 0 || ids.indexOf(m.id) >= 0) return false
            ids.push(m.id)
            return true
        }).slice(0, kMaxWidgets)
    }
    var layouts = settings.layouts
    if (layouts && typeof layouts === "object")
        for (var c = 1; c <= kMaxColumns; ++c)
            if (layouts[String(c)] && typeof layouts[String(c)] === "object")
                out.layouts[String(c)] = layouts[String(c)]
    return out
}

// ── keyboard (basecamp-experiments#48) ──────────────────────────────────

// Where an arrow key takes the focus. The results are the search field,
// one row of `tiles` (Recent apps) and `rows` stacked under it (the Apps
// block's rows, then the cards). `from` is { where: "search" | "tile" |
// "row", index }, `key` "up", "down", "left" or "right", `lastTile` the tile
// the row was left from. `lines` (optional, one per row) says which visual
// line each row is on: cards side by side (a widget's sideways strip, a
// large widget's two columns) share one, and Left and Right move along it
// while Up and Down go to the line above or below, to the card in the same
// place or the nearest. Without it every row is a line of its own. Returns
// the place to focus (the same one when the key stops at an edge), or null
// when the key is not home's to take (Up in the search, Left and Right in a
// line of one).
function navMove(from, key, tiles, rows, lastTile, lines) {
    var where = from ? from.where : ""
    var i = from && typeof from.index === "number" ? from.index : 0
    var clamp = function (n, count) { return Math.max(0, Math.min(count - 1, n)) }
    if (where === "search") {
        if (key !== "down") return null
        if (tiles > 0) return { where: "tile", index: 0 }
        if (rows > 0) return { where: "row", index: 0 }
        return null
    }
    if (where === "tile") {
        if (key === "left") return { where: "tile", index: clamp(i - 1, tiles) }
        if (key === "right") return { where: "tile", index: clamp(i + 1, tiles) }
        if (key === "up") return { where: "search" }
        if (key === "down") return rows > 0 ? { where: "row", index: 0 } : { where: "tile", index: i }
        return null
    }
    if (where === "row") {
        if (rows <= 0) return null
        i = clamp(i, rows)
        var byLine = lines && lines.length === rows
        var lineOf = function (n) { return byLine ? lines[n] : n }
        // The rows on line `l`, left to right.
        var on = function (l) {
            var out = []
            for (var k = 0; k < rows; ++k) if (lineOf(k) === l) out.push(k)
            return out
        }
        var line = on(lineOf(i))
        var place = line.indexOf(i)
        if (key === "left" || key === "right") {
            if (line.length < 2) return null
            return { where: "row", index: line[clamp(place + (key === "left" ? -1 : 1), line.length)] }
        }
        if (key !== "up" && key !== "down") return null
        // The next line up or down that has rows (line numbers may skip).
        var step = key === "down" ? 1 : -1
        var target = []
        for (var k = i + step; k >= 0 && k < rows && target.length === 0; k += step)
            if (lineOf(k) !== lineOf(i)) target = on(lineOf(k))
        if (target.length > 0) return { where: "row", index: target[clamp(place, target.length)] }
        if (key === "down") return { where: "row", index: i }
        if (tiles > 0) return { where: "tile", index: clamp(lastTile || 0, tiles) }
        return { where: "search" }
    }
    return null
}

// Whether a key press types into the search: printable text, no Ctrl, Alt
// or Meta (Shift is part of typing). `modifiers` is the event's bit mask.
function typesText(text, modifiers) {
    if (typeof text !== "string" || text.length === 0) return false
    if ((modifiers & (0x04000000 | 0x08000000 | 0x10000000)) !== 0) return false
    for (var i = 0; i < text.length; ++i) {
        var c = text.charCodeAt(i)
        if (c < 32 || c === 127) return false
    }
    return true
}
