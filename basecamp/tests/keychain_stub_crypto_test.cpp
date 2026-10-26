// srcdeps: Keychain.cpp KeychainBroker.cpp KeychainVerify.cpp
// libdeps: OpenSSL::Crypto
//
// Keychain, part 5: the Stub Signer's Ed25519 against the host's
// (basecamp-experiments#59). The stub signs in QML with TweetNaCl-js; the
// shell checks every signature with OpenSSL EVP (app/KeychainVerify.cpp)
// before it reaches the requester. This test loads the stub's own crypto file
// (fixtures/keychain/SignerCrypto.js, byte for byte the stub's
// keychain_signer/qml/SignerCrypto.js; `sync.sh --check <bc-apps>` in
// basecamp-experiments/experiments/keychain-stubs compares the two) into a
// QJSEngine and checks:
//   - RFC 8032 §7.1 TEST 1 and TEST 2: the stub derives the same public key
//     and makes the same signature, and OpenSSL verifies it;
//   - a key from the stub's own seed generator, over a UTF-8 payload: OpenSSL
//     verifies the stub's signature, and the stub's "Fail next" signature fails;
//   - the whole way through the broker with the real verifier: a stub key
//     registered, a stub signature completes as done, a spoiled one as failed.
//
// Run: nix build .#unit-tests -L

#include "Keychain.h"
#include "KeychainBroker.h"
#include "KeychainVerify.h"

#include <QFile>
#include <QJSEngine>
#include <QSignalSpy>
#include <QtTest/QtTest>

namespace {

QByteArray hex(const char* s) { return QByteArray::fromHex(QByteArray(s)); }
QString b64(const QByteArray& raw) { return QString::fromLatin1(raw.toBase64()); }
QByteArray unb64(const QString& s) { return QByteArray::fromBase64(s.toLatin1()); }

// RFC 8032 §7.1, TEST 1 (empty message) and TEST 2 (one byte, 0x72).
const QByteArray kSeed1 = hex("9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60");
const QByteArray kPub1 = hex("d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a");
const QByteArray kSig1 = hex("e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b");
const QByteArray kSeed2 = hex("4ccd089b28ff96da9db6c346ec114e0f5b8a319f35aba624da8cf6ed4fb8a6fb");
const QByteArray kPub2 = hex("3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c");
const QByteArray kMsg2 = hex("72");
const QByteArray kSig2 = hex("92a009a9f0d4cab8720e820b5f642540a2b27b5416503f8fb3762223ebdb69da085ac1e43e15996e458f3613d0f11d8c387b2eaeb4302aeeb00d291612bb0c00");

} // namespace

class KeychainStubCryptoTest : public QObject {
    Q_OBJECT

    QJSEngine m_js;

    QString call(const char* fn, const QJSValueList& args)
    {
        const QJSValue r = m_js.globalObject().property(QLatin1String(fn)).call(args);
        if (r.isError()) qWarning() << fn << r.toString();
        return r.toString();
    }
    QString publicKey(const QByteArray& seed) { return call("publicKey", { b64(seed) }); }
    QString sign(const QString& seed, const QByteArray& msg) { return call("sign", { seed, b64(msg) }); }

private slots:
    void initTestCase()
    {
        QFile f(QFINDTESTDATA("fixtures/keychain/SignerCrypto.js"));
        QVERIFY2(f.open(QIODevice::ReadOnly), "fixtures/keychain/SignerCrypto.js missing");
        QString src = QString::fromUtf8(f.readAll());
        // `.pragma library` is QML's, not JavaScript.
        QVERIFY(src.startsWith(QStringLiteral(".pragma library")));
        src.remove(0, src.indexOf(QLatin1Char('\n')));
        const QJSValue r = m_js.evaluate(src, QStringLiteral("SignerCrypto.js"));
        QVERIFY2(!r.isError(), qPrintable(r.toString()));
        QVERIFY(m_js.globalObject().property("nacl").property("sign").isObject());
    }

    void rfc8032Vectors()
    {
        QCOMPARE(unb64(publicKey(kSeed1)), kPub1);
        QCOMPARE(unb64(publicKey(kSeed2)), kPub2);
        const QByteArray sig1 = unb64(sign(b64(kSeed1), QByteArray()));
        const QByteArray sig2 = unb64(sign(b64(kSeed2), kMsg2));
        QCOMPARE(sig1, kSig1);
        QCOMPARE(sig2, kSig2);
        QVERIFY(KeychainVerify::verify("ed25519", kPub1, QByteArray(), sig1));
        QVERIFY(KeychainVerify::verify("ed25519", kPub2, kMsg2, sig2));
    }

    void stubKeysVerifyWithOpenSsl()
    {
        const QString seed = call("newSeed", {});
        QCOMPARE(unb64(seed).size(), 32);
        QVERIFY(call("newSeed", {}) != seed);
        const QByteArray pub = unb64(call("publicKey", { seed }));
        QCOMPARE(pub.size(), 32);

        const QByteArray msg = QStringLiteral("Stub Requester: héllo ✓").toUtf8();
        const QString sig = sign(seed, msg);
        QCOMPARE(unb64(sig).size(), 64);
        QVERIFY(KeychainVerify::verify("ed25519", pub, msg, unb64(sig)));
        QVERIFY(!KeychainVerify::verify("ed25519", pub, msg + "x", unb64(sig)));
        // The stub's own check agrees, both ways.
        QCOMPARE(call("verify", { b64(pub), b64(msg), sig }), QStringLiteral("true"));
        const QString spoiled = call("spoil", { sig });
        QVERIFY(spoiled != sig);
        QVERIFY(!KeychainVerify::verify("ed25519", pub, msg, unb64(spoiled)));
        QCOMPARE(call("verify", { b64(pub), b64(msg), spoiled }), QStringLiteral("false"));
        // The stub's UTF-8 is the same bytes as Qt's.
        const QJSValue bytes = m_js.globalObject().property("toBase64").call(
            { m_js.globalObject().property("utf8").call({ QStringLiteral("Stub Requester: héllo ✓") }) });
        QCOMPARE(unb64(bytes.toString()), msg);
    }

    void throughTheBroker()
    {
        Keychain keys;
        KeychainBroker broker(&keys);
        broker.setVerifier(&KeychainVerify::verify);
        const QString seed = call("newSeed", {});
        QVERIFY(keys.registerKey(QStringLiteral("keychain_signer"),
                                 { { "keyId", "daily" }, { "name", "Daily key" },
                                   { "publicKey", call("publicKey", { seed }) },
                                   { "algorithm", "ed25519" } })
                    .value("ok").toBool());
        QSignalSpy results(&broker, &KeychainBroker::signatureResult);
        const QByteArray payload("Stub Requester login challenge");
        auto run = [&](bool spoil) {
            const QString id = broker.requestSignature(QStringLiteral("keychain_requester"),
                { { "payload", b64(payload) }, { "kind", "login" }, { "description", "Sign in" } })
                .value("requestId").toString();
            if (id.isEmpty() || !broker.chooseKey(id, QStringLiteral("keychain_signer"), QStringLiteral("daily")))
                return QStringLiteral("not opened");
            broker.signerReached(id);
            QString sig = sign(seed, payload);
            if (spoil) sig = call("spoil", { sig });
            return broker.completeSignature(QStringLiteral("keychain_signer"), id, sig);
        };
        QCOMPARE(run(false), QStringLiteral("done"));
        QCOMPARE(results.last().at(1).toMap().value("keyName").toString(), QStringLiteral("Daily key"));
        QCOMPARE(run(true), QStringLiteral("failed"));
        QVERIFY(!results.last().at(1).toMap().contains("signature"));
    }
};

QTEST_GUILESS_MAIN(KeychainStubCryptoTest)
#include "keychain_stub_crypto_test.moc"
