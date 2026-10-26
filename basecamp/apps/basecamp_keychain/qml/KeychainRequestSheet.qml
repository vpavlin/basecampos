import QtQuick

import Logos.Controls
import Logos.Theme
import Basecamp.Icons

import "KeychainFormat.js" as F

// The request view (basecamp-experiments#57): an app asks for a signature.
// Who is asking, what it says (marked as its own words: Keychain cannot
// check them), the kind, and the keys that can sign it, the one this app used
// last preselected. Continue hands the request to that key's app, which shows
// it in its own words and signs (#58); Cancel ends it as cancelled.
//
// `request` is ShellApi.keychainRequest(id): { requestId, state, requesterApp,
// requesterName, requesterIcon, requesterFullBleed, description, kind, keys,
// lastKey, waiting }. A phone gets it full screen, anything wider a centred
// sheet.
//
// Every app is named twice: by its display name, which is its own claim (the
// host shows the module id instead when the name is Keychain's, Basecamp's or
// another app's), and by the module id the host attested, which no app picks.
KeychainSheet {
    id: sheet
    objectName: "basecampKeychain.request"

    property var request: ({})

    signal continueRequested(string ownerApp, string keyId)
    signal cancelRequested()
    signal openSignerRequested()

    readonly property var keys: request.keys || []
    readonly property bool waiting: request.state === "keyChosen" || request.state === "signing"
    property int selected: -1
    // The chosen key's app while it signs.
    readonly property string signerName: request.signerName || ""
    readonly property string requesterApp: request.requesterApp || ""
    readonly property string requesterName: request.requesterName || requesterApp
    // Other apps' requests behind this one.
    readonly property int waitingCount: request.waiting || 0

    fullScreenWhenCompact: true
    maxWidth: 520

    function kindText(kind) {
        if (kind === "message") return qsTr("Message")
        if (kind === "transaction") return qsTr("Transaction")
        if (kind === "login") return qsTr("Login")
        return kind || ""
    }

    // A new request starts on its preselected key; the same one refreshed
    // (keys changed) keeps the user's pick when it is still there.
    property string _for: ""
    // Read from `request` itself: the `keys` binding may not have caught up
    // when this runs.
    onRequestChanged: {
        const list = request.keys || []
        if (request.requestId !== _for) {
            _for = request.requestId || ""
            selected = F.preselect(list, request.lastKey)
            return
        }
        if (selected >= list.length) selected = F.preselect(list, request.lastKey)
    }

    // ── Who is asking ───────────────────────────────────────────────────
    LogosText {
        width: parent.width
        text: qsTr("Signature request")
        font.pixelSize: Theme.typography.panelTitleText
        font.weight: Theme.typography.weightBold
    }

    Row {
        width: parent.width
        height: Math.max(48, who.implicitHeight)
        spacing: Theme.spacing.medium
        BasecampAppTile {
            anchors.verticalCenter: parent.verticalCenter
            size: 40
            label: sheet.requesterName
            source: sheet.request.requesterIcon || ""
            fullBleed: sheet.request.requesterFullBleed === true
        }
        Column {
            id: who
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - 40 - Theme.spacing.medium
            LogosText {
                objectName: "basecampKeychain.request.requester"
                width: parent.width
                elide: Text.ElideRight
                textFormat: Text.PlainText
                text: sheet.requesterName
                font.weight: Theme.typography.weightMedium
            }
            LogosText {
                objectName: "basecampKeychain.request.requesterApp"
                visible: sheet.requesterApp !== "" && sheet.requesterApp !== sheet.requesterName
                width: parent.width
                elide: Text.ElideMiddle
                textFormat: Text.PlainText
                color: Theme.palette.textSecondary
                font.pixelSize: Theme.typography.secondaryText
                text: qsTr("App id: %1").arg(sheet.requesterApp)
            }
            LogosText {
                width: parent.width
                elide: Text.ElideRight
                color: Theme.palette.textSecondary
                font.pixelSize: Theme.typography.secondaryText
                text: qsTr("asks for a signature: %1").arg(sheet.kindText(sheet.request.kind))
            }
        }
    }

    // ── What they say, as theirs ────────────────────────────────────────
    Rectangle {
        width: parent.width
        height: claim.implicitHeight + 2 * Theme.spacing.medium
        radius: Theme.spacing.radiusLarge
        color: "transparent"
        border.width: 1
        border.color: Theme.palette.border
        Column {
            id: claim
            x: Theme.spacing.medium
            y: Theme.spacing.medium
            width: parent.width - 2 * Theme.spacing.medium
            spacing: Theme.spacing.tiny
            LogosText {
                width: parent.width
                wrapMode: Text.Wrap
                color: Theme.palette.textSecondary
                font.pixelSize: Theme.typography.secondaryText
                textFormat: Text.PlainText
                text: qsTr("%1 says:").arg(sheet.requesterApp && sheet.requesterApp !== sheet.requesterName
                                           ? qsTr("%1 (%2)").arg(sheet.requesterName).arg(sheet.requesterApp)
                                           : sheet.requesterName)
            }
            LogosText {
                objectName: "basecampKeychain.request.claim"
                width: parent.width
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
                text: "“" + (sheet.request.description || "") + "”"
            }
            LogosText {
                width: parent.width
                wrapMode: Text.Wrap
                color: Theme.palette.textTertiary
                font.pixelSize: Theme.typography.secondaryText
                text: qsTr("In the app's own words. Keychain cannot check them; the app that signs shows you what is really signed.")
            }
        }
    }

    // ── Waiting for the signer ──────────────────────────────────────────
    Column {
        objectName: "basecampKeychain.request.waiting"
        visible: sheet.waiting
        width: parent.width
        spacing: Theme.spacing.small
        LogosText {
            width: parent.width
            wrapMode: Text.Wrap
            font.weight: Theme.typography.weightMedium
            text: qsTr("Waiting for %1").arg(sheet.signerName)
        }
        LogosText {
            width: parent.width
            wrapMode: Text.Wrap
            color: Theme.palette.textSecondary
            text: qsTr("Approve or reject the request in %1. It shows you what will be signed.").arg(sheet.signerName)
        }
    }

    LogosText {
        objectName: "basecampKeychain.request.queued"
        visible: sheet.waitingCount > 0
        width: parent.width
        wrapMode: Text.Wrap
        color: Theme.palette.textSecondary
        font.pixelSize: Theme.typography.secondaryText
        text: sheet.waitingCount === 1
              ? qsTr("1 more request is waiting after this one.")
              : qsTr("%1 more requests are waiting after this one.").arg(sheet.waitingCount)
    }

    // ── Which key ───────────────────────────────────────────────────────
    LogosText {
        visible: !sheet.waiting && sheet.keys.length > 0
        width: parent.width
        text: qsTr("Sign with")
        font.weight: Theme.typography.weightMedium
    }

    LogosText {
        objectName: "basecampKeychain.request.noKey"
        visible: !sheet.waiting && sheet.keys.length === 0
        width: parent.width
        wrapMode: Text.Wrap
        color: Theme.palette.textSecondary
        text: qsTr("No key here can sign this. Keys come from signer apps: an app with an Ed25519 key for %1 signing has to add it first. Cancel, and %2 is told it was not signed.")
              .arg(sheet.kindText(sheet.request.kind).toLowerCase()).arg(sheet.request.requesterName || "")
    }

    Repeater {
        model: sheet.waiting ? [] : sheet.keys

        delegate: Rectangle {
            id: keyRow
            required property var modelData
            required property int index
            readonly property bool chosen: sheet.selected === index
            objectName: "basecampKeychain.request.key." + modelData.ownerApp + "." + modelData.keyId
            width: parent ? parent.width : 0
            height: Math.max(56, keyText.implicitHeight + 2 * Theme.spacing.small)
            radius: Theme.spacing.radiusLarge
            color: chosen ? Theme.palette.backgroundTertiary : "transparent"
            border.width: chosen ? 2 : 1
            border.color: chosen ? Theme.palette.accentOrange : Theme.palette.border

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: sheet.selected = keyRow.index
            }

            BasecampAppTile {
                id: keyTile
                anchors.left: parent.left
                anchors.leftMargin: Theme.spacing.small
                anchors.verticalCenter: parent.verticalCenter
                size: 32
                label: keyRow.modelData.ownerName
                source: keyRow.modelData.ownerIcon
                fullBleed: keyRow.modelData.ownerFullBleed === true
            }
            Column {
                id: keyText
                anchors.left: keyTile.right
                anchors.leftMargin: Theme.spacing.medium
                anchors.right: parent.right
                anchors.rightMargin: Theme.spacing.small
                anchors.verticalCenter: parent.verticalCenter
                LogosText {
                    width: parent.width
                    elide: Text.ElideRight
                    text: F.title(keyRow.modelData)
                    font.weight: Theme.typography.weightMedium
                }
                LogosText {
                    visible: text !== ""
                    width: parent.width
                    elide: Text.ElideRight
                    font.pixelSize: Theme.typography.secondaryText
                    text: F.oneLine(keyRow.modelData.note)
                }
                LogosText {
                    width: parent.width
                    elide: Text.ElideRight
                    color: Theme.palette.textSecondary
                    font.pixelSize: Theme.typography.secondaryText
                    // The owner by name and by its attested id.
                    readonly property string owner: keyRow.modelData.ownerApp !== keyRow.modelData.ownerName
                        ? qsTr("%1 (%2)").arg(keyRow.modelData.ownerName).arg(keyRow.modelData.ownerApp)
                        : keyRow.modelData.ownerApp
                    textFormat: Text.PlainText
                    text: keyRow.modelData.label
                          ? qsTr("%1 · %2").arg(owner).arg(keyRow.modelData.name)
                          : owner
                }
            }
        }
    }

    Flow {
        width: parent.width
        spacing: Theme.spacing.small
        layoutDirection: Qt.RightToLeft
        topPadding: Theme.spacing.small
        LogosButton {
            objectName: "basecampKeychain.request.continue"
            visible: !sheet.waiting && sheet.keys.length > 0
            enabled: sheet.selected >= 0 && sheet.selected < sheet.keys.length
            text: qsTr("Continue")
            variant: LogosButton.Variant.Primary
            onClicked: {
                const k = sheet.keys[sheet.selected]
                sheet.continueRequested(k.ownerApp, k.keyId)
            }
        }
        LogosButton {
            objectName: "basecampKeychain.request.openSigner"
            visible: sheet.waiting && sheet.signerName !== ""
            text: qsTr("Open %1").arg(sheet.signerName)
            variant: LogosButton.Variant.Primary
            onClicked: sheet.openSignerRequested()
        }
        LogosButton {
            objectName: "basecampKeychain.request.cancel"
            text: sheet.waiting ? qsTr("Cancel request") : qsTr("Cancel")
            onClicked: sheet.cancelRequested()
        }
    }
}
