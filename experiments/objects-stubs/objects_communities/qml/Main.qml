import QtQuick

// Stub Communities (SHELL-OBJECTS-SPEC.md §10 step 2): communities with Join /
// Leave and posts with Vote / Like. Every action changes the stub's own state
// and republishes the object under the same key, so the store replaces it in
// place (§3 `key`) and a view sees the new member count or score.
CommunitiesStubApp {
    id: app
    objectName: "objects_communities.root"
    appName: "objects_communities"
    displayName: "Stub Communities"
    blurb: "Publishes 5 communities and 8 posts. Join flips a community to joined; Vote and Like raise a post's score and likes."

    readonly property int minute: 60 * 1000
    property real baseTime: Date.now()

    property var communities: [
        { key: "community:node-guild", name: "Node Operators Guild", members: 1240, joined: true,  age: 9000 },
        { key: "community:zines",      name: "Zine Makers",          members: 312,  joined: false, age: 7000 },
        { key: "community:privacy",    name: "Privacy Researchers",  members: 2875, joined: true,  age: 5000 },
        { key: "community:mesh",       name: "Local Mesh Network",   members: 96,   joined: false, age: 3000 },
        { key: "community:builders",   name: "Basecamp Builders",    members: 640,  joined: false, age: 1000 }
    ]

    // Authors are invented and app-local (`x.objects_communities:<id>`, §6):
    // forum handles here are not the chat identities, so they must not join.
    property var posts: [
        { key: "post:1", community: "community:node-guild", author: "gravel", score: 42, likes: 17, age: 30,
          text: "Checklist for upgrading without losing sync" },
        { key: "post:2", community: "community:node-guild", author: "ferrite", score: 8, likes: 3, age: 140,
          text: "Is anyone else seeing zero peers after the restart?" },
        { key: "post:3", community: "community:zines", author: "inkwell", score: 23, likes: 11, age: 400,
          text: "Issue 4 is out: printing on recycled paper" },
        { key: "post:4", community: "community:privacy", author: "lattice", score: 97, likes: 54, age: 60,
          text: "Reading group: mixnets, week three" },
        { key: "post:5", community: "community:privacy", author: "quorum", score: 15, likes: 6, age: 900,
          text: "What would a private-by-default contact list need?" },
        { key: "post:6", community: "community:mesh", author: "antenna", score: 5, likes: 2, age: 1500,
          text: "Rooftop node on the library is live" },
        { key: "post:7", community: "community:builders", author: "sprocket", score: 31, likes: 19, age: 200,
          text: "Show and tell: a tiny notes app in 200 lines of QML" },
        { key: "post:8", community: "community:builders", author: "marlin", score: 12, likes: 4, age: 20,
          text: "How do you test an app at phone width?" }
    ]

    function community(key) { return communities.find(c => c.key === key) }

    function communityObject(c) {
        return { key: c.key, type: "community", title: c.name,
                 subtitle: c.members + " members" + (c.joined ? " · joined" : ""),
                 time: baseTime - c.age * minute,
                 fields: { name: c.name, members: c.members, joined: c.joined },
                 actions: [c.joined ? { id: "leave", label: "Leave" } : { id: "join", label: "Join" }] }
    }

    function postObject(p) {
        const c = community(p.community)
        return { key: p.key, type: "post", title: clip(p.text, 80),
                 subtitle: c.name + " · " + p.author + " · " + p.score + " points",
                 time: baseTime - p.age * minute,
                 fields: { community: { type: "community", key: c.key },
                           author: "x.objects_communities:" + p.author, authorName: p.author,
                           text: p.text, score: p.score, likes: p.likes },
                 refs: [{ type: "community", key: c.key }],
                 actions: [{ id: "vote", label: "Vote" }, { id: "like", label: "Like" }] }
    }

    baseObjects: function () {
        baseTime = Date.now()
        return communities.map(communityObject).concat(posts.map(postObject))
    }

    floodObject: function (seq, now) {
        const c = communities[seq % communities.length]
        return { key: "flood:post:" + seq, type: "post", title: "Flood post " + seq,
                 subtitle: c.name,
                 time: now - (seq + 10) * minute,
                 fields: { community: { type: "community", key: c.key },
                           author: "x.objects_communities:flood", authorName: "flood",
                           text: "Flood post " + seq, score: seq % 50, likes: seq % 20 },
                 refs: [{ type: "community", key: c.key }],
                 keywords: ["flood"] }
    }

    // Replaces one entry of a var list property (copy, then assign) and
    // returns the updated entry.
    function update(listName, key, change) {
        let updated = null
        app[listName] = app[listName].map(function (e) {
            if (e.key !== key) return e
            updated = Object.assign({}, e, change(e))
            return updated
        })
        return updated
    }

    handleAction: function (p) {
        if (p.action === "open") return { ok: true }
        if (p.action === "join" || p.action === "leave") {
            const join = p.action === "join"
            const c = update("communities", p.key, e => e.joined === join ? {}
                             : { joined: join, members: e.members + (join ? 1 : -1) })
            if (!c) return { ok: false, error: "bad_request" }
            publisher.publish([communityObject(c)], null, p.action)
            return { ok: true }
        }
        if (p.action === "vote" || p.action === "like") {
            const post = update("posts", p.key, e => p.action === "vote"
                                ? { score: e.score + 1 } : { likes: e.likes + 1 })
            if (!post) return { ok: false, error: "bad_request" }
            publisher.publish([postObject(post)], null, p.action)
            return { ok: true }
        }
        return { ok: false, error: "bad_request" }
    }
}
