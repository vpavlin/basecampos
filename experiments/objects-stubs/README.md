# Objects stubs

Five QML-only `ui_qml` apps that publish invented data into the shell's object store, for
[SHELL-OBJECTS-SPEC.md](../../docs/SHELL-OBJECTS-SPEC.md) §10 step 2 (issue #24). No backends:
the data is invented and progress is a timer.

| App | Display name | Publishes | Actions it answers |
|---|---|---|---|
| `objects_wallet` | Stub Wallet | 2 `account`, 5 `asset`, 6 `transaction` (2 `pending`, +10 % every 3 s until `done`), 2 `transaction` summaries | `view` (recorded) |
| `objects_chat` | Stub Chat | 6 `contact`, 3 `x.objects_chat.conversation`, 12 `message` (3 outgoing) | `reply` with text (publishes an outgoing message and updates the conversation), `message` (recorded) |
| `objects_communities` | Stub Communities | 5 `community`, 8 `post` | `join` / `leave` (flip `joined`, ±1 member), `vote` (score +1), `like` (likes +1), all republished in place |
| `objects_news` | Stub News | 6 `news` (`url` as text only) | `open` only |
| `objects_explorer` | Stub Explorer | 5 `transaction` on `lez` | `view` (recorded) |

`open` is answered `ok` by every app. Unknown actions answer `bad_request`; an action from any
requester other than the shell (`main_ui`) answers `failed`.

**Shared subjects.** Chat's contacts are `logos:pk:<64 hex>`. Explorer's counterparties are
Ada, Bram and Cleo, and two of Wallet's are Ada and Juno, with the same keys hardcoded, so a view
can count one person's objects across apps (§4, §6).

## Each app

- `metadata.json` declares `uses` `basecamp.objects.publish` and `basecamp.objects.retract`, and
  `provides` `objects.action` with `params` `key`, `action` (required) and `text` (optional).
- On start publishes its data; if any of it is refused, retries once after 2 s. Every answer is
  logged as `qml: [objects_<x>] …`.
- UI: objects in the store per type, the last answer, the last action, and three buttons.
  objectNames, with `<app>` the app name (e.g. `objects_wallet`):
  - `<app>.root`, `<app>.total`, `<app>.count.<type>`, `<app>.lastAnswer`
  - `<app>.lastAction` (label), `<app>.floodReport` (label)
  - `<app>.flood` (Flood 1000), `<app>.publish` (Publish again), `<app>.retractAll` (Retract all)
- **Flood 1000** tops the store up to the 500-object cap instead of pushing past it (past the cap
  the store evicts the oldest objects, which would delete the base data). It reports
  `published, held back (cap 500), refused by the store`. Flood objects are timed older than
  the base data, so any later overflow evicts them first.

Client-side limits (`common/ObjectPublisher.qml`): at most 20 requests in any 10.5 s (publish and
retract both counted), at most 50 objects and 900 payload nodes per request (the intent bridge
refuses over 1000 nodes). The UI's counts mirror the store from its answers, including its
eviction rule.

## Shared code

`common/StubApp.qml` (frame, start/retry, flood, action routing, UI) and
`common/ObjectPublisher.qml` (queue, rate limit, batching, mirror) are the masters. `sync.sh`
copies them into each app **renamed** (`WalletStubApp.qml`, `WalletObjectPublisher.qml`, …):

- the plugin sandbox refuses QML imports from outside an app's own folder, so each app needs
  its own copy;
- on the shell-roles build (2026-10-24), two `ui_qml` apps that each ship a sibling file with
  the same name collide: the second app to load resolved `StubApp` to the first app's file, the
  sandbox blocked it, and the view failed with `Type StubApp unavailable`. Unique names avoid it.
  This looks like a host bug worth its own issue.

Edit `common/`, then run `./sync.sh` (and `./sync.sh --check` before committing).

## Install into a profile

```
./sync.sh --install <profile>
```

copies each app folder to `<profile>/plugins/<app>/` (the same as `cp -a objects_wallet
<profile>/plugins/objects_wallet` for each). Never into `/extra/tmp/mmorpg/roles-profile`: copy
that first. Open an app headless with `backend.loadUiModule("objects_wallet")`.
