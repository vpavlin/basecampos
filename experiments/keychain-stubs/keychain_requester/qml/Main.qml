import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Logos.Controls
import Logos.Theme

import "RequesterCrypto.js" as Crypto

// Stub Requester (basecamp-experiments#59): an app that asks Keychain for
// signatures, for testing.
//
// Three buttons send a fixed payload and description each. Keychain opens,
// the user picks a key, the signer app approves or rejects, and the answer to
// basecamp.keychain.sign arrives here as `data`:
//   { requestId, status, signature?, publicKey?, keyId?, keyName?, signerApp?, error? }
// or at once { error: busy | bad_request | unavailable | not_in_front }. The
// app shows the last result and checks the signature itself against the
// public key it got.
// "Send two at once" asks twice; the second answer should be `busy`.
//
// The payloads start with STUB-PAYLOAD so a test can grep the run log for
// them: nothing here logs a payload or a description, and neither should the
// shell.
Item {
    id: app
    objectName: "keychain_requester.root"

    readonly property string appName: "keychain_requester"
    readonly property bool compact: Theme.windowClass(width) === "compact"

    readonly property var asks: ({
        message: { kind: "message", text: "STUB-PAYLOAD note: meet at the north gate at 9",
                   description: "Sign a note for Ada" },
        login: { kind: "login", text: "STUB-PAYLOAD login challenge 5f2c9a for weekend-hike",
                 description: "Sign in to Weekend hike" },
        transaction: { kind: "transaction", text: "STUB-PAYLOAD transfer 5 TST to Ada",
                       description: "Send 5 TST to Ada" }
    })

    // { kind, status, error, keyId, keyName, signerApp, signature, verified }
    property var last: null
    property string second: ""
    property int waiting: 0

    function log(msg) { console.log("[" + appName + "] " + msg) }

    // `into`: "last" or "second" (the second of Send two at once).
    function ask(which, into) {
        const a = asks[which]
        const payload = Crypto.toBase64(Crypto.utf8(a.text))
        waiting++
        if (into !== "second") last = null
        log("asking for a " + a.kind + " signature")
        logos.request("basecamp.keychain.sign", { payload: payload, kind: a.kind, description: a.description },
                      function (res) {
            waiting--
            const d = res.data || {}
            const r = { kind: a.kind, status: d.status || "refused", error: d.error || (res.ok ? "" : res.error),
                        keyId: d.keyId || "", keyName: d.keyName || "", signerApp: d.signerApp || "",
                        signature: d.signature || "", verified: false }
            if (r.signature && d.publicKey)
                r.verified = Crypto.verify(d.publicKey, payload, r.signature)
            log(a.kind + ": " + r.status + (r.error ? " (" + r.error + ")" : "")
                + (r.signature ? ", verifies here: " + r.verified : ""))
            if (into === "second") second = r.status + (r.error ? ": " + r.error : "")
            else last = r
        })
    }

    function cancelMine() {
        logos.request("basecamp.keychain.cancel", {}, function (res) {
            const d = res.data || {}
            log("cancel -> " + (d.ok ? "ok" : "nothing to cancel"))
        })
    }

    function abbrev(s) { return s.length > 16 ? s.slice(0, 10) + "…" + s.slice(-6) : s }

    // ---- UI -------------------------------------------------------------------

    Rectangle {
        anchors.fill: parent
        color: Theme.palette.background
    }

    Flickable {
        objectName: "keychain_requester.scroll"
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
                text: "Stub Requester"
                font.pixelSize: 20
                font.weight: Font.DemiBold
            }
            LogosText {
                Layout.fillWidth: true
                text: "Asks Keychain to sign. You pick the key there, and the app that holds it asks you to approve."
                color: Theme.palette.textSecondary
                font.pixelSize: Theme.typography.secondaryText
                wrapMode: Text.Wrap
            }

            GridLayout {
                Layout.fillWidth: true
                columns: app.compact ? 1 : 3
                columnSpacing: Theme.spacing.small
                rowSpacing: Theme.spacing.small
                LogosButton {
                    objectName: "keychain_requester.message"
                    Layout.fillWidth: true
                    Layout.minimumHeight: 44
                    text: "Sign a message"
                    onClicked: app.ask("message")
                }
                LogosButton {
                    objectName: "keychain_requester.login"
                    Layout.fillWidth: true
                    Layout.minimumHeight: 44
                    text: "Sign a login"
                    onClicked: app.ask("login")
                }
                LogosButton {
                    objectName: "keychain_requester.transaction"
                    Layout.fillWidth: true
                    Layout.minimumHeight: 44
                    text: "Sign a transaction"
                    onClicked: app.ask("transaction")
                }
            }
            GridLayout {
                Layout.fillWidth: true
                columns: app.compact ? 1 : 2
                columnSpacing: Theme.spacing.small
                rowSpacing: Theme.spacing.small
                LogosButton {
                    objectName: "keychain_requester.two"
                    Layout.fillWidth: true
                    Layout.minimumHeight: 44
                    text: "Send two at once"
                    onClicked: { app.second = "waiting"; app.ask("login"); app.ask("message", "second") }
                }
                LogosButton {
                    objectName: "keychain_requester.cancel"
                    Layout.fillWidth: true
                    Layout.minimumHeight: 44
                    text: "Cancel my request"
                    enabled: app.waiting > 0
                    onClicked: app.cancelMine()
                }
            }

            // The last result.
            Rectangle {
                objectName: "keychain_requester.result"
                Layout.fillWidth: true
                implicitHeight: resCol.implicitHeight + 2 * Theme.spacing.medium
                radius: Theme.spacing.radiusLarge
                color: Theme.palette.backgroundSecondary

                ColumnLayout {
                    id: resCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Theme.spacing.medium
                    spacing: Theme.spacing.small

                    LogosText {
                        objectName: "keychain_requester.result.status"
                        Layout.fillWidth: true
                        text: app.waiting > 0 && !app.last ? "Waiting for Keychain"
                            : !app.last ? "Nothing asked yet"
                            : "Last " + app.last.kind + ": " + app.last.status
                              + (app.last.error ? " (" + app.last.error + ")" : "")
                        textFormat: Text.PlainText
                        font.weight: Font.DemiBold
                        wrapMode: Text.Wrap
                    }
                    LogosText {
                        objectName: "keychain_requester.result.key"
                        Layout.fillWidth: true
                        visible: !!app.last && app.last.keyId !== ""
                        text: app.last ? "Key: " + app.last.keyName + " (" + app.last.keyId + ") of "
                                         + app.last.signerApp : ""
                        textFormat: Text.PlainText
                        wrapMode: Text.Wrap
                    }
                    LogosText {
                        objectName: "keychain_requester.result.signature"
                        Layout.fillWidth: true
                        visible: !!app.last && app.last.signature !== ""
                        text: app.last ? "Signature: " + app.abbrev(app.last.signature) : ""
                        textFormat: Text.PlainText
                        font.family: "monospace"
                        color: Theme.palette.textSecondary
                        wrapMode: Text.WrapAnywhere
                    }
                    LogosText {
                        objectName: "keychain_requester.result.verified"
                        Layout.fillWidth: true
                        visible: !!app.last && app.last.signature !== ""
                        text: app.last && app.last.verified ? "Verifies here: yes" : "Verifies here: NO"
                        textFormat: Text.PlainText
                        font.weight: Font.DemiBold
                    }
                    LogosText {
                        objectName: "keychain_requester.second"
                        Layout.fillWidth: true
                        visible: app.second !== ""
                        text: "Second request: " + app.second
                        textFormat: Text.PlainText
                        color: Theme.palette.textSecondary
                        wrapMode: Text.Wrap
                    }
                }
            }
        }
    }
}
