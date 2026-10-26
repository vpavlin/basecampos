#pragma once

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <functional>

class Keychain;
class QTimer;

// Keychain, part 2: the signing request broker (basecamp-experiments#56).
//
// The state machine between a requesting app, the Keychain app (whoever holds
// the `keychain` role) and the signer app that owns the key the user picks:
//
//   pending ──chooseKey──▶ keyChosen ──signerReached──▶ signing ──complete──▶ done
//      │                      │                            │
//      └──────────────────────┴────────────────────────────┴──▶ rejected   the signer said no
//                                                              cancelled  Keychain closed, requester cancelled or closed
//                                                              timedOut   5 min without a decision
//                                                              failed     no signer, signer error, bad signature
//
// It decides; it carries nothing. Requests, answers and the hand-offs travel
// as shell intents through the IntentBroker (MainUIBackend wires them): the
// requester's `basecamp.keychain.sign` stays open until signatureResult
// answers it, and the signer hears `keychain.sign` from the shell alone.
// Every app name here is the caller the host attested, never a payload field.
//
// The rules it keeps:
//  - one open request per requesting app (a second one is `busy`);
//  - every signature is checked against the registered public key before it
//    goes anywhere, and one that does not verify ends as `failed` and is
//    never passed on;
//  - the requester hears about the one key the user chose, and only on `done`;
//  - only the app owning the chosen key can complete or reject the request;
//  - the payload lives in memory only and is dropped when the request ends.
//    Nothing here logs it, or the description;
//  - the key the user chose is the key that must sign: its public key is
//    taken when it is chosen, and a registry entry swapped under the request
//    ends it as failed (key_changed);
//  - a request comes from the app in front, and an app that keeps asking is
//    held off (busy): a pause after one it or the user cancelled or let time
//    out, and at most kMaxPerMinute a minute, so nothing can keep pulling
//    the Keychain over what the user is doing;
//  - the 5 minutes run from when a request is the one on screen (the oldest
//    open one), so one queued behind another still gets them all.
//
// QtCore only (the verifier is injected), so keychain_broker_test.cpp runs it
// against fakes.
class KeychainBroker : public QObject {
    Q_OBJECT
public:
    enum class State { Pending, KeyChosen, Signing, Done, Rejected, Cancelled, TimedOut, Failed };
    static QString stateName(State s);

    static constexpr int kTimeoutMs = 5 * 60 * 1000;
    static constexpr int kMaxPayloadBytes = 32 * 1024;
    static constexpr int kMaxDescription = 120;
    static constexpr int kMaxReason = 120;
    // After an ending the requester or the user caused, or a timeout.
    static constexpr int kCooldownMs = 10 * 1000;
    static constexpr int kMaxPerMinute = 5;

    using Verifier = std::function<bool(const QString& algorithm, const QByteArray& publicKey,
                                        const QByteArray& message, const QByteArray& signature)>;

    explicit KeychainBroker(Keychain* keys, QObject* parent = nullptr);

    void setVerifier(Verifier v) { m_verify = std::move(v); }
    // ms; tests move time instead of waiting five minutes.
    void setClock(std::function<qint64()> clock) { m_now = std::move(clock); }
    // An app's display name, for the Keychain's "who is asking".
    void setNameResolver(std::function<QString(const QString&)> fn) { m_nameFor = std::move(fn); }
    // Whether anything holds the keychain role. Nothing can choose a key
    // without it, so a request is refused at once rather than left to time out.
    void setKeychainAvailable(std::function<bool()> fn) { m_hasKeychain = std::move(fn); }
    // Whether `app` is the one the user is looking at. A request from any
    // other is refused (not_in_front): it would bring the Keychain over
    // whatever the user is doing. Unset, every app counts as in front.
    void setInFront(std::function<bool(const QString&)> fn) { m_inFront = std::move(fn); }

    // ── Requester (`requester` is the attested caller) ──────────────────
    // `params`: { payload (base64), kind, description, algorithm?, purpose? }.
    // The kind is the purpose: a `purpose` other than `kind` is bad_request,
    // or a key kept for one purpose could sign what both screens call another.
    // { requestId } or { error }: bad_request | busy | unavailable | not_in_front.
    QVariantMap requestSignature(const QString& requester, const QVariantMap& params);
    // The requester's own open request (`requestId` "" = whichever it has).
    bool cancelByRequester(const QString& requester, const QString& requestId = QString());
    // The state of one of `requester`'s own open requests; "" for anything
    // else, so another app's id tells it nothing, not even that it exists.
    QString stateFor(const QString& requester, const QString& requestId) const;

    // ── Keychain role (the caller checks the role) ──────────────────────
    QStringList openRequests() const;
    // What the Keychain shows: { requestId, state, requesterApp, requesterName,
    // description, kind, algorithm, purpose, keys: [ … ], lastKey, waiting },
    // keys the registry's allKeys() rows that can sign this request, waiting
    // the number of other open requests; once a key is chosen also
    // { signerApp, signerName, chosenKeyId }. {} when unknown. requesterName
    // is Keychain::shownName's: the module id when the app's display name is
    // somebody else's.
    QVariantMap requestForKeychain(const QString& requestId) const;
    // pending → keyChosen; then signerRequested asks the shell to hand it on.
    bool chooseKey(const QString& requestId, const QString& ownerApp, const QString& keyId);
    // The user closed Keychain or pressed Cancel.
    bool cancelByUser(const QString& requestId);
    // The Keychain app was closed: every request still waiting for a key ends
    // as cancelled (keychain_closed). One already with its signer goes on.
    int keychainClosed();
    // The app holding the chosen key of an open request that is with its
    // signer, for "Open <signer>"; "" otherwise.
    QString signerFor(const QString& requestId) const;

    // ── The shell, about the hand-off ───────────────────────────────────
    void signerReached(const QString& requestId);     // keyChosen → signing
    void signerUnreachable(const QString& requestId); // → failed

    // ── Signer (`signer` is the attested caller) ────────────────────────
    // The resulting status ("done", or "failed" for a signature that does not
    // verify), "" when refused: not this app's request, or not open.
    QString completeSignature(const QString& signer, const QString& requestId,
                              const QString& signatureBase64);
    bool rejectSignature(const QString& signer, const QString& requestId,
                         const QString& reason = QString());

    // ── Lifetimes ───────────────────────────────────────────────────────
    // The app is no longer running: as requester its request is cancelled, as
    // the signer holding one it fails.
    void appClosed(const QString& app);
    // The requesters and signers of open requests, for the shell to watch.
    QStringList appsInvolved() const;
    // Time out what is past kTimeoutMs. Runs on a timer; public for tests.
    void sweep();
    int openCount() const { return m_requests.size(); }

signals:
    // A request needs the Keychain app: bring the role holder forward.
    void requestOpened(const QString& requestId);
    // Something the Keychain draws changed (opened, chosen, ended).
    void requestsChanged();
    // Hand to `signerApp` (its keychain.sign intent): { requestId, keyId,
    // payload, kind, purpose, requesterApp, requesterName, description }.
    // Not the label, not the note.
    void signerRequested(const QString& signerApp, const QVariantMap& envelope);
    // Ended while `signerApp` had it, by something other than its own answer.
    void requestEnded(const QString& signerApp, const QString& requestId, const QString& status);
    // The answer for the requester: { requestId, status, error? } and, on
    // done only, { signature, publicKey, keyId, keyName, signerApp } (keyName
    // the signer app's own name for the key, never the user's label).
    void signatureResult(const QString& requester, const QVariantMap& result);
    // The same ending as the shell shows it (the Signed / Not signed sheet):
    // { requestId, status, requesterApp, requesterName, error?, reason?,
    // signerApp?, signerName?, keyName?, keyLabel?, waiting }. keyLabel is the user's label the key was chosen
    // by: only here, for the shell's own sheet, never in signatureResult.
    // waiting is how many other requests are still open.
    void requestFinished(const QString& requester, const QVariantMap& summary);

private:
    struct Request {
        QString id;
        QString requester;
        QString requesterName;
        QString kind;
        QString description;
        QString algorithm;
        QString purpose;
        QByteArray payload;
        State state = State::Pending;
        qint64 created = 0;
        // When it became the one on screen (the oldest open); its 5 minutes
        // run from here. 0 while it waits behind another.
        qint64 started = 0;
        QString ownerApp;   // the chosen key
        QString keyId;
        // The chosen key as it was when chosen: what must sign.
        QString keyAlgorithm;
        QByteArray keyPublic;
        QString keyName;
        QString keyLabel;
    };

    bool matches(const Request& r, const QString& ownerApp, const QString& keyId) const;
    QString findOpen(const QString& requester) const;
    // Ends `id` in `state`: answers the requester, tells the signer if it
    // had the request, drops the payload.
    void end(const QString& id, State state, const QString& error,
             const QString& reason = QString(), const QByteArray& signature = QByteArray());
    qint64 now() const;
    void armTimer();
    // The oldest open request is on screen: start its clock if not yet.
    void startHead();
    // Held off: in its pause, or over kMaxPerMinute.
    bool throttled(const QString& requester) const;

    Keychain* m_keys;
    Verifier m_verify;
    std::function<qint64()> m_now;
    std::function<QString(const QString&)> m_nameFor;
    std::function<bool()> m_hasKeychain;
    std::function<bool(const QString&)> m_inFront;
    QHash<QString, Request> m_requests;              // open requests only
    QHash<QString, QPair<QString, QString>> m_lastKey; // requester -> (owner, keyId)
    QHash<QString, qint64> m_pauseUntil;               // requester -> ms
    QHash<QString, QList<qint64>> m_recent;            // requester -> starts in the last minute
    QTimer* m_timer = nullptr;
};
