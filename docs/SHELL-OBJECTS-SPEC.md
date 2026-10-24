# Shell objects: apps publish, search ranks, views draw

Status: draft, 2026-10-24, not built. Decision: [ADR-072](adr/0072-apps-publish-typed-objects.md).
Grows out of [SHELL-ROLES-SPEC.md](SHELL-ROLES-SPEC.md) Part B and generalises
[NOTIFICATIONS-SPEC.md](NOTIFICATIONS-SPEC.md).

## TL;DR

- **Apps publish objects to the shell:** a transaction, a message, a community, a contact, a
  balance. Each object has a **type** from a shared list, so a wallet and a block explorer both
  publish `transaction` and the user sees one Transactions card, not two apps.
- **Three jobs, three owners:**
  - the **host keeps the objects** (the store) and enforces who may read and write;
  - the **`search` role ranks** them for a query and a filter, and draws nothing;
  - **views draw** them (home, Applications, the tab switcher), one look per type, whichever app
    the object came from.
- Objects **update in place** (a transfer's progress, a balance), **carry actions** (Join, View,
  vote, reply) that go back to the app that published them, and **refer to each other** (a
  transaction's counterparty is a contact) by a shared subject, never by display name.
- The **bar's text is the query** in every view; filters are pills; a home **module is a saved
  filter** with a size.
- Nothing is collected unless an app publishes it; Settings → Search has a switch per app, and
  switching off **deletes** what that app published. **Whether objects may carry private content
  (message text, amounts) is open** (§9); the experiment carries it, in memory only, on the device.

## 1. Why

Each app keeps its world to itself, so the shell can't answer "what happened", "where is that
thing" or "do the next step" across apps. Notifications (ADR-042, ADR-043) were the first object
apps could hand to the shell; they are one type among several. The roles work (ADR-067 to ADR-071)
gave the shell a way to let a user-chosen app do a system job; search and home are those jobs here.

## 2. Who does what

| Part | Who | Does | Never |
|---|---|---|---|
| Publisher | any app declaring `basecamp.objects.publish` | publishes, updates and retracts its own objects; answers actions on them | reads other apps' objects |
| Store | the host (`ObjectStore`, next to `NotificationCenter`) | keeps objects per app, applies limits, switches and retraction, routes actions | ranks or draws |
| Search | the `search` role holder | takes a query + filter, returns ranked object ids | draws, or acts on objects |
| Views | `home`, `launcher`, the shell's switcher | ask for results, draw them by type, raise actions | read objects outside a result |

With no `search` holder, the host's own fallback ranker answers (title and keyword match, newest
first), so search never breaks because a role app did.

## 3. The object

One envelope for every type. Fields marked *host* are set by the host and can't be supplied.

| Field | Type | Rules |
|---|---|---|
| `id` | string | *host.* Stable for `app` + `key`. |
| `app` | string | *host*, from the intent requester. |
| `key` | string | Required, max 128. The app's own id for this thing. Same `app` + `key` **replaces** (the notification `tag` rule, generalised). |
| `type` | string | Required. From the type list (§4), or `x.<app>.<name>` for an app's own type (drawn with the generic card). |
| `kind` | `item` \| `summary` \| `action` \| `setting` | Default `item`. `summary` stands for many ("41 more transactions today", "2 scheduled"); `action` is something to do ("Send", "New chat"); `setting` is a place in the app's settings. |
| `title` | string | Required, plain text, max 80. What the generic card and a one-line row show. |
| `subtitle` | string | Optional, max 120. |
| `fields` | object | The type's fields (§4). Unknown fields are dropped with a diagnostic. |
| `keywords` | string[] | Optional, max 16 × 32. Extra words to match; not shown. |
| `subjects` | string[] | Optional, max 8. Who or what this is about (§6). |
| `refs` | `[{ type, key, app? }]` | Optional, max 8. Other objects this one points at. |
| `actions` | `[{ id, label, input? }]` | Optional, max 3. `input: "text"` asks the view for a line of text (reply). The first action is primary. Clicking the object itself is always `open`. |
| `progress` | 0..100 | Optional. Drawn as a bar; `100` reads as done. |
| `time` | ms since epoch | Optional; defaults to publish time. What the feed sorts by. |
| `updated` | ms | *host.* |
| `seen` | bool | *host.* False until a view shows it; "N new since …" counts unseen. |
| `expires` | ms | Optional. The host drops the object after it. |
| `icon` | url | *host*: the publisher's local icon (ADR-071). Per-object pictures (avatars, coin logos) are not in v0 (§9). |

Plain text only, as for notifications: no markup, links or images from apps.

## 4. Types (v0)

Fields are the minimum each card needs. Every field is marked **label** (says what the thing is)
or **content** (says something personal: amounts, text, who you deal with). The marks are what
the open privacy decision (§9) will act on; v0 carries both.

| Type | Fields (label) | Fields (content) | Typical actions |
|---|---|---|---|
| `account` | `name`, `network` | `balance` `{ amount, unit, fiat? }` | open |
| `asset` | `name`, `symbol`, `account` (ref) | `amount`, `fiat`, `change` (%) | open |
| `transaction` | `direction` (`in` \| `out`), `status` (`pending` \| `sending` \| `receiving` \| `done` \| `failed` \| `scheduled`), `network` | `amount`, `unit`, `counterparty` (subject), `address` | View |
| `message` | `conversation` (ref or name), `outgoing` (bool) | `sender` (subject), `senderName`, `text` | Reply (`input: "text"`) |
| `post` | `community` (ref) | `author` (subject), `authorName`, `text`, `score`, `likes` | Vote, Like |
| `community` | `name`, `members`, `joined` (bool) | | Join / Leave |
| `contact` | `name`, `groups` (`family`, …) | `subject` | View, Message |
| `news` | `source`, `url` (not opened by the shell) | `text` | open |
| `notification` | `level` | `body` | as today (ADR-042) |

Reserved, no fields yet: `profile`, `nft`. Published by the host itself: `app` (installed apps,
for search only), `tab` (open and recently closed tabs).

**Counts across types** ("Ada: 3 transactions") are not fields: a view asks the search role for
objects whose `subjects` include the contact's subject and counts them.

## 5. API

### Publishing (any app)

Declared in `metadata.json` like notifications:

```json
"uses": [{ "intent": "basecamp.objects.publish" }]
```

```qml
logos.request("basecamp.objects.publish", { objects: [
    { key: "tx:0x91…", type: "transaction", title: "Sending to Ada", progress: 20,
      fields: { direction: "out", status: "sending", amount: "0.3319", unit: "BTC",
                counterparty: "logos:pk:…" },
      subjects: ["logos:pk:…"], actions: [{ id: "view", label: qsTr("View") }] }
] }, res => {})
logos.request("basecamp.objects.retract", { keys: ["tx:0x91…"] }, res => {})   // or { all: true }
```

- Answers as for notifications: `ok`, `not_declared`, `bad_request` (with the first bad object's
  index), `failed` (limit). **A switched-off app still gets `ok`.**
- Batches are all-or-nothing.
- **Actions come back** as `basecamp.objects.action` to the publisher: `{ key, action, text? }`.
  The app declares that it handles it (`"provides"`), so only the publisher of `key` receives it.
  `open` is always delivered and the app is brought to front first.

### Search role

The `shell` object (ADR-069) gains, for the `search` holder only:

- `searchRequested(requestId, query, filter)`: a view wants results. `filter` is
  `{ types?, apps?, kinds?, subjects?, unseenOnly? }`.
- `objects(filter)`: the objects the filter allows, as a list of envelopes.
- `answer(requestId, ids)`: ranked ids, at most 200. Unknown ids are dropped.
- `objectsChanged(app)`: something was published or retracted; a search may re-rank.

### Views

For `home` and `launcher` holders (and the shell's own switcher):

- `search(query, filter)`: returns a live results model of envelopes in rank order; it refills when
  the search role answers again.
- `raise(id, action, text?)`: send an action to the object's publisher.
- `markSeen(ids)`.

Views get only the objects in their results, not the whole store.

## 6. Subjects: how objects refer to people and things

A contact in a chat app and the counterparty of a transfer in a wallet must match without either
app knowing the other. Objects say who they are about with **subjects**: opaque strings with a
scheme, compared exactly.

- `logos:pk:<hex>`: a Logos public key (chat identity, node identity).
- `<network>:<address>`: a chain address (`lez:…`, `btc:…`).
- `x.<app>:<id>`: anything app-local (matches only within that app).

Names never join objects: two "Ada"s stay two people. **Open:** a contact usually has a key
while a transfer has an address; something has to say "this address belongs to this contact"
(§9).

## 7. Views and the bar

- **The bar is the search field** in every view; its placeholder names the active filter
  ("Search transactions"). Typing narrows the current view; the view never loses the field to a
  title.
- **Filters are pills**: one per type that has objects, plus per-app; the row scrolls. A pill can
  be pinned.
- **A home module is a saved filter**: `{ name, filter, size (small | medium | large | row),
  pinned, notify }`. "Add custom module" saves the current query and pills. Order, size, pin and
  notify belong to the home app (its own settings), never to the objects.
- **Feed** is all objects by `time`; "N new since 14:48" counts unseen objects since the user last
  looked.
- **Cards are drawn by type** from `fields`, with a generic card (icon, title, subtitle, time,
  actions) for anything else. The card set lives with the shell's shared QML (like the app tile),
  so a replacement home can reuse it or draw its own.

## 8. Settings, limits, revocation

- **Settings → Search:** one row per app that declares `basecamp.objects.publish`, with a switch,
  as for notifications. **Off removes everything that app published at once**; while off, new
  publishes are dropped (still answering `ok`). Uninstalling an app removes its objects.
- Limits per app: 500 objects (oldest `time` evicted first), 20 publish requests per 10 s,
  `fields` at most 2 KB per object. Over the limit answers `failed`.
- **v0 keeps everything in memory** and loses it on restart; apps republish what is current when
  they start. Nothing is written to disk and nothing leaves the device.
- Only the `search` holder reads across apps; views read only their results; publishers read
  nothing.

## 9. Open questions

- **Privacy: content or pointer.** Do objects carry personal content (message text, amounts,
  counterparties), or only labels, with content fetched from the app when shown?
  - *Content in the object* (v0): fast, works with the app closed, but the shell and the search
    app hold a copy of everyone's private data.
  - *Pointer:* the object has labels; the view asks the app for the content when drawing. Nothing
    personal leaves the app, but search can't match on content and a closed app shows blanks.
  - *Per field:* label fields pushed, content fields pulled; the field marks in §4 make this
    possible without changing the types.
  - Whatever is chosen, it is decided before anything ships outside this experiment, and Settings
    must say plainly what each app shares.
- **Subjects and ownership:** who says an address belongs to a contact (the wallet, the contact app,
  the user)?
- **Persistence:** in memory is honest for v0; a feed that survives restarts needs storage, and
  then encryption and deletion become part of the switch.
- **Pictures:** avatars and coin logos need a way to publish small images without remote URLs
  (ADR-071).
- **Ranking signals:** recency, frequency of opening, unseen, pinned pills; which belong to the
  search role and which to the view.
- **Type list ownership:** who adds a type; how an app type (`x.<app>.<name>`) graduates.
- **Layouts:** home and modules are Basecampium-only today (ADR-070); the sidebar and desktop
  layouts need their own views.
- **Abuse:** notifications have a list of forbidden uses (re-engagement, urgency); objects need the
  same before third-party publishers.

## 10. Plan

1. **Store and intents (host).** `ObjectStore`, `basecamp.objects.publish/retract/action`, limits,
   Settings → Search with switches; notifications become a type internally, their intent stays.
   Unit-tested; verified with a test app.
2. **Stub publishers** (`experiments/objects-stubs/`), backends stubbed, each with a "flood" button
   (1000 objects):
   - Wallet: accounts, assets, transactions with moving progress, summaries;
   - Chat: messages, contacts, reply;
   - Communities: communities, posts, Join, votes;
   - News: news items;
   - Explorer: a second source of `transaction`, sharing subjects with Chat's contacts.
3. **`basecamp_search`**, the default `search` role app: ranks by match, recency and unseen.
4. **Views:** home modules, pills and feed in `basecamp_home`; the per-type cards; the bar as query
   everywhere in Basecampium.
5. **Experiments:** grid vs. feed densities; "N new since"; a module per saved filter; replacing the
   search app; flood performance at 390 and 1280; the same data with content vs. pointer.
