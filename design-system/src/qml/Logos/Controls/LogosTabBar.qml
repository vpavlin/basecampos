import QtQuick
import QtQuick.Controls

import Logos.Theme

// Themed TabBar that owns a sliding indicator beneath the active tab.
//
// Public API:
//     indicatorColor      color of the selected underline (default: Theme.palette.primary)
//     indicatorHeight     pixel height of the underline (default: 3)
//     animationDuration   slide duration in ms (default: 200)
//     trackColor          full-width baseline under all tabs
//                         (default: Theme.palette.borderHairline)
//
// Use with LogosTabButton.
//
// Example:
//     LogosTabBar {
//         id: bar
//         Layout.fillWidth: true
//         LogosTabButton { text: qsTr("Files");  iconSource: "qrc:/icons/files.svg"  }
//         LogosTabButton { text: qsTr("Images"); iconSource: "qrc:/icons/images.svg" }
//         LogosTabButton { text: qsTr("Videos") }
//     }
//
//     StackLayout {
//         Layout.fillWidth: true
//         Layout.fillHeight: true
//         currentIndex: bar.currentIndex
//
//         FilesView  {}
//         ImagesView {}
//         VideosView {}
//     }
TabBar {
    id: root

    property color indicatorColor: Theme.palette.primary
    property int indicatorHeight: 3
    property int animationDuration: 200
    property color trackColor: Theme.palette.borderHairline

    // Exposed for inspection (e.g., from tests). Read-only.
    readonly property Item indicatorItem: _indicatorItem
    readonly property Item trackItem: _trackItem
    // Filled in by the background once Qt creates it. Not bindings on
    // `background`: reading it creates the deferred background early (below).
    property Item _indicatorItem: null
    property Item _trackItem: null

    // The background is an inline component so that nothing in it has an id
    // in this file's scope, and nothing binds to `background`: either makes Qt
    // create the background ahead of the tabs instead of deferring it, which
    // loops on implicitWidth ("Binding loop detected for property
    // implicitWidth" when the bar sits in a layout at its implicit width).
    component IndicatorBackground: Item {
        id: bgItem
        required property TabBar bar
        readonly property alias indicator: indicator
        readonly property alias track: track

        clip: true

        Component.onCompleted: {
            bgItem.bar._indicatorItem = indicator
            bgItem.bar._trackItem = track
        }

        // Low-opacity baseline under every tab; selected segment uses indicatorColor.
        Rectangle {
            id: track
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: Math.max(1, Math.round(bgItem.bar.indicatorHeight / 3))
            color: bgItem.bar.trackColor
        }

        Rectangle {
            id: indicator

            z: 1
            height: bgItem.bar.indicatorHeight
            color: bgItem.bar.indicatorColor
            radius: height / 2 * Theme.spacing.roundness
            y: parent.height - height
            x: 0
            width: 0

            property bool ready: true
            visible: ready

            Behavior on x {
                enabled: indicator.ready
                NumberAnimation { duration: bgItem.bar.animationDuration; easing.type: Easing.OutCubic }
            }
            Behavior on width {
                enabled: indicator.ready
                NumberAnimation { duration: bgItem.bar.animationDuration; easing.type: Easing.OutCubic }
            }

            function refresh() {
                const bar = bgItem.bar
                if (!bar.currentItem || bar.currentItem.width <= 0)
                    return
                const p = bar.currentItem.mapToItem(bar, 0, 0)
                indicator.x = p.x
                indicator.width = bar.currentItem.width
            }
        }
    }

    background: IndicatorBackground { bar: root }

    function _refreshIndicator() {
        if (root.indicatorItem)
            root.indicatorItem.refresh()
    }

    onCurrentItemChanged: root._refreshIndicator()
    onWidthChanged: root._refreshIndicator()
    onVisibleChanged: if (visible) Qt.callLater(root._refreshIndicator)
    Component.onCompleted: Qt.callLater(root._refreshIndicator)

    Connections {
        target: root.currentItem
        ignoreUnknownSignals: true
        function onXChanged() { root._refreshIndicator() }
        function onWidthChanged() { root._refreshIndicator() }
    }

    Connections {
        target: root.contentItem
        ignoreUnknownSignals: true
        function onContentXChanged() { root._refreshIndicator() }
        function onWidthChanged() { root._refreshIndicator() }
    }
}
