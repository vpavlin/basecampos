import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Logos.Theme
import Logos.Controls
import Logos.Icons

// Settings: grouped rows with chevrons, values and switches. Rule: one column
// at every size; on wide windows the column stops growing (max 720) and
// centres, so rows never become a 1200 px line to scan.
Flickable {
    id: page
    readonly property string title: "Settings"
    contentHeight: column.implicitHeight + 2 * margin
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    readonly property int margin: Theme.pageMargin(width)

    component Group: ColumnLayout {
        property string heading
        default property alias rows: box.data
        spacing: Theme.spacing.small
        Layout.fillWidth: true
        LogosText {
            text: heading
            font.pixelSize: Theme.typography.secondaryText
            font.weight: Font.DemiBold
            color: Theme.palette.textSecondary
            Layout.leftMargin: Theme.spacing.large
        }
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: box.implicitHeight
            radius: Theme.spacing.radiusXlarge
            color: Theme.palette.backgroundSecondary
            clip: true
            ColumnLayout { id: box; width: parent.width; spacing: 0 }
        }
    }

    ColumnLayout {
        id: column
        x: Math.max(page.margin, (page.width - width) / 2)
        y: page.margin
        width: Math.min(page.width - 2 * page.margin, 720)
        spacing: Theme.spacing.xlarge

        Group {
            heading: "Account"
            LogosListRow { Layout.fillWidth: true; title: "Profile"; subtitle: "Name, avatar, bio"; showChevron: true }
            LogosListRow { Layout.fillWidth: true; title: "Keys"; value: "Keycard"; showChevron: true }
            LogosListRow { Layout.fillWidth: true; title: "Backups"; value: "Never"; showChevron: true; showSeparator: false }
        }
        Group {
            heading: "Network"
            LogosListRow { Layout.fillWidth: true; title: "Network"; value: "Testnet"; showChevron: true }
            LogosListRow {
                Layout.fillWidth: true; title: "Run a node"; subtitle: "Uses about 2 GB of disk"
                accessory: LogosSwitch { checked: true }
            }
            LogosListRow {
                Layout.fillWidth: true; title: "Use Tor for messages"; showSeparator: false
                accessory: LogosSwitch {}
            }
        }
        Group {
            heading: "Notifications"
            LogosListRow {
                Layout.fillWidth: true; title: "Show on the desktop"
                accessory: LogosSwitch { checked: true }
            }
            LogosListRow { Layout.fillWidth: true; title: "Apps"; value: "4 apps"; showChevron: true; showSeparator: false }
        }
        Group {
            heading: "Danger zone"
            LogosListRow { Layout.fillWidth: true; title: "Reset Basecamp"; destructive: true; showSeparator: false }
        }
    }
}
