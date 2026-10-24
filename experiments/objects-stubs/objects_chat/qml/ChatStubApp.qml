import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Logos.Controls
import Logos.Theme

// The frame every objects stub shares (docs/SHELL-OBJECTS-SPEC.md §10 step 2):
// publishes the app's stub data on start, answers `objects.action`, and shows
// what is in the store, the last action and the Flood / Retract all / Publish
// again buttons. Each app's Main.qml supplies only its data and its reactions.
//
// Every stub carries a copy renamed per app (WalletStubApp, …); the master is
// experiments/objects-stubs/common/ and sync.sh copies it (and says why).
//
// The app plugs in through function-valued properties rather than overridden
// functions, so Main.qml stays one declaration with no derived type:
//   baseObjects()          -> [envelope, …]   what the app publishes on start
//   floodObject(seq, now)  -> envelope         one of the Flood button's objects
//   handleAction(params)   -> { ok, error? }   reacts to { key, action, text? }
Item {
    id: app

    property string appName: "objects_stub"
    property string displayName: "Stub"
    property string blurb: ""
    property var baseObjects: function () { return [] }
    property var floodObject: function (seq, now) { return null }
    property var handleAction: function (params) {
        return params.action === "open" ? { ok: true } : { ok: false, error: "bad_request" }
    }

    // The only requester an action may come from. The shell delivers actions
    // with a targeted submit (only to the object's publisher); any other app
    // asking us for `objects.action` through the chooser is refused, so a
    // stranger cannot vote on our posts or reply in our conversations.
    readonly property string shellRequester: "main_ui"

    readonly property alias publisher: pub
    property string lastAction: "none yet"
    property string floodReport: ""
    property int floodSeq: 0

    readonly property int floodSize: 1000
    readonly property bool compact: Theme.windowClass(width) === "compact"

    function log(msg) { console.log("[" + appName + "] " + msg) }

    // " [unavailable, failed]": each error code once, not once per batch.
    function errorList(errors) {
        const unique = errors.filter((e, i) => errors.indexOf(e) === i)
        return unique.length ? " [" + unique.join(", ") + "]" : ""
    }

    // Plain text, cut to the store's limits (title 80, subtitle 120).
    function clip(s, n) {
        s = String(s)
        return s.length <= n ? s : s.slice(0, n - 1) + "…"
    }

    // On start, and once more after 2 s if the store did not take it all: the
    // shell may still be registering intents when an app autoloads.
    // `why` is "start", "retry" or "again" (the Publish again button).
    function publishBase(why) {
        const objs = baseObjects()
        pub.publish(objs, function (r) {
            log("base publish (" + why + "): "
                + r.okObjects + " ok, " + r.refusedObjects + " refused"
                + errorList(r.errors))
            if (r.refusedObjects > 0 && why === "start") {
                log("retrying the base publish in 2 s")
                retryTimer.start()
            }
        }, "base (" + why + ")")
    }

    // Tops the store up to the cap rather than pushing past it: past 500 the
    // store evicts our oldest objects, which would quietly delete the base
    // data the views are being tested with. What did not fit is reported.
    function flood() {
        const now = Date.now()
        const room = pub.room()
        const n = Math.min(floodSize, room)
        const heldBack = floodSize - n
        if (n === 0) {
            floodReport = "Flood: the store already holds " + pub.cap
                + "; all " + floodSize + " held back"
            log(floodReport)
            return
        }
        const objs = []
        for (let i = 0; i < n; ++i) objs.push(floodObject(floodSeq++, now))
        floodReport = "Flood: sending " + n + "…"
        log("flood: sending " + n + ", holding back " + heldBack + " over the cap of " + pub.cap)
        pub.publish(objs, function (r) {
            floodReport = "Flood: " + r.okObjects + " published, " + heldBack
                + " held back (cap " + pub.cap + "), " + r.refusedObjects + " refused by the store"
                + errorList(r.errors)
            log(floodReport + " in " + r.requests + " requests")
        }, "flood")
    }

    function retractAll() {
        pub.retractAll(function (res) {
            floodReport = ""
        })
    }

    Component.onCompleted: publishBase("start")

    Timer {
        id: retryTimer
        interval: 2000
        onTriggered: app.publishBase("retry")
    }

    ChatObjectPublisher {
        id: pub
        tag: app.appName
    }

    Connections {
        target: logos
        function onIntentRequested(requestId, intent, params, requesterName) {
            if (intent !== "objects.action") return
            app.log("action " + JSON.stringify(params) + " from " + requesterName)
            if (requesterName !== app.shellRequester) {
                app.log("refused: actions come only from the shell")
                logos.respond(requestId, false, {}, "failed")
                return
            }
            const key = params && typeof params.key === "string" ? params.key : ""
            const action = params && typeof params.action === "string" ? params.action : ""
            if (key === "" || action === "") {
                logos.respond(requestId, false, {}, "bad_request")
                return
            }
            const text = typeof params.text === "string" ? params.text : ""
            app.lastAction = action + " on " + key + (text ? " (\"" + text + "\")" : "")
            let r
            try {
                r = app.handleAction({ key: key, action: action, text: text })
            } catch (e) {
                app.log("action handler threw: " + e)
                r = { ok: false, error: "failed" }
            }
            app.log("action " + action + " on " + key + " -> " + (r.ok ? "ok" : r.error))
            if (!r.ok) app.lastAction += " -> " + r.error
            logos.respond(requestId, !!r.ok, {}, r.ok ? "" : (r.error || "failed"))
        }
    }

    // ---- UI ---------------------------------------------------------------
    // One column, max 720, centred (mobile-ready-ui: a settings-like page);
    // buttons stack full width in compact.

    Rectangle {
        anchors.fill: parent
        color: Theme.palette.background
    }

    Flickable {
        id: flick
        objectName: app.appName + ".scroll"
        anchors.fill: parent
        contentHeight: column.implicitHeight + 2 * Theme.pageMargin(app.width)
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: LogosScrollBar {}

        ColumnLayout {
            id: column
            x: Math.max(Theme.pageMargin(app.width), (app.width - width) / 2)
            y: Theme.pageMargin(app.width)
            width: Math.min(app.width - 2 * Theme.pageMargin(app.width), 720)
            spacing: Theme.spacing.medium

            LogosText {
                objectName: app.appName + ".title"
                text: app.displayName
                font.pixelSize: 20
                font.weight: Font.DemiBold
            }
            LogosText {
                Layout.fillWidth: true
                visible: app.blurb !== ""
                text: app.blurb
                color: Theme.palette.textSecondary
                font.pixelSize: Theme.typography.secondaryText
                wrapMode: Text.Wrap
            }

            // What the store holds for us, per type.
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: counts.implicitHeight + 2 * Theme.spacing.medium
                radius: Theme.spacing.radiusLarge
                color: Theme.palette.backgroundSecondary

                ColumnLayout {
                    id: counts
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Theme.spacing.medium
                    spacing: Theme.spacing.small

                    LogosText {
                        objectName: app.appName + ".total"
                        Layout.fillWidth: true
                        text: pub.liveCount + " of " + pub.cap + " objects in the store"
                        font.weight: Font.DemiBold
                        wrapMode: Text.Wrap
                    }
                    Repeater {
                        model: Object.keys(pub.typeCounts).sort()
                        delegate: RowLayout {
                            required property var modelData
                            Layout.fillWidth: true
                            LogosText {
                                Layout.fillWidth: true
                                text: modelData
                                textFormat: Text.PlainText
                                elide: Text.ElideRight
                            }
                            LogosText {
                                objectName: app.appName + ".count." + modelData
                                text: String(pub.typeCounts[modelData] || 0)
                            }
                        }
                    }
                    LogosText {
                        Layout.fillWidth: true
                        text: "Requests sent: " + pub.requestsSent
                              + (pub.queued > 0 ? " · " + pub.queued + " waiting for the rate limit" : "")
                        color: Theme.palette.textSecondary
                        font.pixelSize: Theme.typography.secondaryText
                        wrapMode: Text.Wrap
                    }
                    LogosText {
                        objectName: app.appName + ".lastAnswer"
                        Layout.fillWidth: true
                        text: "Last answer: " + pub.lastAnswer
                        textFormat: Text.PlainText
                        color: Theme.palette.textSecondary
                        font.pixelSize: Theme.typography.secondaryText
                        wrapMode: Text.Wrap
                    }
                }
            }

            LogosText {
                objectName: app.appName + ".lastAction"
                Layout.fillWidth: true
                text: "Last action: " + app.lastAction
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
            }
            LogosText {
                objectName: app.appName + ".floodReport"
                Layout.fillWidth: true
                visible: app.floodReport !== ""
                text: app.floodReport
                textFormat: Text.PlainText
                color: Theme.palette.textSecondary
                wrapMode: Text.Wrap
            }

            GridLayout {
                Layout.fillWidth: true
                Layout.topMargin: Theme.spacing.small
                columns: app.compact ? 1 : 3
                columnSpacing: Theme.spacing.small
                rowSpacing: Theme.spacing.small

                LogosButton {
                    objectName: app.appName + ".flood"
                    Layout.fillWidth: true
                    text: "Flood " + app.floodSize
                    enabled: !pub.busy
                    onClicked: app.flood()
                }
                LogosButton {
                    objectName: app.appName + ".publish"
                    Layout.fillWidth: true
                    text: "Publish again"
                    onClicked: app.publishBase("again")
                }
                LogosButton {
                    objectName: app.appName + ".retractAll"
                    Layout.fillWidth: true
                    text: "Retract all"
                    onClicked: app.retractAll()
                }
            }
        }
    }
}
