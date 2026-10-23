import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Logos.Controls
import Logos.Icons
import Logos.Theme
// Shared with the shell (its welcome page and install dialog use them).
import Basecamp.AppManager

import Basecamp.Icons

Item {
    id: root

    property alias installStateIndex: stateTabBar.currentIndex
    property string viewMode: "grid"
    property bool loading: false
    property bool showUnavailable: false
    // Compact (a phone): the tabs and one ⋯ menu holding Reload, Manage
    // Repositories, Show unavailable and the view mode. Touch: 44 px targets.
    // showLabels: the "Apps" title and "View:" label (they fit when expanded).
    property bool compact: false
    property bool touch: false
    property bool showLabels: true
    readonly property int controlHeight: touch ? Theme.minTouchTarget : 40

    signal reloadClicked()
    signal repositoriesClicked()
    signal viewModeChangeRequested(string mode)
    signal showUnavailableRequested(bool show)

    implicitHeight: panelHeader.implicitHeight

    GridLayout {
        id: panelHeader
        anchors.left: parent.left
        anchors.right: parent.right
        columnSpacing: Theme.spacing.large
        rowSpacing: Theme.spacing.medium
        columns: (leftHalf.implicitWidth + rightHalf.implicitWidth + columnSpacing) <= width
                 ? 2 : 1

        RowLayout {
            id: leftHalf
            Layout.fillWidth: true
            spacing: Theme.spacing.large

            LogosText {
                visible: !root.compact
                text: qsTr("Apps")
                font.pixelSize: Theme.typography.panelTitleText
                font.weight: Theme.typography.weightMedium
                color: Theme.palette.text
            }

            LogosTabBar {
                id: stateTabBar
                spacing: Theme.spacing.large

                LogosTabButton { objectName: "appManager.tab.all"; text: qsTr("All"); iconSource: LogosIcons.pages }
                LogosTabButton { objectName: "appManager.tab.installed"; text: qsTr("Installed") }
                LogosTabButton { objectName: "appManager.tab.notInstalled"; text: qsTr("Not Installed") }
            }

            Item { Layout.fillWidth: true }

            // Compact: everything else behind one visible ⋯.
            LogosIconButton {
                id: moreButton
                objectName: "appManager.moreButton"
                visible: root.compact
                Layout.preferredWidth: Theme.minTouchTarget
                Layout.preferredHeight: Theme.minTouchTarget
                size: Theme.minTouchTarget
                flat: true
                iconSource: LogosIcons.more
                Accessible.name: qsTr("More")
                onClicked: moreMenu.popupUnder(moreButton)
            }

            LogosMenu {
                id: moreMenu
                objectName: "appManager.moreMenu"

                LogosMenuItem {
                    objectName: "appManager.more.reload"
                    text: qsTr("Reload")
                    enabled: !root.loading
                    onTriggered: root.reloadClicked()
                }
                LogosMenuItem {
                    objectName: "appManager.more.repositories"
                    text: qsTr("Manage Repositories")
                    onTriggered: root.repositoriesClicked()
                }
                LogosMenuSeparator {}
                LogosMenuItem {
                    objectName: "appManager.more.showUnavailable"
                    checkable: true
                    text: qsTr("Show unavailable")
                    checked: root.showUnavailable
                    onTriggered: root.showUnavailableRequested(checked)
                }
                LogosMenuItem {
                    objectName: "appManager.more.viewMode"
                    text: root.viewMode === "grid" ? qsTr("Show as list") : qsTr("Show as grid")
                    onTriggered: root.viewModeChangeRequested(root.viewMode === "grid" ? "list" : "grid")
                }
            }
        }

        RowLayout {
            id: rightHalf
            visible: !root.compact
            Layout.fillWidth: true
            spacing: Theme.spacing.medium

            Item { Layout.fillWidth: panelHeader.columns === 2 }

            LogosIconButton {
                id: filterButton
                objectName: "appManager.filterButton"
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredWidth: root.controlHeight
                Layout.preferredHeight: root.controlHeight
                size: root.controlHeight
                iconSize: 16
                iconSource: BasecampIcons.filter
                iconColor: root.showUnavailable
                           ? Theme.palette.primary
                           : Theme.palette.text
                onClicked: filterMenu.popupUnder(filterButton)

                background: Rectangle {
                    radius: Theme.spacing.radiusLarge
                    color: (root.showUnavailable || filterButton.hovered
                            || filterButton.pressed || filterButton.activeFocus)
                           ? Theme.palette.backgroundMuted
                           : "transparent"
                }

                LogosToolTip {
                    text: root.showUnavailable
                          ? qsTr("Filters: showing unavailable")
                          : qsTr("Filter apps")
                    placement: LogosToolTip.Bottom
                    visible: filterButton.hovered && !filterMenu.visible
                }
            }

            LogosMenu {
                id: filterMenu
                objectName: "appManager.filterMenu"

                LogosMenuItem {
                    objectName: "appManager.showUnavailableItem"
                    checkable: true
                    text: qsTr("Show unavailable")
                    checked: root.showUnavailable
                    onTriggered: root.showUnavailableRequested(checked)
                }
            }

            LogosButton {
                id: reloadBtn
                objectName: "appManager.reloadButton"
                Layout.fillWidth: true
                Layout.minimumWidth: 80
                // Touch padding needs more than 100 for "Reload" to show whole.
                Layout.preferredWidth: root.touch ? implicitWidth : 100
                Layout.maximumWidth: root.touch ? implicitWidth : 100
                Layout.preferredHeight: root.controlHeight
                radius: Theme.spacing.radiusLarge
                text: qsTr("Reload")
                leadingIcon.source: LogosIcons.refresh
                leadingIcon.size: 18
                enabled: !root.loading
                onClicked: root.reloadClicked()
            }

            LogosButton {
                Layout.fillWidth: true
                Layout.minimumWidth: 100
                Layout.preferredWidth: implicitWidth
                Layout.maximumWidth: implicitWidth
                Layout.preferredHeight: root.controlHeight
                radius: Theme.spacing.radiusLarge
                text: qsTr("Manage Repositories")
                onClicked: root.repositoriesClicked()
            }

            RowLayout {
                // Touch: the 20 px glyphs sit in 44 px hit areas, closer together.
                spacing: root.touch ? 0 : 24
                Layout.preferredHeight: root.touch ? Theme.minTouchTarget : 36

                LogosText {
                    visible: root.showLabels
                    Layout.alignment: Qt.AlignVCenter
                    verticalAlignment: Text.AlignVCenter
                    text: qsTr("View:")
                    color: Theme.palette.textTertiary
                }

                LogosIconButton {
                    objectName: "appManager.viewGrid"
                    Accessible.name: qsTr("Grid view")
                    iconSource: LogosIcons.grid
                    size: root.touch ? Theme.minTouchTarget : 20
                    iconSize: 20
                    iconColor: root.viewMode === "grid"
                               ? Theme.palette.text
                               : Theme.palette.textTertiary
                    onClicked: root.viewModeChangeRequested("grid")
                    background: Item {}
                }

                LogosIconButton {
                    objectName: "appManager.viewList"
                    Accessible.name: qsTr("List view")
                    iconSource: LogosIcons.list
                    size: root.touch ? Theme.minTouchTarget : 20
                    iconSize: 20
                    iconColor: root.viewMode === "list"
                               ? Theme.palette.text
                               : Theme.palette.textTertiary
                    onClicked: root.viewModeChangeRequested("list")
                    background: Item {}
                }
            }
        }
    }
}
