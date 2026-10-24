.pragma library

// Plain-text formatting for the object cards (SHELL-OBJECTS-SPEC §3, §4):
// type names, relative times, amounts, statuses. Pure functions over an
// envelope's values, so they are unit-tested outside QML
// (tests/object_cards_test.cpp). Everything an app sent is untrusted: a value
// of the wrong kind formats as "", never as "[object Object]".

var kMinute = 60 * 1000
var kHour = 60 * kMinute
var kDay = 24 * kHour

// The v0 type list (§4): [singular, plural].
var kTypeNames = {
    "account":      ["Account", "Accounts"],
    "asset":        ["Asset", "Assets"],
    "transaction":  ["Transaction", "Transactions"],
    "message":      ["Message", "Messages"],
    "post":         ["Post", "Posts"],
    "community":    ["Community", "Communities"],
    "contact":      ["Contact", "Contacts"],
    "news":         ["News", "News"],
    "notification": ["Notification", "Notifications"]
}

var kWeekdays = ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"]
var kMonths = ["Jan", "Feb", "Mar", "Apr", "May", "Jun",
               "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"]

// A string or a finite number as text; anything else (a ref object, a list,
// null) as "".
function str(v) {
    if (typeof v === "string") return v
    if (typeof v === "number" && isFinite(v)) return String(v)
    return ""
}

// A list as a JS array. A list that came through C++ (an envelope from
// ObjectResults.get(), a model's modelData) is a sequence object that
// Array.isArray() says no to; anything that is not a list is [].
function arr(v) {
    if (Array.isArray(v)) return v
    if (v !== null && typeof v === "object" && typeof v.length === "number")
        return Array.prototype.slice.call(v)
    return []
}

function isKnownType(type) {
    return Object.prototype.hasOwnProperty.call(kTypeNames, type)
}

// "Transactions" for "transaction"; an app's own type `x.<app>.<name>` reads
// as its last part ("conversation" -> "Conversations").
function typeLabel(type, plural) {
    type = str(type)
    if (isKnownType(type)) return kTypeNames[type][plural ? 1 : 0]
    var name = type.indexOf("x.") === 0 ? type.slice(type.lastIndexOf(".") + 1) : type
    name = name.replace(/[_-]+/g, " ").trim()
    if (name.length === 0) return plural ? "Other" : "Item"
    name = name.charAt(0).toUpperCase() + name.slice(1)
    if (plural && name.charAt(name.length - 1) !== "s") name += "s"
    return name
}

function pad2(n) { return (n < 10 ? "0" : "") + n }

// 24-hour "14:48".
function clock(t) {
    if (typeof t !== "number" || !isFinite(t)) return ""
    var d = new Date(t)
    return pad2(d.getHours()) + ":" + pad2(d.getMinutes())
}

// "now", "5 min", "3 h", "Yesterday", "Mon", "12 Oct", "12 Oct 2025".
function relativeTime(t, now) {
    if (typeof t !== "number" || !isFinite(t)) return ""
    var diff = now - t
    if (diff < kMinute) return "now"            // also a time slightly ahead of ours
    if (diff < kHour) return Math.floor(diff / kMinute) + " min"
    if (diff < kDay) return Math.floor(diff / kHour) + " h"
    var d = new Date(t)
    var n = new Date(now)
    var startOfToday = new Date(n.getFullYear(), n.getMonth(), n.getDate()).getTime()
    if (t >= startOfToday - kDay) return "Yesterday"
    if (diff < 6 * kDay) return kWeekdays[d.getDay()]
    var s = d.getDate() + " " + kMonths[d.getMonth()]
    return d.getFullYear() === n.getFullYear() ? s : s + " " + d.getFullYear()
}

// 1240 -> "1,240".
function grouped(n) {
    if (typeof n !== "number" || !isFinite(n)) return str(n)
    var neg = n < 0
    var s = String(Math.floor(Math.abs(n)))
    var out = ""
    while (s.length > 3) {
        out = "," + s.slice(-3) + out
        s = s.slice(0, -3)
    }
    return (neg ? "-" : "") + s + out
}

// "12.00 TST", or "" without an amount.
function amount(value, unit) {
    var a = str(value)
    if (a.length === 0) return ""
    var u = str(unit)
    return u.length > 0 ? a + " " + u : a
}

// A transaction's amount with its direction: "+40.00 TST", "−12.00 TST".
function signedAmount(fields) {
    fields = fields || {}
    var a = amount(fields.amount, fields.unit)
    if (a.length === 0) return ""
    var dir = str(fields.direction)
    return (dir === "in" ? "+" : dir === "out" ? "−" : "") + a
}

var kStatusLabels = {
    "pending": "Pending", "sending": "Sending", "receiving": "Receiving",
    "done": "Done", "failed": "Failed", "scheduled": "Scheduled"
}

function statusLabel(status) {
    status = str(status)
    return kStatusLabels.hasOwnProperty(status) ? kStatusLabels[status] : ""
}

// Which colour a status reads in: "success", "error", "warning", "info" or "".
function statusTone(status) {
    switch (str(status)) {
    case "done": return "success"
    case "failed": return "error"
    case "pending": case "sending": case "receiving": return "warning"
    case "scheduled": return "info"
    }
    return ""
}

// An asset's change in percent: "+1.8%", "−2.4%", "0.0%"; "" if not a number.
function change(pct) {
    if (typeof pct !== "number" || !isFinite(pct)) return ""
    var s = Math.abs(pct).toFixed(1) + "%"
    return pct > 0 ? "+" + s : pct < 0 ? "−" + s : s
}

function members(n) {
    if (typeof n !== "number" || !isFinite(n)) return ""
    return grouped(n) + (n === 1 ? " member" : " members")
}

// A list of strings ("friends, work"); other entries are skipped.
function list(v) {
    if (typeof v !== "object" || v === null) return str(v)
    v = arr(v)
    var out = []
    for (var i = 0; i < v.length; ++i) {
        var s = str(v[i])
        if (s.length > 0) out.push(s)
    }
    return out.join(", ")
}

// The object's actions the card can draw: at most 3 (§3), each with a string
// id and label; `input` kept only when it is "text".
function actions(envelope) {
    var a = envelope ? arr(envelope.actions) : []
    var out = []
    for (var i = 0; i < a.length && out.length < 3; ++i) {
        var id = str(a[i] && a[i].id)
        if (id.length === 0 || id === "open") continue
        var label = str(a[i].label)
        out.push({ id: id, label: label.length > 0 ? label : id,
                   input: a[i].input === "text" ? "text" : "" })
    }
    return out
}

// Whether the app already offers its own "View" among `actions` (the result
// of actions()): the card then leaves out its own, so there are not two.
function offersView(actions) {
    for (var i = 0; i < actions.length; ++i)
        if (actions[i].id === "view" || actions[i].label.toLowerCase() === "view") return true
    return false
}

// Which card draws `type`: the type itself when it has one, else "generic".
function cardFor(type) {
    return isKnownType(str(type)) ? str(type) : "generic"
}

// The time the card shows and the feed sorts by: `time`, else `updated`.
function timeOf(envelope) {
    if (!envelope) return 0
    if (typeof envelope.time === "number" && isFinite(envelope.time)) return envelope.time
    if (typeof envelope.updated === "number" && isFinite(envelope.updated)) return envelope.updated
    return 0
}
