import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Logos.Theme
import Logos.Controls

// Toasts and the notification list (docs/NOTIFICATIONS-SPEC.md §5).
//
// Floats top-right under the shell's bar, in its own translucent view sized to
// this item: zero height when there is nothing to show, so it never covers the
// page. The data is backend.notifications (NotificationCenter, host-side).
Item {
    id: root

    // The app in the current tab; its info/success toasts are not shown.
    property string currentApp: ""
    property bool panelOpen: false

    readonly property var center: backend ? backend.notifications : null
    // Compact (phone-width) layout: banners span the window, the list is a
    // sheet above the bottom bar. Set by the shell (MainContainer).
    property bool compact: false
    property int availableWidth: 1000
    property int availableHeight: 800
    readonly property int cardWidth: compact ? Math.max(200, availableWidth - 16) : 360

    function togglePanel() {
        panelOpen = !panelOpen
        if (panelOpen) {
            toasts.clear()
            if (center) center.markAllRead()
        }
    }
    function closePanel() { panelOpen = false }
    // BASECAMP_NOTIFY_SELFTEST=1: one local toast, to check the layer renders.
    function selfTest() {
        toasts.insert(0, { nid: "selftest", app: "basecamp", title: "Notifications self-test",
                           body: "If you can read this, toasts render on this display.",
                           level: "warning", tag: "", icon: "", actionLabel: "" })
    }

    function appLabel(app) {
        if (app === "basecamp") return "Basecamp"
        const name = backend && backend.displayNameFor ? backend.displayNameFor(app) : ""
        return name || app
    }
    function timeLabel(ms) {
        return Qt.formatTime(new Date(ms), "hh:mm")
    }

    width: cardWidth
    implicitWidth: cardWidth
    // Never zero: the view is sized to this item, and a zero-height view stops
    // rendering, so a toast added after that would never lay out.
    height: Math.max(1, panelOpen ? panel.implicitHeight : toastColumn.implicitHeight)
    implicitHeight: height

    Keys.onEscapePressed: closePanel()

    // ── Toasts ──────────────────────────────────────────────────────────
    ListModel { id: toasts }

    Connections {
        target: root.center
        function onPosted(n, quiet) {
            if (quiet || root.panelOpen) return
            const calm = n.level === "info" || n.level === "success"
            if (calm && n.app === root.currentApp) return
            for (let i = 0; i < toasts.count; ++i) {
                const t = toasts.get(i)
                if (n.tag !== "" && t.app === n.app && t.tag === n.tag) { toasts.remove(i); break }
            }
            toasts.insert(0, { nid: n.id, app: n.app, title: n.title, body: n.body,
                               level: n.level, tag: n.tag, icon: n.icon || "",
                               actionLabel: n.action ? n.action.label : "" })
            while (toasts.count > 3) toasts.remove(toasts.count - 1)
        }
        function onItemsChanged() {
            // A dismissed or cleared notification takes its toast with it.
            const live = {}
            for (const n of root.center.items) live[n.id] = true
            for (let i = toasts.count - 1; i >= 0; --i)
                if (!live[toasts.get(i).nid]) toasts.remove(i)
        }
    }

    // Not hidden while the list is open: the list clears the toasts instead. A
    // Column that is hidden and shown again stopped laying out new toasts.
    Column {
        id: toastColumn
        width: root.cardWidth
        spacing: 8

        Repeater {
            model: toasts
            delegate: Card {
                id: toastCard
                required property int index
                required property string nid
                required property string app
                required property string title
                required property string body
                required property string level
                required property string actionLabel
                required property string icon

                notificationId: nid
                iconUrl: icon
                appName: app
                titleText: title
                bodyText: body
                levelName: level
                action: actionLabel
                showClose: true
                onClosed: toasts.remove(index)

                Timer {
                    interval: 6000
                    running: (toastCard.levelName === "info" || toastCard.levelName === "success")
                             && !toastCard.hovered
                    onTriggered: toasts.remove(toastCard.index)
                }
            }
        }
    }

    // ── List ────────────────────────────────────────────────────────────
    Rectangle {
        id: panel
        visible: root.panelOpen
        width: root.cardWidth
        implicitHeight: Math.min(root.compact ? Math.max(240, root.availableHeight * 0.7) : 560,
                                 header.height + listCol.implicitHeight + 32)
        height: implicitHeight
        radius: Theme.spacing.radiusLarge
        color: Theme.palette.backgroundSecondary
        border.width: 1
        border.color: Theme.palette.borderSubtle

        RowLayout {
            id: header
            anchors { left: parent.left; right: parent.right; top: parent.top
                      leftMargin: 14; rightMargin: 14; topMargin: 12 }
            height: 24
            LogosText {
                Layout.fillWidth: true
                text: qsTr("Notifications")
                font.pixelSize: 15
                font.weight: Font.DemiBold
            }
            TextLink {
                visible: root.center && root.center.items.length > 0
                label: qsTr("Clear all")
                onActivated: root.center.clearAll()
            }
        }

        Flickable {
            anchors { left: parent.left; right: parent.right; top: header.bottom
                      bottom: parent.bottom; margins: 8 }
            contentHeight: listCol.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            Column {
                id: listCol
                width: parent.width
                spacing: 6

                LogosText {
                    visible: !root.center || root.center.items.length === 0
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    topPadding: 16
                    bottomPadding: 16
                    text: qsTr("No notifications")
                    color: Theme.palette.textSecondary
                }
                Repeater {
                    model: root.center ? root.center.items : []
                    delegate: Card {
                        required property var modelData
                        width: listCol.width
                        notificationId: modelData.id
                        appName: modelData.app
                        titleText: modelData.title
                        bodyText: modelData.body
                        levelName: modelData.level
                        iconUrl: modelData.icon || ""
                        action: modelData.action ? modelData.action.label : ""
                        timeText: root.timeLabel(modelData.time)
                        flat: true
                        showClose: true
                        onClosed: root.center.dismiss(modelData.id)
                    }
                }
            }
        }
    }

    // ── One notification ────────────────────────────────────────────────
    component Card: Rectangle {
        id: card
        property string notificationId
        property string appName
        property string titleText
        property string bodyText
        property string levelName: "info"
        property string iconUrl: ""
        property string action: ""
        property string timeText: ""
        property bool flat: false
        property bool showClose: false
        readonly property bool hovered: hover.hovered
        signal closed()

        width: root.cardWidth
        implicitHeight: content.implicitHeight + 20
        height: implicitHeight
        radius: Theme.spacing.radiusLarge
        color: flat ? (hover.hovered ? Theme.palette.surfaceRaised : Theme.palette.backgroundTertiary)
                    : Theme.palette.backgroundSecondary
        border.width: flat ? 0 : 1
        border.color: Theme.palette.borderSubtle

        HoverHandler { id: hover }
        TapHandler {
            onTapped: {
                if (root.center) root.center.activate(card.notificationId, false)
                root.closePanel()
            }
        }

        ColumnLayout {
            id: content
            anchors { left: parent.left; right: parent.right; top: parent.top
                      leftMargin: 14; rightMargin: 34; topMargin: 10 }
            spacing: 3

            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                Image {
                    Layout.preferredWidth: 16
                    Layout.preferredHeight: 16
                    source: card.iconUrl
                    sourceSize: Qt.size(32, 32)
                    fillMode: Image.PreserveAspectFit
                    visible: card.iconUrl !== "" && status === Image.Ready
                }
                LogosText {
                    Layout.fillWidth: true
                    text: root.appLabel(card.appName)
                    font.pixelSize: 11
                    color: Theme.palette.textSecondary
                    elide: Text.ElideRight
                }
                LogosText {
                    visible: card.timeText !== ""
                    text: card.timeText
                    font.pixelSize: 11
                    color: Theme.palette.textSecondary
                }
            }
            LogosText {
                Layout.fillWidth: true
                text: card.titleText
                textFormat: Text.PlainText
                font.pixelSize: 14
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
            }
            LogosText {
                Layout.fillWidth: true
                visible: card.bodyText !== ""
                text: card.bodyText
                textFormat: Text.PlainText
                font.pixelSize: 13
                color: Theme.palette.textSecondary
                wrapMode: Text.Wrap
                maximumLineCount: 4
                elide: Text.ElideRight
            }
            LogosButton {
                visible: card.action !== ""
                Layout.topMargin: 4
                text: card.action
                onClicked: {
                    if (root.center) root.center.activate(card.notificationId, true)
                    card.closed()
                    root.closePanel()
                }
            }
        }

        TextLink {
            visible: card.showClose
            anchors { right: parent.right; top: parent.top; rightMargin: 10; topMargin: 8 }
            label: "✕"
            pixelSize: root.compact ? 16 : 12
            onActivated: card.closed()
        }
    }

    // A text-only control: secondary text that brightens on hover. No box.
    component TextLink: LogosText {
        id: link
        property string label
        property int pixelSize: 13
        signal activated()
        text: label
        font.pixelSize: pixelSize
        color: linkHover.hovered ? Theme.palette.text : Theme.palette.textSecondary
        HoverHandler { id: linkHover; cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: link.activated() }
    }
}
