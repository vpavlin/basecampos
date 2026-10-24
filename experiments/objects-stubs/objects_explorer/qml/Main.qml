import QtQuick

// Stub Explorer (SHELL-OBJECTS-SPEC.md §10 step 2): a second publisher of
// `transaction`, so views draw one Transactions card from two apps.
//
// Counterparties are three of Stub Chat's contact subjects (Ada, Bram, Cleo),
// hardcoded here as in objects_chat: subjects are the only join between apps
// (§6), so "Ada: 3 transactions" works without either app knowing the other.
// The explorer watches one address and reports its transfers on `lez`.
ExplorerStubApp {
    id: app
    objectName: "objects_explorer.root"
    appName: "objects_explorer"
    displayName: "Stub Explorer"
    blurb: "Publishes 5 lez transactions for a watched address; three counterparties are Stub Chat contacts."

    readonly property int minute: 60 * 1000
    readonly property string watched: "lez:6cf415860001b4349021c73262e5835aaf559d33"

    readonly property string pkAda: "logos:pk:38dc3d45be876f9687cbae30b8e046000b31e358ebc6c5b995f23ede0f5381bb"
    readonly property string pkBram: "logos:pk:b3259ec6ea7b7770948b98cbeb81ddff8146105619e68fb4bd9986ec0648a502"
    readonly property string pkCleo: "logos:pk:53e64365aa71e8d9f06313149cc9f44c89f50c7829324f504498f6b9a0aa0556"

    readonly property var txs: [
        { hash: "5d5785fcbb98437838b3a6d70b8e26f96646ff0c1bdfc6d218ace98bcd3cd992", block: 184233,
          direction: "in", amount: "12.00", who: "Ada Morrow", pk: pkAda, status: "done", age: 1 },
        { hash: "ff2d7399a43a279046002e2427279755966818af3ca17cab249bc49caecff100", block: 184190,
          direction: "out", amount: "3.50", who: "Bram Telles", pk: pkBram, status: "done", age: 45 },
        { hash: "a029fdc3de4543853dbad9fab5fc61901a6993051c856cc071c245efa1ed4cd6", block: 184102,
          direction: "in", amount: "60.00", who: "Cleo Annis", pk: pkCleo, status: "done", age: 160 },
        { hash: "48270df5a7552a1cc34bac19286526be2726f64303a2d5467ff286e2b9120926", block: 183870,
          direction: "out", amount: "7.25", who: "Ada Morrow", pk: pkAda, status: "failed", age: 600 },
        { hash: "c6859d49a74bc4384af38da809aa8f04c7847132bc85b393c83af6b957391dd3", block: 183511,
          direction: "in", amount: "0.80", who: "Bram Telles", pk: pkBram, status: "done", age: 1500 }
    ]

    baseObjects: function () {
        const now = Date.now()
        return txs.map(t => ({
            key: "tx:" + t.hash, type: "transaction",
            title: (t.direction === "in" ? "In " : "Out ") + t.amount + " TST, block " + t.block,
            subtitle: (t.status === "failed" ? "Failed · " : "Confirmed · ") + t.hash.slice(0, 12) + "…",
            time: now - t.age * minute,
            fields: { direction: t.direction, status: t.status, network: "lez",
                      amount: t.amount, unit: "TST", counterparty: t.pk, address: watched },
            subjects: [t.pk, watched],
            keywords: [t.hash.slice(0, 16), "block" + t.block],
            actions: [{ id: "view", label: "View" }]
        }))
    }

    floodObject: function (seq, now) {
        const pk = [pkAda, pkBram, pkCleo][seq % 3]
        return { key: "flood:tx:" + seq, type: "transaction",
                 title: "In " + (seq % 100) + ".00 TST, block " + (180000 - seq),
                 subtitle: "Flood " + seq,
                 time: now - (seq + 10) * minute,
                 fields: { direction: "in", status: "done", network: "lez",
                           amount: (seq % 100) + ".00", unit: "TST", counterparty: pk },
                 subjects: [pk], keywords: ["flood"] }
    }

    handleAction: function (p) {
        if (p.action === "open" || p.action === "view") {
            app.log("recorded " + p.action + " on " + p.key)
            return { ok: true }
        }
        return { ok: false, error: "bad_request" }
    }
}
