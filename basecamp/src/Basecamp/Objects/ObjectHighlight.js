.pragma library

// What the search matched, marked in a result (basecamp-experiments#46): the
// start of each word that a query word begins, the same case-insensitive
// word-prefix rule the rankers use (app/ObjectRanker, BasecampSearchRanker.js,
// app/AppSearch), so what is marked is why it matched.
//
// The result is StyledText for a Text whose textFormat is Text.StyledText.
// Publisher text is plain (ADR-072): every character of it is escaped before
// the one tag this adds, so nothing an app sent is read as markup. Only the
// colour changes, never the weight, so marking moves nothing.
//
// Pure functions, unit-tested outside QML (tests/object_cards_test.cpp).
// (A file name no other bundled app uses: two ui_qml apps shipping a file of
// the same name collide, spec §11.)

// Whitespace and the ASCII punctuation ranges !-/ :-@ [-` {-~, as the rankers
// split words.
var kSeparators = /[\s!-\/:-@\[-`{-~]+/
var kColor = /^#[0-9a-fA-F]{6}([0-9a-fA-F]{2})?$/

// The query's words, lower case.
function tokens(query) {
    if (typeof query !== "string" || query.length === 0) return []
    return query.toLowerCase().split(kSeparators).filter(function (t) { return t.length > 0 })
}

// [start, end) of each marked part of `text`, in order: for every word, its
// first characters as long as the longest query word it starts with.
function ranges(text, query) {
    var out = []
    if (typeof text !== "string" || text.length === 0) return out
    var toks = tokens(query)
    if (toks.length === 0) return out
    var word = /[^\s!-\/:-@\[-`{-~]+/g
    var m
    while ((m = word.exec(text)) !== null) {
        var lower = m[0].toLowerCase()
        var best = 0
        for (var i = 0; i < toks.length; ++i)
            if (toks[i].length > best && lower.indexOf(toks[i]) === 0) best = toks[i].length
        if (best > 0) out.push([m.index, m.index + Math.min(best, m[0].length)])
    }
    return out
}

// `text` as StyledText that reads exactly as the plain text would: & < > "
// as entities, line breaks as <br>, and every space after the first in a run
// as &nbsp; (StyledText folds a run of spaces into one).
function escape(text) {
    if (typeof text !== "string") return ""
    var out = ""
    var prevSpace = false
    for (var i = 0; i < text.length; ++i) {
        var c = text.charAt(i)
        if (c === "\r" || c === "\n") {
            if (c === "\r" && text.charAt(i + 1) === "\n") ++i
            out += "<br>"
            prevSpace = false
            continue
        }
        if (/\s/.test(c)) {
            out += prevSpace ? "&nbsp;" : " "
            prevSpace = true
            continue
        }
        prevSpace = false
        if (c === "&") out += "&amp;"
        else if (c === "<") out += "&lt;"
        else if (c === ">") out += "&gt;"
        else if (c === "\"") out += "&quot;"
        else out += c
    }
    return out
}

// `text` escaped, with what `query` matches in `color` ("#rrggbb"; a colour
// from QML, Theme.palette.accentOrange, converts to that). Without a query,
// a match or a valid colour, just the escaped text.
function mark(text, query, color) {
    if (typeof text !== "string" || text.length === 0) return ""
    var c = String(color)
    var found = kColor.test(c) ? ranges(text, query) : []
    if (found.length === 0) return escape(text)
    var out = ""
    var at = 0
    for (var i = 0; i < found.length; ++i) {
        // A match never contains a space, so escaping piecewise keeps runs whole.
        out += escape(text.slice(at, found[i][0]))
        out += "<font color=\"" + c + "\">" + escape(text.slice(found[i][0], found[i][1])) + "</font>"
        at = found[i][1]
    }
    return out + escape(text.slice(at))
}
