import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Logos.Controls
import Logos.Theme

import "SignerCrypto.js" as Crypto

// Stub Signer (basecamp-experiments#59): a signer app for testing Keychain.
//
// On first start it makes two Ed25519 key pairs, "Daily key" (messages and
// logins) and "Cold key" (transactions), keeps them in the data the host
// holds for this app alone (basecamp.appdata.read / write; not QML
// LocalStorage, which any other app in the process could open by name) and
// registers their public halves with Keychain. Keychain never sees the
// private keys; it sends the request here, this app shows it in its own words,
// and the user approves or rejects. "Fail next" makes the next approval send
// a wrong signature, so the shell's check can be seen to stop it.
//
// What it calls: basecamp.appdata.read / write, basecamp.keychain.register /
// update / remove / keys / complete / reject. What it answers: keychain.sign (ok as soon as the request
// is on screen) and keychain.ended, from the shell only.
//
// TEST CODE: the keys are made from Math.random (see SignerCrypto.js) and
// stored unencrypted. Nothing secret and no payload is ever logged.
Item {
    id: app
    objectName: "keychain_signer.root"

    readonly property string appName: "keychain_signer"
    readonly property string shellRequester: "main_ui"
    readonly property bool compact: Theme.windowClass(width) === "compact"

    // [{ keyId, name, purposes, seed, publicKey, added }], seed never shown.
    property var keys: []
    // keyId -> true when Keychain lists it with our public key.
    property var registered: ({})
    // Open requests, oldest first: { requestId, keyId, payload, kind,
    // requesterApp, requesterName, description }. The first one is on screen.
    property var queue: []
    readonly property var req: queue.length > 0 ? queue[0] : null
    property bool failNext: false
    property string lastAnswer: "Nothing signed yet."
    property string ended: ""
    property string status: "Starting"

    readonly property var defaultKeys: [
        { keyId: "daily", name: "Daily key", purposes: ["message", "login"] },
        { keyId: "cold", name: "Cold key", purposes: ["transaction"] }
    ]

    function log(msg) { console.log("[" + appName + "] " + msg) }

    // ---- storage ------------------------------------------------------------
    // The host keeps one object per app and gives it to that app only (the
    // caller it attested): { keys: [{ keyId, name, purposes, seed, added }] }.

    function useKeys(stored) {
        keys = stored.map(r => ({ keyId: r.keyId, name: r.name, seed: r.seed, added: r.added,
                                  purposes: r.purposes || [], publicKey: Crypto.publicKey(r.seed) }))
        log("holding " + keys.length + " keys")
    }

    // Then `done`, only once the keys are kept: a key registered and then
    // lost would sit in Keychain with nothing able to sign for it.
    function loadKeys(done) {
        logos.request("basecamp.appdata.read", {}, function (res) {
            const d = res.data || {}
            if (!res.ok || !d.ok) {
                status = "Could not read this app's data: " + (res.ok ? d.error : res.error)
                return
            }
            const stored = (d.data && d.data.keys) || []
            if (stored.length > 0) {
                useKeys(stored)
                done()
                return
            }
            const now = Date.now()
            const made = defaultKeys.map((k, i) => ({ keyId: k.keyId, name: k.name, purposes: k.purposes,
                                                      seed: Crypto.newSeed(), added: now + i }))
            logos.request("basecamp.appdata.write", { data: { keys: made } }, function (res2) {
                const d2 = res2.data || {}
                if (!res2.ok || !d2.ok) {
                    status = "Could not keep new keys: " + (res2.ok ? d2.error : res2.error)
                    return
                }
                log("made " + made.length + " new key pairs")
                useKeys(made)
                done()
            })
        })
    }

    function keyById(keyId) {
        for (const k of keys) if (k.keyId === keyId) return k
        return null
    }

    // ---- Keychain -----------------------------------------------------------

    function registerKey(k, done) {
        const entry = { keyId: k.keyId, name: k.name, publicKey: k.publicKey,
                        algorithm: "ed25519", purposes: k.purposes }
        logos.request("basecamp.keychain.register", entry, function (res) {
            const d = res.data || {}
            if (res.ok && d.error === "duplicate") {
                // Already there: bring the name and purposes up to date.
                logos.request("basecamp.keychain.update",
                              { keyId: k.keyId, name: k.name, purposes: k.purposes }, function (res2) {
                    const d2 = res2.data || {}
                    log("update " + k.keyId + " -> " + (res2.ok ? (d2.ok ? "ok" : d2.error) : res2.error))
                    if (done) done()
                })
                return
            }
            log("register " + k.keyId + " -> " + (res.ok ? (d.ok ? "ok" : d.error) : res.error))
            if (done) done()
        })
    }

    function registerAll() {
        let left = keys.length
        status = "Registering " + left + " keys"
        for (const k of keys)
            registerKey(k, function () { if (--left === 0) refresh() })
    }

    function removeKey(k) {
        logos.request("basecamp.keychain.remove", { keyId: k.keyId }, function (res) {
            const d = res.data || {}
            log("remove " + k.keyId + " -> " + (res.ok ? (d.ok ? "ok" : d.error) : res.error))
            refresh()
        })
    }

    // What Keychain lists for us, matched by keyId and public key.
    function refresh() {
        logos.request("basecamp.keychain.keys", {}, function (res) {
            const listed = (res.data && res.data.keys) || []
            const next = {}
            for (const row of listed) {
                const k = keyById(row.keyId)
                if (k && row.publicKey === k.publicKey) next[row.keyId] = true
            }
            registered = next
            const n = Object.keys(next).length
            status = res.ok ? n + " of " + keys.length + " keys registered with Keychain"
                            : "Keychain did not answer: " + res.error
        })
    }

    // ---- requests -----------------------------------------------------------

    function approve() {
        const r = req
        if (!r) return
        const k = keyById(r.keyId)
        let sig = Crypto.sign(k.seed, r.payload)
        if (failNext) {
            sig = Crypto.spoil(sig)
            failNext = false
        }
        drop(r.requestId)
        logos.request("basecamp.keychain.complete", { requestId: r.requestId, signature: sig }, function (res) {
            const d = res.data || {}
            lastAnswer = "Last request: approved, Keychain says " + (d.ok ? d.status : "refused") + "."
            log("complete " + r.requestId + " -> " + (d.ok ? d.status : "refused"))
        })
    }

    function reject() {
        const r = req
        if (!r) return
        drop(r.requestId)
        logos.request("basecamp.keychain.reject",
                      { requestId: r.requestId, reason: "You said no in Stub Signer" }, function (res) {
            const d = res.data || {}
            lastAnswer = "Last request: rejected" + (d.ok ? "." : ", Keychain refused it.")
            log("reject " + r.requestId + " -> " + (d.ok ? "ok" : "refused"))
        })
    }

    function drop(requestId) {
        queue = queue.filter(r => r.requestId !== requestId)
    }

    Component.onCompleted: loadKeys(registerAll)

    Connections {
        target: logos
        function onIntentRequested(requestId, intent, params, requesterName) {
            if (intent !== "keychain.sign" && intent !== "keychain.ended") return
            if (requesterName !== app.shellRequester) {
                app.log(intent + " refused: only the shell may ask")
                logos.respond(requestId, false, {}, "failed")
                return
            }
            if (intent === "keychain.ended") {
                app.drop(params.requestId)
                app.ended = "Request ended: " + params.status
                app.log("request " + params.requestId + " ended: " + params.status)
                logos.respond(requestId, true, {}, "")
                return
            }
            // keychain.sign: on screen first, then the answer.
            const r = { requestId: String(params.requestId), keyId: String(params.keyId),
                        payload: String(params.payload), kind: String(params.kind),
                        requesterApp: String(params.requesterApp || ""),
                        requesterName: String(params.requesterName),
                        description: String(params.description) }
            app.ended = ""
            app.queue = app.queue.concat([r])
            app.log("request " + r.requestId + " for key " + r.keyId)
            logos.respond(requestId, true, {}, "")
            if (!app.keyById(r.keyId)) {
                app.drop(r.requestId)
                logos.request("basecamp.keychain.reject",
                              { requestId: r.requestId, reason: "Stub Signer no longer has that key" },
                              function () {})
            }
        }
    }

    // ---- UI -------------------------------------------------------------------
    // One column, max 720, centred; buttons full width in compact.

    function payloadBytes(r) {
        const b = r ? Crypto.fromBase64(r.payload) : null
        return b ? b : new Uint8Array(0)
    }
    function who(r) {
        return r.requesterApp && r.requesterApp !== r.requesterName
            ? r.requesterName + " (" + r.requesterApp + ")" : r.requesterName
    }
    function abbrev(b64) { return b64.length > 16 ? b64.slice(0, 8) + "…" + b64.slice(-6) : b64 }

    Rectangle {
        anchors.fill: parent
        color: Theme.palette.background
    }

    Flickable {
        id: flick
        objectName: "keychain_signer.scroll"
        anchors.fill: parent
        contentHeight: column.implicitHeight + 2 * Theme.pageMargin(app.width)
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: LogosScrollBar {}

        ColumnLayout {
            id: column
            x: Math.max(Theme.pageMargin(app.width), (app.width - width) / 2)
            y: Theme.pageMargin(app.width)
            width: Math.min(app.width - 2 * Theme.pageMargin(app.width), 720)
            spacing: Theme.spacing.medium

            LogosText {
                text: "Stub Signer"
                font.pixelSize: 20
                font.weight: Font.DemiBold
            }
            LogosText {
                Layout.fillWidth: true
                text: "Holds two test keys and signs what you approve. Keychain only knows their public halves."
                color: Theme.palette.textSecondary
                font.pixelSize: Theme.typography.secondaryText
                wrapMode: Text.Wrap
            }

            // The request on screen.
            Rectangle {
                objectName: "keychain_signer.request"
                Layout.fillWidth: true
                visible: app.req !== null
                implicitHeight: reqCol.implicitHeight + 2 * Theme.spacing.medium
                radius: Theme.spacing.radiusLarge
                color: Theme.palette.backgroundSecondary
                border.width: 1
                border.color: Theme.palette.primary

                ColumnLayout {
                    id: reqCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Theme.spacing.medium
                    spacing: Theme.spacing.small

                    LogosText {
                        objectName: "keychain_signer.request.title"
                        Layout.fillWidth: true
                        // The name is the app's own claim; the id is the one
                        // the host attested.
                        text: app.req ? app.who(app.req) + " asks you to sign a " + app.req.kind : ""
                        textFormat: Text.PlainText
                        font.weight: Font.DemiBold
                        wrapMode: Text.Wrap
                    }
                    LogosText {
                        objectName: "keychain_signer.request.description"
                        Layout.fillWidth: true
                        text: app.req ? app.who(app.req) + " says: “" + app.req.description + "”" : ""
                        textFormat: Text.PlainText
                        wrapMode: Text.Wrap
                    }
                    LogosText {
                        objectName: "keychain_signer.request.key"
                        Layout.fillWidth: true
                        text: {
                            const k = app.req ? app.keyById(app.req.keyId) : null
                            return "With: " + (k ? k.name : "a key this app does not have")
                        }
                        textFormat: Text.PlainText
                        wrapMode: Text.Wrap
                    }
                    LogosText {
                        objectName: "keychain_signer.request.payload"
                        Layout.fillWidth: true
                        text: {
                            const b = app.payloadBytes(app.req)
                            return "Bytes (" + b.length + "): " + Crypto.hex(b, 64) + (b.length > 64 ? "…" : "")
                        }
                        textFormat: Text.PlainText
                        color: Theme.palette.textSecondary
                        font.family: "monospace"
                        font.pixelSize: Theme.typography.secondaryText
                        wrapMode: Text.WrapAnywhere
                    }
                    LogosText {
                        Layout.fillWidth: true
                        visible: app.queue.length > 1
                        text: (app.queue.length - 1) + " more waiting"
                        color: Theme.palette.textSecondary
                        font.pixelSize: Theme.typography.secondaryText
                    }
                    GridLayout {
                        Layout.fillWidth: true
                        columns: app.compact ? 1 : 2
                        columnSpacing: Theme.spacing.small
                        rowSpacing: Theme.spacing.small
                        LogosButton {
                            objectName: "keychain_signer.approve"
                            Layout.fillWidth: true
                            Layout.minimumHeight: 44
                            text: "Approve"
                            variant: LogosButton.Variant.Primary
                            enabled: app.req !== null && app.keyById(app.req.keyId) !== null
                            onClicked: app.approve()
                        }
                        LogosButton {
                            objectName: "keychain_signer.reject"
                            Layout.fillWidth: true
                            Layout.minimumHeight: 44
                            text: "Reject"
                            onClicked: app.reject()
                        }
                    }
                }
            }

            LogosText {
                objectName: "keychain_signer.idle"
                Layout.fillWidth: true
                visible: app.req === null
                text: "No request to sign."
                color: Theme.palette.textSecondary
            }
            LogosText {
                objectName: "keychain_signer.ended"
                Layout.fillWidth: true
                visible: app.ended !== ""
                text: app.ended
                textFormat: Text.PlainText
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
            }
            LogosText {
                objectName: "keychain_signer.lastAnswer"
                Layout.fillWidth: true
                text: app.lastAnswer
                textFormat: Text.PlainText
                color: Theme.palette.textSecondary
                wrapMode: Text.Wrap
            }

            LogosSwitch {
                objectName: "keychain_signer.failNext"
                Layout.minimumHeight: 44
                text: "Fail next: send a wrong signature"
                checked: app.failNext
                onToggled: app.failNext = checked
            }

            // The keys.
            LogosText {
                Layout.topMargin: Theme.spacing.small
                text: "Keys"
                font.weight: Font.DemiBold
            }
            LogosText {
                objectName: "keychain_signer.status"
                Layout.fillWidth: true
                text: app.status
                textFormat: Text.PlainText
                color: Theme.palette.textSecondary
                font.pixelSize: Theme.typography.secondaryText
                wrapMode: Text.Wrap
            }
            Repeater {
                model: app.keys
                delegate: Rectangle {
                    required property var modelData
                    objectName: "keychain_signer.key." + modelData.keyId
                    Layout.fillWidth: true
                    implicitHeight: keyCol.implicitHeight + 2 * Theme.spacing.medium
                    radius: Theme.spacing.radiusLarge
                    color: Theme.palette.backgroundSecondary

                    ColumnLayout {
                        id: keyCol
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: Theme.spacing.medium
                        spacing: Theme.spacing.small

                        LogosText {
                            Layout.fillWidth: true
                            text: modelData.name
                            textFormat: Text.PlainText
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }
                        LogosText {
                            objectName: "keychain_signer.key." + modelData.keyId + ".state"
                            Layout.fillWidth: true
                            text: (app.registered[modelData.keyId] ? "In Keychain" : "Not in Keychain")
                                  + " · " + (modelData.purposes.length ? modelData.purposes.join(", ") : "any")
                                  + " · " + app.abbrev(modelData.publicKey)
                            textFormat: Text.PlainText
                            color: Theme.palette.textSecondary
                            font.pixelSize: Theme.typography.secondaryText
                            wrapMode: Text.Wrap
                        }
                        GridLayout {
                            Layout.fillWidth: true
                            columns: app.compact ? 1 : 2
                            columnSpacing: Theme.spacing.small
                            rowSpacing: Theme.spacing.small
                            LogosButton {
                                objectName: "keychain_signer.key." + modelData.keyId + ".register"
                                Layout.fillWidth: true
                                Layout.minimumHeight: 44
                                text: "Register again"
                                onClicked: app.registerKey(modelData, app.refresh)
                            }
                            LogosButton {
                                objectName: "keychain_signer.key." + modelData.keyId + ".remove"
                                Layout.fillWidth: true
                                Layout.minimumHeight: 44
                                text: "Remove from Keychain"
                                enabled: !!app.registered[modelData.keyId]
                                onClicked: app.removeKey(modelData)
                            }
                        }
                    }
                }
            }
            LogosButton {
                objectName: "keychain_signer.registerAll"
                Layout.fillWidth: true
                Layout.minimumHeight: 44
                text: "Register both again"
                onClicked: app.registerAll()
            }
        }
    }
}
