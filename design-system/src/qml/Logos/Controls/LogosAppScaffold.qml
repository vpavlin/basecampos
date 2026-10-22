import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Logos.Theme
import Logos.Icons

// An app's frame that adapts to its width (docs/RESPONSIVE.md):
//   expanded (≥ 840)  a sidebar with labels
//   medium   (≥ 600)  a rail of icons with short labels
//   compact  (< 600)  a top bar with ☰ that opens the same list in a drawer;
//                     with `canGoBack`, a back chevron replaces ☰
// Put the app's content inside it; read `windowClass` to adapt the content.
//
//   LogosAppScaffold {
//       title: "Mail"
//       navModel: [{ id: "inbox", label: "Inbox", icon: LogosIcons.list, badge: 3 }]
//       currentNav: "inbox"
//       onNavActivated: id => currentNav = id
//       MyPage { anchors.fill: parent }
//   }
Item {
    id: root

    property string title: ""            // the app's name (sidebar header)
    property string pageTitle: title     // the current page (top bar)
    property var navModel: []            // [{ id, label, icon, badge }]
    property string currentNav: ""
    property bool canGoBack: false       // compact: show ‹ instead of ☰
    // Items placed at the right end of the top bar (icon buttons).
    property list<Item> actions
    // Search in compact: a 🔍 at the right of the top bar that unfolds across
    // it (LogosSearchToggle). Pages show their own search bar when wider and
    // read `searchText` when compact.
    property bool searchEnabled: false
    property string searchPlaceholder: qsTr("Search")
    readonly property string searchText: compact && searchEnabled ? searchToggle.text : ""
    default property alias content: contentArea.data

    readonly property string windowClass: Theme.windowClass(width)
    readonly property bool compact: windowClass === "compact"

    // A compact window floors every DS tappable at Theme.minTouchTarget, at
    // any density (Theme.touch). This engine's Theme only: a screen hosted in
    // its own QML engine sets Theme.compactWindow itself (see Theme.qml).
    Binding {
        target: Theme
        property: "compactWindow"
        // Visibility is part of the value, not a `when`: a Binding switched
        // off and on again by `when` did not re-apply its value (Qt 6.9).
        value: root.visible && root.width > 0 && root.compact
    }
    // A Binding does not restore its target when it is destroyed.
    Component.onDestruction: Theme.compactWindow = false
    readonly property alias drawer: navDrawer

    signal navActivated(string id)
    signal backRequested()

    readonly property int _sidebarWidth: 240
    readonly property int _railWidth: 80
    readonly property int _barHeight: 56

    // ── Navigation, shared by sidebar, rail and drawer ──────────────────────
    component NavItem: AbstractButton {
        id: item
        required property var modelData
        property bool rail: false
        readonly property bool current: modelData.id === root.currentNav

        implicitHeight: rail ? 64 : Math.max(Theme.minTouchTarget, 44)
        hoverEnabled: true
        Accessible.name: modelData.label
        onClicked: {
            root.navActivated(modelData.id)
            navDrawer.close()
        }

        background: Rectangle {
            radius: Theme.spacing.radiusLarge
            color: item.current ? Theme.components.listItem.backgroundSelected
                 : item.pressed ? Theme.components.listItem.backgroundPressed
                 : item.hovered ? Theme.components.listItem.backgroundHover : "transparent"
        }
        contentItem: Item {
            // Sidebar / drawer: icon + label in a row.
            RowLayout {
                visible: !item.rail
                anchors.fill: parent
                anchors.leftMargin: Theme.spacing.medium
                anchors.rightMargin: Theme.spacing.medium
                spacing: Theme.spacing.medium
                LogosIcon {
                    visible: !!item.modelData.icon
                    source: item.modelData.icon || ""
                    color: item.current ? Theme.palette.text : Theme.palette.textSecondary
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 20
                }
                LogosText {
                    Layout.fillWidth: true
                    text: item.modelData.label
                    elide: Text.ElideRight
                    color: item.current ? Theme.palette.text : Theme.palette.textSecondary
                    font.weight: item.current ? Font.DemiBold : Font.Normal
                }
                Badge { count: item.modelData.badge || 0 }
            }
            // Rail: icon over a short label.
            ColumnLayout {
                visible: item.rail
                anchors.centerIn: parent
                spacing: 4
                Item {
                    Layout.alignment: Qt.AlignHCenter
                    implicitWidth: 22
                    implicitHeight: 22
                    LogosIcon {
                        anchors.fill: parent
                        source: item.modelData.icon || ""
                        color: item.current ? Theme.palette.text : Theme.palette.textSecondary
                    }
                    Badge {
                        count: item.modelData.badge || 0
                        anchors { left: parent.right; bottom: parent.top; leftMargin: -8; bottomMargin: -8 }
                    }
                }
                LogosText {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.maximumWidth: root._railWidth - 8
                    text: item.modelData.label
                    elide: Text.ElideRight
                    font.pixelSize: 11
                    color: item.current ? Theme.palette.text : Theme.palette.textSecondary
                }
            }
        }
    }

    component Badge: Rectangle {
        property int count: 0
        visible: count > 0
        implicitWidth: Math.max(18, badgeText.implicitWidth + 10)
        implicitHeight: 18
        radius: 9
        color: Theme.palette.primary
        LogosText {
            id: badgeText
            anchors.centerIn: parent
            text: parent.count > 99 ? "99+" : parent.count
            font.pixelSize: 11
            font.weight: Font.DemiBold
            color: Theme.palette.primaryForeground
        }
    }

    component NavList: ColumnLayout {
        property bool rail: false
        spacing: 2
        Repeater {
            model: root.navModel
            delegate: NavItem {
                rail: parent.rail
                Layout.fillWidth: true
            }
        }
    }

    // ── Layout ──────────────────────────────────────────────────────────────
    Rectangle {
        id: side
        visible: !root.compact
        width: root.windowClass === "expanded" ? root._sidebarWidth : root._railWidth
        anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
        color: Theme.palette.backgroundSecondary

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.spacing.small
            spacing: Theme.spacing.small
            LogosText {
                visible: root.windowClass === "expanded"
                Layout.fillWidth: true
                Layout.leftMargin: Theme.spacing.medium
                Layout.topMargin: Theme.spacing.medium
                Layout.bottomMargin: Theme.spacing.small
                text: root.title
                font.pixelSize: 18
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
            NavList {
                Layout.fillWidth: true
                rail: root.windowClass !== "expanded"
            }
            Item { Layout.fillHeight: true }
        }
        Rectangle {
            anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
            width: 1
            color: Theme.palette.borderSubtle
        }
    }

    Rectangle {
        id: topBar
        anchors { left: side.visible ? side.right : parent.left; right: parent.right; top: parent.top }
        height: root._barHeight
        color: Theme.palette.background

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: root.compact ? Theme.spacing.tiny : Theme.pageMargin(root.width)
            anchors.rightMargin: Theme.spacing.tiny
            spacing: Theme.spacing.tiny

            LogosIconButton {
                objectName: "scaffold.navButton"
                visible: root.compact
                Layout.preferredWidth: Theme.minTouchTarget
                Layout.preferredHeight: Theme.minTouchTarget
                size: Theme.minTouchTarget
                flat: true
                iconSource: root.canGoBack ? LogosIcons.arrowLeft : LogosIcons.menu
                Accessible.name: root.canGoBack ? qsTr("Back") : qsTr("Menu")
                onClicked: root.canGoBack ? root.backRequested() : navDrawer.open()
            }
            LogosText {
                visible: !searchToggle.expanded
                Layout.fillWidth: true
                text: root.pageTitle
                font.pixelSize: root.compact ? 17 : 20
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
            LogosSearchToggle {
                id: searchToggle
                objectName: "scaffold.search"
                visible: root.compact && root.searchEnabled
                placeholderText: root.searchPlaceholder
            }
            Row {
                id: actionRow
                visible: !searchToggle.expanded
                spacing: Theme.spacing.tiny
                children: root.actions
            }
        }
        Rectangle {
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            height: 1
            color: Theme.palette.borderSubtle
        }
    }

    Item {
        id: contentArea
        anchors { left: topBar.left; right: parent.right; top: topBar.bottom; bottom: parent.bottom }
        clip: true
    }

    LogosDrawer {
        id: navDrawer
        objectName: "scaffold.drawer"
        width: Math.min(root.width * 0.85, 320)
        height: root.height
        edge: Qt.LeftEdge
        interactive: root.compact
        // Dim the page behind, so the drawer reads as on top of it.
        Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.5) }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.spacing.small
            spacing: Theme.spacing.small
            LogosText {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.spacing.medium
                Layout.topMargin: Theme.spacing.large
                Layout.bottomMargin: Theme.spacing.small
                text: root.title
                font.pixelSize: 18
                font.weight: Font.DemiBold
            }
            NavList { Layout.fillWidth: true }
            Item { Layout.fillHeight: true }
        }
    }
    // Growing out of compact with the drawer open: the sidebar takes over.
    onCompactChanged: if (!compact) navDrawer.close()
    // A page change starts with search folded and empty.
    onCurrentNavChanged: searchToggle.close()
    function openSearch() { if (compact && searchEnabled) searchToggle.open() }
}
