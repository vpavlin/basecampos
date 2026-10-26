.pragma library

// What the Keychain app shows, as pure functions (basecamp-experiments#57).
// No Qt, no shell: tests/keychain_app_test.cpp runs this file in a QJSEngine,
// the way object_cards_test runs home's.
//
// Rows are ShellApi.keychainKeys / keychainRequest(id).keys rows:
// { ownerApp, ownerName, ownerIcon, ownerFullBleed, keyId, name, label, note,
//   publicKey, fingerprint, algorithm, purposes, added }.

var maxLabel = 40;
var maxNote = 500;

// The user's label is the key's name when set; the app's own name otherwise.
function title(key) {
    return key && key.label ? key.label : (key && key.name) || "";
}

// The app's own name under the label, only when a label hides it.
function subtitle(key) {
    return key && key.label ? key.name || "" : "";
}

// Grouped by owner, apps and keys in the order the rows came (the registry's
// registration order): [{ ownerApp, ownerName, ownerIcon, ownerFullBleed, keys }].
function groups(rows) {
    var out = [];
    var at = {};
    for (var i = 0; i < (rows || []).length; ++i) {
        var k = rows[i];
        if (!at.hasOwnProperty(k.ownerApp)) {
            at[k.ownerApp] = out.length;
            out.push({ ownerApp: k.ownerApp, ownerName: k.ownerName || k.ownerApp,
                       ownerIcon: k.ownerIcon || "", ownerFullBleed: k.ownerFullBleed === true,
                       keys: [] });
        }
        out[at[k.ownerApp]].keys.push(k);
    }
    return out;
}

function algorithmName(algorithm) {
    if (algorithm === "ed25519") return "Ed25519";
    return algorithm || "";
}

function capitalised(word) {
    return word ? word.charAt(0).toUpperCase() + word.slice(1) : "";
}

// "Login, Message"; a key that names none signs for anything.
function purposesText(purposes, anyText) {
    if (!purposes || purposes.length === 0) return anyText;
    var out = [];
    for (var i = 0; i < purposes.length; ++i) out.push(capitalised(purposes[i]));
    return out.join(", ");
}

// "Ed25519 · ABCDEF…UVWXYZ · Login, Message"
function details(key, anyText) {
    var parts = [algorithmName(key.algorithm)];
    if (key.fingerprint) parts.push(key.fingerprint);
    parts.push(purposesText(key.purposes, anyText));
    return parts.join(" · ");
}

// How long ago, in whole units: { unit: "now" | "min" | "h" | "d" | "date", n, date }.
// The caller words it (qsTr); `date` is YYYY-MM-DD for anything a week old.
function age(addedMs, nowMs) {
    var s = Math.max(0, Math.floor((nowMs - addedMs) / 1000));
    if (s < 60) return { unit: "now", n: 0 };
    if (s < 3600) return { unit: "min", n: Math.floor(s / 60) };
    if (s < 86400) return { unit: "h", n: Math.floor(s / 3600) };
    if (s < 7 * 86400) return { unit: "d", n: Math.floor(s / 86400) };
    return { unit: "date", n: 0, date: new Date(addedMs).toISOString().slice(0, 10) };
}

function sameKey(a, b) {
    return !!a && !!b && a.ownerApp === b.ownerApp && a.keyId === b.keyId;
}

// Which key a request starts on: the one this requester used last, when it
// can sign this request, else the first. -1 with none.
function preselect(keys, lastKey) {
    if (!keys || keys.length === 0) return -1;
    for (var i = 0; i < keys.length; ++i)
        if (sameKey(keys[i], lastKey)) return i;
    return 0;
}

// The user's words are kept whole or not at all (Keychain::setKeyLabel).
function fits(text, max) {
    return (text || "").length <= max;
}

function oneLine(text) {
    return (text || "").replace(/\s+/g, " ").trim();
}
