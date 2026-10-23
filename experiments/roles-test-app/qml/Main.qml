import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Shell-roles test app (SHELL-ROLES-SPEC.md). Claims the `packages` role and
// raises basecamp.packages.confirm_uninstall, which only the role's current
// holder may ask for: as a non-holder the answer is `unavailable`.
Rectangle {
    id: root
    objectName: "altPackages.root"
    color: "#1d2a24"
    property string result: "not asked yet"

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 32, 420)
        spacing: 16
        Label {
            objectName: "altPackages.title"
            text: "Alt Packages"
            color: "#e8f5ee"; font.pixelSize: 24; font.bold: true
            Layout.alignment: Qt.AlignHCenter
        }
        Label {
            text: "A stand-in app that claims the packages role."
            color: "#a8c4b6"; wrapMode: Text.WordWrap
            Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter
        }
        Button {
            objectName: "altPackages.ask"
            text: "Ask to uninstall responsive_lab"
            Layout.alignment: Qt.AlignHCenter
            onClicked: {
                root.result = "asking…"
                logos.request("basecamp.packages.confirm_uninstall",
                              { names: ["responsive_lab"] },
                              function (res) {
                                  root.result = res.ok ? "ok" : ("refused: " + res.error)
                              })
            }
        }
        Label {
            objectName: "altPackages.result"
            text: "Result: " + root.result
            color: "#e8f5ee"
            Layout.alignment: Qt.AlignHCenter
        }
    }
}
