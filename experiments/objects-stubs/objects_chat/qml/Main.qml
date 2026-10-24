import QtQuick

// Stub Chat (SHELL-OBJECTS-SPEC.md §10 step 2): contacts, conversations and
// messages, with Reply. No network: a reply is published as an outgoing
// message and nothing is sent anywhere.
//
// Contacts are identified by `logos:pk:<hex>` subjects (§6). Stub Explorer
// and Stub Wallet hardcode the same keys for Ada, Bram, Cleo and Juno, so a
// view can join a transfer's counterparty to a contact without either app
// knowing the other. Names never join objects: only these strings do.
//
// Conversations have no type in the v0 list (§4), so they are published as
// this app's own type, `x.objects_chat.conversation`, drawn with the generic
// card; a message points at its conversation through `refs` and names it in
// `fields.conversation`.
ChatStubApp {
    id: app
    objectName: "objects_chat.root"
    appName: "objects_chat"
    displayName: "Stub Chat"
    blurb: "Publishes 6 contacts, 3 conversations and 12 messages. Reply on a message or conversation publishes your answer as a new outgoing message."

    readonly property string convType: "x.objects_chat.conversation"
    readonly property int minute: 60 * 1000

    readonly property var contacts: [
        { id: "ada",  name: "Ada Morrow",  groups: ["friends"],
          pk: "logos:pk:38dc3d45be876f9687cbae30b8e046000b31e358ebc6c5b995f23ede0f5381bb" },
        { id: "bram", name: "Bram Telles", groups: ["family"],
          pk: "logos:pk:b3259ec6ea7b7770948b98cbeb81ddff8146105619e68fb4bd9986ec0648a502" },
        { id: "cleo", name: "Cleo Annis",  groups: ["friends", "work"],
          pk: "logos:pk:53e64365aa71e8d9f06313149cc9f44c89f50c7829324f504498f6b9a0aa0556" },
        { id: "juno", name: "Juno Vale",   groups: ["family"],
          pk: "logos:pk:0016c5021b7c70f3957d01960d342e0d6a12861bd30f232c4cbaa42084847233" },
        { id: "esme", name: "Esme Quill",  groups: ["work"],
          pk: "logos:pk:a14c9319bade4320fae9f9b274374406f040eda738e1be8ccc18abf7f50e5235" },
        { id: "femi", name: "Femi Arden",  groups: ["work"],
          pk: "logos:pk:6d17dbb9f5da72328b96a65f359c4f2afefe43820f8f751f2b0482e38b0b6f76" }
    ]

    readonly property var conversations: [
        { key: "conv:ada",  name: "Ada Morrow",     members: ["ada"] },
        { key: "conv:hike", name: "Weekend hike",   members: ["bram", "cleo", "juno"] },
        { key: "conv:ops",  name: "Node operators", members: ["esme", "femi", "ada"] }
    ]

    // from: a contact id, or "" for an outgoing message. age in minutes.
    property var messages: [
        { key: "msg:ada:1",  conv: "conv:ada",  from: "ada",  age: 190, text: "Did the transfer go through?" },
        { key: "msg:ada:2",  conv: "conv:ada",  from: "",     age: 185, text: "Sending it now, give it a minute." },
        { key: "msg:ada:3",  conv: "conv:ada",  from: "ada",  age: 12,  text: "Got it, thanks! Lunch on Friday?" },
        { key: "msg:hike:1", conv: "conv:hike", from: "bram", age: 300, text: "Weather looks good for Saturday." },
        { key: "msg:hike:2", conv: "conv:hike", from: "cleo", age: 280, text: "I can bring the stove and the map." },
        { key: "msg:hike:3", conv: "conv:hike", from: "",     age: 270, text: "I'll sort out snacks and water." },
        { key: "msg:hike:4", conv: "conv:hike", from: "juno", age: 95,  text: "Meet at the station at 8?" },
        { key: "msg:hike:5", conv: "conv:hike", from: "bram", age: 40,  text: "8 works. Bring a rain jacket just in case." },
        { key: "msg:ops:1",  conv: "conv:ops",  from: "esme", age: 600, text: "My node fell behind after the restart." },
        { key: "msg:ops:2",  conv: "conv:ops",  from: "",     age: 590, text: "Check the peer count first, it was zero on mine." },
        { key: "msg:ops:3",  conv: "conv:ops",  from: "femi", age: 120, text: "New release is out, upgrading tonight." },
        { key: "msg:ops:4",  conv: "conv:ops",  from: "esme", age: 25,  text: "Synced again. Thanks for the tip." }
    ]
    property int replySeq: 0
    property real baseTime: Date.now()

    function contact(id) { return contacts.find(c => c.id === id) }
    function conversation(key) { return conversations.find(c => c.key === key) }

    function contactObject(c, i) {
        return { key: "contact:" + c.id, type: "contact", title: c.name,
                 subtitle: c.groups.join(", "),
                 time: baseTime - (60 + i) * 24 * 60 * minute,
                 fields: { name: c.name, groups: c.groups, subject: c.pk },
                 subjects: [c.pk],
                 actions: [{ id: "message", label: "Message" }] }
    }

    function messageObject(m) {
        const conv = conversation(m.conv)
        const out = m.from === ""
        const sender = out ? null : contact(m.from)
        const o = { key: m.key, type: "message",
                    title: clip((out ? "You" : sender.name) + ": " + m.text, 80),
                    subtitle: conv.name,
                    time: m.time !== undefined ? m.time : baseTime - m.age * minute,
                    fields: { conversation: conv.name, outgoing: out,
                              senderName: out ? "You" : sender.name,
                              text: m.text.slice(0, 500) },
                    refs: [{ type: convType, key: conv.key }] }
        if (!out) {
            o.fields.sender = sender.pk
            o.subjects = [sender.pk]
            o.actions = [{ id: "reply", label: "Reply", input: "text" }]
        } else if (conv.members.length === 1) {
            o.subjects = [contact(conv.members[0]).pk]   // a direct chat is about that person
        }
        return o
    }

    function conversationObject(conv) {
        const own = messages.filter(m => m.conv === conv.key)
        const last = own[own.length - 1]
        const lastTime = last.time !== undefined ? last.time : baseTime - last.age * minute
        return { key: conv.key, type: convType, title: conv.name,
                 subtitle: clip((last.from === "" ? "You: " : contact(last.from).name + ": ") + last.text, 120),
                 time: lastTime,
                 fields: {},
                 subjects: conv.members.map(id => contact(id).pk),
                 keywords: conv.members.length > 1 ? ["group"] : ["direct"],
                 actions: [{ id: "reply", label: "Reply", input: "text" }] }
    }

    baseObjects: function () {
        baseTime = Date.now()
        return contacts.map(contactObject)
            .concat(conversations.map(conversationObject), messages.map(messageObject))
    }

    floodObject: function (seq, now) {
        const c = contacts[seq % contacts.length]
        return { key: "flood:msg:" + seq, type: "message",
                 title: c.name + ": flood message " + seq, subtitle: "Flood room",
                 time: now - (seq + 10) * minute,
                 fields: { conversation: "Flood room", outgoing: false, sender: c.pk,
                           senderName: c.name, text: "Flood message " + seq },
                 subjects: [c.pk], keywords: ["flood"] }
    }

    // Reply: on a message, answers in that message's conversation; on a
    // conversation, in that one. Publishes the new message and the
    // conversation (its subtitle and time follow the last message).
    handleAction: function (p) {
        if (p.action === "open" || p.action === "message") {
            app.log("recorded " + p.action + " on " + p.key)
            return { ok: true }
        }
        if (p.action !== "reply") return { ok: false, error: "bad_request" }
        if (p.text.trim() === "") return { ok: false, error: "bad_request" }

        const msg = messages.find(m => m.key === p.key)
        const conv = msg ? conversation(msg.conv) : conversation(p.key)
        if (!conv) return { ok: false, error: "bad_request" }

        const m = { key: "msg:" + conv.key.slice(5) + ":out" + (++replySeq), conv: conv.key,
                    from: "", time: Date.now(), text: p.text.trim() }
        messages = messages.concat([m])
        publisher.publish([messageObject(m), conversationObject(conv)], null, "reply")
        return { ok: true }
    }
}
