// srcdeps: Keychain.cpp
//
// Keychain, part 1: the key registry (app/Keychain.{h,cpp},
// basecamp-experiments#55). A directory of public keys apps register; the
// private halves never come near it. Covered here: register / update /
// remove and their refusals (missing fields, a public key of the wrong length,
// a duplicate keyId, the 32-key limit); one app never touching another's
// entries; the user's label and note (set only through the role-side calls,
// kept across updateKey, absent from myKeys, length limits); the persistence
// round trip through a second Keychain over the same file, as a restart
// would; and uninstall removing an app's entries with its label and note.
//
// Run: nix build .#unit-tests -L

#include "Keychain.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest/QtTest>

namespace {

const QString kSigner = QStringLiteral("stub_signer");
const QString kOther = QStringLiteral("other_signer");

QString b64(const QByteArray& raw) { return QString::fromLatin1(raw.toBase64()); }

QString edKey(char fill = 'a') { return b64(QByteArray(32, fill)); }

QVariantMap key(const QString& id, const QString& pub = edKey(),
                const QString& algorithm = QStringLiteral("ed25519"))
{
    return {
        { QStringLiteral("keyId"), id },
        { QStringLiteral("name"), QStringLiteral("Daily key") },
        { QStringLiteral("publicKey"), pub },
        { QStringLiteral("algorithm"), algorithm },
        { QStringLiteral("purposes"), QVariantList{ QStringLiteral("message"), QStringLiteral("login") } },
    };
}

QString err(const QVariantMap& r) { return r.value(QStringLiteral("error")).toString(); }
bool ok(const QVariantMap& r) { return r.value(QStringLiteral("ok")).toBool(); }

} // namespace

class KeychainTest : public QObject {
    Q_OBJECT
private slots:
    void registersAndLists()
    {
        Keychain kc;
        kc.setClock([] { return qint64(1234); });
        kc.setNameResolver([](const QString&) { return QStringLiteral("Stub Signer"); });
        QSignalSpy changed(&kc, &Keychain::keysChanged);
        QVERIFY(ok(kc.registerKey(kSigner, key(QStringLiteral("daily")))));
        QCOMPARE(changed.count(), 1);

        const QVariantList mine = kc.myKeys(kSigner);
        QCOMPARE(mine.size(), 1);
        const QVariantMap k = mine.first().toMap();
        QCOMPARE(k.value("keyId").toString(), QStringLiteral("daily"));
        QCOMPARE(k.value("name").toString(), QStringLiteral("Daily key"));
        QCOMPARE(k.value("publicKey").toString(), edKey());
        QCOMPARE(k.value("algorithm").toString(), QStringLiteral("ed25519"));
        QCOMPARE(k.value("purposes").toList().size(), 2);
        QCOMPARE(k.value("added").toLongLong(), qint64(1234));

        const QVariantMap row = kc.allKeys().first().toMap();
        QCOMPARE(row.value("ownerApp").toString(), kSigner);
        QCOMPARE(row.value("ownerName").toString(), QStringLiteral("Stub Signer"));
        QCOMPARE(row.value("fingerprint").toString(), edKey().left(6) + QStringLiteral("…") + edKey().right(6));
    }

    void refusesBadFields_data()
    {
        QTest::addColumn<QVariantMap>("in");
        QTest::addColumn<QString>("error");
        QVariantMap noId = key(QString()); noId.remove("keyId");
        QVariantMap noName = key("a"); noName.remove("name");
        QVariantMap blankName = key("a"); blankName["name"] = QStringLiteral("   ");
        QVariantMap longName = key("a"); longName["name"] = QString(Keychain::kMaxName + 1, 'n');
        QVariantMap newline = key("a"); newline["name"] = QStringLiteral("two\nlines");
        QVariantMap noKey = key("a"); noKey.remove("publicKey");
        QVariantMap noAlg = key("a"); noAlg.remove("algorithm");
        QVariantMap badAlg = key("a"); badAlg["algorithm"] = QStringLiteral("rsa");
        QVariantMap badPurpose = key("a"); badPurpose["purposes"] = QVariantList{ QStringLiteral("everything") };
        QVariantMap purposeNotList = key("a"); purposeNotList["purposes"] = QStringLiteral("message");
        QVariantMap badId = key(QStringLiteral("has space"));
        QVariantMap longId = key(QString(Keychain::kMaxKeyId + 1, 'i'));
        QVariantMap idNotString = key("a"); idNotString["keyId"] = 7;
        QTest::newRow("no keyId") << noId << "bad_request";
        QTest::newRow("no name") << noName << "bad_request";
        QTest::newRow("blank name") << blankName << "bad_request";
        QTest::newRow("long name") << longName << "bad_request";
        QTest::newRow("name with a newline") << newline << "bad_request";
        QTest::newRow("no algorithm") << noAlg << "bad_request";
        QTest::newRow("unknown algorithm") << badAlg << "bad_request";
        QTest::newRow("unknown purpose") << badPurpose << "bad_request";
        QTest::newRow("purposes not a list") << purposeNotList << "bad_request";
        QTest::newRow("keyId with a space") << badId << "bad_request";
        QTest::newRow("keyId too long") << longId << "bad_request";
        QTest::newRow("keyId not a string") << idNotString << "bad_request";
        QTest::newRow("no public key") << noKey << "bad_key";
        QTest::newRow("not base64") << key("a", QStringLiteral("not*base64!")) << "bad_key";
        QTest::newRow("ed25519 31 bytes") << key("a", b64(QByteArray(31, 'x'))) << "bad_key";
        QTest::newRow("ed25519 33 bytes") << key("a", b64(QByteArray(33, 'x'))) << "bad_key";
        QTest::newRow("secp256k1 32 bytes") << key("a", b64(QByteArray(32, 'x')), "secp256k1") << "bad_key";
        QTest::newRow("secp256k1 33 bad prefix") << key("a", b64(QByteArray(33, 0x05)), "secp256k1") << "bad_key";
        QTest::newRow("secp256k1 65 bad prefix") << key("a", b64(QByteArray(65, 0x02)), "secp256k1") << "bad_key";
    }
    void refusesBadFields()
    {
        QFETCH(QVariantMap, in);
        QFETCH(QString, error);
        Keychain kc;
        const QVariantMap r = kc.registerKey(kSigner, in);
        QVERIFY(!ok(r));
        QCOMPARE(err(r), error);
        QCOMPARE(kc.count(), 0);
    }

    void acceptsSecp256k1Lengths()
    {
        Keychain kc;
        QByteArray compressed(33, 'x'); compressed[0] = 0x02;
        QByteArray uncompressed(65, 'y'); uncompressed[0] = 0x04;
        QVERIFY(ok(kc.registerKey(kSigner, key("c", b64(compressed), "secp256k1"))));
        QVERIFY(ok(kc.registerKey(kSigner, key("u", b64(uncompressed), "secp256k1"))));
        QCOMPARE(kc.count(kSigner), 2);
    }

    void duplicateKeyIdIsRefusedPerApp()
    {
        Keychain kc;
        QVERIFY(ok(kc.registerKey(kSigner, key("daily"))));
        QCOMPARE(err(kc.registerKey(kSigner, key("daily", edKey('b')))), QStringLiteral("duplicate"));
        // The same keyId is another app's own business.
        QVERIFY(ok(kc.registerKey(kOther, key("daily"))));
        QCOMPARE(kc.count(), 2);
    }

    void thirtyTwoKeysPerApp()
    {
        Keychain kc;
        for (int i = 0; i < Keychain::kMaxKeysPerApp; ++i)
            QVERIFY(ok(kc.registerKey(kSigner, key(QStringLiteral("k%1").arg(i)))));
        QCOMPARE(err(kc.registerKey(kSigner, key("one-more"))), QStringLiteral("too_many"));
        QCOMPARE(kc.count(kSigner), Keychain::kMaxKeysPerApp);
        // Another app has its own 32.
        QVERIFY(ok(kc.registerKey(kOther, key("k0"))));
        // Removing one frees a place.
        QVERIFY(ok(kc.removeKey(kSigner, "k0")));
        QVERIFY(ok(kc.registerKey(kSigner, key("one-more"))));
    }

    void updateChangesNameAndPurposesOnly()
    {
        Keychain kc;
        QVERIFY(ok(kc.registerKey(kSigner, key("daily"))));
        QVERIFY(ok(kc.updateKey(kSigner, "daily", { { "name", QStringLiteral("Renamed") },
                                                    { "purposes", QVariantList{ QStringLiteral("transaction") } } })));
        QVariantMap k = kc.myKeys(kSigner).first().toMap();
        QCOMPARE(k.value("name").toString(), QStringLiteral("Renamed"));
        QCOMPARE(k.value("purposes").toList(), QVariantList{ QStringLiteral("transaction") });
        // Purposes can be cleared (sign anything).
        QVERIFY(ok(kc.updateKey(kSigner, "daily", { { "purposes", QVariantList{} } })));
        QVERIFY(kc.myKeys(kSigner).first().toMap().value("purposes").toList().isEmpty());

        // Not the key itself, not the user's fields, not a bad name.
        QCOMPARE(err(kc.updateKey(kSigner, "daily", { { "publicKey", edKey('z') } })), QStringLiteral("bad_request"));
        QCOMPARE(err(kc.updateKey(kSigner, "daily", { { "algorithm", "secp256k1" } })), QStringLiteral("bad_request"));
        QCOMPARE(err(kc.updateKey(kSigner, "daily", { { "label", "mine" } })), QStringLiteral("bad_request"));
        QCOMPARE(err(kc.updateKey(kSigner, "daily", { { "note", "mine" } })), QStringLiteral("bad_request"));
        QCOMPARE(err(kc.updateKey(kSigner, "daily", { { "name", "" } })), QStringLiteral("bad_request"));
        QCOMPARE(err(kc.updateKey(kSigner, "nope", { { "name", "x" } })), QStringLiteral("not_found"));
        QCOMPARE(kc.myKeys(kSigner).first().toMap().value("publicKey").toString(), edKey());
    }

    void removeTakesTheEntry()
    {
        Keychain kc;
        QVERIFY(ok(kc.registerKey(kSigner, key("daily"))));
        QVERIFY(ok(kc.removeKey(kSigner, "daily")));
        QCOMPARE(kc.count(), 0);
        QCOMPARE(err(kc.removeKey(kSigner, "daily")), QStringLiteral("not_found"));
    }

    void anAppCannotTouchAnotherAppsKey()
    {
        Keychain kc;
        QVERIFY(ok(kc.registerKey(kSigner, key("daily"))));
        QCOMPARE(err(kc.updateKey(kOther, "daily", { { "name", "Hijacked" } })), QStringLiteral("not_found"));
        QCOMPARE(err(kc.removeKey(kOther, "daily")), QStringLiteral("not_found"));
        QVERIFY(kc.myKeys(kOther).isEmpty());
        QCOMPARE(kc.myKeys(kSigner).first().toMap().value("name").toString(), QStringLiteral("Daily key"));
        // An empty caller (the host could not attest one) owns nothing.
        QVERIFY(!ok(kc.registerKey(QString(), key("x"))));
        QCOMPARE(err(kc.removeKey(QString(), "daily")), QStringLiteral("not_found"));
        // Registering the same keyId never reaches into another app's entry.
        QVERIFY(ok(kc.registerKey(kOther, key("daily", edKey('q')))));
        QCOMPARE(kc.find(kSigner, "daily")->publicKey, QByteArray(32, 'a'));
    }

    void anotherSourceOfTheSameNameIsAnotherApp()
    {
        Keychain kc;
        QString source = QStringLiteral("https://repo-a");
        kc.setSourceResolver([&](const QString&) { return source; });
        QVERIFY(ok(kc.registerKey(kSigner, key("daily"))));
        QCOMPARE(kc.myKeys(kSigner).size(), 1);
        // A package of the same name from another catalog source does not
        // inherit the entry: not listed, not removable, and dropped once it
        // acts.
        source = QStringLiteral("https://repo-b");
        QVERIFY(kc.myKeys(kSigner).isEmpty());
        QCOMPARE(err(kc.updateKey(kSigner, "daily", { { "name", "x" } })), QStringLiteral("not_found"));
        QCOMPARE(kc.count(), 0);
    }

    void labelAndNoteBelongToTheUser()
    {
        Keychain kc;
        QVERIFY(ok(kc.registerKey(kSigner, key("daily"))));
        QVERIFY(kc.setKeyLabel(kSigner, "daily", QStringLiteral("  Hiking club  ")));
        QVERIFY(kc.setKeyNote(kSigner, "daily", QStringLiteral("Used for the club's sign-in.\nNot for money.")));

        QVariantMap row = kc.allKeys().first().toMap();
        QCOMPARE(row.value("label").toString(), QStringLiteral("Hiking club"));
        QCOMPARE(row.value("note").toString(), QStringLiteral("Used for the club's sign-in.\nNot for money."));

        // The owning app never reads them.
        const QVariantMap mine = kc.myKeys(kSigner).first().toMap();
        QVERIFY(!mine.contains("label"));
        QVERIFY(!mine.contains("note"));
        const QByteArray dump = QJsonDocument::fromVariant(kc.myKeys(kSigner)).toJson();
        QVERIFY(!dump.contains("Hiking"));
        QVERIFY(!dump.contains("Not for money"));

        // updateKey keeps them.
        QVERIFY(ok(kc.updateKey(kSigner, "daily", { { "name", QStringLiteral("Renamed") } })));
        row = kc.allKeys().first().toMap();
        QCOMPARE(row.value("label").toString(), QStringLiteral("Hiking club"));
        QCOMPARE(row.value("note").toString(), QStringLiteral("Used for the club's sign-in.\nNot for money."));

        // Cleared with "".
        QVERIFY(kc.setKeyLabel(kSigner, "daily", QString()));
        QVERIFY(kc.setKeyNote(kSigner, "daily", QString()));
        row = kc.allKeys().first().toMap();
        QVERIFY(row.value("label").toString().isEmpty());
        QVERIFY(row.value("note").toString().isEmpty());
    }

    void labelAndNoteLimits()
    {
        Keychain kc;
        QVERIFY(ok(kc.registerKey(kSigner, key("daily"))));
        QVERIFY(kc.setKeyLabel(kSigner, "daily", QString(Keychain::kMaxLabel, 'l')));
        QVERIFY(!kc.setKeyLabel(kSigner, "daily", QString(Keychain::kMaxLabel + 1, 'l')));
        QVERIFY(!kc.setKeyLabel(kSigner, "daily", QStringLiteral("two\nlines")));
        QVERIFY(kc.setKeyNote(kSigner, "daily", QString(Keychain::kMaxNote, 'n')));
        QVERIFY(!kc.setKeyNote(kSigner, "daily", QString(Keychain::kMaxNote + 1, 'n')));
        QVERIFY(!kc.setKeyNote(kSigner, "daily", QStringLiteral("bell\a")));
        // A refusal keeps the old text whole.
        const QVariantMap row = kc.allKeys().first().toMap();
        QCOMPARE(row.value("label").toString(), QString(Keychain::kMaxLabel, 'l'));
        QCOMPARE(row.value("note").toString(), QString(Keychain::kMaxNote, 'n'));
        // An unknown key.
        QVERIFY(!kc.setKeyLabel(kSigner, "nope", "x"));
        QVERIFY(!kc.setKeyNote(kOther, "daily", "x"));
    }

    void persistsAcrossRestart()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("keychain.json");
        {
            Keychain kc(path);
            kc.setClock([] { return qint64(42); });
            QVERIFY(ok(kc.registerKey(kSigner, key("daily"))));
            QByteArray secp(33, 'x'); secp[0] = 0x03;
            QVERIFY(ok(kc.registerKey(kOther, key("cold", b64(secp), "secp256k1"))));
            QVERIFY(kc.setKeyLabel(kSigner, "daily", QStringLiteral("Club")));
            QVERIFY(kc.setKeyNote(kSigner, "daily", QStringLiteral("A note")));
        }
        QVERIFY(QFile::exists(path));
        Keychain again(path);
        QCOMPARE(again.count(), 2);
        const Keychain::Key* k = again.find(kSigner, "daily");
        QVERIFY(k);
        QCOMPARE(k->publicKey, QByteArray(32, 'a'));
        QCOMPARE(k->label, QStringLiteral("Club"));
        QCOMPARE(k->note, QStringLiteral("A note"));
        QCOMPARE(k->added, qint64(42));
        QCOMPARE(k->purposes, (QStringList{ "message", "login" }));
        QCOMPARE(again.find(kOther, "cold")->algorithm, QStringLiteral("secp256k1"));
        // And a removal is written too.
        QVERIFY(ok(again.removeKey(kSigner, "daily")));
        QCOMPARE(Keychain(path).count(), 1);
    }

    void damagedEntriesAreSkippedOnLoad()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("keychain.json");
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(R"({"version":1,"keys":[
            {"ownerApp":"a","keyId":"good","name":"Good","algorithm":"ed25519","publicKey":")"
                + QByteArray(32, 'a').toBase64() + R"("},
            {"ownerApp":"a","keyId":"short","name":"Short","algorithm":"ed25519","publicKey":"AAAA"},
            {"ownerApp":"","keyId":"anon","name":"Anon","algorithm":"ed25519","publicKey":")"
                + QByteArray(32, 'a').toBase64() + R"("}
        ]})");
        f.close();
        Keychain kc(path);
        QCOMPARE(kc.count(), 1);
        QVERIFY(kc.find("a", "good"));
        // An unreadable file is an empty keychain, not a crash.
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("{ not json");
        f.close();
        QCOMPARE(Keychain(path).count(), 0);
    }

    void writtenAtomically()
    {
        // QSaveFile leaves no temp file beside it and replaces the file whole.
        QTemporaryDir dir;
        const QString path = dir.filePath("keychain.json");
        Keychain kc(path);
        QVERIFY(ok(kc.registerKey(kSigner, key("daily"))));
        QCOMPARE(QDir(dir.path()).entryList(QDir::Files), QStringList{ "keychain.json" });
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
        QCOMPARE(root.value("version").toInt(), 1);
        QCOMPARE(root.value("keys").toArray().size(), 1);
    }

    void uninstallRemovesEntries()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("keychain.json");
        Keychain kc(path);
        QVERIFY(ok(kc.registerKey(kSigner, key("daily"))));
        QVERIFY(ok(kc.registerKey(kSigner, key("cold", edKey('c')))));
        QVERIFY(ok(kc.registerKey(kOther, key("daily"))));
        QVERIFY(kc.setKeyLabel(kSigner, "daily", "Club"));
        QSignalSpy changed(&kc, &Keychain::keysChanged);
        kc.removeApp(kSigner);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(kc.count(kSigner), 0);
        QCOMPARE(kc.count(kOther), 1);
        QCOMPARE(Keychain(path).count(kSigner), 0);
        // Reinstalled, it starts from nothing: no label waiting for it.
        QVERIFY(ok(kc.registerKey(kSigner, key("daily"))));
        QVERIFY(kc.allKeys().last().toMap().value("label").toString().isEmpty());
        // Nothing to remove says nothing.
        kc.removeApp(QStringLiteral("never_installed"));
        QCOMPARE(changed.count(), 2);
    }

    void userCanRemoveAnEntry()
    {
        Keychain kc;
        QVERIFY(ok(kc.registerKey(kSigner, key("daily"))));
        QVERIFY(kc.removeEntry(kSigner, "daily"));
        QVERIFY(!kc.removeEntry(kSigner, "daily"));
        QCOMPARE(kc.count(), 0);
    }
};

QTEST_GUILESS_MAIN(KeychainTest)
#include "keychain_test.moc"
