import QtQuick
import QtQuick.Controls
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
                d.resultTitle = r.assets
                    ? qsTr("Installed “%1” with %2 files").arg(r.name).arg(r.assets)
                    : qsTr("Installed “%1”").arg(r.name)
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

    // Layout and wallpaper are shell preferences (MainContainer's ShellPrefs),
    // absent where the shell isn't the host (shell-preview, doctests).
    readonly property var prefs: typeof shellPrefs !== "undefined" ? shellPrefs : null

    FileDialog {
        id: wallpaperDialog
        title: qsTr("Choose a wallpaper")
        nameFilters: [qsTr("Images (*.png *.jpg *.jpeg)")]
        onAccepted: {
            const err = root.prefs.setCustomWallpaper(selectedFile)
            d.resultSeverity = err.length ? LogosNotice.Error : LogosNotice.Success
            d.resultTitle = err.length ? qsTr("That image can't be used") : qsTr("Wallpaper set")
            d.resultMessage = err
        }
    }

    component WallpaperChoice: AbstractButton {
        id: choice
        property string wallpaperId
        property url preview: ""
        readonly property bool selected: root.prefs && root.prefs.wallpaper === wallpaperId
        implicitWidth: 152
        implicitHeight: 112
        hoverEnabled: true
        onClicked: root.prefs.setWallpaper(wallpaperId)

        contentItem: ColumnLayout {
            spacing: Theme.spacing.tiny
            Rectangle {
                Layout.preferredWidth: 152
                Layout.preferredHeight: 86
                radius: Theme.spacing.radiusSmall
                color: Theme.palette.background
                border.width: choice.selected ? 2 : 1
                border.color: choice.selected ? Theme.palette.accentOrange
                             : choice.hovered ? Theme.palette.borderStrong : Theme.palette.border
                clip: true
                Image {
                    anchors.fill: parent
                    anchors.margins: parent.border.width
                    source: choice.preview
                    visible: source.toString().length > 0
                    fillMode: Image.PreserveAspectCrop
                    sourceSize: Qt.size(304, 172)
                    asynchronous: true
                }
            }
            LogosText {
                text: choice.text
                font.pixelSize: Theme.typography.secondaryText
                color: choice.selected ? Theme.palette.text : Theme.palette.textSecondary
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

                // Font the active theme asks for, and whether it is installed.
                LogosText {
                    objectName: "appearance.fontLine"
                    Layout.fillWidth: true
                    readonly property string wanted: Theme.typography.familyRequested
                    text: wanted.length === 0
                          ? qsTr("Font: %1 (default)").arg(Theme.typography.family)
                          : Theme.typography.familyAvailable
                            ? qsTr("Font: %1").arg(Theme.typography.family)
                            : qsTr("Font: “%1” isn't installed on this computer, so %2 is used instead.")
                                  .arg(wanted).arg(Theme.typography.family)
                    textFormat: Text.PlainText
                    font.pixelSize: Theme.typography.secondaryText
                    color: Theme.typography.familyAvailable ? Theme.palette.textTertiary : Theme.palette.warning
                    wrapMode: Text.WordWrap
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.spacing.small

                LogosText {
                    text: qsTr("Density")
                    font.pixelSize: Theme.typography.primaryText
                    font.weight: Theme.typography.weightMedium
                    color: Theme.palette.text
                }

                // "" follows the theme; the others are the user's own choice.
                LogosComboBox {
                    id: densityCombo
                    objectName: "appearance.densityCombo"
                    Layout.preferredWidth: 360
                    readonly property var values: ["", "compact", "comfortable", "touch"]
                    model: [qsTr("Theme default"), qsTr("Compact"), qsTr("Comfortable"),
                            qsTr("Touch (larger targets)")]
                    currentIndex: Math.max(0, values.indexOf(Theme.settings.density))
                    onActivated: index => Theme.setDensity(values[index])
                }

                LogosText {
                    Layout.fillWidth: true
                    text: qsTr("Using: %1").arg(Theme.density)
                    font.pixelSize: Theme.typography.secondaryText
                    color: Theme.palette.textTertiary
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.spacing.small
                visible: root.prefs !== null

                LogosText {
                    text: qsTr("Layout")
                    font.pixelSize: Theme.typography.primaryText
                    font.weight: Theme.typography.weightMedium
                    color: Theme.palette.text
                }

                LogosComboBox {
                    objectName: "appearance.layoutCombo"
                    Layout.preferredWidth: 360
                    readonly property var values: ["sidebar", "desktop"]
                    model: [qsTr("Sidebar"), qsTr("Desktop (full screen, dock, windows)")]
                    currentIndex: root.prefs ? Math.max(0, values.indexOf(root.prefs.layout)) : 0
                    onActivated: index => root.prefs.layout = values[index]
                }

                LogosText {
                    Layout.fillWidth: true
                    text: root.prefs && root.prefs.layout !== root.prefs.activeLayout
                          ? qsTr("Takes effect the next time you start Basecamp.")
                          : qsTr("In use now.")
                    font.pixelSize: Theme.typography.secondaryText
                    color: root.prefs && root.prefs.layout !== root.prefs.activeLayout
                           ? Theme.palette.warning : Theme.palette.textTertiary
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.spacing.small
                visible: root.prefs !== null

                LogosText {
                    text: qsTr("Wallpaper")
                    font.pixelSize: Theme.typography.primaryText
                    font.weight: Theme.typography.weightMedium
                    color: Theme.palette.text
                }
                LogosText {
                    Layout.fillWidth: true
                    text: qsTr("Shown behind the windows in the desktop layout.")
                    font.pixelSize: Theme.typography.secondaryText
                    color: Theme.palette.textTertiary
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: Theme.spacing.medium

                    WallpaperChoice {
                        objectName: "appearance.wallpaper.basecamp"
                        wallpaperId: "basecamp"
                        text: qsTr("Basecamp")
                        preview: root.prefs ? root.prefs.builtinWallpaperUrl : ""
                    }
                    WallpaperChoice {
                        objectName: "appearance.wallpaper.plain"
                        wallpaperId: "plain"
                        text: qsTr("Plain")
                    }
                    WallpaperChoice {
                        objectName: "appearance.wallpaper.custom"
                        wallpaperId: "custom"
                        visible: root.prefs && root.prefs.customWallpaperUrl.toString().length > 0
                        text: qsTr("Your image")
                        preview: root.prefs ? root.prefs.customWallpaperUrl : ""
                    }
                    LogosButton {
                        objectName: "appearance.wallpaper.choose"
                        text: qsTr("Choose image…")
                        onClicked: wallpaperDialog.open()
                    }
                }

                LogosComboBox {
                    objectName: "appearance.wallpaperFit"
                    Layout.preferredWidth: 360
                    readonly property var values: ["fit", "fill", "stretch"]
                    model: [qsTr("Fit: whole image, blurred edges"), qsTr("Fill: cover the screen, crop"),
                            qsTr("Stretch")]
                    currentIndex: root.prefs ? Math.max(0, values.indexOf(root.prefs.wallpaperFit)) : 0
                    onActivated: index => root.prefs.wallpaperFit = values[index]
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
                    text: qsTr("A Logos theme file (.json), or a bundle with its icons inside. Installed to %1").arg(ThemeInstaller.themesDir)
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
