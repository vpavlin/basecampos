import QtQuick

// Stub Hardware Signer: a pretend hardware wallet, the second signer app, so
// Keychain has keys from two apps to offer for a transaction or a login.
// "Vault key" signs transactions, "Badge key" logins. Everything else is the
// shared signer in HwSignerApp.qml (master: ../../common/SignerApp.qml).
HwSignerApp {
    objectName: "keychain_signer_hw.root"
    appName: "keychain_signer_hw"
    displayName: "Stub Hardware Signer"
    blurb: "A pretend hardware wallet: two test keys, signs what you approve. Keychain only knows their public halves."
    badge: "Hw"
    accent: "#8a5a14"
    defaultKeys: [
        { keyId: "vault", name: "Vault key", purposes: ["transaction"] },
        { keyId: "badge", name: "Badge key", purposes: ["login"] }
    ]
}
