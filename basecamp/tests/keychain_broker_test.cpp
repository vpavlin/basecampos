// srcdeps: Keychain.cpp KeychainBroker.cpp KeychainVerify.cpp
// libdeps: OpenSSL::Crypto
//
// Keychain, part 2: the signing request broker (app/KeychainBroker.{h,cpp},
// basecamp-experiments#56), against fakes: a registry in memory, a fake
// verifier, an injected clock. Covered: every transition and every exit
// (done, rejected, cancelled three ways, timedOut, failed four ways), busy,
// request validation, a requester unable to read or cancel another's request,
// only the key's owner completing it, a bad signature never passed on, nothing
// about unchosen keys (or the user's label and note) in any result or in the
// signer's envelope, and the payload dropped when the request ends. And the
// #60 review fixes: a borrowed display name shown as the app's id, purpose
// equal to kind, a key swapped under a request failing, the user's label on
// the shell's sheet only, a requester held off after cancels and over five a
// minute, only the app in front asking, a queued request's full 5 minutes.
//
// The real verifier (OpenSSL EVP Ed25519) is checked against RFC 8032's test
// vectors, and once through the broker end to end.
//
// Run: nix build .#unit-tests -L

#include "Keychain.h"
#include "KeychainBroker.h"
#include "KeychainVerify.h"

#include <QJsonDocument>
#include <QSignalSpy>
#include <QtTest/QtTest>

namespace {

const QString kRequester = QStringLiteral("stub_requester");
const QString kSigner = QStringLiteral("stub_signer");
const QString kOtherSigner = QStringLiteral("other_signer");
const QString kStranger = QStringLiteral("stranger");

QByteArray hex(const char* s) { return QByteArray::fromHex(QByteArray(s)); }
QString b64(const QByteArray& raw) { return QString::fromLatin1(raw.toBase64()); }

// RFC 8032 §7.1, TEST 1 (empty message) and TEST 2 (one byte, 0x72).
const QByteArray kRfcPub1 = hex("d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a");
const QByteArray kRfcSig1 = hex("e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b");
const QByteArray kRfcPub2 = hex("3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c");
const QByteArray kRfcMsg2 = hex("72");
const QByteArray kRfcSig2 = hex("92a009a9f0d4cab8720e820b5f642540a2b27b5416503f8fb3762223ebdb69da085ac1e43e15996e458f3613d0f11d8c387b2eaeb4302aeeb00d291612bb0c00");

// The fake: a signature is "sig:" + the public key + the message.
QByteArray fakeSign(const QByteArray& pub, const QByteArray& msg) { return "sig:" + pub + msg; }

const QByteArray kPayload = QByteArrayLiteral("PAYLOAD-MARKER-7f3a sign in to Weekend hike");

QVariantMap keyRow(const QString& id, char fill, const QVariantList& purposes = {},
                   const QString& algorithm = QStringLiteral("ed25519"))
{
    QByteArray pub(algorithm == QStringLiteral("ed25519") ? 32 : 33, fill);
    if (algorithm != QStringLiteral("ed25519")) pub[0] = 0x02;
    return { { "keyId", id }, { "name", id + QStringLiteral(" key") }, { "publicKey", b64(pub) },
             { "algorithm", algorithm }, { "purposes", purposes } };
}

QVariantMap ask(const QString& kind = QStringLiteral("login"), const QByteArray& payload = kPayload)
{
    return { { "payload", b64(payload) }, { "kind", kind },
             { "description", QStringLiteral("Sign in to Weekend hike") } };
}

} // namespace

class KeychainBrokerTest : public QObject {
    Q_OBJECT

    Keychain* m_keys = nullptr;
    KeychainBroker* m_broker = nullptr;
    qint64 m_now = 0;
    bool m_keychainApp = true;

    // Past the pause a cancelled or timed-out request leaves its requester in.
    void later() { m_now += KeychainBroker::kCooldownMs; }

    QString open(const QString& requester = kRequester, const QVariantMap& params = ask())
    {
        const QVariantMap r = m_broker->requestSignature(requester, params);
        return r.value("requestId").toString();
    }

private slots:
    void init()
    {
        m_now = 1'000'000;
        m_keychainApp = true;
        m_keys = new Keychain(QString(), this);
        m_keys->setNameResolver([](const QString& app) {
            return app == kSigner ? QStringLiteral("Stub Signer") : QString();
        });
        QVERIFY(m_keys->registerKey(kSigner, keyRow("daily", 'd', { "message", "login" })).value("ok").toBool());
        QVERIFY(m_keys->registerKey(kSigner, keyRow("cold", 'c', { "transaction" })).value("ok").toBool());
        QVERIFY(m_keys->registerKey(kOtherSigner, keyRow("any", 'a')).value("ok").toBool());
        QVERIFY(m_keys->registerKey(kOtherSigner, keyRow("evm", 'e', {}, "secp256k1")).value("ok").toBool());
        QVERIFY(m_keys->setKeyLabel(kSigner, "daily", QStringLiteral("LABEL-MARKER")));
        QVERIFY(m_keys->setKeyNote(kSigner, "daily", QStringLiteral("NOTE-MARKER")));
        QVERIFY(m_keys->setKeyLabel(kOtherSigner, "any", QStringLiteral("OTHER-LABEL")));

        m_broker = new KeychainBroker(m_keys, this);
        m_broker->setClock([this] { return m_now; });
        m_broker->setKeychainAvailable([this] { return m_keychainApp; });
        m_broker->setNameResolver([](const QString& app) {
            return app == kRequester ? QStringLiteral("Stub Requester") : QString();
        });
        m_broker->setVerifier([](const QString&, const QByteArray& pub, const QByteArray& msg,
                                 const QByteArray& sig) { return sig == fakeSign(pub, msg); });
    }
    void cleanup()
    {
        delete m_broker;
        delete m_keys;
    }

    void happyPathGoesThroughEveryState()
    {
        QSignalSpy opened(m_broker, &KeychainBroker::requestOpened);
        QSignalSpy toSigner(m_broker, &KeychainBroker::signerRequested);
        QSignalSpy results(m_broker, &KeychainBroker::signatureResult);
        QSignalSpy finished(m_broker, &KeychainBroker::requestFinished);
        QSignalSpy ended(m_broker, &KeychainBroker::requestEnded);

        const QString id = open();
        QVERIFY(!id.isEmpty());
        QCOMPARE(opened.count(), 1);
        QCOMPARE(m_broker->stateFor(kRequester, id), QStringLiteral("pending"));

        const QVariantMap view = m_broker->requestForKeychain(id);
        QCOMPARE(view.value("requesterApp").toString(), kRequester);
        QCOMPARE(view.value("requesterName").toString(), QStringLiteral("Stub Requester"));
        QCOMPARE(view.value("description").toString(), QStringLiteral("Sign in to Weekend hike"));
        QCOMPARE(view.value("kind").toString(), QStringLiteral("login"));

        QVERIFY(m_broker->chooseKey(id, kSigner, "daily"));
        QCOMPARE(m_broker->stateFor(kRequester, id), QStringLiteral("keyChosen"));
        QCOMPARE(toSigner.count(), 1);
        QCOMPARE(toSigner.first().at(0).toString(), kSigner);
        const QVariantMap env = toSigner.first().at(1).toMap();
        QCOMPARE(env.value("requestId").toString(), id);
        QCOMPARE(env.value("keyId").toString(), QStringLiteral("daily"));
        QCOMPARE(env.value("payload").toString(), b64(kPayload));
        QCOMPARE(env.value("kind").toString(), QStringLiteral("login"));
        QCOMPARE(env.value("requesterName").toString(), QStringLiteral("Stub Requester"));
        // The attested id too, so a signer can tell two apps of one name apart.
        QCOMPARE(env.value("requesterApp").toString(), kRequester);
        QCOMPARE(env.value("purpose").toString(), QStringLiteral("login"));
        QCOMPARE(env.value("description").toString(), QStringLiteral("Sign in to Weekend hike"));
        // The user's words never reach the signer.
        const QByteArray envDump = QJsonDocument::fromVariant(env).toJson();
        QVERIFY(!envDump.contains("LABEL-MARKER"));
        QVERIFY(!envDump.contains("NOTE-MARKER"));

        m_broker->signerReached(id);
        QCOMPARE(m_broker->stateFor(kRequester, id), QStringLiteral("signing"));

        const QByteArray sig = fakeSign(QByteArray(32, 'd'), kPayload);
        QCOMPARE(m_broker->completeSignature(kSigner, id, b64(sig)), QStringLiteral("done"));
        QCOMPARE(results.count(), 1);
        QCOMPARE(results.first().at(0).toString(), kRequester);
        const QVariantMap r = results.first().at(1).toMap();
        QCOMPARE(r.value("requestId").toString(), id);
        QCOMPARE(r.value("status").toString(), QStringLiteral("done"));
        QCOMPARE(r.value("signature").toString(), b64(sig));
        QCOMPARE(r.value("publicKey").toString(), b64(QByteArray(32, 'd')));
        QCOMPARE(r.value("keyId").toString(), QStringLiteral("daily"));
        QCOMPARE(r.value("signerApp").toString(), kSigner);
        QCOMPARE(r.value("keyName").toString(), QStringLiteral("daily key"));
        QVERIFY(!r.contains("error"));

        const QVariantMap s = finished.first().at(1).toMap();
        QCOMPARE(s.value("status").toString(), QStringLiteral("done"));
        QCOMPARE(s.value("signerName").toString(), QStringLiteral("Stub Signer"));
        QCOMPARE(s.value("keyName").toString(), QStringLiteral("daily key"));

        // The signer answered itself: no "ended" notice. The request is gone.
        QCOMPARE(ended.count(), 0);
        QCOMPARE(m_broker->openCount(), 0);
        QCOMPARE(m_broker->stateFor(kRequester, id), QString());
    }

    void signerMayCompleteBeforeItsAcknowledgement()
    {
        const QString id = open();
        QVERIFY(m_broker->chooseKey(id, kSigner, "daily"));
        QCOMPARE(m_broker->completeSignature(kSigner, id, b64(fakeSign(QByteArray(32, 'd'), kPayload))),
                 QStringLiteral("done"));
    }

    void rejectedBySigner()
    {
        QSignalSpy results(m_broker, &KeychainBroker::signatureResult);
        QSignalSpy finished(m_broker, &KeychainBroker::requestFinished);
        QSignalSpy ended(m_broker, &KeychainBroker::requestEnded);
        const QString id = open();
        QVERIFY(m_broker->chooseKey(id, kSigner, "daily"));
        m_broker->signerReached(id);
        QVERIFY(!m_broker->rejectSignature(kOtherSigner, id, "not mine"));
        QVERIFY(m_broker->rejectSignature(kSigner, id, QStringLiteral("You said no")));
        const QVariantMap r = results.first().at(1).toMap();
        QCOMPARE(r.value("status").toString(), QStringLiteral("rejected"));
        QVERIFY(!r.contains("signature"));
        QVERIFY(!r.contains("publicKey"));
        QVERIFY(!r.contains("keyId"));
        // The signer's words go to the shell's sheet, not to the requester.
        QVERIFY(!QJsonDocument::fromVariant(r).toJson().contains("You said no"));
        QCOMPARE(finished.first().at(1).toMap().value("reason").toString(), QStringLiteral("You said no"));
        QCOMPARE(ended.count(), 0);
    }

    void cancelledByUserBeforeAndAfterChoosing()
    {
        QSignalSpy results(m_broker, &KeychainBroker::signatureResult);
        QSignalSpy ended(m_broker, &KeychainBroker::requestEnded);
        QString id = open();
        QVERIFY(m_broker->cancelByUser(id));
        QCOMPARE(results.last().at(1).toMap().value("status").toString(), QStringLiteral("cancelled"));
        QCOMPARE(results.last().at(1).toMap().value("error").toString(), QStringLiteral("user_cancelled"));
        QCOMPARE(ended.count(), 0);   // no signer had it

        later();
        id = open();
        QVERIFY(m_broker->chooseKey(id, kSigner, "daily"));
        m_broker->signerReached(id);
        QVERIFY(m_broker->cancelByUser(id));
        QCOMPARE(ended.count(), 1);   // the signer had it: told
        QCOMPARE(ended.first().at(0).toString(), kSigner);
        QCOMPARE(ended.first().at(1).toString(), id);
        QCOMPARE(ended.first().at(2).toString(), QStringLiteral("cancelled"));
        QVERIFY(!m_broker->cancelByUser(id));   // once
    }

    void keychainClosedCancelsOnlyWhatWaitsForAKey()
    {
        // basecamp-experiments#58: closing the Keychain app is "the user closed
        // Keychain". A request already with its signer is the signer's to end.
        QSignalSpy results(m_broker, &KeychainBroker::signatureResult);
        QSignalSpy ended(m_broker, &KeychainBroker::requestEnded);
        const QString waiting = open();
        const QString withSigner = open(kStranger);
        QVERIFY(m_broker->chooseKey(withSigner, kSigner, "daily"));
        m_broker->signerReached(withSigner);

        QCOMPARE(m_broker->keychainClosed(), 1);
        QCOMPARE(results.count(), 1);
        QCOMPARE(results.last().at(0).toString(), kRequester);
        QCOMPARE(results.last().at(1).toMap().value("status").toString(), QStringLiteral("cancelled"));
        QCOMPARE(results.last().at(1).toMap().value("error").toString(), QStringLiteral("keychain_closed"));
        QCOMPARE(ended.count(), 0);
        QCOMPARE(m_broker->stateFor(kRequester, waiting), QString());
        QCOMPARE(m_broker->stateFor(kStranger, withSigner), QStringLiteral("signing"));
        QCOMPARE(m_broker->keychainClosed(), 0);
    }

    void keychainSeesWhoSignsOnceChosen()
    {
        const QString id = open();
        QVariantMap view = m_broker->requestForKeychain(id);
        QVERIFY(!view.contains("signerApp"));
        QVERIFY(!view.contains("signerName"));
        QVERIFY(m_broker->chooseKey(id, kSigner, "daily"));
        view = m_broker->requestForKeychain(id);
        QCOMPARE(view.value("state").toString(), QStringLiteral("keyChosen"));
        QCOMPARE(view.value("signerApp").toString(), kSigner);
        QCOMPARE(view.value("signerName").toString(), QStringLiteral("Stub Signer"));
        QCOMPARE(view.value("chosenKeyId").toString(), QStringLiteral("daily"));
    }

    void cancelledByRequester()
    {
        QSignalSpy results(m_broker, &KeychainBroker::signatureResult);
        const QString id = open();
        QVERIFY(!m_broker->cancelByRequester(kStranger, id));
        QVERIFY(!m_broker->cancelByRequester(kStranger));
        QCOMPARE(m_broker->openCount(), 1);
        QVERIFY(m_broker->cancelByRequester(kRequester));   // its one open request
        QCOMPARE(results.last().at(1).toMap().value("status").toString(), QStringLiteral("cancelled"));
        QCOMPARE(results.last().at(1).toMap().value("error").toString(), QStringLiteral("requester_cancelled"));
    }

    void requesterClosedCancelsAndTellsTheSigner()
    {
        QSignalSpy results(m_broker, &KeychainBroker::signatureResult);
        QSignalSpy ended(m_broker, &KeychainBroker::requestEnded);
        const QString id = open();
        QVERIFY(m_broker->chooseKey(id, kSigner, "daily"));
        m_broker->signerReached(id);
        QCOMPARE(m_broker->appsInvolved(), (QStringList{ kRequester, kSigner }));
        m_broker->appClosed(kRequester);
        QCOMPARE(results.last().at(1).toMap().value("status").toString(), QStringLiteral("cancelled"));
        QCOMPARE(results.last().at(1).toMap().value("error").toString(), QStringLiteral("requester_closed"));
        QCOMPARE(ended.count(), 1);
        QCOMPARE(ended.first().at(0).toString(), kSigner);
        QCOMPARE(m_broker->openCount(), 0);
    }

    void signerClosedFails()
    {
        QSignalSpy results(m_broker, &KeychainBroker::signatureResult);
        const QString id = open();
        QVERIFY(m_broker->chooseKey(id, kSigner, "daily"));
        // Not yet holding it: the signer starting up is not the signer closing.
        m_broker->appClosed(kSigner);
        QCOMPARE(m_broker->openCount(), 1);
        m_broker->signerReached(id);
        m_broker->appClosed(kSigner);
        QCOMPARE(results.last().at(1).toMap().value("status").toString(), QStringLiteral("failed"));
        QCOMPARE(results.last().at(1).toMap().value("error").toString(), QStringLiteral("signer_closed"));
    }

    void signerUnreachableFails()
    {
        QSignalSpy results(m_broker, &KeychainBroker::signatureResult);
        QString id = open();
        m_broker->signerUnreachable(id);   // nothing chosen yet: ignored
        QCOMPARE(m_broker->openCount(), 1);
        QVERIFY(m_broker->chooseKey(id, kSigner, "daily"));
        m_broker->signerUnreachable(id);
        QCOMPARE(results.last().at(1).toMap().value("status").toString(), QStringLiteral("failed"));
        QCOMPARE(results.last().at(1).toMap().value("error").toString(), QStringLiteral("signer_unavailable"));
    }

    void timesOutAfterFiveMinutesInAnyOpenState()
    {
        QSignalSpy results(m_broker, &KeychainBroker::signatureResult);
        QSignalSpy ended(m_broker, &KeychainBroker::requestEnded);
        const QString pending = open();
        m_now += KeychainBroker::kTimeoutMs - 1;
        m_broker->sweep();
        QCOMPARE(m_broker->openCount(), 1);
        m_now += 1;
        m_broker->sweep();
        QCOMPARE(m_broker->openCount(), 0);
        QCOMPARE(results.last().at(1).toMap().value("requestId").toString(), pending);
        QCOMPARE(results.last().at(1).toMap().value("status").toString(), QStringLiteral("timedOut"));

        // Choosing a key does not restart the clock: five minutes in all.
        later();
        const QString signing = open();
        m_now += 60 * 1000;
        QVERIFY(m_broker->chooseKey(signing, kSigner, "daily"));
        m_broker->signerReached(signing);
        m_now += KeychainBroker::kTimeoutMs - 60 * 1000;
        m_broker->sweep();
        QCOMPARE(results.last().at(1).toMap().value("status").toString(), QStringLiteral("timedOut"));
        QCOMPARE(ended.count(), 1);
        QCOMPARE(ended.first().at(2).toString(), QStringLiteral("timedOut"));
        // A late answer changes nothing.
        QCOMPARE(m_broker->completeSignature(kSigner, signing, b64(fakeSign(QByteArray(32, 'd'), kPayload))),
                 QString());
        QCOMPARE(results.count(), 2);
    }

    void badSignatureFailsAndIsNotPassedOn()
    {
        QSignalSpy results(m_broker, &KeychainBroker::signatureResult);
        QSignalSpy ended(m_broker, &KeychainBroker::requestEnded);
        const QString id = open();
        QVERIFY(m_broker->chooseKey(id, kSigner, "daily"));
        m_broker->signerReached(id);
        // Signed with the other key's public half: does not verify.
        const QByteArray wrong = fakeSign(QByteArray(32, 'c'), kPayload);
        QCOMPARE(m_broker->completeSignature(kSigner, id, b64(wrong)), QStringLiteral("failed"));
        const QVariantMap r = results.first().at(1).toMap();
        QCOMPARE(r.value("status").toString(), QStringLiteral("failed"));
        QCOMPARE(r.value("error").toString(), QStringLiteral("bad_signature"));
        QVERIFY(!r.contains("signature"));
        QVERIFY(!r.contains("publicKey"));
        QVERIFY(!QJsonDocument::fromVariant(r).toJson().contains(wrong.toBase64()));
        QCOMPARE(ended.count(), 0);   // it answered itself
    }

    void malformedSignatureFails()
    {
        QSignalSpy results(m_broker, &KeychainBroker::signatureResult);
        const QString id = open();
        QVERIFY(m_broker->chooseKey(id, kSigner, "daily"));
        QCOMPARE(m_broker->completeSignature(kSigner, id, QStringLiteral("***not base64***")),
                 QStringLiteral("failed"));
        QCOMPARE(results.first().at(1).toMap().value("error").toString(), QStringLiteral("bad_signature"));
    }

    void noVerifierMeansNothingVerifies()
    {
        m_broker->setVerifier({});
        QSignalSpy results(m_broker, &KeychainBroker::signatureResult);
        const QString id = open();
        QVERIFY(m_broker->chooseKey(id, kSigner, "daily"));
        QCOMPARE(m_broker->completeSignature(kSigner, id, b64(fakeSign(QByteArray(32, 'd'), kPayload))),
                 QStringLiteral("failed"));
    }

    void keyRemovedMidRequestFails()
    {
        QSignalSpy results(m_broker, &KeychainBroker::signatureResult);
        const QString id = open();
        QVERIFY(m_broker->chooseKey(id, kSigner, "daily"));
        m_broker->signerReached(id);
        QVERIFY(m_keys->removeEntry(kSigner, "daily"));
        QCOMPARE(m_broker->completeSignature(kSigner, id, b64(fakeSign(QByteArray(32, 'd'), kPayload))),
                 QStringLiteral("failed"));
        QCOMPARE(results.first().at(1).toMap().value("error").toString(), QStringLiteral("key_removed"));
        QVERIFY(!results.first().at(1).toMap().contains("signature"));
    }

    void onlyTheKeysOwnerCompletes()
    {
        QSignalSpy results(m_broker, &KeychainBroker::signatureResult);
        const QString id = open();
        // Before a key is chosen nobody may answer.
        QCOMPARE(m_broker->completeSignature(kSigner, id, b64(fakeSign(QByteArray(32, 'd'), kPayload))), QString());
        QVERIFY(m_broker->chooseKey(id, kSigner, "daily"));
        m_broker->signerReached(id);
        // Another signer, even with a signature that would verify, is refused
        // and the request stays open for the real one.
        QCOMPARE(m_broker->completeSignature(kOtherSigner, id, b64(fakeSign(QByteArray(32, 'd'), kPayload))), QString());
        QCOMPARE(m_broker->completeSignature(kRequester, id, b64(fakeSign(QByteArray(32, 'd'), kPayload))), QString());
        QVERIFY(!m_broker->rejectSignature(kRequester, id));
        QCOMPARE(results.count(), 0);
        QCOMPARE(m_broker->stateFor(kRequester, id), QStringLiteral("signing"));
        QCOMPARE(m_broker->completeSignature(kSigner, QStringLiteral("no-such-id"), QStringLiteral("AAAA")), QString());
    }

    void busyWhileOneIsOpen()
    {
        const QString id = open();
        QVERIFY(!id.isEmpty());
        QCOMPARE(m_broker->requestSignature(kRequester, ask()).value("error").toString(), QStringLiteral("busy"));
        QCOMPARE(m_broker->openCount(), 1);
        // Another app is not affected.
        QVERIFY(!open(kStranger).isEmpty());
        // Once it ends, and the pause after a cancel is over, the app may
        // ask again.
        QVERIFY(m_broker->cancelByUser(id));
        later();
        QVERIFY(!open().isEmpty());
    }

    void refusesBadRequests_data()
    {
        QTest::addColumn<QVariantMap>("params");
        QVariantMap noPayload = ask(); noPayload.remove("payload");
        QVariantMap emptyPayload = ask("login", QByteArray());
        QVariantMap notB64 = ask(); notB64["payload"] = QStringLiteral("***");
        QVariantMap tooBig = ask("login", QByteArray(KeychainBroker::kMaxPayloadBytes + 1, 'x'));
        QVariantMap payloadNotString = ask(); payloadNotString["payload"] = 12;
        QVariantMap badKind = ask(); badKind["kind"] = QStringLiteral("anything");
        QVariantMap noDescription = ask(); noDescription.remove("description");
        QVariantMap longDescription = ask(); longDescription["description"] = QString(KeychainBroker::kMaxDescription + 1, 'd');
        QVariantMap twoLines = ask(); twoLines["description"] = QStringLiteral("Sign in\nand pay");
        QVariantMap secp = ask(); secp["algorithm"] = QStringLiteral("secp256k1");
        QVariantMap badPurpose = ask(); badPurpose["purpose"] = QStringLiteral("everything");
        QTest::newRow("no payload") << noPayload;
        QTest::newRow("empty payload") << emptyPayload;
        QTest::newRow("payload not base64") << notB64;
        QTest::newRow("payload over 32 KB") << tooBig;
        QTest::newRow("payload not a string") << payloadNotString;
        QTest::newRow("unknown kind") << badKind;
        QTest::newRow("no description") << noDescription;
        QTest::newRow("long description") << longDescription;
        QTest::newRow("two-line description") << twoLines;
        QTest::newRow("unverifiable algorithm") << secp;
        QTest::newRow("unknown purpose") << badPurpose;
    }
    void refusesBadRequests()
    {
        QFETCH(QVariantMap, params);
        QCOMPARE(m_broker->requestSignature(kRequester, params).value("error").toString(), QStringLiteral("bad_request"));
        QCOMPARE(m_broker->openCount(), 0);
        QCOMPARE(m_broker->requestSignature(QString(), ask()).value("error").toString(), QStringLiteral("bad_request"));
    }

    void noKeychainAppIsUnavailable()
    {
        m_keychainApp = false;
        QCOMPARE(m_broker->requestSignature(kRequester, ask()).value("error").toString(), QStringLiteral("unavailable"));
        QCOMPARE(m_broker->openCount(), 0);
    }

    void aRequesterCannotReadAnotherRequest()
    {
        const QString mine = open();
        const QString theirs = open(kStranger);
        QCOMPARE(m_broker->stateFor(kRequester, mine), QStringLiteral("pending"));
        QCOMPARE(m_broker->stateFor(kRequester, theirs), QString());
        QCOMPARE(m_broker->stateFor(kStranger, mine), QString());
        QCOMPARE(m_broker->stateFor(kRequester, QStringLiteral("made-up")), QString());
        QVERIFY(!m_broker->cancelByRequester(kRequester, theirs));
        QCOMPARE(m_broker->openCount(), 2);
        // Ids are not guessable counters.
        QVERIFY(mine != theirs);
        QVERIFY(mine.size() >= 32);
    }

    void candidatesFollowAlgorithmAndPurpose()
    {
        // login: daily (message, login) and any (no purposes). Not cold
        // (transaction only), not evm (secp256k1).
        QString id = open(kRequester, ask("login"));
        QVariantList keys = m_broker->requestForKeychain(id).value("keys").toList();
        QStringList ids;
        for (const QVariant& k : keys) ids << k.toMap().value("keyId").toString();
        QCOMPARE(ids, (QStringList{ "daily", "any" }));
        // The Keychain itself sees the user's label and note.
        QCOMPARE(keys.first().toMap().value("label").toString(), QStringLiteral("LABEL-MARKER"));
        QVERIFY(!m_broker->chooseKey(id, kSigner, "cold"));
        QVERIFY(!m_broker->chooseKey(id, kOtherSigner, "evm"));
        QVERIFY(!m_broker->chooseKey(id, kSigner, "missing"));
        QCOMPARE(m_broker->stateFor(kRequester, id), QStringLiteral("pending"));
        QVERIFY(m_broker->cancelByUser(id));

        later();
        id = open(kRequester, ask("transaction"));
        ids.clear();
        for (const QVariant& k : m_broker->requestForKeychain(id).value("keys").toList())
            ids << k.toMap().value("keyId").toString();
        QCOMPARE(ids, (QStringList{ "cold", "any" }));
        // A key once chosen cannot be swapped.
        QVERIFY(m_broker->chooseKey(id, kSigner, "cold"));
        QVERIFY(!m_broker->chooseKey(id, kOtherSigner, "any"));
    }

    void lastUsedKeyIsRemembered()
    {
        QString id = open();
        QVERIFY(m_broker->requestForKeychain(id).value("lastKey").toMap().isEmpty());
        QVERIFY(m_broker->chooseKey(id, kOtherSigner, "any"));
        QCOMPARE(m_broker->completeSignature(kOtherSigner, id, b64(fakeSign(QByteArray(32, 'a'), kPayload))),
                 QStringLiteral("done"));
        id = open();
        const QVariantMap last = m_broker->requestForKeychain(id).value("lastKey").toMap();
        QCOMPARE(last.value("ownerApp").toString(), kOtherSigner);
        QCOMPARE(last.value("keyId").toString(), QStringLiteral("any"));
        // Per requester.
        QVERIFY(m_broker->requestForKeychain(open(kStranger)).value("lastKey").toMap().isEmpty());
    }

    void nothingAboutUnchosenKeysInAnyResult()
    {
        QSignalSpy results(m_broker, &KeychainBroker::signatureResult);
        QSignalSpy toSigner(m_broker, &KeychainBroker::signerRequested);
        // One of each ending.
        auto drive = [&](int how) {
            later();
            const QString id = open();
            QVERIFY(m_broker->chooseKey(id, kSigner, "daily"));
            m_broker->signerReached(id);
            switch (how) {
            case 0: m_broker->completeSignature(kSigner, id, b64(fakeSign(QByteArray(32, 'd'), kPayload))); break;
            case 1: m_broker->rejectSignature(kSigner, id); break;
            case 2: m_broker->cancelByUser(id); break;
            case 3: m_broker->completeSignature(kSigner, id, QStringLiteral("AAAA")); break;
            case 4: m_now += KeychainBroker::kTimeoutMs; m_broker->sweep(); break;
            }
        };
        for (int how = 0; how < 5; ++how) drive(how);
        QCOMPARE(results.count(), 5);

        const QByteArray unchosen[] = {
            QByteArray(32, 'c').toBase64(), QByteArray(32, 'a').toBase64(),
            "cold", "any", "evm", kOtherSigner.toUtf8(),
            "LABEL-MARKER", "NOTE-MARKER", "OTHER-LABEL",
        };
        for (const QList<QVariant>& args : results) {
            const QByteArray dump = QJsonDocument::fromVariant(args.at(1)).toJson();
            for (const QByteArray& marker : unchosen)
                QVERIFY2(!dump.contains(marker), marker.constData());
            // Only the documented fields.
            for (const QString& field : args.at(1).toMap().keys())
                QVERIFY2((QStringList{ "requestId", "status", "error", "signature", "publicKey", "keyId", "keyName", "signerApp" })
                             .contains(field), qPrintable(field));
        }
        for (const QList<QVariant>& args : toSigner) {
            const QByteArray dump = QJsonDocument::fromVariant(args.at(1)).toJson();
            for (const QByteArray& marker : unchosen)
                QVERIFY2(!dump.contains(marker), marker.constData());
        }
    }

    void payloadIsDroppedWhenTheRequestEnds()
    {
        const QString id = open();
        QVERIFY(m_broker->cancelByUser(id));
        later();
        QCOMPARE(m_broker->openCount(), 0);
        QVERIFY(m_broker->requestForKeychain(id).isEmpty());
        // Nothing left that could be handed out again.
        QSignalSpy toSigner(m_broker, &KeychainBroker::signerRequested);
        QVERIFY(!m_broker->chooseKey(id, kSigner, "daily"));
        QCOMPARE(toSigner.count(), 0);
        // And the Keychain's view never carries it.
        const QString next = open();
        QVERIFY(!QJsonDocument::fromVariant(m_broker->requestForKeychain(next)).toJson()
                     .contains(QByteArray(kPayload.toBase64())));
    }

    void openRequestsOldestFirst()
    {
        const QString a = open(kRequester);
        m_now += 10;
        const QString b = open(kStranger);
        QCOMPARE(m_broker->openRequests(), (QStringList{ a, b }));
    }

    // ── Review fixes (basecamp-experiments#60) ──────────────────────────

    void aBorrowedNameIsShownAsTheAppItself()
    {
        // A requester whose display name is "Keychain" (or another app's) is
        // still shown as itself: its module id, everywhere the user looks.
        QString claimed = QStringLiteral("Keychain");
        m_broker->setNameResolver([&claimed](const QString& app) {
            return app == kStranger ? claimed : QStringLiteral("Stub Requester");
        });
        QSignalSpy toSigner(m_broker, &KeychainBroker::signerRequested);
        QSignalSpy finished(m_broker, &KeychainBroker::requestFinished);

        QString id = open(kStranger);
        QVariantMap view = m_broker->requestForKeychain(id);
        QCOMPARE(view.value("requesterName").toString(), kStranger);
        QCOMPARE(view.value("requesterApp").toString(), kStranger);
        QVERIFY(m_broker->chooseKey(id, kSigner, "daily"));
        QCOMPARE(toSigner.last().at(1).toMap().value("requesterName").toString(), kStranger);
        QCOMPARE(toSigner.last().at(1).toMap().value("requesterApp").toString(), kStranger);
        QVERIFY(m_broker->cancelByUser(id));
        QCOMPARE(finished.last().at(1).toMap().value("requesterName").toString(), kStranger);
        QCOMPARE(finished.last().at(1).toMap().value("requesterApp").toString(), kStranger);

        // Spelled differently, still Keychain's; "Basecamp" too.
        for (const QString& name : { QStringLiteral("key-CHAIN "), QStringLiteral("Basecamp") }) {
            claimed = name;
            later();
            id = open(kStranger);
            QCOMPARE(m_broker->requestForKeychain(id).value("requesterName").toString(), kStranger);
            QVERIFY(m_broker->cancelByUser(id));
        }
        // Another installed app's name.
        m_keys->setTakenNames([](const QString& app) {
            return app == kStranger ? QStringList{ QStringLiteral("Stub Requester") } : QStringList();
        });
        claimed = QStringLiteral("Stub Requester");
        later();
        id = open(kStranger);
        QCOMPARE(m_broker->requestForKeychain(id).value("requesterName").toString(), kStranger);
        // An own name nobody else has is shown as it is.
        QCOMPARE(m_broker->requestForKeychain(open()).value("requesterName").toString(),
                 QStringLiteral("Stub Requester"));
    }

    void aSignerWithAnotherSignersNameIsShownAsItself()
    {
        m_keys->setNameResolver([](const QString&) { return QStringLiteral("Stub Signer"); });
        m_keys->setTakenNames([](const QString& app) {
            return app == kOtherSigner ? QStringList{ QStringLiteral("Stub Signer") } : QStringList();
        });
        QVERIFY(m_keys->updateKey(kOtherSigner, "any", { { "name", "Daily key" } }).value("ok").toBool());
        for (const QVariant& v : m_keys->allKeys()) {
            const QVariantMap k = v.toMap();
            QCOMPARE(k.value("ownerName").toString(),
                     k.value("ownerApp").toString() == kSigner ? QStringLiteral("Stub Signer") : kOtherSigner);
        }
        const QString id = open();
        QVERIFY(m_broker->chooseKey(id, kOtherSigner, "any"));
        QCOMPARE(m_broker->requestForKeychain(id).value("signerName").toString(), kOtherSigner);
        QCOMPARE(Keychain::shownName("x", "Keychain", {}), QStringLiteral("x"));
        QCOMPARE(Keychain::shownName("x", "  ", {}), QStringLiteral("x"));
        QCOMPARE(Keychain::shownName("x", "Wallet", { "Weekend" }), QStringLiteral("Wallet"));
    }

    void thePurposeIsTheKind()
    {
        // A key kept for transactions must never sign what both screens call
        // a login, and the other way round.
        QVariantMap loginAsTx = ask("login"); loginAsTx["purpose"] = QStringLiteral("transaction");
        QVariantMap txAsLogin = ask("transaction"); txAsLogin["purpose"] = QStringLiteral("login");
        QCOMPARE(m_broker->requestSignature(kRequester, loginAsTx).value("error").toString(), QStringLiteral("bad_request"));
        QCOMPARE(m_broker->requestSignature(kRequester, txAsLogin).value("error").toString(), QStringLiteral("bad_request"));
        QCOMPARE(m_broker->openCount(), 0);
        QVariantMap same = ask("transaction"); same["purpose"] = QStringLiteral("transaction");
        const QString id = m_broker->requestSignature(kRequester, same).value("requestId").toString();
        QVERIFY(!id.isEmpty());
        QCOMPARE(m_broker->requestForKeychain(id).value("purpose").toString(), QStringLiteral("transaction"));
    }

    void aKeySwappedUnderTheRequestFails()
    {
        // The user chose "daily" with public key D. The signer removes it and
        // registers "daily" again with public key X, then signs with X: the
        // key the user chose never signed, so nothing is passed on.
        QSignalSpy results(m_broker, &KeychainBroker::signatureResult);
        const QString id = open();
        QVERIFY(m_broker->chooseKey(id, kSigner, "daily"));
        m_broker->signerReached(id);
        QVERIFY(m_keys->removeKey(kSigner, "daily").value("ok").toBool());
        QVERIFY(m_keys->registerKey(kSigner, keyRow("daily", 'x', { "login" })).value("ok").toBool());
        const QByteArray byX = fakeSign(QByteArray(32, 'x'), kPayload);
        QCOMPARE(m_broker->completeSignature(kSigner, id, b64(byX)), QStringLiteral("failed"));
        const QVariantMap r = results.first().at(1).toMap();
        QCOMPARE(r.value("error").toString(), QStringLiteral("key_changed"));
        QVERIFY(!r.contains("signature"));
        QVERIFY(!r.contains("publicKey"));
        QVERIFY(!QJsonDocument::fromVariant(r).toJson().contains(byX.toBase64()));
    }

    void theResultCarriesTheKeyAsChosen()
    {
        // Renamed by its app while it signs: the requester hears the name and
        // key the user chose by.
        QSignalSpy results(m_broker, &KeychainBroker::signatureResult);
        const QString id = open();
        QVERIFY(m_broker->chooseKey(id, kSigner, "daily"));
        QVERIFY(m_keys->updateKey(kSigner, "daily", { { "name", "Renamed" } }).value("ok").toBool());
        QCOMPARE(m_broker->completeSignature(kSigner, id, b64(fakeSign(QByteArray(32, 'd'), kPayload))),
                 QStringLiteral("done"));
        QCOMPARE(results.first().at(1).toMap().value("keyName").toString(), QStringLiteral("daily key"));
        QCOMPARE(results.first().at(1).toMap().value("publicKey").toString(), b64(QByteArray(32, 'd')));
    }

    void theSheetNamesTheKeyByTheUsersLabel()
    {
        // The shell's Signed sheet says what the user chose ("LABEL-MARKER"),
        // the requester's result never does.
        QSignalSpy results(m_broker, &KeychainBroker::signatureResult);
        QSignalSpy finished(m_broker, &KeychainBroker::requestFinished);
        const QString id = open();
        QVERIFY(m_broker->chooseKey(id, kSigner, "daily"));
        QCOMPARE(m_broker->completeSignature(kSigner, id, b64(fakeSign(QByteArray(32, 'd'), kPayload))),
                 QStringLiteral("done"));
        const QVariantMap s = finished.first().at(1).toMap();
        QCOMPARE(s.value("keyLabel").toString(), QStringLiteral("LABEL-MARKER"));
        QCOMPARE(s.value("keyName").toString(), QStringLiteral("daily key"));
        QVERIFY(!QJsonDocument::fromVariant(results.first().at(1)).toJson().contains("LABEL-MARKER"));
        // A key without a label: none.
        QVERIFY(m_keys->setKeyLabel(kSigner, "daily", QString()));
        const QString next = open();
        QVERIFY(m_broker->chooseKey(next, kSigner, "daily"));
        QVERIFY(m_broker->cancelByUser(next));
        QVERIFY(!finished.last().at(1).toMap().contains("keyLabel"));
    }

    void aCancelledRequesterIsHeldOff()
    {
        // Cancel and ask again, in a loop, would keep pulling the Keychain
        // over what the user is doing.
        QSignalSpy opened(m_broker, &KeychainBroker::requestOpened);
        QVERIFY(m_broker->cancelByRequester(kRequester, open()));
        QCOMPARE(m_broker->requestSignature(kRequester, ask()).value("error").toString(), QStringLiteral("busy"));
        m_now += KeychainBroker::kCooldownMs - 1;
        QCOMPARE(m_broker->requestSignature(kRequester, ask()).value("error").toString(), QStringLiteral("busy"));
        m_now += 1;
        QString id = open();
        QVERIFY(!id.isEmpty());
        // The user's cancel, and a timeout, the same.
        QVERIFY(m_broker->cancelByUser(id));
        QCOMPARE(m_broker->requestSignature(kRequester, ask()).value("error").toString(), QStringLiteral("busy"));
        later();
        id = open();
        m_now += KeychainBroker::kTimeoutMs;
        m_broker->sweep();
        QCOMPARE(m_broker->requestSignature(kRequester, ask()).value("error").toString(), QStringLiteral("busy"));
        // Another app is not held off by it.
        QVERIFY(!open(kStranger).isEmpty());
        QCOMPARE(opened.count(), 4);
    }

    void atMostFiveAMinute()
    {
        // Even ending each one some other way (here: the signer says no).
        QSignalSpy opened(m_broker, &KeychainBroker::requestOpened);
        for (int i = 0; i < KeychainBroker::kMaxPerMinute; ++i) {
            const QString id = open();
            QVERIFY2(!id.isEmpty(), qPrintable(QString::number(i)));
            QVERIFY(m_broker->chooseKey(id, kSigner, "daily"));
            QVERIFY(m_broker->rejectSignature(kSigner, id));
            m_now += 1000;
        }
        QCOMPARE(m_broker->requestSignature(kRequester, ask()).value("error").toString(), QStringLiteral("busy"));
        QCOMPARE(opened.count(), KeychainBroker::kMaxPerMinute);
        m_now += 60 * 1000;
        QVERIFY(!open().isEmpty());
    }

    void onlyTheAppInFrontMayAsk()
    {
        QString front = kRequester;
        m_broker->setInFront([&front](const QString& app) { return app == front; });
        QSignalSpy opened(m_broker, &KeychainBroker::requestOpened);
        QCOMPARE(m_broker->requestSignature(kStranger, ask()).value("error").toString(), QStringLiteral("not_in_front"));
        QCOMPARE(opened.count(), 0);
        QCOMPARE(m_broker->openCount(), 0);
        QVERIFY(!open().isEmpty());
        QCOMPARE(opened.count(), 1);
    }

    void aQueuedRequestGetsItsFullFiveMinutes()
    {
        // B asks while the user is on A's request: its 5 minutes start when
        // it is the one on screen, and both say how many are waiting.
        QSignalSpy results(m_broker, &KeychainBroker::signatureResult);
        QSignalSpy finished(m_broker, &KeychainBroker::requestFinished);
        const QString a = open(kRequester);
        m_now += 1000;
        const QString b = open(kStranger);
        QCOMPARE(m_broker->requestForKeychain(a).value("waiting").toInt(), 1);
        m_now += 4 * 60 * 1000;
        QVERIFY(m_broker->cancelByUser(a));
        QCOMPARE(finished.last().at(1).toMap().value("waiting").toInt(), 1);
        QCOMPARE(m_broker->requestForKeychain(b).value("waiting").toInt(), 0);
        // 8 minutes after B asked, 4 on screen: still open.
        m_now += 4 * 60 * 1000;
        m_broker->sweep();
        QCOMPARE(m_broker->stateFor(kStranger, b), QStringLiteral("pending"));
        m_now += 60 * 1000;
        m_broker->sweep();
        QCOMPARE(m_broker->stateFor(kStranger, b), QString());
        QCOMPARE(results.last().at(1).toMap().value("status").toString(), QStringLiteral("timedOut"));
    }

    void theWaitingViewCanOpenTheSigner()
    {
        const QString id = open();
        QCOMPARE(m_broker->signerFor(id), QString());
        QVERIFY(m_broker->chooseKey(id, kSigner, "daily"));
        QCOMPARE(m_broker->signerFor(id), kSigner);
        m_broker->signerReached(id);
        QCOMPARE(m_broker->signerFor(id), kSigner);
        QVERIFY(m_broker->cancelByUser(id));
        QCOMPARE(m_broker->signerFor(id), QString());
        QCOMPARE(m_broker->signerFor(QStringLiteral("made-up")), QString());
    }

    // ── The real verifier ───────────────────────────────────────────────

    void opensslVerifiesRfc8032Vectors()
    {
        QVERIFY(KeychainVerify::verify("ed25519", kRfcPub1, QByteArray(), kRfcSig1));
        QVERIFY(KeychainVerify::verify("ed25519", kRfcPub2, kRfcMsg2, kRfcSig2));
        // A flipped bit anywhere fails.
        QByteArray sig = kRfcSig1; sig[10] = char(sig.at(10) ^ 0x01);
        QVERIFY(!KeychainVerify::verify("ed25519", kRfcPub1, QByteArray(), sig));
        QVERIFY(!KeychainVerify::verify("ed25519", kRfcPub1, QByteArray("x"), kRfcSig1));
        QVERIFY(!KeychainVerify::verify("ed25519", kRfcPub2, kRfcMsg2, kRfcSig1));
        // Wrong lengths and other algorithms are false, not a crash.
        QVERIFY(!KeychainVerify::verify("ed25519", kRfcPub1.left(31), QByteArray(), kRfcSig1));
        QVERIFY(!KeychainVerify::verify("ed25519", kRfcPub1, QByteArray(), kRfcSig1.left(63)));
        QVERIFY(!KeychainVerify::verify("secp256k1", kRfcPub1, QByteArray(), kRfcSig1));
    }

    void brokerWithTheRealVerifier()
    {
        m_broker->setVerifier(&KeychainVerify::verify);
        QVERIFY(m_keys->registerKey(kSigner, { { "keyId", "rfc" }, { "name", "RFC key" },
                                               { "publicKey", b64(kRfcPub2) }, { "algorithm", "ed25519" } })
                    .value("ok").toBool());
        QSignalSpy results(m_broker, &KeychainBroker::signatureResult);
        QString id = open(kRequester, ask("message", kRfcMsg2));
        QVERIFY(m_broker->chooseKey(id, kSigner, "rfc"));
        QCOMPARE(m_broker->completeSignature(kSigner, id, b64(kRfcSig2)), QStringLiteral("done"));
        QCOMPARE(results.last().at(1).toMap().value("signature").toString(), b64(kRfcSig2));
        QCOMPARE(results.last().at(1).toMap().value("publicKey").toString(), b64(kRfcPub2));

        // TEST 1's signature over TEST 2's message: failed, not passed on.
        id = open(kRequester, ask("message", kRfcMsg2));
        QVERIFY(m_broker->chooseKey(id, kSigner, "rfc"));
        QCOMPARE(m_broker->completeSignature(kSigner, id, b64(kRfcSig1)), QStringLiteral("failed"));
        QVERIFY(!results.last().at(1).toMap().contains("signature"));
    }
};

QTEST_GUILESS_MAIN(KeychainBrokerTest)
#include "keychain_broker_test.moc"
