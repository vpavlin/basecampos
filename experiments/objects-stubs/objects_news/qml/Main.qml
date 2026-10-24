import QtQuick

// Stub News (SHELL-OBJECTS-SPEC.md §10 step 2): news items with a source and
// a text. `url` is carried as text only: the shell never opens it (§4), and
// nothing here does either. The only action is the implicit `open`.
NewsStubApp {
    id: app
    objectName: "objects_news.root"
    appName: "objects_news"
    displayName: "Stub News"
    blurb: "Publishes 6 news items. Opening one is recorded; nothing is fetched."

    readonly property int minute: 60 * 1000

    readonly property var items: [
        { id: "testnet-epoch", source: "Network Status", age: 15,
          title: "Testnet passes epoch 40 without a missed block",
          text: "The longest clean run so far. Operators who restarted during the last upgrade should check their peer count." },
        { id: "release-notes", source: "Builders Weekly", age: 180,
          title: "This week: smaller packages and faster installs",
          text: "Package downloads now come from the storage network first, with a fallback to the web." },
        { id: "mesh-meetup", source: "Forum Digest", age: 420,
          title: "Mesh meetup moves to the library",
          text: "The monthly meetup has a new home with a rooftop for test antennas." },
        { id: "privacy-paper", source: "Research Notes", age: 1440,
          title: "A plain-language summary of mixnet timing",
          text: "Why delays hide who talks to whom, and what they cost in latency." },
        { id: "zine-issue", source: "Forum Digest", age: 2900,
          title: "Zine Makers publish their fourth issue",
          text: "Printed on recycled paper and pinned on the storage network by forty volunteers." },
        { id: "app-contest", source: "Builders Weekly", age: 5000,
          title: "Small apps contest opens",
          text: "Build something useful in under 500 lines of QML. Entries close next month." }
    ]

    baseObjects: function () {
        const now = Date.now()
        return items.map(n => ({
            key: "news:" + n.id, type: "news", title: n.title, subtitle: n.source,
            time: now - n.age * minute,
            fields: { source: n.source, url: "https://news.example.org/" + n.id, text: n.text }
        }))
    }

    floodObject: function (seq, now) {
        return { key: "flood:news:" + seq, type: "news", title: "Flood headline " + seq,
                 subtitle: "Flood Wire",
                 time: now - (seq + 10) * minute,
                 fields: { source: "Flood Wire", url: "https://news.example.org/flood/" + seq,
                           text: "Flood item " + seq },
                 keywords: ["flood"] }
    }

    handleAction: function (p) {
        if (p.action === "open") {
            app.log("recorded open on " + p.key)
            return { ok: true }
        }
        return { ok: false, error: "bad_request" }
    }
}
