import QtQuick
import QtWayland.Compositor
import QtWayland.Compositor.XdgShell
import Logos.Theme
import Logos.Controls

// A native app (@NAME@) shown INSIDE this Basecamp window: the view is a small
// Wayland compositor (Qt's own QtWayland.Compositor, shipped with Basecamp)
// on a private socket, and basecampos_system starts the app with
// WAYLAND_DISPLAY pointing at it. Moving, resizing and minimising this window
// moves the app with it. Generated per app by the BasecampOS flake.
Item {
    id: root
    anchors.fill: parent

    readonly property string externalId: "@ID@"
    // One socket per view instance: Qt aborts the whole process when a compositor
    // finds its socket locked, and an old instance may not be gone yet.
    readonly property string socketName: "basecamp-@ID@-" + Date.now()
    property var bridge: typeof logos !== "undefined" ? logos : null
    property var toplevel: null
    property string message: "Starting @NAME@…"

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

    // Keys reach the app only while the surface item has Qt's active focus and
    // the compositor's seat has given the app keyboard focus.
    function focusApp() {
        surfaceItem.forceActiveFocus()
        surfaceItem.takeFocus()
    }

    function fit() {
        if (toplevel && width > 0 && height > 0)
            toplevel.sendMaximized(Qt.size(Math.round(width), Math.round(height)))
    }
    onWidthChanged: fit()
    onHeightChanged: fit()

    Rectangle { anchors.fill: parent; color: Theme.palette.background }

    // The output must be bound to the QQuickWindow the view is drawn in: that
    // window sends the clients' frame callbacks, so with a stale one an app draws
    // once and then waits forever. The view has no window when it is created and
    // Basecamp may move it to another one, and an initialised output cannot
    // change windows: so an output per window, replaced when the window changes.
    // The app starts once the first output exists.
    property var output: null
    property bool launched: false
    // Shown when the app didn't open or has closed: a way to start it again,
    // rather than "Starting…" for good (an app that dies before its first
    // window says nothing to the compositor).
    property bool showRetry: false
    function launch() {
        launched = true
        showRetry = false
        message = "Starting @NAME@…"
        let r = call("openEmbedded", [externalId, socketName])
        if (r.error) {
            message = r.error
            showRetry = true
        } else {
            startTimer.restart()
        }
    }
    Timer {
        id: startTimer
        interval: 40000
        onTriggered: if (!root.toplevel) { root.message = "@NAME@ didn't open."; root.showRetry = true }
    }
    function attachOutput() {
        let w = root.Window.window
        if (!w || (output && output.window === w))
            return
        if (output)
            output.destroy()
        output = outputComponent.createObject(compositor, { compositor: compositor, window: w })
        fit()
        if (!launched)
            launch()
    }
    Window.onWindowChanged: attachOutput()
    Component.onCompleted: attachOutput()

    Component {
        id: outputComponent
        WaylandOutput { sizeFollowsWindow: false }
    }

    WaylandCompositor {
        id: compositor
        socketName: root.socketName

        // The Basecamp window is the frame: apps draw no title bar of their own.
        // (Client-drawn ones also break: their shm pool grows while Qt still
        // holds the old one, and the compositor rejects the app's next buffer.)
        XdgDecorationManagerV1 { preferredMode: XdgToplevel.ServerSideDecoration }

        XdgShell {
            onToplevelCreated: (toplevel, xdgSurface) => {
                root.toplevel = toplevel
                surfaceItem.shellSurface = xdgSurface
                startTimer.stop()
                root.showRetry = false
                root.message = ""
                root.fit()
                root.focusApp()
            }
        }
    }

    ShellSurfaceItem {
        id: surfaceItem
        anchors.fill: parent
        autoCreatePopupItems: true
        focus: true
        TapHandler { onTapped: root.focusApp() }
        onSurfaceDestroyed: { root.toplevel = null; root.message = "@NAME@ closed."; root.showRetry = true }
    }

    Column {
        anchors.centerIn: parent
        spacing: 16
        visible: root.message.length > 0
        LogosText {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.message
            color: Theme.palette.textSecondary
        }
        LogosButton {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: root.showRetry
            text: "Open again"
            onClicked: root.launch()
        }
    }
}
