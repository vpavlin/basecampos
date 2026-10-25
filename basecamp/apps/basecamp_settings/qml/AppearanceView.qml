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
    // Shells can be installed while Basecamp runs (Package Manager): look
    // again whenever this page is shown.
    onVisibleChanged: if (visible && root.prefs) root.prefs.rescanShells()
    id: root

    // Responsive, by this pane's own width (RESPONSIVE.md §3.2): one column
    // that stops at 720; on a phone the pickers fill the width (up to 360),
    // a control's help text drops under it, and pickers are 44 px tall.
    readonly property bool compact: Theme.windowClass(width) === "compact"
    // The DS combo floors at 44 under Theme.touch (touch density, or a
    // phone-narrow window); no override needed.
    readonly property int pickerHeight: Theme.components.combo.height

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
        onAccepted: root.installTheme(selectedFile)
    }

    function installTheme(file) {
        const r = ThemeInstaller.install(file)
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

    // Layout and wallpaper are shell preferences (MainContainer's ShellPrefs),
    // absent where the shell isn't the host (shell-preview, doctests).
    readonly property var prefs: typeof shell !== "undefined" && shell ? shell.prefs : null

    FileDialog {
        id: wallpaperDialog
        title: qsTr("Choose a wallpaper")
        nameFilters: [qsTr("Images (*.png *.jpg *.jpeg)")]
        onAccepted: root.setWallpaperFile(selectedFile)
    }

    function setWallpaperFile(file) {
        const err = root.prefs.setCustomWallpaper(file)
        d.resultSeverity = err.length ? LogosNotice.Error : LogosNotice.Success
        d.resultTitle = err.length ? qsTr("That image can't be used") : qsTr("Wallpaper set")
        d.resultMessage = err
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
            width: Math.min(scroll.availableWidth, 720)
            spacing: Theme.spacing.large

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.spacing.tiny

                LogosText {
                    text: qsTr("Customisation")
                    font.pixelSize: Theme.typography.panelTitleText
                    font.weight: Theme.typography.weightMedium
                    color: Theme.palette.text
                }
                LogosText {
                    text: qsTr("Theme, density, layout and shell. Themes are plain data files; anyone can make one.")
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
                    Layout.fillWidth: true
                    Layout.maximumWidth: 360
                    Layout.preferredHeight: root.pickerHeight
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

                GridLayout {
                    Layout.fillWidth: true
                    columns: root.compact ? 1 : 2
                    columnSpacing: Theme.spacing.medium
                    rowSpacing: Theme.spacing.small

                    LogosButton {
                        objectName: "appearance.uploadButton"
                        text: qsTr("Upload your own theme…")
                        variant: LogosButton.Variant.Primary
                        onClicked: {
                            if (root.prefs) {
                                const f = root.prefs.pickFile(fileDialog.title, qsTr("Logos theme (*.json)"))
                                if (f.toString().length > 0)
                                    root.installTheme(f)
                            } else {
                                fileDialog.open()
                            }
                        }
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

                LogosNotice {
                    objectName: "appearance.restartNotice"
                    Layout.fillWidth: true
                    severity: LogosNotice.Info
                    title: qsTr("Apps switch after a restart")
                    message: qsTr("Basecamp switches right away. Apps that support themes run in their own process and switch the next time you start Basecamp.")
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
                    Layout.fillWidth: true
                    Layout.maximumWidth: 360
                    Layout.preferredHeight: root.pickerHeight
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

            // Shell packages (prototype): the whole UI, swapped after a restart.
            ColumnLayout {
                objectName: "customisation.shell"
                Layout.fillWidth: true
                spacing: Theme.spacing.small
                visible: root.prefs !== null

                RowLayout {
                    spacing: Theme.spacing.small
                    LogosText {
                        text: qsTr("Shell")
                        font.pixelSize: Theme.typography.primaryText
                        font.weight: Theme.typography.weightMedium
                        color: Theme.palette.text
                    }
                    LogosBadge { text: qsTr("Experimental"); color: Theme.palette.warning }
                }
                LogosText {
                    Layout.fillWidth: true
                    text: qsTr("A shell replaces all of Basecamp's UI. Your apps, keys and theme stay the same.")
                    font.pixelSize: Theme.typography.secondaryText
                    color: Theme.palette.textTertiary
                    wrapMode: Text.WordWrap
                }

                // On a phone "Add shell…" goes under the picker, so the
                // picker has the whole width for the shell's name.
                GridLayout {
                    Layout.fillWidth: true
                    columns: root.compact ? 1 : 2
                    columnSpacing: Theme.spacing.medium
                    rowSpacing: Theme.spacing.small

                    // One list: the two arrangements of Basecamp Legacy, the shell
                    // built into Basecamp (by Logos, no warning), and the shell
                    // packages: Basecamp itself (the default, no warning when it
                    // ships with Basecamp) and installed ones (warning).
                    LogosComboBox {
                        id: shellCombo
                        objectName: "customisation.shellCombo"
                        Layout.fillWidth: true
                        Layout.maximumWidth: root.compact ? Number.POSITIVE_INFINITY : 360
                        Layout.preferredHeight: root.pickerHeight
                        readonly property var packages: root.prefs ? root.prefs.shellPackages : []
                        readonly property var values: ["builtin:sidebar", "builtin:desktop"].concat(packages.map(p => p.id))
                        // Each shell's name; the list adds who made it. Closed,
                        // the box shows the name alone, so it fits at phone
                        // width beside "Add shell…" (the line below says who).
                        readonly property var names: [qsTr("Basecamp Legacy"), qsTr("Basecamp Legacy (desktop)")]
                            .concat(packages.map(p => p.name))
                        model: names.map((n, i) => qsTr("%1 by %2").arg(n).arg(
                                   i < 2 ? qsTr("Logos")
                                         : (packages[i - 2].signer || packages[i - 2].author || qsTr("unknown"))))
                        displayText: names[currentIndex] || ""
                        readonly property string active: root.prefs
                            ? (root.prefs.activeShell !== "" ? root.prefs.activeShell : "builtin:" + root.prefs.activeLayout) : ""
                        readonly property string chosen: root.prefs
                            ? (root.prefs.shell !== "" ? root.prefs.shell : "builtin:" + root.prefs.layout) : ""
                        function sync() { currentIndex = Math.max(0, (values || []).indexOf(chosen)) }
                        Component.onCompleted: sync()
                        onPackagesChanged: sync()
                        onChosenChanged: sync()
                        onActivated: index => {
                            const id = values[index]
                            if (!root.prefs)
                                return
                            if (id.startsWith("builtin:")) {
                                // Built in, by Logos: no warning, applies on restart.
                                root.prefs.shell = ""
                                root.prefs.layout = id.slice(8)
                                sync()
                            } else if (id !== chosen && packages[index - 2].builtIn) {
                                root.prefs.shell = id
                                sync()
                            } else if (id !== chosen) {
                                shellWarning.pkg = packages[index - 2]
                                shellWarning.checks = root.prefs.checkShell(id)
                                shellWarning.open()
                            }
                        }
                    }

                    LogosButton {
                        objectName: "customisation.addShell"
                        text: qsTr("Add shell…")
                        onClicked: addShellDialog.open()
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: root.compact ? 1 : 2
                    columnSpacing: Theme.spacing.small
                    rowSpacing: Theme.spacing.small
                    visible: shellCombo.chosen !== shellCombo.active

                    LogosText {
                        Layout.fillWidth: root.compact
                        text: qsTr("Takes effect the next time you start Basecamp.")
                        font.pixelSize: Theme.typography.secondaryText
                        color: Theme.palette.warning
                        wrapMode: Text.WordWrap
                    }
                    LogosButton {
                        objectName: "customisation.shellRestart"
                        text: qsTr("Restart now")
                        onClicked: root.prefs.restart()
                    }
                }

                LogosText {
                    objectName: "customisation.activeShell"
                    Layout.fillWidth: true
                    readonly property var active: {
                        const pk = root.prefs ? root.prefs.shellPackages : []
                        for (let i = 0; i < pk.length; ++i)
                            if (pk[i].id === (root.prefs ? root.prefs.activeShell : "")) return pk[i]
                        return null
                    }
                    text: active && active.builtIn
                          ? qsTr("In use: %1, built into Basecamp.").arg(active.name)
                          : active
                          ? qsTr("In use: %1 %2 by %3%4").arg(active.name).arg(active.version || "")
                                .arg(active.signer || active.author || qsTr("unknown builder"))
                                .arg(active.signed ? qsTr(" (signed, not verified by Logos)") : qsTr(" (unsigned)"))
                          : root.prefs && root.prefs.activeLayout === "desktop"
                            ? qsTr("In use: Basecamp Legacy (desktop), built into Basecamp.")
                            : qsTr("In use: Basecamp Legacy, built into Basecamp.")
                    font.pixelSize: Theme.typography.secondaryText
                    color: Theme.palette.textTertiary
                    wrapMode: Text.WordWrap
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.spacing.small
                visible: false   // folded into the Shell list (Default / Desktop by Logos)

                LogosText {
                    text: qsTr("Layout")
                    font.pixelSize: Theme.typography.primaryText
                    font.weight: Theme.typography.weightMedium
                    color: Theme.palette.text
                }

                LogosComboBox {
                    objectName: "appearance.layoutCombo"
                    Layout.fillWidth: true
                    Layout.maximumWidth: 360
                    Layout.preferredHeight: root.pickerHeight
                    readonly property var values: ["sidebar", "desktop"]
                    model: [qsTr("Window (default)"), qsTr("Desktop")]
                    currentIndex: root.prefs ? Math.max(0, values.indexOf(root.prefs.layout)) : 0
                    onActivated: index => root.prefs.layout = values[index]
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacing.small

                    LogosText {
                        text: root.prefs && root.prefs.layout !== root.prefs.activeLayout
                              ? qsTr("Takes effect the next time you start Basecamp.")
                              : qsTr("In use now.")
                        font.pixelSize: Theme.typography.secondaryText
                        color: root.prefs && root.prefs.layout !== root.prefs.activeLayout
                               ? Theme.palette.warning : Theme.palette.textTertiary
                    }

                    LogosButton {
                        objectName: "appearance.restartButton"
                        text: qsTr("Restart now")
                        visible: root.prefs !== null && root.prefs.layout !== root.prefs.activeLayout
                        onClicked: root.prefs.restart()
                    }

                    Item { Layout.fillWidth: true }
                }
            }

            // Wallpaper only exists in the desktop layout; offered once Basecamp
            // is running in it.
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.spacing.small
                visible: root.prefs !== null && !root.prefs.tabsShell && root.prefs.activeLayout === "desktop"

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
                        preview: root.prefs ? "image://basecamp/wallpaper/builtin" : ""
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
                        // The path in the id makes a newly chosen file load, not the cached one.
                        preview: root.prefs && root.prefs.customWallpaperUrl.toString().length > 0
                                 ? "image://basecamp/wallpaper/custom/" + encodeURIComponent(root.prefs.customWallpaperUrl.toString()) : ""
                    }
                    LogosButton {
                        objectName: "appearance.wallpaper.choose"
                        text: qsTr("Choose image…")
                        onClicked: {
                            const f = root.prefs.pickFile(wallpaperDialog.title, qsTr("Images (*.png *.jpg *.jpeg)"))
                            if (f.toString().length > 0)
                                root.setWallpaperFile(f)
                        }
                    }
                }

                LogosComboBox {
                    objectName: "appearance.wallpaperFit"
                    Layout.fillWidth: true
                    Layout.maximumWidth: 360
                    Layout.preferredHeight: root.pickerHeight
                    readonly property var values: ["fit", "fill", "stretch"]
                    model: [qsTr("Fit: whole image, blurred edges"), qsTr("Fill: cover the screen, crop"),
                            qsTr("Stretch")]
                    currentIndex: root.prefs ? Math.max(0, values.indexOf(root.prefs.wallpaperFit)) : 0
                    onActivated: index => root.prefs.wallpaperFit = values[index]
                }
            }
        }
    }

    // ── Shell switching ─────────────────────────────────────────────────────
    LogosDialog {
        id: shellWarning
        objectName: "customisation.shellWarning"
        property var pkg: ({})
        property var checks: []
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(600, parent ? parent.width - 48 : 600)
        modal: true
        title: qsTr("Use “%1” by %2 as your Basecamp?").arg(pkg.name || "")
                   .arg(pkg.signer || pkg.author || qsTr("an unknown builder"))
        onOpened: trust.checked = false
        onClosed: shellCombo.sync()

        contentItem: ColumnLayout {
            spacing: Theme.spacing.medium

            LogosText {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.palette.text
                font.pixelSize: Theme.typography.secondaryText
                text: qsTr("A shell draws all of Basecamp. It can see everything shown in Basecamp and everything you type into it, including passwords and recovery phrases you enter in apps. Logos has not reviewed it.")
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: checkCol.implicitHeight + 2 * Theme.spacing.medium
                radius: Theme.spacing.radiusMedium
                color: Theme.palette.backgroundTertiary
                ColumnLayout {
                    id: checkCol
                    anchors.fill: parent
                    anchors.margins: Theme.spacing.medium
                    spacing: Theme.spacing.small
                    LogosText {
                        text: qsTr("What Basecamp checked (hints, not guarantees)")
                        font.pixelSize: Theme.typography.secondaryText
                        font.weight: Theme.typography.weightMedium
                        color: Theme.palette.text
                    }
                    Repeater {
                        model: shellWarning.checks
                        delegate: RowLayout {
                            required property var modelData
                            Layout.fillWidth: true
                            spacing: Theme.spacing.small
                            Rectangle {
                                Layout.alignment: Qt.AlignTop
                                Layout.topMargin: 5
                                width: 8; height: 8; radius: 4
                                color: modelData.status === "ok" ? Theme.palette.success
                                     : modelData.status === "fail" ? Theme.palette.error
                                     : modelData.status === "warn" ? Theme.palette.warning
                                     : Theme.palette.textTertiary
                            }
                            LogosText {
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                font.pixelSize: Theme.typography.secondaryText
                                color: Theme.palette.textSecondary
                                text: "<b>" + modelData.title + ":</b> " + modelData.detail
                                textFormat: Text.StyledText
                            }
                        }
                    }
                }
            }

            LogosText {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.palette.textSecondary
                font.pixelSize: Theme.typography.secondaryText
                text: qsTr("To go back: Settings → Customisation → Shell, or hold Shift while starting Basecamp (or start it with --default-shell) to get Basecamp Legacy. Uninstalling the shell in Package Manager and restarting also returns you to Basecamp Legacy.")
            }

            LogosCheckbox {
                id: trust
                objectName: "customisation.trustShell"
                text: qsTr("I trust this builder and use this shell at my own risk")
            }
        }

        rightActions: [
            LogosButton {
                text: qsTr("Cancel")
                onClicked: shellWarning.close()
            },
            LogosButton {
                objectName: "customisation.switchShell"
                text: qsTr("Switch and restart")
                variant: LogosButton.Variant.Primary
                enabled: trust.checked
                onClicked: {
                    shellWarning.close()
                    root.prefs.switchShell(shellWarning.pkg.id)
                }
            }
        ]
    }

    LogosDialog {
        id: addShellDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(480, parent ? parent.width - 32 : 480)
        modal: true
        title: qsTr("Add a shell")
        message: qsTr("Shells are packages (.lgx), installed like apps. In Package Manager, choose Install Local Package and pick the shell's .lgx file. It then appears in this list; installing doesn't switch to it.")
        rightActions: [
            LogosButton { text: qsTr("Cancel"); onClicked: addShellDialog.close() },
            LogosButton {
                text: qsTr("Open Package Manager")
                variant: LogosButton.Variant.Primary
                onClicked: { addShellDialog.close(); shell.openRole("packages") }
            }
        ]
    }
}
