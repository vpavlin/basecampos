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

    // Responsive (RESPONSIVE.md §3.2): one column that stops at 720. Every
    // row is one tap target: tapping anywhere on it flips its switch, and the
    // switch itself is at least 44 x 44.

    function setDesktop(on) { backend.notifications.desktop = on }

    ColumnLayout {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.margins: Theme.spacing.large
        width: Math.min(parent.width - 2 * Theme.spacing.large, 720)
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

        // Hand-off to the desktop's notification centre (DesktopNotifier).
        Rectangle {
            Layout.fillWidth: true
            Layout.topMargin: Theme.spacing.small
            // Grows with the wrapped help text on a narrow pane.
            implicitHeight: Math.max(64, desktopRow.implicitHeight + 2 * Theme.spacing.medium)
            radius: Theme.spacing.radiusLarge
            color: Theme.palette.backgroundSecondary

            MouseArea {
                anchors.fill: parent
                enabled: !!(backend && backend.notifications)
                onClicked: root.setDesktop(!desktopSwitch.checked)
            }

            RowLayout {
                id: desktopRow
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: Theme.spacing.medium
                anchors.rightMargin: Theme.spacing.medium
                spacing: Theme.spacing.medium

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    LogosText {
                        Layout.fillWidth: true
                        text: qsTr("Show on the desktop")
                        elide: Text.ElideRight
                    }
                    LogosText {
                        Layout.fillWidth: true
                        text: qsTr("While Basecamp is in the background, notifications also appear in your system's notifications.")
                        color: Theme.palette.textSecondary
                        font.pixelSize: Theme.typography.secondaryText
                        wrapMode: Text.WordWrap
                    }
                }
                LogosSwitch {
                    id: desktopSwitch
                    objectName: "settings.notifications.desktop"
                    // The DS switch is 40 x 24; give it the 44 px hit area.
                    Layout.preferredWidth: Math.max(implicitWidth, Theme.minTouchTarget)
                    Layout.preferredHeight: Theme.minTouchTarget
                    checked: backend && backend.notifications ? backend.notifications.desktop : false
                    onToggled: backend.notifications.desktop = checked
                }
            }
        }

        LogosText {
            Layout.topMargin: Theme.spacing.medium
            text: qsTr("Apps")
            font.weight: Font.DemiBold
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
                id: appRow
                required property var modelData
                width: ListView.view.width
                height: 56
                radius: Theme.spacing.radiusLarge
                color: Theme.palette.backgroundSecondary

                MouseArea {
                    anchors.fill: parent
                    onClicked: backend.setNotificationsEnabled(appRow.modelData.name, !appRow.modelData.enabled)
                }

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
                        Layout.preferredWidth: Math.max(implicitWidth, Theme.minTouchTarget)
                        Layout.preferredHeight: Theme.minTouchTarget
                        checked: modelData.enabled
                        onToggled: backend.setNotificationsEnabled(modelData.name, checked)
                    }
                }
            }
        }
    }
}
