import QtQuick

import Logos.Controls
import Logos.Theme
import Basecamp.Icons

import "KeychainFormat.js" as F

// The keys view (basecamp-experiments#57): every registered public key,
// grouped by the app that owns it. The user's label, when set, is the key's
// name, with the app's own name under it; the note follows, one line until
// tapped. Edit label and note, copy the public key, remove the entry (the
// app keeps its private key and can add it again).
Item {
    id: view

    property var keys: []
    property double now: Date.now()
    // (ownerApp, keyId, label, note) -> bool, and (ownerApp, keyId) -> bool:
    // the shell calls, passed in so this file never reaches `shell` itself.
    property var saveKey: null
    property var removeKey: null

    readonly property var groups: F.groups(keys)
    readonly property int gutter: width < 600 ? 16 : Theme.spacing.large

    // "ownerApp/keyId" of the key whose note is open.
    property string expanded: ""

    function ageText(added) {
        const a = F.age(added, now)
        if (a.unit === "now") return qsTr("Added just now")
        if (a.unit === "min") return qsTr("Added %n min ago", "", a.n)
        if (a.unit === "h") return qsTr("Added %n h ago", "", a.n)
        if (a.unit === "d") return qsTr("Added %n days ago", "", a.n)
        return qsTr("Added on %1").arg(a.date)
    }

    function openEditor(key) {
        editor.key = key
        labelField.text = key.label || ""
        noteField.text = key.note || ""
        editor.error = ""
        editor.visible = true
        labelField.textInput.forceActiveFocus()
    }

    function openRemove(key) {
        removeSheet.key = key
        removeSheet.visible = true
    }

    // The clipboard, through a text edit (what LogosCopyButton does).
    TextEdit { id: clipboard; visible: false }
    function copyKey(key) {
        clipboard.text = key.publicKey
        clipboard.selectAll()
        clipboard.copy()
        clipboard.text = ""
        copiedToast.show(undefined, qsTr("%1 copied").arg(F.title(key)))
    }

    Flickable {
        id: flick
        objectName: "basecampKeychain.keys"
        anchors.fill: parent
        contentWidth: width
        contentHeight: column.implicitHeight + 2 * view.gutter
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: column
            x: Math.max(view.gutter, (view.width - width) / 2)
            y: view.gutter
            width: Math.min(view.width - 2 * view.gutter, 720)
            spacing: Theme.spacing.medium

            LogosText {
                width: parent.width
                text: qsTr("Keychain")
                font.pixelSize: 20
                font.weight: Font.DemiBold
            }
            LogosText {
                width: parent.width
                wrapMode: Text.Wrap
                color: Theme.palette.textSecondary
                font.pixelSize: Theme.typography.secondaryText
                text: qsTr("Public keys your apps have added. When an app asks for a signature, you choose a key here and the app that owns it signs. Keychain never holds private keys.")
            }

            // Empty: what this is, and that apps bring their own keys.
            Rectangle {
                objectName: "basecampKeychain.empty"
                visible: view.groups.length === 0
                width: parent.width
                height: emptyText.implicitHeight + 2 * Theme.spacing.large
                radius: Theme.spacing.radiusLarge
                color: Theme.palette.backgroundSecondary
                LogosText {
                    id: emptyText
                    anchors.fill: parent
                    anchors.margins: Theme.spacing.large
                    wrapMode: Text.Wrap
                    color: Theme.palette.textSecondary
                    text: qsTr("No keys yet. Apps that can sign, such as wallets, add their keys here themselves. Their private keys stay in those apps.")
                }
            }

            Repeater {
                model: view.groups

                delegate: Column {
                    id: group
                    required property var modelData
                    width: column.width
                    spacing: Theme.spacing.small

                    Row {
                        width: parent.width
                        height: 48
                        spacing: Theme.spacing.medium
                        BasecampAppTile {
                            anchors.verticalCenter: parent.verticalCenter
                            size: 32
                            label: group.modelData.ownerName
                            source: group.modelData.ownerIcon
                            fullBleed: group.modelData.ownerFullBleed
                        }
                        // Its name, and the module id the host attested: the
                        // name is the app's claim, the id is not.
                        Column {
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width - 32 - Theme.spacing.medium
                            LogosText {
                                width: parent.width
                                elide: Text.ElideRight
                                textFormat: Text.PlainText
                                text: group.modelData.ownerName
                                font.weight: Theme.typography.weightMedium
                            }
                            LogosText {
                                objectName: "basecampKeychain.owner." + group.modelData.ownerApp
                                visible: group.modelData.ownerApp !== group.modelData.ownerName
                                width: parent.width
                                elide: Text.ElideMiddle
                                textFormat: Text.PlainText
                                color: Theme.palette.textSecondary
                                font.pixelSize: Theme.typography.secondaryText
                                text: qsTr("App id: %1").arg(group.modelData.ownerApp)
                            }
                        }
                    }

                    Repeater {
                        model: group.modelData.keys

                        delegate: Rectangle {
                            id: row
                            required property var modelData
                            readonly property string rowId: modelData.ownerApp + "/" + modelData.keyId
                            readonly property bool open: view.expanded === rowId
                            objectName: "basecampKeychain.key." + modelData.ownerApp + "." + modelData.keyId
                            width: group.width
                            height: rowColumn.implicitHeight + 2 * Theme.spacing.medium
                            radius: Theme.spacing.radiusLarge
                            color: Theme.palette.backgroundSecondary

                            // Tapping the text opens or closes the note.
                            MouseArea {
                                anchors.fill: parent
                                enabled: row.modelData.note !== ""
                                cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                                onClicked: view.expanded = row.open ? "" : row.rowId
                            }

                            Column {
                                id: rowColumn
                                x: Theme.spacing.medium
                                y: Theme.spacing.medium
                                width: parent.width - 2 * Theme.spacing.medium
                                spacing: Theme.spacing.tiny

                                LogosText {
                                    objectName: row.objectName + ".title"
                                    width: parent.width
                                    elide: Text.ElideRight
                                    text: F.title(row.modelData)
                                    font.weight: Theme.typography.weightMedium
                                }
                                LogosText {
                                    visible: text !== ""
                                    width: parent.width
                                    elide: Text.ElideRight
                                    color: Theme.palette.textSecondary
                                    font.pixelSize: Theme.typography.secondaryText
                                    text: F.subtitle(row.modelData) !== ""
                                          ? qsTr("Named by the app: %1").arg(F.subtitle(row.modelData)) : ""
                                }
                                LogosText {
                                    objectName: row.objectName + ".note"
                                    visible: row.modelData.note !== ""
                                    width: parent.width
                                    wrapMode: row.open ? Text.Wrap : Text.NoWrap
                                    elide: row.open ? Text.ElideNone : Text.ElideRight
                                    maximumLineCount: row.open ? 100 : 1
                                    text: row.open ? row.modelData.note : F.oneLine(row.modelData.note)
                                }
                                LogosText {
                                    width: parent.width
                                    wrapMode: Text.Wrap
                                    color: Theme.palette.textSecondary
                                    font.pixelSize: Theme.typography.secondaryText
                                    text: F.details(row.modelData, qsTr("Any use"))
                                }
                                LogosText {
                                    width: parent.width
                                    color: Theme.palette.textTertiary
                                    font.pixelSize: Theme.typography.secondaryText
                                    text: view.ageText(row.modelData.added)
                                }

                                Flow {
                                    width: parent.width
                                    spacing: Theme.spacing.small
                                    topPadding: Theme.spacing.small
                                    LogosButton {
                                        objectName: row.objectName + ".edit"
                                        text: qsTr("Edit label and note")
                                        onClicked: view.openEditor(row.modelData)
                                    }
                                    LogosButton {
                                        objectName: row.objectName + ".copy"
                                        text: qsTr("Copy public key")
                                        onClicked: view.copyKey(row.modelData)
                                    }
                                    LogosButton {
                                        objectName: row.objectName + ".remove"
                                        text: qsTr("Remove")
                                        onClicked: view.openRemove(row.modelData)
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // ── Edit label and note ─────────────────────────────────────────────
    KeychainSheet {
        id: editor
        objectName: "basecampKeychain.editor"
        anchors.fill: parent
        property var key: ({})
        property string error: ""
        readonly property bool labelFits: F.fits(labelField.text.trim(), F.maxLabel)
        readonly property bool noteFits: F.fits(noteField.text.trim(), F.maxNote)

        function save() {
            if (!labelFits || !noteFits) return
            if (view.saveKey && view.saveKey(key.ownerApp, key.keyId, labelField.text.trim(), noteField.text.trim())) {
                visible = false
            } else {
                error = qsTr("Could not save. The key may have been removed.")
            }
        }

        LogosText {
            width: parent.width
            text: qsTr("Label and note")
            font.pixelSize: Theme.typography.panelTitleText
            font.weight: Theme.typography.weightBold
        }
        LogosText {
            width: parent.width
            wrapMode: Text.Wrap
            color: Theme.palette.textSecondary
            font.pixelSize: Theme.typography.secondaryText
            text: qsTr("For you only: no app sees them, not even %1. Both are optional; leave one empty to clear it.")
                  .arg(editor.key.ownerName || "")
        }
        LogosText {
            width: parent.width
            text: qsTr("Label")
        }
        LogosTextField {
            id: labelField
            objectName: "basecampKeychain.editor.label"
            width: parent.width
            placeholderText: editor.key.name || ""
            textInput.maximumLength: F.maxLabel
            Keys.onReturnPressed: editor.save()
        }
        LogosText {
            width: parent.width
            horizontalAlignment: Text.AlignRight
            color: editor.labelFits ? Theme.palette.textTertiary : Theme.palette.error
            font.pixelSize: Theme.typography.secondaryText
            text: labelField.text.trim().length + " / " + F.maxLabel
        }
        LogosText {
            width: parent.width
            text: qsTr("Note")
        }
        LogosTextArea {
            id: noteField
            objectName: "basecampKeychain.editor.note"
            width: parent.width
            height: Math.max(96, implicitHeight)
            wrapMode: TextEdit.Wrap
        }
        LogosText {
            width: parent.width
            horizontalAlignment: Text.AlignRight
            color: editor.noteFits ? Theme.palette.textTertiary : Theme.palette.error
            font.pixelSize: Theme.typography.secondaryText
            text: noteField.text.trim().length + " / " + F.maxNote
        }
        LogosText {
            visible: editor.error !== ""
            width: parent.width
            wrapMode: Text.Wrap
            color: Theme.palette.error
            text: editor.error
        }
        Flow {
            width: parent.width
            spacing: Theme.spacing.small
            layoutDirection: Qt.RightToLeft
            LogosButton {
                objectName: "basecampKeychain.editor.save"
                text: qsTr("Save")
                variant: LogosButton.Variant.Primary
                enabled: editor.labelFits && editor.noteFits
                onClicked: editor.save()
            }
            LogosButton {
                objectName: "basecampKeychain.editor.cancel"
                text: qsTr("Cancel")
                onClicked: editor.visible = false
            }
        }
    }

    // ── Remove ──────────────────────────────────────────────────────────
    KeychainSheet {
        id: removeSheet
        objectName: "basecampKeychain.remove"
        anchors.fill: parent
        property var key: ({})

        LogosText {
            width: parent.width
            wrapMode: Text.Wrap
            text: qsTr("Remove %1?").arg(F.title(removeSheet.key))
            font.pixelSize: Theme.typography.panelTitleText
            font.weight: Theme.typography.weightBold
        }
        LogosText {
            width: parent.width
            wrapMode: Text.Wrap
            color: Theme.palette.textSecondary
            text: qsTr("It leaves Keychain, with your label and note. %1 keeps its private key and can add the key again.")
                  .arg(removeSheet.key.ownerName || "")
        }
        Flow {
            width: parent.width
            spacing: Theme.spacing.small
            layoutDirection: Qt.RightToLeft
            LogosButton {
                objectName: "basecampKeychain.remove.confirm"
                text: qsTr("Remove")
                variant: LogosButton.Variant.Primary
                onClicked: {
                    if (view.removeKey) view.removeKey(removeSheet.key.ownerApp, removeSheet.key.keyId)
                    removeSheet.visible = false
                }
            }
            LogosButton {
                objectName: "basecampKeychain.remove.cancel"
                text: qsTr("Cancel")
                onClicked: removeSheet.visible = false
            }
        }
    }

    LogosToast {
        id: copiedToast
        objectName: "basecampKeychain.copied"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.spacing.large
        width: Math.min(parent.width - 2 * view.gutter, 420)
        severity: LogosNotice.Success
        title: qsTr("Public key copied")
    }
}
