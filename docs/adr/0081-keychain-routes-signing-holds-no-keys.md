# ADR-081: Keychain is a directory of public keys and a router for signing; it holds no private keys

**Status:** Accepted (experiment) · 2026-10-26 · Builds on ADR-067 (shell roles) and the shell intents of the objects work. Issues #55 to #60, epic #62.

## Context
Apps that need a signature (a login, a message, a transaction) and apps that hold keys had no shared way to meet, and the user had no single place to see which keys exist. `keystore_module` holds keys itself and has its own approval flow with `evm_signer_ui`; copying that into the shell would make Basecamp a key store, and every signer would have to trust it with secrets.

## Decision
- **Keychain holds public keys only.** Signer apps keep their private keys and register the public half, a name, the algorithm and what the key is for. The user may add a label (40 characters) and a note (500) per key, only in the Keychain app; no other app can read them.
- **Identity comes from the host.** The owner of a key and the requester of a signature are the caller the intent broker attested (module name plus catalog source), never a field the app sends. Wherever an app is named, its app id is shown next to its display name, and a name that is somebody else's is replaced by the id.
- **Built on what exists.** Apps call shell intents declared in their manifest (`basecamp.keychain.register / update / remove / keys / complete / reject / sign / cancel`), answered in C++ like `basecamp.objects.publish`. The shell reaches the signer with `keychain.sign` and `keychain.ended` through the same intent broker, which also brings apps forward. The Keychain app is a role app (`keychain`, Settings > Default apps) reaching the host only through the role-gated `shell` object.
- **Flow:** requester asks; Keychain shows who asks, their words marked as theirs, and the keys that can sign; the user picks one; the signer shows the request in its own words and the user approves or rejects; **Keychain verifies the signature** against the public key as it was when chosen (OpenSSL EVP Ed25519); the requester gets status, signature, public key and key id, and the shell shows Signed or Not signed over it.
- **Rules:** the requester hears only about the key the user chose, and only on `done`; one open request per app, plus a 10-second pause after a cancel or timeout and at most 5 a minute; only the app in front may ask; 5 minutes per request from when it is on screen; a bad signature ends as `failed` and is never passed on; only the key's owner can answer; the payload stays in memory and is never logged.
- **Signers keep secrets in host per-app data** (`basecamp.appdata.read / write`, one owner-only file per attested app), not QML LocalStorage, which other apps in the same process can open by name.

## Consequences
+ A requester needs to know no signer; a signer needs no trust from Keychain beyond its public key.
+ The user sees every key in one place and names them in their own words, without any app learning those words.
+ A compromised or buggy signer cannot hand the requester a signature that does not match the registered key.
- Only Ed25519 is verified. secp256k1 keys are stored but not offered, so EVM wallets need the `keystore_module` adapter (later) before they can sign through Keychain.
- Every answer is `ok` with the outcome in `data`, because the intent broker rewrites unknown error codes to `failed`.
- The shell's own Signed sheet names the key by the user's label; it is drawn over the requester but the requester never receives the label.
- On wide screens the request is a sheet over the Keychain app, not over the requesting app.
- Uninstall cleanup and the catalog source in the owner id are unit-tested only.

## Evidence
`app/Keychain.{h,cpp}`, `app/KeychainBroker.{h,cpp}`, `app/KeychainVerify.cpp`, `app/AppDataStore.*`, `app/ShellIntents.h`, `app/ShellApi.*`, `apps/basecamp_keychain/`, `src/Basecamp/Shell/KeychainResultDialog.qml`; tests `keychain_test`, `keychain_broker_test`, `keychain_app_test`, `keychain_stub_crypto_test`, `app_data_store_test`, `tst_qml_sandbox`. Commits c0d96ae, bc789ea, 946ab7e, 6c892e9, e8239c8, eb1860b. Stubs: `experiments/keychain-stubs` (369c013, d19596f). Spec: [KEYCHAIN-SPEC.md](../KEYCHAIN-SPEC.md). Screens: [request-1280](../keychain/1280-request.png), [signed-390](../keychain/390-signed.png).
