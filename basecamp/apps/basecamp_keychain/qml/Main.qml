import QtQuick

import Logos.Controls
import Logos.Theme

// Basecamp's default `keychain` role app (basecamp-experiments#57): the keys
// view, and the request view the shell opens it for when an app asks for a
// signature. A directory of public keys only: the private keys stay in the
// signer apps, and a signature is made there (#58).
//
// Everything goes through `shell` (ShellApi), which the host injects while
// this app claims or holds the role and re-checks per call: keychainKeys,
// setKeyLabel / setKeyNote / removeKey, keychainRequests / keychainRequest,
// chooseKey, cancelSignatureRequest. Opened without the role it says so.
Rectangle {
    id: root
    objectName: "basecampKeychain.root"
    color: Theme.palette.background

    readonly property bool granted: typeof shell !== "undefined" && shell !== null
                                    && shell.roles.keychain === "basecamp_keychain"

    // The oldest open request is the one on screen; the next one follows when
    // it ends (the broker allows one per app, so several apps can be waiting).
    readonly property string requestId: granted && shell.keychainRequests.length > 0
                                        ? shell.keychainRequests[0] : ""
    property var request: ({})
    // Clock for "Added 3 min ago", refreshed while the app is open.
    property double now: Date.now()

    function refreshRequest() {
        request = requestId !== "" ? shell.keychainRequest(requestId) : ({})
    }
    onRequestIdChanged: refreshRequest()

    Connections {
        target: root.granted ? shell : null
        function onKeychainRequestsChanged() { root.refreshRequest() }
        // The keys that can sign it may have changed under an open request.
        function onKeychainChanged() { root.refreshRequest() }
    }

    Timer {
        interval: 30000
        repeat: true
        running: root.visible
        onTriggered: root.now = Date.now()
    }

    KeychainKeysView {
        id: keysView
        anchors.fill: parent
        visible: root.granted
        keys: root.granted ? shell.keychainKeys : []
        now: root.now
        // Behind a request, it keeps still: nothing in it can be reached.
        enabled: root.requestId === ""
        saveKey: function (ownerApp, keyId, label, note) {
            const okLabel = shell.setKeyLabel(ownerApp, keyId, label)
            const okNote = shell.setKeyNote(ownerApp, keyId, note)
            return okLabel && okNote
        }
        removeKey: (ownerApp, keyId) => shell.removeKey(ownerApp, keyId)
    }

    KeychainRequestSheet {
        anchors.fill: parent
        visible: root.granted && root.requestId !== "" && root.request.requestId === root.requestId
        request: root.request
        onContinueRequested: function (ownerApp, keyId) {
            if (!shell.chooseKey(root.requestId, ownerApp, keyId)) root.refreshRequest()
        }
        onCancelRequested: shell.cancelSignatureRequest(root.requestId)
        onOpenSignerRequested: shell.openSigner(root.requestId)
    }

    Column {
        objectName: "basecampKeychain.notGranted"
        visible: !root.granted
        anchors.centerIn: parent
        width: Math.min(parent.width - 2 * Theme.spacing.large, 420)
        spacing: Theme.spacing.small
        LogosText {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Another app is set as Keychain")
            font.pixelSize: 18
            font.weight: Font.DemiBold
        }
        LogosText {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            color: Theme.palette.textSecondary
            text: qsTr("Basecamp's Keychain shows your keys and signing requests only while it is the default Keychain app. Choose it again in Settings under Default apps.")
        }
    }
}
