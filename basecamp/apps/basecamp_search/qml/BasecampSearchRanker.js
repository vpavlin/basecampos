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
    function byId(a, b) { return a.id < b.id ? -1 : (a.id > b.id ? 1 : 0) }
    if (queryTokens.length === 0) {
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
