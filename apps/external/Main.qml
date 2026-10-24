import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Logos.Theme
import Logos.Controls

// Launcher for a native app (@NAME@) that runs outside Basecamp, full screen
// on its own workspace. Opening this app switches to it; the window that stays
// here offers the way back to it. Generated per app by the BasecampOS flake.
Item {
    id: root
    anchors.fill: parent

    readonly property string externalId: "@ID@"
    readonly property string appName: "@NAME@"
    property var bridge: typeof logos !== "undefined" ? logos : null
    property string message: ""

    function call(method, args) {
        if (!bridge || !bridge.callModule)
            return { error: "the basecampos_system module is not loaded" }
        try {
            let v = JSON.parse(String(bridge.callModule("basecampos_system", method, args || [])))
            return typeof v === "string" ? JSON.parse(v) : v
        } catch (e) {
            return { error: String(e) }
        }
    }

    function open() {
        let r = call("openExternal", [externalId])
        message = r.error || ""
    }

    Component.onCompleted: open()

    Rectangle { anchors.fill: parent; color: Theme.palette.background }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 2 * Theme.spacing.xxlarge, 520)
        spacing: Theme.spacing.large

        Image {
            source: "icon.png"
            Layout.preferredWidth: 96
            Layout.preferredHeight: 96
            Layout.alignment: Qt.AlignHCenter
            fillMode: Image.PreserveAspectFit
        }
        LogosText {
            text: root.appName
            font.pixelSize: Theme.typography.pageTitleText
            font.weight: Theme.typography.weightBold
            Layout.alignment: Qt.AlignHCenter
        }
        LogosText {
            text: root.appName + " runs full screen, next to Basecamp. Swipe with three fingers or press Super+Esc to come back here."
            color: Theme.palette.textSecondary
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
        }
        LogosButton {
            objectName: "openExternal"
            text: "Switch to " + root.appName
            variant: LogosButton.Variant.Primary
            Layout.alignment: Qt.AlignHCenter
            onClicked: root.open()
        }
        LogosText {
            visible: text.length > 0
            text: root.message
            color: Theme.palette.error
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
        }
    }
}
