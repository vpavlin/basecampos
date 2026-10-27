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

    // Scale: the output follows the window's pixel size, so pixels per view
    // unit = output width / window width (2 on the Duet: sway's 1.2 reaches
    // Basecamp as a whole 2). Qt shows a surface at output scale / that ratio,
    // so with the output's scale equal to it the app draws sharp, sized in view
    // units, and its popups (menus, the address bar's suggestions) land where
    // and as big as it means them. A fractional ratio has no whole scale: then
    // scale 1 and sizes in pixels (right geometry, smaller text). Sizes in view
    // units at scale 1 got stretched to fit, and the popups (not stretched) came
    // out half size and off target (Duet).
    function pixelRatio() {
        let w = root.Window.window
        return output && w && w.width > 0 && output.geometry.width > 0 ? output.geometry.width / w.width : 1
    }
    function applyScale() {
        if (!output)
            return
        let r = pixelRatio(), whole = Math.round(r)
        let s = whole >= 1 && Math.abs(r - whole) < 0.02 ? whole : 1
        if (output.scaleFactor !== s)
            output.scaleFactor = s
        fit()
    }
    property var watchedPopups: []
    function watchPopups(parentItem) {
        watchedPopups = watchedPopups.filter(c => !!c)
        for (let i = 0; i < parentItem.children.length; i++) {
            let c = parentItem.children[i]
            if (c.shellSurface === undefined || watchedPopups.indexOf(c) >= 0)
                continue
            watchedPopups.push(c)
            let clamp = () => root.keepInView(c)
            c.xChanged.connect(clamp); c.yChanged.connect(clamp)
            c.widthChanged.connect(clamp); c.heightChanged.connect(clamp)
            c.childrenChanged.connect(() => Qt.callLater(root.watchPopups, c))
            clamp()
        }
    }
    property bool sliding: false
    function slide(pos, size, room) {
        let d = Math.min(0, room - (pos + size))
        return d - Math.min(0, pos + d)
    }
    function keepInView(item) {
        if (sliding || item.width <= 0 || item.height <= 0)
            return
        let p = item.parent.mapToItem(surfaceItem, item.x, item.y)
        let x = item.x + slide(p.x, item.width, surfaceItem.width)
        let y = item.y + slide(p.y, item.height, surfaceItem.height)
        sliding = true
        item.x = x
        item.y = y
        sliding = false
    }
    function fit() {
        if (!toplevel || width <= 0 || height <= 0 || !output)
            return
        let k = pixelRatio() / output.scaleFactor
        toplevel.sendMaximized(Qt.size(Math.round(width * k), Math.round(height * k)))
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
        output.geometryChanged.connect(applyScale)
        applyScale()
        if (!launched)
            launch()
    }
    Window.onWindowChanged: attachOutput()
    Component.onCompleted: attachOutput()

    Component {
        id: outputComponent
        // A real mode (the window's size): with none, Brave crashed opening its
        // menu and squeezed the suggestions into a strip.
        WaylandOutput { sizeFollowsWindow: true }
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
        // Qt places popups where the app asks and ignores the positioner's
        // "slide into view" (a TODO in Qt): Brave's menu, anchored at the right
        // edge, ran off the view. Popup items are created as children of their
        // parent's item; slid back inside the view here, nested ones too. (Later:
        // the signal comes while the new item is still being constructed.)
        onChildrenChanged: Qt.callLater(root.watchPopups, surfaceItem)
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
