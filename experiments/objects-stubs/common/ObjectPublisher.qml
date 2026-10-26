import QtQuick

// The client side of the shell's object store (docs/SHELL-OBJECTS-SPEC.md §5,
// §8) for one stub app: queues publish and retract requests, keeps them under
// the store's limits, and mirrors what the store holds for this app so the UI
// can show counts per type.
//
// Every stub app carries a copy, renamed per app (WalletObjectPublisher, …).
// The master is experiments/objects-stubs/common/; sync.sh copies it and says
// why: the plugin sandbox keeps each app inside its own folder, and same-named
// sibling files in two apps collide in the host.
//
// What it enforces on our side, so a stub never trips a limit by accident:
//   - at most 20 requests in any 10.5 s (the store allows 20 per 10 s; the
//     half second absorbs dispatch jitter between our clock and the host's).
//     Retracts are counted too: the spec only counts publishes, but a stub
//     that is careful in both directions cannot be the reason a test fails.
//   - each request's payload stays under 900 nodes and 50 objects. The intent
//     bridge refuses payloads over 1000 nodes (app-to-app-intents.md §3), and
//     an object with fields, refs and actions is 15 to 30 nodes.
// What it deliberately does NOT enforce is the 500-object cap: the flood
// button asks for its room up front (room()), but an app that publishes past
// the cap is the case the store's eviction exists for, and the mirror below
// evicts the same way so the counts stay honest: current state (`state:
// true`) over its own cap of 50 loses its oldest, then the app over 500 loses
// its oldest events, never its state (spec §8).
//
// publish(…, { noWait: true }) skips our rate limit (the Flood past the cap
// button): every request goes at once and the store's own limit answers.
QtObject {
    id: pub

    property string tag: "objects_stub"          // log prefix, the app's name

    readonly property int cap: 500
    readonly property int stateCap: 50
    readonly property int rateLimit: 20
    readonly property int rateWindowMs: 10500
    readonly property int nodeBudget: 900
    readonly property int maxBatch: 50

    // key -> { type, time, state }: what the store holds for us, as far as its
    // answers say. Reassigned (never mutated in place) so bindings refresh.
    property var live: ({})
    property int liveCount: 0
    property var typeCounts: ({})                // type -> count, for the UI
    property int stateCount: 0                   // of liveCount, current state

    property string lastAnswer: "nothing sent yet"
    property int requestsSent: 0
    property int queued: 0                       // requests waiting for the rate window
    property int inFlight: 0                     // requests sent, not answered
    property int pendingObjects: 0               // objects in queued + in-flight publishes
    readonly property bool busy: queued > 0 || inFlight > 0

    property var _queue: []
    property var _sentAt: []

    property Timer _pump: Timer {
        interval: 100
        repeat: true
        running: pub.queued > 0
        onTriggered: pub._pumpOnce()
    }

    function log(msg) { console.log("[" + tag + "] " + msg) }

    function isLive(key) { return live.hasOwnProperty(key) }

    // How many new objects fit before the store starts evicting.
    function room() { return Math.max(0, cap - liveCount - pendingObjects) }

    // Publishes `objects` in as many requests as the limits need. `done` is
    // called once, after the last batch answers, with
    //   { requests, okObjects, refusedObjects, errors: [code, …] }.
    // Batches are all-or-nothing in the store, so a batch is counted whole.
    // `options.noWait`: send every request now, past our rate limit.
    function publish(objects, done, label, options) {
        const noWait = !!(options && options.noWait)
        const batches = _split(objects)
        const what = label || "publish"
        const summary = { requests: batches.length, okObjects: 0, refusedObjects: 0, errors: [] }
        if (batches.length === 0) {
            if (done) done(summary)
            return
        }
        let remaining = batches.length
        for (const batch of batches) {
            pendingObjects += batch.length
            const onAnswer = function (res) {
                pendingObjects -= batch.length
                if (res.ok) {
                    summary.okObjects += batch.length
                    _remember(batch)
                } else {
                    summary.refusedObjects += batch.length
                    summary.errors.push(res.error)
                }
                log(what + ": " + batch.length + " object(s) -> "
                    + (res.ok ? "ok" : res.error))
                lastAnswer = what + ": " + batch.length + " -> " + (res.ok ? "ok" : res.error)
                if (--remaining === 0 && done) done(summary)
            }
            if (noWait)
                _send({ intent: "basecamp.objects.publish", payload: { objects: batch }, onAnswer: onAnswer })
            else
                _enqueue("basecamp.objects.publish", { objects: batch }, onAnswer)
        }
    }

    function retract(keys, done) {
        _enqueue("basecamp.objects.retract", { keys: keys }, function (res) {
            if (res.ok) {
                const next = Object.assign({}, live)
                for (const k of keys) delete next[k]
                _setLive(next)
            }
            log("retract " + keys.length + " -> " + (res.ok ? "ok" : res.error))
            lastAnswer = "retract " + keys.length + " -> " + (res.ok ? "ok" : res.error)
            if (done) done(res)
        })
    }

    function retractAll(done) {
        _enqueue("basecamp.objects.retract", { all: true }, function (res) {
            if (res.ok) _setLive({})
            log("retract all -> " + (res.ok ? "ok" : res.error))
            lastAnswer = "retract all -> " + (res.ok ? "ok" : res.error)
            if (done) done(res)
        })
    }

    // ---- internals --------------------------------------------------------

    // Nodes as the intent bridge counts them: every value, containers included.
    function _nodes(v) {
        if (v === null || typeof v !== "object") return 1
        let n = 1
        if (Array.isArray(v)) { for (const x of v) n += _nodes(x) }
        else { for (const k in v) n += _nodes(v[k]) }
        return n
    }

    function _split(objects) {
        const batches = []
        let batch = [], nodes = 3                // { objects: [ … ] }
        for (const o of objects) {
            const n = _nodes(o)
            if (batch.length > 0 && (nodes + n > nodeBudget || batch.length >= maxBatch)) {
                batches.push(batch)
                batch = []; nodes = 3
            }
            batch.push(o); nodes += n
        }
        if (batch.length > 0) batches.push(batch)
        return batches
    }

    // The queue and the send times are copied, never mutated in place: a
    // `var` property hands QML a converted value in some paths, and a push
    // onto a copy is a silent no-op that would stall the queue for good.
    function _enqueue(intent, payload, onAnswer) {
        _queue = _queue.concat([{ intent: intent, payload: payload, onAnswer: onAnswer }])
        queued = _queue.length
        _pumpOnce()
    }

    function _pumpOnce() {
        const now = Date.now()
        const sent = _sentAt.filter(t => now - t < rateWindowMs)
        const queue = _queue.slice()
        const jobs = []
        while (queue.length > 0 && sent.length < rateLimit) {
            jobs.push(queue.shift())
            sent.push(now)
        }
        _sentAt = sent
        _queue = queue
        queued = queue.length
        for (const job of jobs) _request(job)
    }

    // Past the queue: counted in the window, so what is queued after it
    // still waits its turn.
    function _send(job) {
        _sentAt = _sentAt.concat([Date.now()])
        _request(job)
    }

    function _request(job) {
        requestsSent++
        inFlight++
        logos.request(job.intent, job.payload, function (res) {
            inFlight--
            job.onAnswer(res)
        })
    }

    function _remember(batch) {
        const next = Object.assign({}, live)
        const now = Date.now()
        for (const o of batch)
            next[o.key] = { type: o.type, time: o.time !== undefined ? o.time : now, state: o.state === true }
        // Mirror the store's eviction: oldest `time` first, current state over
        // its own cap, then events down to the cap (ties by key here, by id
        // in the store: a tie is the only place the two can differ).
        const oldestFirst = (a, b) => next[a].time - next[b].time || (a < b ? -1 : a > b ? 1 : 0)
        const states = Object.keys(next).filter(k => next[k].state).sort(oldestFirst)
        for (let i = 0; i < states.length - stateCap; ++i) delete next[states[i]]
        const keys = Object.keys(next)
        if (keys.length > cap) {
            const events = keys.filter(k => !next[k].state).sort(oldestFirst)
            for (let i = 0; i < keys.length - cap; ++i) delete next[events[i]]
        }
        _setLive(next)
    }

    function _setLive(next) {
        const counts = {}
        let states = 0
        for (const k in next) {
            counts[next[k].type] = (counts[next[k].type] || 0) + 1
            if (next[k].state) ++states
        }
        live = next
        liveCount = Object.keys(next).length
        stateCount = states
        typeCounts = counts
    }
}
