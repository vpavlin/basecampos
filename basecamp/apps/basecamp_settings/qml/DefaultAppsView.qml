import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Logos.Controls
import Logos.Theme

// Settings → Default apps (SHELL-ROLES-SPEC §3, ADR-067): which installed app
// fills each of the shell's roles. Basecamp's own app is the default; any app
// whose manifest claims a role can be chosen. Installing an app never takes a
// role by itself: only this screen does.
Item {
    id: root

    readonly property var roleRows: [
        { role: "packages", label: qsTr("Packages"),
          hint: qsTr("Installs, updates and removes apps. Whichever app you choose may ask to uninstall or downgrade packages.") },
        { role: "settings", label: qsTr("Settings"),
          hint: qsTr("This screen. The app you choose can change Basecamp's preferences and default apps.") },
        { role: "search", label: qsTr("Search"), allowsNone: true,
          hint: qsTr("Ranks what apps share with Basecamp when you search or open Home. The app you choose reads everything shared in Settings → Search. With none, Basecamp ranks by itself.") }
    ]

    Flickable {
        anchors.fill: parent
        contentHeight: column.implicitHeight + 2 * Theme.spacing.large
        clip: true

        ColumnLayout {
            id: column
            x: Theme.spacing.large
            y: Theme.spacing.large
            width: Math.min(root.width - 2 * Theme.spacing.large, 720)
            spacing: Theme.spacing.medium

            LogosText {
                text: qsTr("Default apps")
                font.pixelSize: 20
                font.weight: Font.DemiBold
            }
            LogosText {
                Layout.fillWidth: true
                text: qsTr("Basecamp opens these apps for its own buttons and menus. Any installed app that says it can do the job can replace Basecamp's.")
                color: Theme.palette.textSecondary
                font.pixelSize: Theme.typography.secondaryText
                wrapMode: Text.Wrap
            }

            Repeater {
                model: root.roleRows

                delegate: ColumnLayout {
                    id: roleBlock
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.topMargin: Theme.spacing.medium
                    spacing: Theme.spacing.small

                    // Re-read whenever any role changes hands.
                    readonly property var candidates: { shell.roles; return shell.roleCandidates(modelData.role) }

                    LogosText {
                        text: roleBlock.modelData.label
                        font.weight: Font.DemiBold
                    }
                    LogosText {
                        Layout.fillWidth: true
                        text: roleBlock.modelData.hint
                        color: Theme.palette.textSecondary
                        font.pixelSize: Theme.typography.secondaryText
                        wrapMode: Text.Wrap
                    }
                    LogosText {
                        visible: roleBlock.candidates.length === 0 && !roleBlock.modelData.allowsNone
                        text: qsTr("No installed app can do this.")
                        color: Theme.palette.textSecondary
                    }

                    Repeater {
                        model: roleBlock.candidates

                        delegate: Rectangle {
                            id: row
                            required property var modelData
                            objectName: "settings.defaultApps." + roleBlock.modelData.role + "." + modelData.name
                            Layout.fillWidth: true
                            Layout.preferredHeight: 56
                            radius: Theme.spacing.radiusLarge
                            color: modelData.current ? Theme.palette.backgroundTertiary
                                                     : Theme.palette.backgroundSecondary
                            border.width: modelData.current ? 1 : 0
                            border.color: Theme.palette.primary

                            MouseArea {
                                anchors.fill: parent
                                onClicked: shell.assignRole(roleBlock.modelData.role, row.modelData.name)
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: Theme.spacing.medium
                                anchors.rightMargin: Theme.spacing.medium
                                spacing: Theme.spacing.medium

                                Image {
                                    Layout.preferredWidth: 28
                                    Layout.preferredHeight: 28
                                    source: row.modelData.icon || ""
                                    sourceSize: Qt.size(56, 56)
                                    fillMode: Image.PreserveAspectFit
                                    visible: status === Image.Ready
                                }
                                LogosText {
                                    Layout.fillWidth: true
                                    text: row.modelData.displayName
                                          + (row.modelData.isDefault ? "  ·  " + qsTr("Basecamp's") : "")
                                    elide: Text.ElideRight
                                }
                                LogosText {
                                    visible: row.modelData.current
                                    text: qsTr("In use")
                                    color: Theme.palette.primary
                                    font.pixelSize: Theme.typography.secondaryText
                                }
                            }
                        }
                    }

                    // "No app" for a role the shell can do itself (search).
                    Rectangle {
                        id: noneRow
                        readonly property bool current: (shell.roles[roleBlock.modelData.role] || "") === ""
                        visible: roleBlock.modelData.allowsNone === true
                        objectName: "settings.defaultApps." + roleBlock.modelData.role + ".none"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 56
                        radius: Theme.spacing.radiusLarge
                        color: current ? Theme.palette.backgroundTertiary : Theme.palette.backgroundSecondary
                        border.width: current ? 1 : 0
                        border.color: Theme.palette.primary

                        MouseArea {
                            anchors.fill: parent
                            onClicked: shell.assignRole(roleBlock.modelData.role, "")
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: Theme.spacing.medium
                            anchors.rightMargin: Theme.spacing.medium
                            spacing: Theme.spacing.medium

                            LogosText {
                                Layout.fillWidth: true
                                text: qsTr("None: Basecamp ranks by itself")
                                elide: Text.ElideRight
                            }
                            LogosText {
                                visible: noneRow.current
                                text: qsTr("In use")
                                color: Theme.palette.primary
                                font.pixelSize: Theme.typography.secondaryText
                            }
                        }
                    }
                }
            }
        }
    }
}
