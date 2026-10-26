# Testing Basecamp shell test build 2

Build: [basecamp-shell-2](https://github.com/xAlisher/basecamp-experiments/releases/tag/basecamp-shell-2). Linux x86_64. A prototype, not an official Logos release.

## Run

1. Download `basecamp-shell-test-x86_64.AppImage` and `basecamp-shell-test-starter.tar.gz` into one folder.
2. `tar xzf basecamp-shell-test-starter.tar.gz`
3. `./basecamp-shell-test/run.sh`

It uses its own profile in `~/basecamp-shell-test`; your regular Basecamp is untouched. If you ran build 1, the same profile is kept and the demo apps are updated.

Eight demo apps are installed:

- **Activity:** Stub Wallet, Stub Chat, Stub Communities, Stub News, Stub Explorer. Open them once so they share activity with home.
- **Keychain:** Stub Signer, Stub Hardware Signer (each holds two test keys) and Stub Requester (asks for signatures).

All demo data is made up and the test keys are not protected. Nothing is fetched from the network.

## 1. Keychain

Keychain is a directory of public keys and a router for signing requests. It holds no private keys: the app that owns a key signs, and Keychain checks the signature before passing it on. Spec: [KEYCHAIN-SPEC](KEYCHAIN-SPEC.md), decision: [ADR-081](adr/0081-keychain-routes-signing-holds-no-keys.md).

### 1.1 Keys appear

1. Open **Stub Signer**, then **Stub Hardware Signer**. On first start each adds two keys.
2. Open **Keychain**.

Expected: two groups, each with its app id. Stub Signer has Daily key and Cold key; Stub Hardware Signer has Vault key and Badge key.

### 1.2 Your label and note

1. On a key, choose **Edit label and note**. Type a label (up to 40 characters) and a note (up to 500). **Save**.
2. Restart Basecamp.

Expected: the label is shown as the key's name, with "Named by the app: …" under it, and both survive the restart. No app ever sees them.

### 1.3 Choose a key and sign (the main test)

1. In **Stub Requester**, tap **Sign a transaction**. The requester has to be the app in front.
2. Keychain comes forward with **Signature request**: who asks (with app id), their words in quotes, and the keys that can sign a transaction: **Cold key** (Stub Signer) and **Vault key** (Stub Hardware Signer).
3. Pick **Vault key**, tap **Continue**.
4. Stub Hardware Signer opens (amber "Hw" badge) and shows who asks, the key and the bytes to sign. Tap **Approve**.

Expected: a **Signed** sheet: "Stub Hardware Signer (keychain_signer_hw) signed it with Vault key". The requester's last result shows status done and that the signature verifies.

Then:

- Repeat and pick **Cold key**: Stub Signer (teal "Si") opens instead.
- **Sign a login** offers Daily key and Badge key.

### 1.4 When it does not sign

| Do | Expected |
|---|---|
| Sign something, then **Reject** in the signer | Not signed: "… said no." Requester: rejected |
| In a signer turn on **Fail next**, then sign and approve | Not signed: failed. The requester gets no signature |
| Start a request, **Cancel** in Keychain | "You cancelled it in Keychain." |
| Ask again right after a cancel | Refused: busy. After 10 seconds it works |
| After **Continue**, Keychain shows "Waiting for …": tap **Open …** | The signer comes to the front |
| Close the requester's tab while the signer shows the request | The signer shows "Request ended" |
| **Send two at once** | The second answer: refused: busy |
| Tap a sign button while another app is in front | Refused: not in front, Keychain stays put |

### 1.5 Remove and add back

1. In Keychain, **Remove** a key and confirm.
2. In its signer app, tap **Register again**.

Expected: the key leaves Keychain with your label and note, then comes back without them.

## 2. Home widgets

Home's cards are widgets on a grid that you arrange. Spec: [HOME-GRID-SPEC](HOME-GRID-SPEC.md), decision: [ADR-080](adr/0080-home-cards-on-a-cell-grid.md).

### 2.1 The grid

1. On home, switch from the feed to the grid with the grid button next to the filter pills.
2. Widen and narrow the window.

Expected: the grid fills the window: 1 column under 600 px, 3 at about 1280 px, 5 at 1800 px, up to 8. A widget shows more items when it is bigger. There is one **Edit home** button, at the bottom.

### 2.2 Arrange

1. Tap **Edit home**, or press and hold a widget's header for half a second.
2. Drag a widget by its header to move it; the others make room.
3. Drag a widget's right edge, bottom edge or corner to resize it.
4. With the keyboard: Tab to a widget, arrow keys move it, Shift and arrows resize it.
5. Remove a widget with its ×, then tap **Undo**.
6. Tap **Done**, then restart Basecamp.

Expected: the hint at the bottom reads "Drag a widget to move it…". After the restart the layout is as you left it.

### 2.3 Cancel

1. Make a change in edit mode, then tap **Cancel**, or press Esc.

Expected: the layout returns to how it was.

### 2.4 Phone width

1. Narrow the window below 600 px. Enter edit mode.
2. Move a widget, resize its height with the bottom edge (the page scrolls when you drag near the edge).
3. Press Esc.

Expected: Esc leaves edit mode and home stays on screen.

## Not yet checked by a person

Real touch on a phone, real mouse drag on a wide monitor, whether **Copy public key** reaches your clipboard, and typing a label then pressing Return to save. Reports welcome: https://github.com/xAlisher/basecamp-experiments/issues
