import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Logos.Controls
import Logos.Theme

Item {
    id: root

    // Responsive: page margin by this pane's width (40 on desktop as before,
    // 16 on a phone); commit rows stack name over hash when two columns
    // would squeeze the hash.
    readonly property bool narrow: scroll.availableWidth < 520

    LogosScrollView {
        id: scroll
        anchors.fill: parent
        anchors.margins: Theme.pageMargin(root.width)
        clip: true

        ColumnLayout {
            width: scroll.availableWidth
            spacing: 20

            LogosText {
                text: "Dashboard"
                font.pixelSize: 24
                font.weight: Font.Bold
                color: Theme.tokenOr("text", "#ffffff")
            }

            // Build summary: version (release only) + build type.
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                // "Version" stays a plain label so a drag-select copies the
                // bare version string, not the prefix.
                RowLayout {
                    Layout.fillWidth: true
                    visible: shell.buildVersion.length > 0
                    spacing: 6

                    LogosText {
                        text: "Version"
                        font.pixelSize: 18
                        color: Theme.tokenOr("text", "#ffffff")
                    }

                    LogosSelectableText {
                        Layout.fillWidth: true
                        text: shell.buildVersion
                        font.pixelSize: 18
                        color: Theme.tokenOr("text", "#ffffff")
                        wrapMode: TextEdit.WrapAnywhere
                    }
                }

                LogosText {
                    text: shell.isPortableBuild ? "Portable build" : "Dev build"
                    font.pixelSize: 14
                    color: Theme.tokenOr("textSecondary", "#a0a0a0")
                }
            }

            // Commit hashes for basecamp + all flake dependencies.
            ColumnLayout {
                Layout.fillWidth: true
                Layout.topMargin: 10
                spacing: 4

                LogosText {
                    text: "Commits"
                    font.pixelSize: 16
                    font.weight: Font.Bold
                    color: Theme.tokenOr("text", "#ffffff")
                }

                Repeater {
                    model: shell.buildCommits
                    delegate: GridLayout {
                        Layout.fillWidth: true
                        Layout.bottomMargin: root.narrow ? 6 : 0
                        columns: root.narrow ? 1 : 2
                        columnSpacing: 12
                        rowSpacing: 0

                        // Module name and commit hash are both copy targets, so
                        // they wrap instead of eliding — an elided hash cannot
                        // be selected in full.
                        LogosSelectableText {
                            text: modelData.name
                            color: Theme.tokenOr("textSecondary", "#a0a0a0")
                            font.pixelSize: 13
                            Layout.preferredWidth: root.narrow ? -1 : 260
                            Layout.fillWidth: root.narrow
                            Layout.alignment: Qt.AlignTop
                            wrapMode: TextEdit.WrapAnywhere
                        }
                        LogosSelectableText {
                            text: modelData.commit
                            color: Theme.tokenOr("textSubtle", "#d0d0d0")
                            font.family: Theme.typography.mono
                            font.pixelSize: 13
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignTop
                            wrapMode: TextEdit.WrapAnywhere
                        }
                    }
                }
            }

            Item { Layout.fillHeight: true }
        }
    }
}
