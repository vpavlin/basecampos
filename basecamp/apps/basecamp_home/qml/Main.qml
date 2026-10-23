import QtQuick

import Logos.Controls
import Logos.Theme

// Basecamp's New tab page as an ordinary app filling the `home` role
// (SHELL-ROLES-SPEC.md, ADR-067). The shell shows this view inside every New
// tab; the bar's search text arrives through `shell`, and opening something
// asks the shell to turn the New tab into it.
Rectangle {
    id: root
    objectName: "basecampHome.root"
    color: Theme.palette.background

    readonly property bool granted: typeof shell !== "undefined" && shell !== null
                                    && shell.roles.home === "basecamp_home"

    // The shell's own apps, by role rather than by a built-in screen.
    readonly property var roleForKey: ({
        "sys:applications": "launcher", "sys:settings": "settings", "sys:packages": "packages"
    })

    // Called by the shell (desktop New tab: focus the page's own search).
    function focusSearch() { if (page.item) page.item.focusSearch() }
    function clearSearch() { if (page.item) page.item.clearSearch() }

    Loader {
        id: page
        anchors.fill: parent
        active: root.granted
        sourceComponent: NewTabPage {
            id: newTab
            searchInBar: shell.searchInBar
            barQuery: shell.homeQuery
            onLaunchApp: name => shell.launch(name)
            onOpenSystemApp: key => shell.openRole(root.roleForKey[key] || "")

            Connections {
                target: shell
                function onHomeQueryAccepted() { newTab.openFirst() }
            }
        }
    }

    LogosText {
        objectName: "basecampHome.notGranted"
        visible: !root.granted
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, 420)
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        color: Theme.palette.textSecondary
        text: qsTr("Another app is set as Home. Choose Basecamp's again in Settings under Default apps.")
    }
}
