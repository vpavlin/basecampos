import QtQuick

import Logos.Theme
import Logos.Controls

// LogosToast — a LogosNotice that closes itself.
//
// Additional public API over LogosNotice:
//     duration              Auto-close after N ms. 0 disables it, leaving a toast
//                           that stays until dismissed. The countdown is held
//                           while the pointer is over the toast, so the message
//                           can be read and selected without it vanishing.
//                           A tap (or click) on the toast pins it: the
//                           countdown stops until it is dismissed or shown
//                           again, so touch users, who have no hover, can
//                           keep it too.
//     pinned                Read-only; true once tapped, until the next show().
//     show(title, message)  Open, updating either field if given, and arm the
//                           timer when duration > 0.
//
// A toast should not be the only place a user can read something they cannot
// recover — a path that has just been forgotten, a code they need to report.
// Set `duration: 0` for those.
//
//     LogosToast {
//         anchors.horizontalCenter: parent.horizontalCenter
//         anchors.bottom: parent.bottom
//         anchors.bottomMargin: Theme.spacing.large
//         severity: LogosNotice.Success
//         title: qsTr("Module published")
//         message: qsTr("logos-chat-ui 0.2.2 is live on the release channel.")
//     }
LogosNotice {
    id: root

    property int duration: 4000
    readonly property bool pinned: d.pinned

    closable: true
    shown: false

    function show(newTitle, newMessage) {
        if (newTitle !== undefined)
            root.title = newTitle
        if (newMessage !== undefined)
            root.message = newMessage
        root.shown = true
        d.pinned = false
        d.rearm()
    }

    onDismissed: d.autoHideTimer.stop()
    onShownChanged: d.rearm()
    HoverHandler {
        id: pointerOver
        onHoveredChanged: d.rearm()
    }
    // Tap to pin: the touch counterpart of holding the pointer over it.
    TapHandler {
        enabled: root.shown
        onTapped: {
            d.pinned = true
            d.rearm()
        }
    }

    QtObject {
        id: d

        property bool pinned: false

        function rearm() {
            if (root.shown && root.duration > 0 && !pointerOver.hovered && !d.pinned)
                d.autoHideTimer.restart()
            else
                d.autoHideTimer.stop()
        }

        readonly property Timer autoHideTimer: Timer {
            interval: root.duration
            repeat: false
            onTriggered: root.hide()
        }
    }

    opacity: shown ? 1 : 0
    Behavior on opacity {
        NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
    }

    transform: Translate {
        id: slide
        y: root.shown ? 0 : 8
        Behavior on y {
            NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
        }
    }
}
