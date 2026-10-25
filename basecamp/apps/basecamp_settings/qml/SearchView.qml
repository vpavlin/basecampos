import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Logos.Controls
import Logos.Theme
import Basecamp.Icons

// Settings → Search (SHELL-OBJECTS-SPEC.md §8): every app whose manifest
// declares `basecamp.objects.publish`, with a switch. Off deletes everything
// that app has shared at once, and nothing it sends while off is kept; the app
// is not told. Laid out like Notifications.
Item {
    id: root

    readonly property var apps: shell ? shell.searchApps : []

    ColumnLayout {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.margins: Theme.spacing.large
        width: Math.min(parent.width - 2 * Theme.spacing.large, 720)
        spacing: Theme.spacing.medium

        LogosText {
            text: qsTr("Search")
            font.pixelSize: 20
            font.weight: Font.DemiBold
        }
        LogosText {
            Layout.fillWidth: true
            text: qsTr("Apps that share things with search and the home screen: transactions, messages, communities, their notifications and more. It stays on this device and is forgotten when Basecamp quits. Turn an app off and what it shared is deleted at once.")
            color: Theme.palette.textSecondary
            font.pixelSize: Theme.typography.secondaryText
            wrapMode: Text.Wrap
        }

        LogosText {
            Layout.topMargin: Theme.spacing.medium
            text: qsTr("Apps")
            font.weight: Font.DemiBold
        }

        LogosText {
            objectName: "settings.search.empty"
            visible: root.apps.length === 0
            Layout.topMargin: Theme.spacing.medium
            Layout.fillWidth: true
            text: qsTr("No installed app shares anything with search yet.")
            color: Theme.palette.textSecondary
            wrapMode: Text.Wrap
        }

        ListView {
            objectName: "settings.search.list"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.topMargin: Theme.spacing.small
            clip: true
            spacing: Theme.spacing.small
            model: root.apps

            delegate: Rectangle {
                id: appRow
                required property var modelData
                objectName: "settings.search." + modelData.name
                width: ListView.view.width
                height: 56
                radius: Theme.spacing.radiusLarge
                color: Theme.palette.backgroundSecondary

                // The whole row is the tap target.
                MouseArea {
                    anchors.fill: parent
                    onClicked: shell.setSearchEnabled(appRow.modelData.name, !appRow.modelData.enabled)
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spacing.medium
                    anchors.rightMargin: Theme.spacing.medium
                    spacing: Theme.spacing.medium

                    BasecampAppTile {
                        Layout.preferredWidth: size
                        Layout.preferredHeight: size
                        size: 28
                        label: modelData.displayName || modelData.name || ""
                        source: modelData.icon || ""
                        fullBleed: modelData.supportsFullBleedIcon === true
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        LogosText {
                            Layout.fillWidth: true
                            text: modelData.displayName
                            elide: Text.ElideRight
                        }
                        LogosText {
                            objectName: "settings.search.count." + modelData.name
                            Layout.fillWidth: true
                            text: !modelData.enabled ? qsTr("Off: nothing kept")
                                : modelData.count === 0 ? qsTr("Nothing shared right now")
                                : modelData.count === 1 ? qsTr("1 item shared")
                                : qsTr("%1 items shared").arg(modelData.count)
                            color: Theme.palette.textSecondary
                            font.pixelSize: Theme.typography.secondaryText
                            elide: Text.ElideRight
                        }
                    }
                    LogosSwitch {
                        objectName: "settings.search.switch." + modelData.name
                        Layout.preferredWidth: Math.max(implicitWidth, Theme.minTouchTarget)
                        Layout.preferredHeight: Theme.minTouchTarget
                        checked: modelData.enabled
                        onToggled: shell.setSearchEnabled(modelData.name, checked)
                    }
                }
            }
        }
    }
}
