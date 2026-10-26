#pragma once

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <functional>

// Keychain, part 1: the key registry (basecamp-experiments#55).
//
// A DIRECTORY OF PUBLIC KEYS. Signer apps keep their private keys themselves;
// what they register here is the public half, a name and what the key is for,
// so the shell can route a signing request to the app that can answer it
// (KeychainBroker) and check the signature that comes back.
//
// The owning app is always the caller the host sees: the broker's attested
// requester for the `basecamp.keychain.*` shell intents (MainUIBackend), plus
// the catalog source the host knows it was installed from. Nothing in the
// arguments names an owner, so an app can only ever touch its own entries.
//
// The user's own label and note belong to the user. Only the `keychain` role
// sets or reads them (ShellApi checks the role; this class does not know who
// holds it). myKeys() leaves them out and updateKey() never clears them.
//
// One file, keychain.json in the profile, rewritten whole through QSaveFile on
// every change. An empty path keeps everything in memory (tests).
//
// QtCore only, so keychain_test.cpp can drive it without the rest of the host.
class Keychain : public QObject {
    Q_OBJECT
public:
    static constexpr int kMaxKeysPerApp = 32;
    static constexpr int kMaxKeyId = 64;
    static constexpr int kMaxName = 64;
    static constexpr int kMaxLabel = 40;
    static constexpr int kMaxNote = 500;

    struct Key {
        QString ownerApp;      // module name, from the host
        QString ownerSource;   // catalog source the host knows ("" = none)
        QString ownerName;     // the app's display name, from its manifest
        QString keyId;         // the app's own id for the key, unique per app
        QString name;
        QString algorithm;     // "ed25519" | "secp256k1"
        QByteArray publicKey;  // raw bytes
        QStringList purposes;  // "message" | "transaction" | "login"; none = any
        qint64 added = 0;      // ms since epoch
        QString label;         // the user's, <= kMaxLabel
        QString note;          // the user's, <= kMaxNote
    };

    // The algorithms a key may be registered with. Only ed25519 signatures
    // are checked today; secp256k1 keys are kept for later and never offered
    // for signing (KeychainBroker).
    static const QStringList& algorithms();
    static const QStringList& purposeNames();
    // The raw public key length(s) `algorithm` takes.
    static bool validPublicKey(const QString& algorithm, const QByteArray& key);
    // First and last 6 characters of the base64 public key.
    static QString fingerprint(const QByteArray& publicKey);

    // Who the user is told an app is (basecamp-experiments#60). An app's
    // display name is its own claim, so it is shown only when it is not
    // somebody else's: not "Keychain" or "Basecamp" (reservedNames), and not
    // one of `taken` (the names of the other installed apps and of the role
    // holders). Compared on letters and digits, case folded, so "Key-chain"
    // is "Keychain". Otherwise, or when empty, the module id the host
    // attested. The id is shown next to the name everywhere anyway.
    static const QStringList& reservedNames();
    static QString shownName(const QString& app, const QString& displayName, const QStringList& taken);
    // The same with this registry's view of the other apps' names.
    QString shownName(const QString& app, const QString& displayName) const;

    explicit Keychain(QString filePath = QString(), QObject* parent = nullptr);

    // Who the host sees: the catalog source an installed app came from, and
    // its display name. Both read at the time of each call.
    void setSourceResolver(std::function<QString(const QString&)> fn) { m_sourceFor = std::move(fn); }
    void setNameResolver(std::function<QString(const QString&)> fn) { m_nameFor = std::move(fn); }
    // The display names `app` may not use: every other installed app's and
    // each role holder's (shownName). Read at the time of each call.
    void setTakenNames(std::function<QStringList(const QString&)> fn) { m_takenFor = std::move(fn); }
    void setClock(std::function<qint64()> clock) { m_now = std::move(clock); }

    // ── For apps (`app` is the attested caller) ─────────────────────────
    // { ok: true } or { ok: false, error }, error one of:
    //   bad_request  a field missing, malformed or over its limit
    //   bad_key      not base64, or the wrong length for the algorithm
    //   duplicate    this app already has `keyId` (use updateKey)
    //   too_many     the app has kMaxKeysPerApp keys
    //   not_found    no such key of this app's
    QVariantMap registerKey(const QString& app, const QVariantMap& key);
    // `changes`: { name?, purposes? }. The public key and algorithm stay; to
    // change them, remove the key and register a new one.
    QVariantMap updateKey(const QString& app, const QString& keyId, const QVariantMap& changes);
    QVariantMap removeKey(const QString& app, const QString& keyId);
    // { keyId, name, publicKey, algorithm, purposes, added }: no label, no note.
    QVariantList myKeys(const QString& app) const;

    // ── For the keychain role (the caller checks the role) ──────────────
    // Every entry, label and note included, grouped by owner in registration
    // order (ownerName as shownName gives it): { ownerApp, ownerName, keyId, name, label, note, publicKey,
    // fingerprint, algorithm, purposes, added }.
    QVariantList allKeys() const;
    // "" clears. False for an unknown key or text over the limit (nothing is
    // cut short: the user's words are kept whole or not at all).
    bool setKeyLabel(const QString& ownerApp, const QString& keyId, const QString& label);
    bool setKeyNote(const QString& ownerApp, const QString& keyId, const QString& note);
    // The entry leaves the directory; the signer app keeps its private key.
    bool removeEntry(const QString& ownerApp, const QString& keyId);

    // ── For the broker ──────────────────────────────────────────────────
    const Key* find(const QString& ownerApp, const QString& keyId) const;
    const QList<Key>& keys() const { return m_keys; }

    // The app was uninstalled: its entries go, label and note with them.
    void removeApp(const QString& app);
    int count(const QString& app = QString()) const;

signals:
    void keysChanged();

private:
    int indexOf(const QString& app, const QString& keyId) const;
    // Entries `app` registered from another source than it now comes from
    // belong to a different package of the same name: they are dropped.
    void forgetOtherSource(const QString& app);
    QString sourceFor(const QString& app) const;
    qint64 now() const;
    void load();
    bool save() const;
    void changed();

    QString m_path;
    QList<Key> m_keys;
    std::function<QString(const QString&)> m_sourceFor;
    std::function<QString(const QString&)> m_nameFor;
    std::function<QStringList(const QString&)> m_takenFor;
    std::function<qint64()> m_now;
};
