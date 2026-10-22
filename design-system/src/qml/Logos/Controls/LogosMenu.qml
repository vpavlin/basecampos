import QtQuick
import QtQuick.Controls

import Logos.Theme

// LogosMenu — themed Menu.
//
// Public API (plus Menu's own count / items / open / close / popup):
//     borderColor / backgroundColor
//     popupUnder(anchor)  Open under anchor with a Theme.spacing.tiny (4px)
//                         gap below the control. Use this for button/toolbar
//                         menus so keyboard Space/Enter does not place the
//                         menu at the mouse cursor. For true context menus
//                         (right-click), keep using popup() / popup(pos).
//
// Placement: margins (default Theme.spacing.small) keep the whole menu inside
// the window, so one opened from a ⋯ at the right edge shifts left instead
// of running off it.
//
// Read-only inspection aliases:
//     backgroundItem
Menu {
    id: root

    property color borderColor: Theme.palette.border
    property color backgroundColor: Theme.palette.backgroundSecondary

    // Exposed for inspection (e.g., from tests). Read-only.
    readonly property alias backgroundItem: bg

    padding: Theme.spacing.tiny
    margins: Theme.spacing.small

    // Opens below the anchor with a tiny gap. Prefer over bare popup() when
    // the menu is triggered from a control (mouse or keyboard).
    function popupUnder(anchor) {
        const a = anchor ? anchor : root.parent
        if (!a) {
            root.popup()
            return
        }
        root.popup(a, 0, a.height + Theme.spacing.tiny)
    }

    background: Rectangle {
        id: bg
        implicitWidth: 200
        color: root.backgroundColor
        border.color: root.borderColor
        radius: Theme.spacing.radiusSmall
    }
}
