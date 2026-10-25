// srcdeps: NotificationCenter.cpp ObjectStore.cpp NotificationObjects.cpp
//
// Notifications in the home feed (basecamp-experiments #31): what an app posts
// is also a `notification` object from that app. Covered here: the envelope,
// same tag replacing in place (and new again), the two switches (Settings →
// Notifications mutes and clears; Settings → Search drops), Basecamp's own
// notices staying out, the per-app cap, and the app's own objects untouched.

#include "NotificationCenter.h"
#include "NotificationObjects.h"
#include "ObjectStore.h"

#include <QCoreApplication>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

namespace {

const QString kRadio = QStringLiteral("radio_ui");
const QString kWallet = QStringLiteral("wallet_ui");

QVariantMap note(const QString& title, const QString& tag = QString(),
                 const QString& level = QStringLiteral("info"))
{
    QVariantMap n{ { QStringLiteral("title"), title },
                   { QStringLiteral("body"), QStringLiteral("Body of ") + title },
                   { QStringLiteral("level"), level } };
    if (!tag.isEmpty())
        n[QStringLiteral("tag")] = tag;
    return n;
}

QVariantList notificationsOf(ObjectStore& store, const QString& app)
{
    return store.list(QVariantMap{ { QStringLiteral("apps"), QStringList{ app } },
                                   { QStringLiteral("types"), QStringList{ QStringLiteral("notification") } } });
}

} // namespace

class TestNotificationObjects : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QVERIFY(m_settingsDir.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settingsDir.path());
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("NotificationObjectsTest"));
        QCoreApplication::setApplicationName(QStringLiteral("NotificationObjectsTest"));
    }
    void init() { QSettings().clear(); }

    void testPostBecomesAnObjectFromTheSender()
    {
        NotificationCenter center;
        ObjectStore store;
        NotificationObjects mirror(&center, &store);

        QString error;
        const QString id = center.post(kRadio, note(QStringLiteral("3 new stations")), &error);
        QVERIFY(!id.isEmpty());
        const QVariantList objects = notificationsOf(store, kRadio);
        QCOMPARE(objects.size(), 1);
        const QVariantMap o = objects.first().toMap();
        QCOMPARE(o.value("app").toString(), kRadio);
        QCOMPARE(o.value("type").toString(), QStringLiteral("notification"));
        QCOMPARE(o.value("title").toString(), QStringLiteral("3 new stations"));
        QCOMPARE(o.value("fields").toMap().value("level").toString(), QStringLiteral("info"));
        QCOMPARE(o.value("fields").toMap().value("body").toString(), QStringLiteral("Body of 3 new stations"));
        QCOMPARE(o.value("seen").toBool(), false);
        QVERIFY(NotificationObjects::isMirrorKey(o.value("key").toString()));
        // The notification's own time, so the feed orders it where it happened.
        QCOMPARE(o.value("time").toLongLong(),
                 center.items().first().toMap().value("time").toLongLong());
    }

    void testSameTagReplacesInPlaceAndIsNewAgain()
    {
        NotificationCenter center;
        ObjectStore store;
        NotificationObjects mirror(&center, &store);

        QVERIFY(!center.post(kRadio, note(QStringLiteral("1 new station"), QStringLiteral("stations"))).isEmpty());
        const QString first = notificationsOf(store, kRadio).first().toMap().value("id").toString();
        store.markSeen({ first });
        QVERIFY(store.get(first).value("seen").toBool());

        QVERIFY(!center.post(kRadio, note(QStringLiteral("2 new stations"), QStringLiteral("stations"))).isEmpty());
        const QVariantList objects = notificationsOf(store, kRadio);
        QCOMPARE(objects.size(), 1);
        QCOMPARE(objects.first().toMap().value("id").toString(), first);
        QCOMPARE(objects.first().toMap().value("title").toString(), QStringLiteral("2 new stations"));
        QCOMPARE(objects.first().toMap().value("seen").toBool(), false);

        // Untagged ones are each their own card.
        QVERIFY(!center.post(kRadio, note(QStringLiteral("Hello"))).isEmpty());
        QVERIFY(!center.post(kRadio, note(QStringLiteral("Hello"))).isEmpty());
        QCOMPARE(notificationsOf(store, kRadio).size(), 3);
    }

    void testMutedAppMakesNoCardsAndMutingClearsThem()
    {
        NotificationCenter center;
        ObjectStore store;
        NotificationObjects mirror(&center, &store);

        QVERIFY(!center.post(kRadio, note(QStringLiteral("Before"))).isEmpty());
        QCOMPARE(notificationsOf(store, kRadio).size(), 1);

        center.setMuted(kRadio, true);
        QCOMPARE(notificationsOf(store, kRadio).size(), 0);
        QVERIFY(!center.post(kRadio, note(QStringLiteral("While muted"))).isEmpty());
        QCOMPARE(notificationsOf(store, kRadio).size(), 0);

        center.setMuted(kRadio, false);
        QVERIFY(!center.post(kRadio, note(QStringLiteral("After"))).isEmpty());
        QCOMPARE(notificationsOf(store, kRadio).size(), 1);
    }

    void testSearchSwitchDropsCards()
    {
        NotificationCenter center;
        ObjectStore store;
        NotificationObjects mirror(&center, &store);

        QVERIFY(!center.post(kRadio, note(QStringLiteral("Before"))).isEmpty());
        store.setEnabled(kRadio, false);
        QCOMPARE(notificationsOf(store, kRadio).size(), 0);
        // Still a notification (toast, tab dot), just not a card.
        QVERIFY(!center.post(kRadio, note(QStringLiteral("While off"))).isEmpty());
        QCOMPARE(center.items().size(), 2);
        QCOMPARE(notificationsOf(store, kRadio).size(), 0);
    }

    void testBasecampNoticesStayOut()
    {
        NotificationCenter center;
        ObjectStore store;
        NotificationObjects mirror(&center, &store);

        center.postSystem(QStringLiteral("Radio stopped unexpectedly"), QStringLiteral("Reopen it."),
                          QStringLiteral("error"));
        QCOMPARE(center.items().size(), 1);
        QCOMPARE(store.total(), 0);
    }

    void testNotCountedAgainstTheAppsOwnRate()
    {
        NotificationCenter center;
        ObjectStore store;
        NotificationObjects mirror(&center, &store);

        // Notifications are rate-limited on their own (5 a minute); a mirror
        // must not spend the app's publish budget (20 per 10 s).
        for (int i = 0; i < ObjectStore::kRateRequests; ++i)
            emit center.posted(QVariantMap{ { "id", QStringLiteral("x%1").arg(i) }, { "app", kWallet },
                                            { "title", QStringLiteral("N %1").arg(i) },
                                            { "level", "info" }, { "tag", QString() },
                                            { "time", qint64(1000 + i) } }, false);
        const QVariantMap own{ { "objects", QVariantList{ QVariantMap{
            { "key", "acct" }, { "type", "account" }, { "title", "Main" } } } } };
        QCOMPARE(store.publish(kWallet, own), QString());
        QCOMPARE(store.count(kWallet), ObjectStore::kRateRequests + 1);
    }

    void testCapPerAppKeepsNewestAndLeavesTheAppsOwnObjects()
    {
        NotificationCenter center;
        ObjectStore store;
        NotificationObjects mirror(&center, &store);

        // The app's own `notification`-type object is its own, never trimmed.
        const QVariantMap own{ { "objects", QVariantList{ QVariantMap{
            { "key", "mine" }, { "type", "notification" }, { "title", "Pinned" }, { "time", qint64(1) } } } } };
        QCOMPARE(store.publish(kWallet, own), QString());

        const int posts = NotificationObjects::kMaxPerApp + 5;
        for (int i = 0; i < posts; ++i)
            emit center.posted(QVariantMap{ { "id", QStringLiteral("n%1").arg(i) }, { "app", kWallet },
                                            { "title", QStringLiteral("N %1").arg(i) },
                                            { "level", "info" }, { "tag", QString() },
                                            { "time", qint64(1000 + i) } }, false);
        const QVariantList objects = notificationsOf(store, kWallet);
        QCOMPARE(objects.size(), NotificationObjects::kMaxPerApp + 1);
        QCOMPARE(objects.first().toMap().value("title").toString(), QStringLiteral("N %1").arg(posts - 1));
        QCOMPARE(objects.last().toMap().value("key").toString(), QStringLiteral("mine"));
        // The oldest five mirrored cards went.
        for (const QVariant& v : objects)
            QVERIFY(v.toMap().value("title").toString() != QStringLiteral("N 4"));
    }

    void testEnvelopeShape()
    {
        const QVariantMap e = NotificationObjects::envelopeFor(QVariantMap{
            { "id", "n7" }, { "app", kRadio }, { "title", "Hi" }, { "body", "" },
            { "level", "warning" }, { "tag", "" }, { "time", qint64(42) } });
        QCOMPARE(e.value("key").toString(), QStringLiteral("basecamp.notification:n7"));
        QCOMPARE(e.value("time").toLongLong(), qint64(42));
        QVERIFY(!e.value("fields").toMap().contains("body"));   // empty: left out
        QCOMPARE(NotificationObjects::keyFor(QVariantMap{ { "id", "n8" }, { "tag", "t" } }),
                 QStringLiteral("basecamp.notification:tag:t"));
        QVERIFY(!NotificationObjects::isMirrorKey(QStringLiteral("mine")));
    }

private:
    QTemporaryDir m_settingsDir;
};

QTEST_GUILESS_MAIN(TestNotificationObjects)
#include "notification_objects_test.moc"
