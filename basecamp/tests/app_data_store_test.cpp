// srcdeps: AppDataStore.cpp
//
// An app's own data, kept by the host (app/AppDataStore.{h,cpp},
// basecamp-experiments#60): what the Stub Signer keeps its test keys in,
// because QML LocalStorage hands one app's open database to any other app
// asking for its name (tst_qml_sandbox localStorageInTheAppsFolder). Covered:
// app B reads nothing of app A's and cannot overwrite it, on disk and across
// a restart; the file is the owner's alone; names that would point outside
// the folder are refused; the size limit; uninstall removes it.
//
// Run: nix build .#unit-tests -L

#include "AppDataStore.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest/QtTest>

namespace {

const QString kSigner = QStringLiteral("keychain_signer");
const QString kOther = QStringLiteral("other_app");
const QString kSeed = QStringLiteral("SEED-MARKER-9c1e");

QVariantMap seeds() { return { { "keys", QVariantList{ QVariantMap{ { "keyId", "daily" }, { "seed", kSeed } } } } }; }

} // namespace

class AppDataStoreTest : public QObject {
    Q_OBJECT

private slots:
    void anotherAppReadsNothingOfIt()
    {
        QTemporaryDir dir;
        AppDataStore store(dir.path());
        QVERIFY(store.write(kSigner, seeds()).value("ok").toBool());
        QCOMPARE(store.read(kSigner).value("data").toMap(), seeds());

        // App B is the attested caller: there is no argument to name A with.
        const QVariantMap theirs = store.read(kOther);
        QVERIFY(theirs.value("ok").toBool());
        QVERIFY(theirs.value("data").toMap().isEmpty());
        QVERIFY(!QJsonDocument::fromVariant(theirs).toJson().contains(kSeed.toUtf8()));

        // Writing its own leaves A's alone.
        QVERIFY(store.write(kOther, QVariantMap{ { "keys", QVariantList{} } }).value("ok").toBool());
        QCOMPARE(store.read(kSigner).value("data").toMap(), seeds());

        // And after a restart, from disk.
        AppDataStore again(dir.path());
        QCOMPARE(again.read(kSigner).value("data").toMap(), seeds());
        QVERIFY(!QJsonDocument::fromVariant(again.read(kOther)).toJson().contains(kSeed.toUtf8()));
    }

    void namesThatLeaveTheFolderAreRefused_data()
    {
        QTest::addColumn<QString>("app");
        QTest::newRow("empty") << QString();
        QTest::newRow("dot dot") << QStringLiteral("..");
        QTest::newRow("hidden") << QStringLiteral(".keychain_signer");
        QTest::newRow("slash") << QStringLiteral("other/../keychain_signer");
        QTest::newRow("backslash") << QStringLiteral("other\\keychain_signer");
    }
    void namesThatLeaveTheFolderAreRefused()
    {
        QFETCH(QString, app);
        QTemporaryDir dir;
        AppDataStore store(dir.path());
        QVERIFY(store.write(kSigner, seeds()).value("ok").toBool());
        QCOMPARE(store.read(app).value("error").toString(), QStringLiteral("bad_request"));
        QCOMPARE(store.write(app, seeds()).value("error").toString(), QStringLiteral("bad_request"));
        QCOMPARE(store.read(kSigner).value("data").toMap(), seeds());
    }

    void theFileIsTheOwnersAlone()
    {
        QTemporaryDir dir;
        AppDataStore store(dir.path());
        QVERIFY(store.write(kSigner, seeds()).value("ok").toBool());
        const QFileInfo info(dir.filePath(kSigner + QStringLiteral("/appdata.json")));
        QVERIFY(info.exists());
        QCOMPARE(info.permissions() & (QFileDevice::ReadGroup | QFileDevice::ReadOther
                                       | QFileDevice::WriteGroup | QFileDevice::WriteOther),
                 QFileDevice::Permissions());
    }

    void onlyAnObjectWithinTheLimit()
    {
        AppDataStore store;
        QCOMPARE(store.write(kSigner, QVariantList{ 1, 2 }).value("error").toString(), QStringLiteral("bad_request"));
        QCOMPARE(store.write(kSigner, QStringLiteral("text")).value("error").toString(), QStringLiteral("bad_request"));
        const QVariantMap big{ { "blob", QString(AppDataStore::kMaxBytes, 'x') } };
        QCOMPARE(store.write(kSigner, big).value("error").toString(), QStringLiteral("too_big"));
        QVERIFY(store.read(kSigner).value("data").toMap().isEmpty());
        QVERIFY(store.write(kSigner, seeds()).value("ok").toBool());
        QCOMPARE(store.read(kSigner).value("data").toMap(), seeds());
    }

    void uninstallRemovesIt()
    {
        QTemporaryDir dir;
        AppDataStore store(dir.path());
        QVERIFY(store.write(kSigner, seeds()).value("ok").toBool());
        store.removeApp(kSigner);
        QVERIFY(store.read(kSigner).value("data").toMap().isEmpty());
        QVERIFY(!QFile::exists(dir.filePath(kSigner + QStringLiteral("/appdata.json"))));
        AppDataStore again(dir.path());
        QVERIFY(again.read(kSigner).value("data").toMap().isEmpty());
    }
};

QTEST_GUILESS_MAIN(AppDataStoreTest)
#include "app_data_store_test.moc"
