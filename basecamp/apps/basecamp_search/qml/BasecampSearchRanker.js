.pragma library

// basecamp_search's ranking (SHELL-OBJECTS-SPEC §5). The SAME rules as the
// host's fallback ranker, app/ObjectRanker.{h,cpp}, so results do not jump when
// the host answers instead of this app; change both together. The rules are
// written out in ObjectRanker.h.
//
// (A file name no other bundled app uses: two ui_qml apps shipping a file of
// the same name collide, spec §11.)

// Whether the string values of `fields` (content in v0: amounts, message text)
// are matched. The privacy decision (spec §9) is open; this is the one switch,
// mirrored by ObjectRanker::kMatchContentFields.
var kMatchContentFields = true

var kMaxResults = 200
var kTitleStrength = 4
var kKeywordStrength = 3
var kSubtitleStrength = 2
var kFieldStrength = 1
var kMatchWeight = 10
var kRecencyWeight = 5
var kRecencyHalfLifeMs = 24 * 60 * 60 * 1000
var kUnseenBoost = 3

// Whitespace and the ASCII punctuation ranges !-/ :-@ [-` {-~.
var kSeparators = /[\s!-\/:-@\[-`{-~]+/

function tokens(text) {
    if (typeof text !== "string" || text.length === 0) return []
    return text.toLowerCase().split(kSeparators).filter(function (t) { return t.length > 0 })
}

function hasWordStartingWith(text, token) {
    // A word of it starts with the token only if the text holds the token:
    // that check is cheap, the split is not, and most objects miss.
    if (typeof text !== "string" || text.toLowerCase().indexOf(token) < 0) return false
    var words = tokens(text)
    for (var i = 0; i < words.length; ++i)
        if (words[i].indexOf(token) === 0) return true
    return false
}

function collectStrings(v, out, depth) {
    if (typeof v === "string") {
        out.push(v)
    } else if (depth < 2 && Array.isArray(v)) {
        for (var i = 0; i < v.length; ++i) collectStrings(v[i], out, depth + 1)
    } else if (depth < 2 && v !== null && typeof v === "object") {
        for (var k in v) collectStrings(v[k], out, depth + 1)
    }
}

function strength(o, token) {
    if (hasWordStartingWith(o.title, token)) return kTitleStrength
    var keywords = o.keywords || []
    for (var i = 0; i < keywords.length; ++i)
        if (hasWordStartingWith(keywords[i], token)) return kKeywordStrength
    if (hasWordStartingWith(o.subtitle, token)) return kSubtitleStrength
    if (kMatchContentFields) {
        var strings = []
        collectStrings(o.fields, strings, 0)
        for (var j = 0; j < strings.length; ++j)
            if (hasWordStartingWith(strings[j], token)) return kFieldStrength
    }
    return 0
}

function timeOf(o) {
    return o.time !== undefined ? Number(o.time) : Number(o.updated || 0)
}

// -1 when a token does not match.
function score(o, queryTokens, nowMs) {
    var match = 0
    for (var i = 0; i < queryTokens.length; ++i) {
        var s = strength(o, queryTokens[i])
        if (s === 0) return -1
        match += s
    }
    var age = Math.max(0, nowMs - timeOf(o))
    var recency = kRecencyWeight * Math.pow(2, -age / kRecencyHalfLifeMs)
    var unseen = o.seen ? 0 : kUnseenBoost
    return kMatchWeight * match + recency + unseen
}

// `objects` (envelopes) ranked for `query`, best first: their ids.
function rank(objects, query, nowMs) {
    var queryTokens = tokens(query)
    var hits = []
    for (var i = 0; i < objects.length; ++i) {
        var o = objects[i]
        if (!o || !o.id) continue
        if (queryTokens.length === 0) {
            hits.push({ score: 0, time: timeOf(o), unseen: !o.seen, id: String(o.id) })
            continue
        }
        var s = score(o, queryTokens, nowMs)
        if (s >= 0) hits.push({ score: s, time: 0, unseen: false, id: String(o.id) })
    }
    return ordered(hits, queryTokens.length === 0)
}

// `hits` ({ score, time, unseen, id }) sorted as rank() orders them: their
// first kMaxResults ids.
function ordered(hits, emptyQuery) {
    function byId(a, b) { return a.id < b.id ? -1 : (a.id > b.id ? 1 : 0) }
    if (emptyQuery) {
        hits.sort(function (a, b) {
            if (a.unseen !== b.unseen) return a.unseen ? -1 : 1
            if (a.time !== b.time) return b.time - a.time
            return byId(a, b)
        })
    } else {
        hits.sort(function (a, b) {
            if (a.score !== b.score) return b.score - a.score
            return byId(a, b)
        })
    }
    var out = []
    for (var k = 0; k < hits.length && k < kMaxResults; ++k) out.push(hits[k].id)
    return out
}

// ── A kept copy of the store (basecamp-experiments#27) ─────────────────────
// Asking the shell for every object on every query copied the whole store
// into this engine each time (about 55 ms for 2500 objects) and split every
// object's words again. Instead this app keeps an index: the shell says what
// changed since the version it holds (shell.objectsSince), and each object's
// words are split once, when it arrives. Ranking the index gives the same ids
// as rank() over the same objects (unit-tested), and ObjectRanker::Prepared
// is the host's twin of it.
//
// Each place a token can match becomes one string of lowercased words, each
// after a space: " ada lovelace". A token has no spaces, so " " + token can
// only be found at the start of a word and cannot run into the next one,
// which is exactly hasWordStartingWith().

function wordsOf(text) {
    if (typeof text !== "string" || text.length === 0) return ""
    var words = tokens(text)
    return words.length > 0 ? " " + words.join(" ") : ""
}

// What an empty query needs (id, time, seen) at once; the words come from
// prepareWords(), when the app is idle or before a query that needs them.
function prepareOrder(o) {
    return { id: String(o.id), time: timeOf(o), seen: !!o.seen, words: false, o: o,
             title: "", keywords: "", subtitle: "", fields: "" }
}

function prepareWords(p) {
    var o = p.o
    var keywords = o.keywords || []
    var kw = ""
    for (var i = 0; i < keywords.length; ++i) kw += wordsOf(keywords[i])
    var fieldWords = ""
    if (kMatchContentFields) {
        var strings = []
        collectStrings(o.fields, strings, 0)
        for (var j = 0; j < strings.length; ++j) fieldWords += wordsOf(strings[j])
    }
    p.title = wordsOf(o.title)
    p.keywords = kw
    p.subtitle = wordsOf(o.subtitle)
    p.fields = fieldWords
    p.words = true
    p.o = null
}

function prepare(o) {
    var p = prepareOrder(o)
    prepareWords(p)
    return p
}

function preparedStrength(p, spacedToken) {
    if (p.title.indexOf(spacedToken) >= 0) return kTitleStrength
    if (p.keywords.indexOf(spacedToken) >= 0) return kKeywordStrength
    if (p.subtitle.indexOf(spacedToken) >= 0) return kSubtitleStrength
    if (kMatchContentFields && p.fields.indexOf(spacedToken) >= 0) return kFieldStrength
    return 0
}

function newIndex() {
    return { version: 0, byId: {}, size: 0, waiting: [] }
}

// Splits the words of up to `max` objects still waiting (all: max < 0).
// Returns how many still wait.
function prepareSome(index, max) {
    var done = 0
    while (index.waiting.length > 0 && (max < 0 || done < max)) {
        var p = index.byId[index.waiting.pop()]
        if (!p || p.words) continue
        prepareWords(p)
        ++done
    }
    return index.waiting.length
}

// Applies shell.objectsSince(index.version, …) to `index`. false when
// `changes` is not one (an older shell without objectsSince, or not the
// holder): the caller then ranks shell.objects() as before.
function applyChanges(index, changes) {
    if (!changes || changes.version === undefined || changes.version === null) return false
    if (changes.reset) { index.byId = {}; index.size = 0; index.waiting = [] }
    var removed = changes.removed || []
    for (var i = 0; i < removed.length; ++i) {
        if (index.byId.hasOwnProperty(removed[i])) { delete index.byId[removed[i]]; index.size -= 1 }
    }
    var objects = changes.objects || []
    for (var j = 0; j < objects.length; ++j) {
        var o = objects[j]
        if (!o || !o.id) continue
        var id = String(o.id)
        if (!index.byId.hasOwnProperty(id)) index.size += 1
        index.byId[id] = prepareOrder(o)
        index.waiting.push(id)
    }
    index.version = changes.version
    return true
}

// rank() over the index: all of it, or only `ids` (what a filter allows).
function rankIndex(index, ids, query, nowMs) {
    var queryTokens = tokens(query)
    var spaced = queryTokens.map(function (t) { return " " + t })
    if (spaced.length > 0) prepareSome(index, -1)
    var hits = []
    var consider = function (p) {
        if (!p) return
        if (spaced.length === 0) {
            hits.push({ score: 0, time: p.time, unseen: !p.seen, id: p.id })
            return
        }
        var match = 0
        for (var k = 0; k < spaced.length; ++k) {
            var s = preparedStrength(p, spaced[k])
            if (s === 0) return
            match += s
        }
        var age = Math.max(0, nowMs - p.time)
        var score = kMatchWeight * match + kRecencyWeight * Math.pow(2, -age / kRecencyHalfLifeMs)
                    + (p.seen ? 0 : kUnseenBoost)
        hits.push({ score: score, time: 0, unseen: false, id: p.id })
    }
    if (ids) {
        for (var i = 0; i < ids.length; ++i)
            if (index.byId.hasOwnProperty(ids[i])) consider(index.byId[ids[i]])
    } else {
        for (var id in index.byId) consider(index.byId[id])
    }
    return ordered(hits, spaced.length === 0)
}
