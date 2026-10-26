// srcdeps: HomeSettingsStore.cpp
//
// Home's own settings, kept by the shell (app/HomeSettingsStore.{h,cpp},
// basecamp-experiments#51): its widgets and their layout survive a restart.
// Each check reads back through a SECOND store over the same file, as a
// restart would. The rules that matter: one app never reads another's,
// what is too large is refused and the old settings stay, and saving the
// same settings again reports Unchanged (the shell then tells no view).
//
// Run: nix build .#unit-tests -L

#include "../app/HomeSettingsStore.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class HomeSettingsStoreTest : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;
    QString path() const { return m_dir.filePath(QStringLiteral("sub/home-settings.json")); }

    static QVariantMap layoutSettings(int x)
    {
        const QVariantMap block{{"id", "news"}, {"x", x}, {"y", 0}, {"w", 2}, {"h", 3}};
        const QVariantMap layout{{"columns", 4}, {"blocks", QVariantList{block}}};
        return {{"version", 1}, {"layouts", QVariantMap{{"4", layout}}},
                {"widgets", QVariantList{QVariantMap{{"id", "news"}, {"name", "News"}}}}};
    }

private slots:
    void init()
    {
        QVERIFY(m_dir.isValid());
        QFile::remove(path());
    }

    void keptAcrossRestart()
    {
        HomeSettingsStore store(path());
        QCOMPARE(store.load(QStringLiteral("basecamp_home")), QVariantMap());
        // The folder does not exist yet: the store makes it.
        QCOMPARE(store.save(QStringLiteral("basecamp_home"), layoutSettings(2)),
                 HomeSettingsStore::Result::Saved);
        const QVariantMap back = HomeSettingsStore(path()).load(QStringLiteral("basecamp_home"));
        const QVariantMap block = back.value("layouts").toMap().value("4").toMap()
                                      .value("blocks").toList().value(0).toMap();
        QCOMPARE(block.value("id").toString(), QStringLiteral("news"));
        QCOMPARE(block.value("x").toInt(), 2);
        QCOMPARE(back.value("widgets").toList().size(), 1);
        // A new save replaces the old one.
        QCOMPARE(store.save(QStringLiteral("basecamp_home"), layoutSettings(0)),
                 HomeSettingsStore::Result::Saved);
        QCOMPARE(HomeSettingsStore(path()).load(QStringLiteral("basecamp_home"))
                     .value("layouts").toMap().value("4").toMap()
                     .value("blocks").toList().value(0).toMap().value("x").toInt(), 0);
    }

    void sameAgainIsUnchanged()
    {
        HomeSettingsStore store(path());
        QCOMPARE(store.save(QStringLiteral("basecamp_home"), layoutSettings(1)),
                 HomeSettingsStore::Result::Saved);
        QCOMPARE(HomeSettingsStore(path()).save(QStringLiteral("basecamp_home"), layoutSettings(1)),
                 HomeSettingsStore::Result::Unchanged);
    }

    void eachAppItsOwn()
    {
        HomeSettingsStore store(path());
        QCOMPARE(store.save(QStringLiteral("basecamp_home"), layoutSettings(1)),
                 HomeSettingsStore::Result::Saved);
        QCOMPARE(store.load(QStringLiteral("other_home")), QVariantMap());
        QCOMPARE(store.save(QStringLiteral("other_home"), {{"version", 7}}),
                 HomeSettingsStore::Result::Saved);
        // Saving one app's leaves the other's as it was.
        QCOMPARE(HomeSettingsStore(path()).load(QStringLiteral("basecamp_home"))
                     .value("layouts").toMap().value("4").toMap()
                     .value("blocks").toList().value(0).toMap().value("x").toInt(), 1);
        QCOMPARE(HomeSettingsStore(path()).load(QStringLiteral("other_home")).value("version").toInt(), 7);
        QCOMPARE(store.save(QString(), layoutSettings(1)), HomeSettingsStore::Result::Refused);
        QCOMPARE(store.load(QString()), QVariantMap());
    }

    void tooLargeIsRefused()
    {
        HomeSettingsStore store(path());
        QCOMPARE(store.save(QStringLiteral("basecamp_home"), layoutSettings(3)),
                 HomeSettingsStore::Result::Saved);
        const QVariantMap big{{"version", 1}, {"junk", QString(HomeSettingsStore::kMaxBytes, QChar('x'))}};
        QCOMPARE(store.save(QStringLiteral("basecamp_home"), big), HomeSettingsStore::Result::Refused);
        QCOMPARE(HomeSettingsStore(path()).load(QStringLiteral("basecamp_home"))
                     .value("layouts").toMap().value("4").toMap()
                     .value("blocks").toList().value(0).toMap().value("x").toInt(), 3);
    }

    // A file from before home's widgets were renamed (its list under
    // `modules`): it loads as saved, and home's next save (`widgets` only,
    // BasecampHomeWidgets.js settingsOf) replaces its object whole, so the
    // old key is gone from the file.
    void oldModulesKeyDroppedOnSave()
    {
        QDir().mkpath(QFileInfo(path()).absolutePath());
        QFile f(path());
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(R"({"basecamp_home":{"version":1,"modules":[{"id":"news","name":"News"}],)"
                R"("layouts":{"4":{"columns":4,"blocks":[{"id":"news","x":2,"y":0,"w":2,"h":3}]}}}})");
        f.close();
        HomeSettingsStore store(path());
        const QVariantMap old = store.load(QStringLiteral("basecamp_home"));
        QCOMPARE(old.value("modules").toList().size(), 1);
        QCOMPARE(store.save(QStringLiteral("basecamp_home"), layoutSettings(2)),
                 HomeSettingsStore::Result::Saved);
        const QVariantMap back = HomeSettingsStore(path()).load(QStringLiteral("basecamp_home"));
        QVERIFY(back.contains("widgets"));
        QVERIFY(!back.contains("modules"));
        QCOMPARE(back.value("layouts").toMap().value("4").toMap()
                     .value("blocks").toList().value(0).toMap().value("x").toInt(), 2);
        QFile raw(path());
        QVERIFY(raw.open(QIODevice::ReadOnly));
        QVERIFY(!raw.readAll().contains("\"modules\""));
    }

    void brokenFileReadsAsNothing()
    {
        QDir().mkpath(QFileInfo(path()).absolutePath());
        QFile f(path());
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("{ not json");
        f.close();
        HomeSettingsStore store(path());
        QCOMPARE(store.load(QStringLiteral("basecamp_home")), QVariantMap());
        // Saving over it starts a good file again.
        QCOMPARE(store.save(QStringLiteral("basecamp_home"), layoutSettings(1)),
                 HomeSettingsStore::Result::Saved);
        QCOMPARE(HomeSettingsStore(path()).load(QStringLiteral("basecamp_home")).value("version").toInt(), 1);
    }
};

QTEST_GUILESS_MAIN(HomeSettingsStoreTest)
#include "home_settings_store_test.moc"
