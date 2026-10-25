.pragma library

// Ed25519 for the Keychain stubs (basecamp-experiments#59), in plain JS so a
// QML-only app can sign and check signatures.
//
// The master is experiments/keychain-stubs/common/KeychainCrypto.js. sync.sh
// writes each app's copy (SignerCrypto.js, RequesterCrypto.js) with
// common/nacl-fast.js put in place of the marker line below, unchanged:
// TweetNaCl-js 1.0.3, public domain, from the npm tarball fetched through nix
// (source and sha256 in the README). Nothing else in here does any crypto.
//
// No `Qt` object is used, so the same file runs in a bare QJSEngine: the host's
// unit test (tests/keychain_stub_crypto_test.cpp in Basecamp) loads a copy and
// checks its signatures with OpenSSL against RFC 8032.
//
// TEST CODE. The seed comes from Math.random mixed through SHA-512 (QML has no
// crypto.getRandomValues); a real signer takes its randomness from the OS.

// nacl-fast.js ends with `(… : (self.nacl = self.nacl || {}))`: in a browser
// `self` is the window, here it is this object.
var self = {};

// @@TWEETNACL@@

var nacl = self.nacl;

var B64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

function toBase64(bytes) {
    var out = "";
    for (var i = 0; i < bytes.length; i += 3) {
        var a = bytes[i], b = i + 1 < bytes.length ? bytes[i + 1] : 0
        var c = i + 2 < bytes.length ? bytes[i + 2] : 0
        var n = (a << 16) | (b << 8) | c
        out += B64.charAt((n >> 18) & 63) + B64.charAt((n >> 12) & 63)
        out += i + 1 < bytes.length ? B64.charAt((n >> 6) & 63) : "="
        out += i + 2 < bytes.length ? B64.charAt(n & 63) : "="
    }
    return out
}

// Standard base64 with padding; null for anything else.
function fromBase64(text) {
    if (typeof text !== "string" || text.length % 4 !== 0) return null
    var pad = text.endsWith("==") ? 2 : text.endsWith("=") ? 1 : 0
    var bytes = new Uint8Array(text.length / 4 * 3 - pad)
    var j = 0
    for (var i = 0; i < text.length; i += 4) {
        var n = 0
        for (var k = 0; k < 4; ++k) {
            var ch = text.charAt(i + k)
            var v = ch === "=" && i + k >= text.length - pad ? 0 : B64.indexOf(ch)
            if (v < 0) return null
            n = (n << 6) | v
        }
        if (j < bytes.length) bytes[j++] = (n >> 16) & 255
        if (j < bytes.length) bytes[j++] = (n >> 8) & 255
        if (j < bytes.length) bytes[j++] = n & 255
    }
    return bytes
}

// UTF-8 bytes of a string (code points, surrogate pairs joined).
function utf8(text) {
    var out = []
    for (var i = 0; i < text.length; ++i) {
        var c = text.charCodeAt(i)
        if (c >= 0xd800 && c < 0xdc00 && i + 1 < text.length) {
            var d = text.charCodeAt(i + 1)
            if (d >= 0xdc00 && d < 0xe000) { c = 0x10000 + ((c - 0xd800) << 10) + (d - 0xdc00); ++i }
        }
        if (c < 0x80) out.push(c)
        else if (c < 0x800) out.push(0xc0 | (c >> 6), 0x80 | (c & 63))
        else if (c < 0x10000) out.push(0xe0 | (c >> 12), 0x80 | ((c >> 6) & 63), 0x80 | (c & 63))
        else out.push(0xf0 | (c >> 18), 0x80 | ((c >> 12) & 63), 0x80 | ((c >> 6) & 63), 0x80 | (c & 63))
    }
    return new Uint8Array(out)
}

function hex(bytes, max) {
    var n = Math.min(bytes.length, max === undefined ? bytes.length : max)
    var out = ""
    for (var i = 0; i < n; ++i) out += ("0" + bytes[i].toString(16)).slice(-2)
    return out
}

// 32 bytes for a new key: Math.random and the clock, through SHA-512.
function newSeed() {
    var noise = new Uint8Array(128)
    for (var i = 0; i < noise.length; ++i) noise[i] = Math.floor(Math.random() * 256)
    var t = Date.now()
    for (var k = 0; k < 8; ++k) { noise[k] ^= t & 255; t = Math.floor(t / 256) }
    return toBase64(nacl.hash(noise).subarray(0, 32))
}

// The base64 public key of the key pair made from a base64 32-byte seed.
function publicKey(seedB64) {
    var seed = fromBase64(seedB64)
    if (!seed || seed.length !== 32) return ""
    return toBase64(nacl.sign.keyPair.fromSeed(seed).publicKey)
}

// The base64 Ed25519 signature of the base64 payload; "" when either is bad.
function sign(seedB64, payloadB64) {
    var seed = fromBase64(seedB64)
    var msg = fromBase64(payloadB64)
    if (!seed || seed.length !== 32 || !msg) return ""
    return toBase64(nacl.sign.detached(msg, nacl.sign.keyPair.fromSeed(seed).secretKey))
}

function verify(publicKeyB64, payloadB64, signatureB64) {
    var key = fromBase64(publicKeyB64), msg = fromBase64(payloadB64)
    var sig = fromBase64(signatureB64)
    if (!key || key.length !== 32 || !msg || !sig || sig.length !== 64) return false
    return nacl.sign.detached.verify(msg, sig, key)
}

// The same signature with one bit flipped: what "Fail next" sends.
function spoil(signatureB64) {
    var sig = fromBase64(signatureB64)
    if (!sig || sig.length === 0) return signatureB64
    sig[sig.length - 1] ^= 1
    return toBase64(sig)
}
