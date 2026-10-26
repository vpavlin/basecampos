import QtQuick

// Stub Signer (basecamp-experiments#59): a signer app for testing Keychain.
// "Daily key" signs messages and logins, "Cold key" transactions. Everything
// else is the shared signer in StubSignerApp.qml (master: ../../common/SignerApp.qml).
StubSignerApp {
    objectName: "keychain_signer.root"
    appName: "keychain_signer"
    displayName: "Stub Signer"
    blurb: "Holds two test keys and signs what you approve. Keychain only knows their public halves."
    badge: "Si"
    accent: "#2e7d8c"
    defaultKeys: [
        { keyId: "daily", name: "Daily key", purposes: ["message", "login"] },
        { keyId: "cold", name: "Cold key", purposes: ["transaction"] }
    ]
}
