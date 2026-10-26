#pragma once

#include <QString>
#include <QStringList>

class IntentRegistry;

namespace ShellIntents {

inline const QStringList kPackageConfirmIntents = {
    QStringLiteral("basecamp.packages.confirm_install"),
    QStringLiteral("basecamp.packages.confirm_uninstall"),
    QStringLiteral("basecamp.packages.confirm_upgrade"),
};

// Of those, the ones no third party has a legitimate reason to raise. Restricted
// to whichever app holds the `packages` role (ADR-067) — the user's assignment,
// not a self-declared module name. The default holder is package_manager_ui.
inline const QString kPackagesRole = QStringLiteral("packages");
inline const QStringList kRestrictedToPackageManagerUi = {
    QStringLiteral("basecamp.packages.confirm_uninstall"),
    QStringLiteral("basecamp.packages.confirm_upgrade"),
};

// PURE NAVIGATION, and hand-offs to a one. `ok` means "you are there", not "we
// are done", so the broker must not bounce the user back out of a destination
// it was asked to take them to.
inline const QStringList kNavigationIntents = {
    QStringLiteral("basecamp.repositories.manage"),
    QStringLiteral("basecamp.settings.open"),
    QStringLiteral("basecamp.apps.open"),
    QStringLiteral("basecamp.apps.launch"),
    QStringLiteral("basecamp.packages.open"),
};

// Post a notification: `{ title, body?, level?, tag? }` (NOTIFICATIONS-SPEC.md).
// Not navigation: it answers with `{ id }` and takes the user nowhere.
inline const QString kNotificationsPostIntent =
    QStringLiteral("basecamp.notifications.post");

// Shell objects (SHELL-OBJECTS-SPEC.md, ADR-072). Publish `{ objects: [ … ] }`
// and retract `{ keys: [ … ] }` or `{ all: true }`, answered in C++ like
// notifications (ObjectStore). Not navigation either.
inline const QString kObjectsPublishIntent =
    QStringLiteral("basecamp.objects.publish");
inline const QString kObjectsRetractIntent =
    QStringLiteral("basecamp.objects.retract");

// An action on an object, sent back to the app that published it alone:
// `{ key, action, text? }`. NOT "basecamp.*": the shell is the requester here
// and the publishing app the provider, and the registry refuses that
// namespace from any app's `provides`. A publisher that wants actions declares
// it in "provides"; the shell names it as the only provider (submitTo), so a
// second app providing the same intent never hears another app's actions.
// And the shell is its only permitted REQUESTER (registerWith restricts it):
// an app declaring it in `uses` is answered `unavailable`. A publisher should
// still accept it only from requester "main_ui" — belt and braces, and the
// rule an app can check on its own side.
inline const QString kObjectsActionIntent = QStringLiteral("objects.action");

// Keychain (basecamp-experiments#55, #56; Keychain.h, KeychainBroker.h): a
// directory of public keys and a router for signing requests. Answered in C++
// like the objects intents, with the caller the broker attested as the owner
// or requester, never a payload field. Not navigation.
//
// Every answer is `ok` with its outcome in `data`: the broker coerces error
// codes it does not know (busy, duplicate, …) to `failed`, which would hide
// from an app why it was refused.
//
// Signer apps:
//   register { keyId, name, publicKey, algorithm, purposes? } -> { ok, error? }
//   update   { keyId, name?, purposes? }                       -> { ok, error? }
//   remove   { keyId }                                         -> { ok, error? }
//   keys     {}                                                -> { keys: [ … ] }
//   complete { requestId, signature }                          -> { ok, status? }
//   reject   { requestId, reason? }                            -> { ok }
// Requesting apps:
//   sign     { payload, kind, description, algorithm?, purpose? }
//            (purpose, when given, must be the kind: bad_request otherwise)
//            answered once the request ends, with signatureResult's map
//            { requestId, status, signature?, publicKey?, keyId?, keyName?,
//            signerApp?, error? }, or at once with { error } (bad_request, busy,
//            unavailable, not_in_front). Held open meanwhile: a dispatch the shell is
//            handling has the broker's 10-minute backstop, above the
//            Keychain's own 5-minute timeout.
//   cancel   { requestId? }                                    -> { ok }
inline const QString kKeychainRole = QStringLiteral("keychain");
inline const QString kKeychainRegisterIntent = QStringLiteral("basecamp.keychain.register");
inline const QString kKeychainUpdateIntent   = QStringLiteral("basecamp.keychain.update");
inline const QString kKeychainRemoveIntent   = QStringLiteral("basecamp.keychain.remove");
inline const QString kKeychainKeysIntent     = QStringLiteral("basecamp.keychain.keys");
inline const QString kKeychainSignIntent     = QStringLiteral("basecamp.keychain.sign");
inline const QString kKeychainCancelIntent   = QStringLiteral("basecamp.keychain.cancel");
inline const QString kKeychainCompleteIntent = QStringLiteral("basecamp.keychain.complete");
inline const QString kKeychainRejectIntent   = QStringLiteral("basecamp.keychain.reject");
inline const QStringList kKeychainIntents = {
    kKeychainRegisterIntent, kKeychainUpdateIntent, kKeychainRemoveIntent,
    kKeychainKeysIntent, kKeychainSignIntent, kKeychainCancelIntent,
    kKeychainCompleteIntent, kKeychainRejectIntent,
};

// The shell to the signer app that owns the chosen key, and to it alone
// (submitTo), like `objects.action`: `keychain.sign` { requestId, keyId,
// payload, kind, purpose, requesterApp, requesterName, description }, answered `ok` once the signer
// shows it; `keychain.ended` { requestId, status } when the request ended
// while it had it, for a reason other than its own answer. The signer
// declares both in "provides"; the shell is their only permitted requester.
inline const QString kKeychainSignerIntent = QStringLiteral("keychain.sign");
inline const QString kKeychainEndedIntent  = QStringLiteral("keychain.ended");

// An app's own data, kept by the host (AppDataStore.h): one JSON object per
// app that no other app can read, which QML LocalStorage cannot promise
// inside one process. The app is the attested caller. Answered `ok` with the
// outcome in `data`, as the keychain intents:
//   read  {}           -> { ok, data }
//   write { data: {} } -> { ok, error? }   (bad_request, too_big, failed)
inline const QString kAppDataReadIntent  = QStringLiteral("basecamp.appdata.read");
inline const QString kAppDataWriteIntent = QStringLiteral("basecamp.appdata.write");
inline const QStringList kAppDataIntents = { kAppDataReadIntent, kAppDataWriteIntent };

// Bring a named app forward: `{ "app": "wallet_ui" }`.
inline const QString kAppLaunchIntent = QStringLiteral("basecamp.apps.launch");
inline const QString kPackageManagerAppName =
    QStringLiteral("package_manager_ui");
inline const QString kPackagesOpenIntent =
    QStringLiteral("basecamp.packages.open");
inline const QString kAppLaunchParam  = QStringLiteral("app");

// Which of the shell's own capabilities a URL clicked outside Basecamp may
// reach. A SUBSET of what the shell provides, and deliberately not all of it:
// every entry here is something a web page can cause, so the confirm intents —
// which gate installing and removing packages — are absent and must stay so.
//
// SPELLED OUT, NOT `= kNavigationIntents`. The alias was the bug: because the
// broker skips the chooser when the shell is the provider (IntentBroker's
// `isShellProvider(only)` branch), this list is the entire boundary between a
// web page and a shell capability — there is no consent step behind it. As an
// alias, adding a navigation intent for an internal reason silently published
// it to every browser, in one line, with nothing at the edit site saying so.
//
// Adding an entry here is a decision to let any web page cause it, unprompted.
inline const QStringList kWebReachableIntents = {
    QStringLiteral("basecamp.repositories.manage"),
    QStringLiteral("basecamp.settings.open"),
    QStringLiteral("basecamp.apps.open"),
    QStringLiteral("basecamp.apps.launch"),
    QStringLiteral("basecamp.packages.open"),
};

// Declare the shell's provides, hand-offs, uses and requester restrictions.
void registerWith(IntentRegistry* registry,
                  const QString& shellModuleName,
                  const QString& displayName,
                  const QString& iconSource);

} // namespace ShellIntents
