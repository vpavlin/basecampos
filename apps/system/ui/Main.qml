import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Logos.Theme
import Logos.Controls

// BasecampOS System: the machine under Basecamp — power, sound, network.
//
// Everything goes through the basecampos_system core module: a ui_qml view is
// sandboxed and can run nothing itself. Calls are synchronous and return JSON;
// a failure carries an "error" field, shown on the line under the section.
//
// Colours and type come from Logos.Theme, so a Basecamp theme restyles this
// view like the shell around it.
Item {
    id: root
    anchors.fill: parent

    property var bridge: typeof logos !== "undefined" ? logos : null
    readonly property bool haveCore: !!bridge && !!bridge.callModule

    property var sys: ({})
    property var net: ({})
    property var vol: ({})
    property var light: ({})
    property var bt: ({})
    property var charge: ({})
    property var tab: ({})
    property var disp: ({})
    property var scl: ({})
    property var mic: ({})
    property var kbd: ({})
    property var share: ({})
    property string shotMessage: ""
    property bool btScanning: false
    property string btMessage: ""
    property var wifi: []
    property bool scanning: false
    property string powerArmed: ""     // the action waiting for a second tap
    property string selectedSsid: ""
    property string netMessage: ""
    property string powerMessage: ""

    function call(method, args) {
        if (!haveCore)
            return { error: "the basecampos_system module is not loaded" }
        try {
            let raw = String(bridge.callModule("basecampos_system", method, args || []))
            let v = JSON.parse(raw)
            // The bridge may hand back a JSON string that is itself quoted.
            return typeof v === "string" ? JSON.parse(v) : v
        } catch (e) {
            return { error: String(e) }
        }
    }

    // Every call below starts a program and blocks Basecamp's UI thread until
    // it returns, so the periodic refresh reads only the cheap values; the rest
    // loads on open, after an action, and from the Refresh button.
    function refreshLight() {
        vol = call("volume")
        light = call("brightness")
        sys = call("info")
    }
    function refresh() {
        refreshLight()
        net = call("network")
        charge = call("chargeLimit")
        tab = call("tablet")
        disp = call("displays")
        scl = call("scale")
        mic = call("microphone")
        kbd = call("keyboardLayout")
        share = call("screenShare")
        if (!btScanning)
            bt = call("bluetooth")
    }

    function scan() {
        scanning = true
        let r = call("wifiList")
        wifi = r.networks || []
        netMessage = r.error || ""
        scanning = false
    }

    function formatUptime(s) {
        if (!s) return ""
        let d = Math.floor(s / 86400), h = Math.floor(s % 86400 / 3600), m = Math.floor(s % 3600 / 60)
        return (d ? d + " d " : "") + (h ? h + " h " : "") + m + " min"
    }

    Component.onCompleted: { refresh(); scan() }
    Timer { interval: 10000; running: root.visible; repeat: true; onTriggered: root.refreshLight() }
    Timer { id: disarm; interval: 4000; onTriggered: root.powerArmed = "" }

    Rectangle { anchors.fill: parent; color: Theme.palette.background }

    component Section: Rectangle {
        default property alias content: col.data
        property string title
        Layout.fillWidth: true
        implicitHeight: col.implicitHeight + 2 * Theme.spacing.large
        radius: Theme.spacing.radiusLarge
        color: Theme.palette.surface
        border.color: Theme.palette.borderSubtle
        ColumnLayout {
            id: col
            anchors { fill: parent; margins: Theme.spacing.large }
            spacing: Theme.spacing.medium
            LogosText {
                text: title
                font.pixelSize: Theme.typography.subtitleText
                font.weight: Theme.typography.weightBold
            }
        }
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: Math.min(parent.width, 760)
            x: (parent.width - width) / 2
            spacing: Theme.spacing.large

            Item { Layout.preferredHeight: Theme.spacing.large }

            // --- About ------------------------------------------------------
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacing.large
                ColumnLayout {
                    spacing: Theme.spacing.tiny
                    LogosText {
                        text: root.sys.hostname || "BasecampOS"
                        font.pixelSize: Theme.typography.pageTitleText
                        font.weight: Theme.typography.weightBold
                    }
                    LogosText {
                        text: [root.sys.os, root.sys.kernel ? "Linux " + root.sys.kernel : ""].filter(s => s).join(" · ")
                        color: Theme.palette.textSecondary
                    }
                    LogosText {
                        text: "Up " + root.formatUptime(root.sys.uptime)
                              + (root.sys.battery ? " · Battery " + root.sys.battery.percent + "% (" + root.sys.battery.status + ")" : "")
                        color: Theme.palette.textTertiary
                        font.pixelSize: Theme.typography.secondaryText
                    }
                }
                Item { Layout.fillWidth: true }
                LogosButton {
                    objectName: "refresh"
                    text: "Refresh"
                    Layout.alignment: Qt.AlignTop
                    onClicked: root.refresh()
                }
            }

            // --- Power ------------------------------------------------------
            Section {
                title: "Power"
                RowLayout {
                    spacing: Theme.spacing.medium
                    Repeater {
                        model: [
                            { action: "poweroff", label: "Shut down" },
                            { action: "reboot", label: "Restart" },
                            { action: "suspend", label: "Sleep" }
                        ]
                        LogosButton {
                            required property var modelData
                            objectName: "power_" + modelData.action
                            text: root.powerArmed === modelData.action ? "Tap again to " + modelData.label.toLowerCase() : modelData.label
                            variant: root.powerArmed === modelData.action ? LogosButton.Variant.Primary : LogosButton.Variant.Secondary
                            onClicked: {
                                if (root.powerArmed !== modelData.action) {
                                    root.powerArmed = modelData.action
                                    disarm.restart()
                                    return
                                }
                                root.powerArmed = ""
                                let r = root.call("power", [modelData.action])
                                root.powerMessage = r.error || ""
                            }
                        }
                    }
                }
                LogosText {
                    visible: text.length > 0
                    text: root.powerMessage
                    color: Theme.palette.error
                    font.pixelSize: Theme.typography.secondaryText
                }
            }

            // --- Sound ------------------------------------------------------
            Section {
                title: "Sound"
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacing.medium
                    visible: !root.vol.error
                    LogosSlider {
                        id: volumeSlider
                        objectName: "volumeSlider"
                        Layout.fillWidth: true
                        from: 0; to: 100; stepSize: 1
                        value: root.vol.percent || 0
                        onMoved: root.call("setVolume", [Math.round(value)])
                    }
                    LogosText {
                        text: Math.round(volumeSlider.value) + "%"
                        Layout.preferredWidth: 48
                        color: Theme.palette.textSecondary
                    }
                    LogosSwitch {
                        objectName: "muteSwitch"
                        text: "Mute"
                        checked: !!root.vol.muted
                        onToggled: { root.call("toggleMute"); root.vol = root.call("volume") }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacing.medium
                    visible: !!root.mic.available
                    LogosText { text: "Microphone"; Layout.preferredWidth: 110 }
                    LogosSlider {
                        id: micSlider
                        objectName: "micSlider"
                        Layout.fillWidth: true
                        from: 0; to: 300; stepSize: 5
                        value: root.mic.percent || 0
                        onMoved: root.call("setMicrophone", [Math.round(value)])
                    }
                    LogosText { text: Math.round(micSlider.value) + "%"; Layout.preferredWidth: 48; color: Theme.palette.textSecondary }
                    LogosSwitch {
                        objectName: "micMute"
                        text: "Mute"
                        checked: !!root.mic.muted
                        onToggled: { root.call("toggleMicrophone"); root.mic = root.call("microphone") }
                    }
                }
                LogosText {
                    visible: !!root.vol.error
                    text: root.vol.error || ""
                    color: Theme.palette.textTertiary
                }
            }

            // --- Display ----------------------------------------------------
            Section {
                title: "Display"
                visible: !!root.light.available || !!root.tab.keyboard || !!root.tab.orientationSensor || !!root.disp.external || !!root.scl.available
                RowLayout {
                    Layout.fillWidth: true
                    visible: !!root.light.available
                    spacing: Theme.spacing.medium
                    LogosText { text: "Brightness"; Layout.preferredWidth: 110 }
                    LogosSlider {
                        id: lightSlider
                        objectName: "brightnessSlider"
                        Layout.fillWidth: true
                        from: 1; to: 100; stepSize: 1
                        value: root.light.percent || 50
                        onMoved: root.call("setBrightness", [Math.round(value)])
                    }
                    LogosText { text: Math.round(lightSlider.value) + "%"; Layout.preferredWidth: 48; color: Theme.palette.textSecondary }
                }
                RowLayout {
                    spacing: Theme.spacing.large
                    LogosSwitch {
                        objectName: "rotationLock"
                        visible: !!root.tab.orientationSensor
                        text: "Lock rotation"
                        checked: !!root.tab.rotationLocked
                        onToggled: { root.call("setRotationLock", [checked ? "on" : "off"]); root.tab = root.call("tablet") }
                    }
                    LogosButton {
                        objectName: "keyboardToggle"
                        visible: !!root.tab.keyboard
                        text: "On-screen keyboard"
                        onClicked: root.call("toggleKeyboard")
                    }
                }
                RowLayout {
                    visible: !!root.disp.external
                    LogosSwitch {
                        objectName: "mirrorSwitch"
                        text: "Mirror to " + (root.disp.external || "external display")
                        checked: !!root.disp.mirroring
                        onToggled: { root.call("setMirror", [checked ? "on" : "off"]); root.disp = root.call("displays") }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    visible: !!root.scl.available
                    spacing: Theme.spacing.medium
                    LogosText { text: "Scale"; Layout.preferredWidth: 110 }
                    LogosSlider {
                        id: scaleSlider
                        objectName: "scaleSlider"
                        Layout.fillWidth: true
                        from: 100; to: 250; stepSize: 10
                        value: root.scl.percent || 100
                        // applied on release: every step re-lays out the whole screen
                        onPressedChanged: if (!pressed) root.call("setScale", [Math.round(value)])
                    }
                    LogosText { text: (scaleSlider.value / 100).toFixed(1) + "×"; Layout.preferredWidth: 48; color: Theme.palette.textSecondary }
                }
                RowLayout {
                    spacing: Theme.spacing.large
                    LogosButton {
                        objectName: "screenshot"
                        text: "Screenshot"
                        onClicked: { let r = root.call("screenshot"); root.shotMessage = r.error || ("Saved " + r.path) }
                    }
                    LogosButton {
                        objectName: "keyboardLayout"
                        visible: !!root.kbd.available && root.kbd.layouts > 1
                        text: "Keyboard: " + (root.kbd.active || "") + " → next"
                        onClicked: { root.call("nextKeyboardLayout"); root.kbd = root.call("keyboardLayout") }
                    }
                    LogosSwitch {
                        objectName: "screenShare"
                        visible: !!root.share.available
                        text: "Share screen (VNC on the mesh)"
                        checked: !!root.share.active
                        onToggled: { root.call("setScreenShare", [checked ? "on" : "off"]); root.share = root.call("screenShare") }
                    }
                }
                LogosText {
                    visible: text.length > 0
                    text: root.shotMessage
                    color: Theme.palette.textTertiary
                    font.pixelSize: Theme.typography.secondaryText
                }
            }

            // --- Bluetooth --------------------------------------------------
            Section {
                title: "Bluetooth"
                visible: !!root.bt.available
                RowLayout {
                    Layout.fillWidth: true
                    LogosSwitch {
                        objectName: "btPower"
                        text: root.bt.powered ? "On" : "Off"
                        checked: !!root.bt.powered
                        onToggled: { root.call("bluetoothPower", [checked ? "on" : "off"]); root.bt = root.call("bluetooth") }
                    }
                    Item { Layout.fillWidth: true }
                    LogosButton {
                        objectName: "btScan"
                        text: root.btScanning ? "Scanning…" : "Scan"
                        enabled: !root.btScanning && !!root.bt.powered
                        onClicked: {
                            root.btScanning = true
                            root.bt = root.call("bluetoothScan")
                            root.btScanning = false
                        }
                    }
                }
                LogosText {
                    visible: !!root.bt.powered && (root.bt.devices || []).length === 0
                    text: "No devices yet. Put a device in pairing mode and press Scan."
                    color: Theme.palette.textTertiary
                }
                Repeater {
                    model: root.bt.powered ? (root.bt.devices || []) : []
                    RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        LogosText {
                            text: modelData.name
                            font.weight: modelData.connected ? Theme.typography.weightBold : Theme.typography.weightRegular
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                        LogosText {
                            text: modelData.connected ? "Connected" : (modelData.paired ? "Paired" : "")
                            color: modelData.connected ? Theme.palette.success : Theme.palette.textTertiary
                        }
                        LogosButton {
                            text: modelData.connected ? "Disconnect" : "Connect"
                            onClicked: {
                                root.btMessage = (modelData.connected ? "Disconnecting " : "Connecting ") + modelData.name + "…"
                                let r = root.call("bluetoothDevice", [modelData.mac, modelData.connected ? "disconnect" : "connect"])
                                root.btMessage = r.error || ""
                                root.bt = root.call("bluetooth")
                            }
                        }
                        LogosButton {
                            visible: !!modelData.paired
                            text: "Forget"
                            onClicked: { root.call("bluetoothDevice", [modelData.mac, "forget"]); root.bt = root.call("bluetooth") }
                        }
                    }
                }
                LogosText {
                    visible: text.length > 0
                    text: root.btMessage
                    color: Theme.palette.textSecondary
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
            }

            // --- Battery ----------------------------------------------------
            Section {
                title: "Battery"
                visible: !!root.sys.battery || !!root.charge.available
                LogosText {
                    visible: !!root.sys.battery
                    text: root.sys.battery ? root.sys.battery.percent + "% · " + root.sys.battery.status
                          + (root.sys.battery.minutes > 0
                             ? " · " + Math.floor(root.sys.battery.minutes / 60) + " h " + (root.sys.battery.minutes % 60) + " min "
                               + (root.sys.battery.status === "Charging" ? "to full" : "left")
                             : "") : ""
                    color: Theme.palette.textSecondary
                }
                RowLayout {
                    Layout.fillWidth: true
                    visible: !!root.charge.available
                    spacing: Theme.spacing.medium
                    LogosText { text: "Charge limit"; Layout.preferredWidth: 110 }
                    LogosSlider {
                        id: chargeSlider
                        objectName: "chargeLimit"
                        Layout.fillWidth: true
                        from: 50; to: 100; stepSize: 5
                        value: root.charge.upper || 100
                        onMoved: root.call("setChargeLimit", [Math.round(value)])
                    }
                    LogosText {
                        text: chargeSlider.value >= 100 ? "Off" : Math.round(chargeSlider.value) + "%"
                        Layout.preferredWidth: 48
                        color: Theme.palette.textSecondary
                    }
                }
            }

            // --- Network ----------------------------------------------------
            Section {
                title: "Network"
                LogosText {
                    text: root.net.error ? root.net.error
                        : "Status: " + (root.net.state || "unknown") + " · connectivity: " + (root.net.connectivity || "unknown")
                    color: root.net.error ? Theme.palette.error : Theme.palette.textSecondary
                }
                Repeater {
                    model: root.net.devices || []
                    LogosText {
                        required property var modelData
                        text: modelData.device + " (" + modelData.type + ") — " + modelData.state
                              + (modelData.connection ? " · " + modelData.connection : "")
                        color: Theme.palette.textTertiary
                        font.pixelSize: Theme.typography.secondaryText
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: !root.net.error
                    LogosSwitch {
                        objectName: "wifiSwitch"
                        text: "Wi-Fi"
                        checked: root.net.wifi === "enabled"
                        onToggled: { root.call("wifiRadio", [checked ? "on" : "off"]); root.refresh() }
                    }
                    Item { Layout.fillWidth: true }
                    LogosButton {
                        objectName: "wifiScan"
                        text: root.scanning ? "Scanning…" : "Scan"
                        enabled: !root.scanning && root.net.wifi === "enabled"
                        onClicked: root.scan()
                    }
                }

                LogosText {
                    visible: root.net.wifi === "enabled" && root.wifi.length === 0 && !root.scanning
                    text: "No Wi-Fi networks in range."
                    color: Theme.palette.textTertiary
                }

                Repeater {
                    model: root.net.wifi === "enabled" ? root.wifi : []
                    Rectangle {
                        required property var modelData
                        Layout.fillWidth: true
                        implicitHeight: row.implicitHeight + Theme.spacing.medium * 2
                        radius: Theme.spacing.radiusMedium
                        color: hover.hovered ? Theme.palette.surfaceInteractiveHover : "transparent"
                        HoverHandler { id: hover }
                        TapHandler { onTapped: root.selectedSsid = modelData.inUse ? "" : modelData.ssid }
                        ColumnLayout {
                            id: row
                            anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: Theme.spacing.medium }
                            RowLayout {
                                LogosText {
                                    text: modelData.ssid
                                    font.weight: modelData.inUse ? Theme.typography.weightBold : Theme.typography.weightRegular
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }
                                LogosText {
                                    text: (modelData.inUse ? "Connected · " : "")
                                          + (modelData.security ? "🔒 " : "") + modelData.signal + "%"
                                    color: modelData.inUse ? Theme.palette.success : Theme.palette.textTertiary
                                }
                            }
                            RowLayout {
                                visible: root.selectedSsid === modelData.ssid
                                Layout.fillWidth: true
                                LogosTextField {
                                    id: pass
                                    Layout.fillWidth: true
                                    visible: !!modelData.security
                                    placeholderText: "Password"
                                    echoMode: TextInput.Password
                                }
                                LogosButton {
                                    text: "Connect"
                                    variant: LogosButton.Variant.Primary
                                    onClicked: {
                                        root.netMessage = "Connecting to " + modelData.ssid + "…"
                                        let r = root.call("wifiConnect", [modelData.ssid, pass.text])
                                        root.netMessage = r.error || ""
                                        if (!r.error) { root.selectedSsid = ""; root.refresh(); root.scan() }
                                    }
                                }
                            }
                        }
                    }
                }

                LogosText {
                    visible: text.length > 0
                    text: root.netMessage
                    color: Theme.palette.textSecondary
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
            }

            Item { Layout.preferredHeight: Theme.spacing.xxlarge * 3 }
        }
    }
}
