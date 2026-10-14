import QtQuick
import QtQuick.Layouts
import QtQuick.Dialogs

import Logos.Theme
import Logos.Controls

import Basecamp.Backend 1.0

// Settings → Appearance. Pick an installed theme, or install your own
// theme.json (validated as data before it is copied in).
Item {
    id: root

    QtObject {
        id: d

        // Theme names and authors come from third-party files: escape them so
        // the styled-text label can't carry markup (e.g. an <img src> URL).
        function escapeHtml(s) {
            return String(s).replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
        }

        // [{ id, label }] in Theme.themeNames order. The label is the theme's
        // name with a quieter "by <author>" after it.
        readonly property var themes: {
            const out = []
            const names = Theme.themeNames
            const byColor = Theme.palette.textTertiary
            for (let i = 0; i < names.length; ++i) {
                const info = Theme.themeInfo[names[i]] || { name: names[i], author: "" }
                let label = escapeHtml(info.name)
                if (info.author)
                    label += "&nbsp;&nbsp;<font color=\"" + byColor + "\">"
                             + qsTr("by %1").arg(escapeHtml(info.author)) + "</font>"
                out.push({ id: names[i], label: label })
            }
            return out
        }

        function indexOf(id) {
            for (let i = 0; i < themes.length; ++i)
                if (themes[i].id === id) return i
            return -1
        }

        // Result of the last install, shown in a notice.
        property int resultSeverity: LogosNotice.Info
        property string resultTitle: ""
        property string resultMessage: ""
    }

    FileDialog {
        id: fileDialog
        title: qsTr("Choose a theme file")
        nameFilters: [qsTr("Logos theme (*.json)")]
        onAccepted: {
            const r = ThemeInstaller.install(selectedFile)
            if (r.ok) {
                ThemeRegistry.reload()
                Theme.setTheme(r.id)
                d.resultSeverity = LogosNotice.Success
                d.resultTitle = qsTr("Installed “%1”").arg(r.name)
                d.resultMessage = r.warnings.length
                    ? qsTr("Applied with warnings: %1").arg(r.warnings.join("; "))
                    : qsTr("It is now active. Apps pick it up the next time you start Basecamp.")
            } else {
                d.resultSeverity = LogosNotice.Error
                d.resultTitle = qsTr("That file isn't a valid theme")
                d.resultMessage = r.errors.join("; ")
            }
        }
    }

    LogosScrollView {
        id: scroll
        anchors.fill: parent
        anchors.margins: Theme.spacing.large
        clip: true

        ColumnLayout {
            width: scroll.availableWidth
            spacing: Theme.spacing.large

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.spacing.tiny

                LogosText {
                    text: qsTr("Appearance")
                    font.pixelSize: Theme.typography.panelTitleText
                    font.weight: Theme.typography.weightMedium
                    color: Theme.palette.text
                }
                LogosText {
                    text: qsTr("Choose how Basecamp looks. Themes are plain data files; anyone can make one.")
                    font.pixelSize: Theme.typography.secondaryText
                    color: Theme.palette.textSecondary
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.spacing.small

                LogosText {
                    text: qsTr("Theme")
                    font.pixelSize: Theme.typography.primaryText
                    font.weight: Theme.typography.weightMedium
                    color: Theme.palette.text
                }

                LogosComboBox {
                    id: themeCombo
                    objectName: "appearance.themeCombo"
                    Layout.preferredWidth: 360
                    model: d.themes
                    textRole: "label"
                    currentIndex: d.indexOf(Theme.currentTheme)
                    onActivated: index => Theme.setTheme(d.themes[index].id)
                }
            }

            LogosNotice {
                objectName: "appearance.restartNotice"
                Layout.fillWidth: true
                severity: LogosNotice.Info
                title: qsTr("Apps switch after a restart")
                message: qsTr("Basecamp switches right away. Apps that support themes run in their own process and switch the next time you start Basecamp.")
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacing.medium

                LogosButton {
                    objectName: "appearance.uploadButton"
                    text: qsTr("Upload your own theme…")
                    variant: LogosButton.Variant.Primary
                    onClicked: fileDialog.open()
                }

                LogosText {
                    Layout.fillWidth: true
                    text: qsTr("A Logos theme file (theme.json). Installed to %1").arg(ThemeInstaller.themesDir)
                    font.pixelSize: Theme.typography.secondaryText
                    color: Theme.palette.textTertiary
                    wrapMode: Text.WrapAnywhere
                }
            }

            LogosNotice {
                objectName: "appearance.resultNotice"
                Layout.fillWidth: true
                shown: d.resultTitle.length > 0
                closable: true
                severity: d.resultSeverity
                title: d.resultTitle
                message: d.resultMessage
                onDismissed: d.resultTitle = ""
            }
        }
    }
}
