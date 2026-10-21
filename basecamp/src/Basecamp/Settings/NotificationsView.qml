import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Logos.Controls
import Logos.Theme

// Settings → Notifications (docs/NOTIFICATIONS-SPEC.md §6): every app whose
// manifest declares `basecamp.notifications.post`, with a switch. Off means
// nothing from that app is shown or listed; the app is not told.
Item {
    id: root

    readonly property var apps: backend && backend.notificationApps ? backend.notificationApps : []

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacing.large
        spacing: Theme.spacing.medium

        LogosText {
            text: qsTr("Notifications")
            font.pixelSize: 20
            font.weight: Font.DemiBold
        }
        LogosText {
            Layout.fillWidth: true
            text: qsTr("Apps that can send you notifications. Turn one off and nothing from it is shown or kept in the list.")
            color: Theme.palette.textSecondary
            font.pixelSize: Theme.typography.secondaryText
            wrapMode: Text.Wrap
        }

        LogosText {
            visible: root.apps.length === 0
            Layout.topMargin: Theme.spacing.medium
            text: qsTr("No installed app sends notifications yet.")
            color: Theme.palette.textSecondary
        }

        ListView {
            objectName: "settings.notifications.list"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.topMargin: Theme.spacing.small
            clip: true
            spacing: Theme.spacing.small
            model: root.apps

            delegate: Rectangle {
                required property var modelData
                width: ListView.view.width
                height: 56
                radius: Theme.spacing.radiusLarge
                color: Theme.palette.backgroundSecondary

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spacing.medium
                    anchors.rightMargin: Theme.spacing.medium
                    spacing: Theme.spacing.medium

                    Image {
                        Layout.preferredWidth: 28
                        Layout.preferredHeight: 28
                        source: modelData.icon || ""
                        sourceSize: Qt.size(56, 56)
                        fillMode: Image.PreserveAspectFit
                        visible: status === Image.Ready
                    }
                    LogosText {
                        Layout.fillWidth: true
                        text: modelData.displayName
                        elide: Text.ElideRight
                    }
                    LogosSwitch {
                        objectName: "settings.notifications.switch." + modelData.name
                        checked: modelData.enabled
                        onToggled: backend.setNotificationsEnabled(modelData.name, checked)
                    }
                }
            }
        }
    }
}
