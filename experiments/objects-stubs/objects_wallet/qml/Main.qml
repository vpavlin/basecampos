import QtQuick

// Stub Wallet (SHELL-OBJECTS-SPEC.md §10 step 2): accounts, assets,
// transactions whose progress moves, and summaries. No real backend: the data
// below is invented, and "progress" is a timer.
//
// Two counterparties are the same subjects Stub Chat publishes for its
// contacts (Ada Morrow, Juno Vale), so a view can count one person's objects
// across apps (§4 "Counts across types") with Explorer and Chat installed.
WalletStubApp {
    id: app
    objectName: "objects_wallet.root"
    appName: "objects_wallet"
    displayName: "Stub Wallet"
    blurb: "Publishes 2 accounts, 5 assets, 6 transactions (2 pending, moving every 3 s) and 2 summaries."

    readonly property string pkAda: "logos:pk:38dc3d45be876f9687cbae30b8e046000b31e358ebc6c5b995f23ede0f5381bb"
    readonly property string pkJuno: "logos:pk:0016c5021b7c70f3957d01960d342e0d6a12861bd30f232c4cbaa42084847233"

    readonly property int minute: 60 * 1000
    readonly property int hour: 60 * minute
    readonly property int day: 24 * hour

    // The two pending transfers; the ticker advances them. Reset on every
    // publish of the base data, so "Publish again" replays the movement.
    property var pending: ({})
    property real startedAt: Date.now()

    readonly property var viewAction: [{ id: "view", label: "View" }]

    function accountObjects() {
        return [
            { key: "acct:everyday", type: "account", title: "Everyday",
              subtitle: "1,204.50 TST on lez", time: startedAt - 5 * minute,
              fields: { name: "Everyday", network: "lez",
                        balance: { amount: "1204.50", unit: "TST", fiat: "$602.25" } },
              keywords: ["checking", "main"] },
            { key: "acct:savings", type: "account", title: "Savings",
              subtitle: "0.4821 BTC", time: startedAt - 2 * day,
              fields: { name: "Savings", network: "btc",
                        balance: { amount: "0.4821", unit: "BTC", fiat: "$31,340.00" } },
              keywords: ["cold", "vault"] }
        ]
    }

    function assetObjects() {
        function asset(sym, name, acct, amount, fiat, change, age) {
            return { key: "asset:" + sym.toLowerCase(), type: "asset",
                     title: name, subtitle: amount + " " + sym + " · " + fiat,
                     time: startedAt - age,
                     fields: { name: name, symbol: sym, account: { type: "account", key: acct },
                               amount: amount, fiat: fiat, change: change },
                     refs: [{ type: "account", key: acct }] }
        }
        return [
            asset("TST", "Testnet Token", "acct:everyday", "1204.50", "$602.25", 1.8, 10 * minute),
            asset("USDC", "USD Coin", "acct:everyday", "250.00", "$250.00", 0.0, 3 * hour),
            asset("ETH", "Ether", "acct:everyday", "0.1200", "$312.40", -2.4, 6 * hour),
            asset("BTC", "Bitcoin", "acct:savings", "0.4821", "$31,340.00", 0.9, 2 * day),
            asset("DAI", "Dai", "acct:savings", "75.00", "$75.00", 0.1, 4 * day)
        ]
    }

    function txObject(t) {
        const o = { key: t.key, type: "transaction", title: t.title, subtitle: t.subtitle,
                    time: t.time,
                    fields: { direction: t.direction, status: t.status, network: t.network,
                              amount: t.amount, unit: t.unit, address: t.address },
                    actions: viewAction,
                    refs: [{ type: "account", key: t.account }] }
        if (t.counterparty) {
            o.fields.counterparty = t.counterparty
            o.subjects = [t.counterparty]
        }
        if (t.progress !== undefined) o.progress = t.progress
        return o
    }

    function txs() {
        const p = pending
        return [
            { key: "tx:941499ddcca1ac610c6fe1ab9165bae2ba979fd02ba4a6e5bb09ef1cbb771655",
              title: p.out.progress >= 100 ? "Sent 12.00 TST to Ada Morrow" : "Sending 12.00 TST to Ada Morrow",
              subtitle: p.out.progress >= 100 ? "Done" : "Sending, " + p.out.progress + "%",
              direction: "out", status: p.out.status, network: "lez", amount: "12.00", unit: "TST",
              address: "lez:6cf415860001b4349021c73262e5835aaf559d33", counterparty: pkAda,
              account: "acct:everyday", time: startedAt - 1 * minute, progress: p.out.progress },
            { key: "tx:3341a76d47425d33b5160a711f431bf72b5bdab2bea786678f9138e61ecc22d6",
              title: p["in"].progress >= 100 ? "Received 0.0150 BTC" : "Receiving 0.0150 BTC",
              subtitle: p["in"].progress >= 100 ? "Done" : "Receiving, " + p["in"].progress + "%",
              direction: "in", status: p["in"].status, network: "btc", amount: "0.0150", unit: "BTC",
              address: "btc:bc1q8rg7x0mlf2c4yqv3sn5d0hk6zw9e4tq2a7u5lp",
              account: "acct:savings", time: startedAt - 2 * minute, progress: p["in"].progress },
            { key: "tx:f3aed4dcc959c0cb57b3acb69506a6be447ad2dd36617b26c55b8dd3c0408eb9",
              title: "Received 40.00 TST from Juno Vale", subtitle: "Done",
              direction: "in", status: "done", network: "lez", amount: "40.00", unit: "TST",
              address: "lez:590fdd9e11579bed9c95d230299c6d9cfb8b09e1", counterparty: pkJuno,
              account: "acct:everyday", time: startedAt - 3 * hour },
            { key: "tx:ac0479322cfe42eb80fd94a248c5c53b25d40b7a7cbefcac46b5d92a9a92e589",
              title: "Sent 25.00 USDC", subtitle: "Done",
              direction: "out", status: "done", network: "lez", amount: "25.00", unit: "USDC",
              address: "lez:3feaa8aa2c073fc8279677425efb570ee0d3c688",
              account: "acct:everyday", time: startedAt - 1 * day },
            { key: "tx:fbbd324a9177a58addfb4c109afc3bdcb1557a42cf8d722adfe424ea5f2e4191",
              title: "Send 0.0500 ETH failed", subtitle: "Not enough for the fee",
              direction: "out", status: "failed", network: "lez", amount: "0.0500", unit: "ETH",
              address: "lez:10104395d5e3bf3c4de31663e403ecaef197cdbc",
              account: "acct:everyday", time: startedAt - 2 * day },
            { key: "tx:a9471e0a37af8a60e7e4f48d5542affec65987d3e93db5e6859595c7aa0efb6b",
              title: "Rent, 300.00 TST", subtitle: "Scheduled for the 1st",
              direction: "out", status: "scheduled", network: "lez", amount: "300.00", unit: "TST",
              address: "lez:a3119ad0af2c0750bf623e9467eeb719f728f7d0",
              account: "acct:everyday", time: startedAt - 3 * day }
        ]
    }

    function summaryObjects() {
        return [
            { key: "summary:scheduled", type: "transaction", kind: "summary",
              title: "2 scheduled, next in 6 days", subtitle: "Rent and a savings transfer",
              time: startedAt - 4 * minute,
              fields: { status: "scheduled", network: "lez" } },
            { key: "summary:today", type: "transaction", kind: "summary",
              title: "41 more transactions today", subtitle: "Most of them small payments",
              time: startedAt - 3 * minute,
              fields: { network: "lez" } }
        ]
    }

    baseObjects: function () {
        startedAt = Date.now()
        pending = { out: { progress: 20, status: "pending" },
                    "in": { progress: 50, status: "pending" } }
        ticker.restart()
        return accountObjects().concat(assetObjects(), txs().map(txObject), summaryObjects())
    }

    // Progress moves by republishing under the same key: the store replaces
    // the object in place (§3 `key`). Only while the store still holds it, so
    // Retract all is not undone by the next tick.
    Timer {
        id: ticker
        interval: 3000
        repeat: true
        onTriggered: {
            const next = Object.assign({}, app.pending)
            const moved = []                 // indexes into txs(): 0 = out, 1 = in
            let moving = false
            ;["out", "in"].forEach(function (k, i) {
                if (next[k].progress >= 100) return
                const progress = Math.min(100, next[k].progress + 10)
                next[k] = { progress: progress, status: progress >= 100 ? "done" : "pending" }
                moved.push(i)
                moving = moving || progress < 100
            })
            app.pending = next
            const all = app.txs()
            const changed = moved.map(i => app.txObject(all[i]))
                .filter(o => app.publisher.isLive(o.key))
            if (changed.length > 0)
                app.publisher.publish(changed, null, "progress")
            if (!moving) {
                app.log("pending transfers done")
                stop()
            }
        }
    }

    floodObject: function (seq, now) {
        const amount = (1 + (seq % 97) * 0.37).toFixed(2)
        const incoming = seq % 3 !== 0
        return { key: "flood:tx:" + seq, type: "transaction",
                 title: (incoming ? "Received " : "Sent ") + amount + " TST",
                 subtitle: "Flood " + seq,
                 // Older than the base data, so a later overflow evicts these first.
                 time: now - (seq + 10) * minute,
                 fields: { direction: incoming ? "in" : "out", status: "done", network: "lez",
                           amount: amount, unit: "TST" },
                 keywords: ["flood"] }
    }

    handleAction: function (p) {
        if (p.action === "open" || p.action === "view") {
            app.log("recorded " + p.action + " on " + p.key)
            return { ok: true }
        }
        return { ok: false, error: "bad_request" }
    }
}
