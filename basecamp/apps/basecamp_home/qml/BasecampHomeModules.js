.pragma library

// Home's pills, modules and feed (SHELL-OBJECTS-SPEC §7) as pure functions
// over envelopes, so they are unit-tested outside QML
// (tests/object_cards_test.cpp). NewTabPage.qml holds the state; nothing
// here talks to the shell.
//
// (A file name no other bundled app uses: two ui_qml apps shipping a file of
// the same name collide, spec §11.)

var kSizes = ["small", "medium", "large", "row"]

// The host keeps 8 live results per view (ObjectSearch::kMaxLivePerView):
// one for the pills and the feed, one per module.
var kMaxModules = 7

// How many objects a module of each size shows: small = one-line rows,
// medium and large = whole cards (large is two columns wide), row = a
// sideways strip of cards across the whole grid.
var kShownBySize = { "small": 3, "medium": 2, "large": 4, "row": 10 }

var kMaxNameLength = 40
var kMaxQueryLength = 80

// Basecamp's starting modules: saved filters, not data.
function defaultModules() {
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

// A list as a JS array. One that came through C++ (a module as a model's
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

// A module as home stores it: { id, name, query, filter { types?, apps? },
// size, pinned, notify, custom }. Anything else is dropped.
function sanitizeModule(m) {
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

// Pinned modules first; otherwise the saved order.
function ordered(modules) {
    var pinned = [], rest = []
    for (var i = 0; i < modules.length; ++i)
        (modules[i].pinned ? pinned : rest).push(modules[i])
    return pinned.concat(rest)
}

// "Add custom module": the current query and pill as a new module, or null
// when home already has kMaxModules or the same module exists.
function addModule(modules, name, query, pill) {
    if (modules.length >= kMaxModules) return null
    var m = sanitizeModule({ name: name, query: query, filter: pillFilter(pill),
                             size: "medium", notify: true, custom: true })
    if (m.name.length === 0) return null
    for (var i = 0; i < modules.length; ++i) {
        var o = modules[i]
        if (o.name === m.name || (o.query === m.query
                                  && JSON.stringify(o.filter) === JSON.stringify(m.filter)))
            return null
    }
    var n = 1
    var ids = modules.map(function (o) { return o.id })
    while (ids.indexOf("custom-" + n) >= 0) ++n
    m.id = "custom-" + n
    return modules.concat([m])
}

function updateModule(modules, id, change) {
    return modules.map(function (m) {
        return m.id === id ? sanitizeModule(Object.assign({}, m, change)) : m
    })
}

function removeModule(modules, id) {
    return modules.filter(function (m) { return m.id !== id })
}

function nextSize(size) {
    var i = kSizes.indexOf(size)
    return kSizes[(i + 1) % kSizes.length]
}

function shownCount(size) {
    return kShownBySize.hasOwnProperty(size) ? kShownBySize[size] : kShownBySize.medium
}

// ── pills ───────────────────────────────────────────────────────────────
// A pill is { type, app }: "" = not narrowed that way.

function pillFilter(pill) {
    var f = {}
    if (pill && str(pill.type).length > 0) f.types = [pill.type]
    if (pill && str(pill.app).length > 0) f.apps = [pill.app]
    return f
}

// A module's filter narrowed by the pill, or null when the two exclude each
// other (a Messages module under the Transactions pill shows nothing).
function effectiveFilter(moduleFilter, pill) {
    moduleFilter = moduleFilter || {}
    var out = {}
    var keys = [["types", "type"], ["apps", "app"]]
    for (var i = 0; i < keys.length; ++i) {
        var m = strings(moduleFilter[keys[i][0]])
        var p = pill ? str(pill[keys[i][1]]) : ""
        if (p.length > 0) {
            if (m.length > 0 && m.indexOf(p) < 0) return null
            out[keys[i][0]] = [p]
        } else if (m.length > 0) {
            out[keys[i][0]] = m
        }
    }
    var kinds = strings(moduleFilter.kinds), subjects = strings(moduleFilter.subjects)
    if (kinds.length > 0) out.kinds = kinds
    if (subjects.length > 0) out.subjects = subjects
    if (moduleFilter.unseenOnly === true) out.unseenOnly = true
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

// A name for "Add module": "Transactions: btc", "btc" or "Transactions".
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
// (the unseen boost goes), and a module's rows must not jump under the user's
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

// ── grid ────────────────────────────────────────────────────────────────

// Columns for the grid at `width`: modules at least `minWidth` wide, at most
// `maxColumns`; a phone gets one.
function columnsFor(width, minWidth, gap, maxColumns) {
    var n = Math.floor((width + gap) / (minWidth + gap))
    return Math.max(1, Math.min(maxColumns, n))
}

// How many columns a module of `size` takes.
function spanFor(size, columns) {
    if (size === "row") return columns
    if (size === "large") return Math.min(2, columns)
    return 1
}

// Places modules in columns, each where its columns are lowest so far, in
// order. spans and heights are per module. Returns { items: [{ col, y }],
// height }.
function layout(spans, heights, columns, gap) {
    var cols = []
    for (var c = 0; c < columns; ++c) cols.push(0)
    var items = []
    for (var i = 0; i < spans.length; ++i) {
        var span = Math.max(1, Math.min(columns, spans[i]))
        var best = 0, bestY = Infinity
        for (var s = 0; s + span <= columns; ++s) {
            var y = 0
            for (var k = s; k < s + span; ++k) y = Math.max(y, cols[k])
            if (y < bestY) { bestY = y; best = s }
        }
        items.push({ col: best, y: bestY })
        for (var k2 = best; k2 < best + span; ++k2) cols[k2] = bestY + heights[i] + gap
    }
    var height = 0
    for (var c2 = 0; c2 < columns; ++c2) height = Math.max(height, cols[c2])
    return { items: items, height: Math.max(0, height - (items.length > 0 ? gap : 0)) }
}

// ── keyboard (basecamp-experiments#48) ──────────────────────────────────

// Where an arrow key takes the focus. The results are the search field,
// one row of `tiles` (Recent apps) and `rows` stacked under it (the Apps
// block's rows, then the cards). `from` is { where: "search" | "tile" |
// "row", index }, `key` "up", "down", "left" or "right", `lastTile` the tile
// the row was left from. `lines` (optional, one per row) says which visual
// line each row is on: cards side by side (a module's sideways strip, a
// large module's two columns) share one, and Left and Right move along it
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
