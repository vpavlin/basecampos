// srcdeps: ObjectStore.cpp
//
// Shell objects (SHELL-OBJECTS-SPEC.md, ADR-072): the host's store of what apps
// publish. Covered here: the envelope and v0 type validation, upsert by app +
// key, retraction, the per-app limits (eviction, the rate window, the fields
// cap), the user's switch (off deletes and drops, still answering ok), and
// expiry. The clock is injected so the time-based rules cost nothing to test.

#include "ObjectStore.h"

#include "LogosIntent.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <cmath>
#include <limits>
#include <memory>

namespace {

const QString kWallet = QStringLiteral("wallet_ui");
const QString kChat = QStringLiteral("chat_ui");

QVariantMap tx(const QString& key, qint64 time = 0)
{
    QVariantMap o{
        { QStringLiteral("key"), key },
        { QStringLiteral("type"), QStringLiteral("transaction") },
        { QStringLiteral("title"), QStringLiteral("Sending to Ada") },
        { QStringLiteral("fields"), QVariantMap{
              { QStringLiteral("direction"), QStringLiteral("out") },
              { QStringLiteral("status"), QStringLiteral("sending") },
              { QStringLiteral("amount"), QStringLiteral("0.3319") },
              { QStringLiteral("unit"), QStringLiteral("BTC") } } },
    };
    if (time) o[QStringLiteral("time")] = time;
    return o;
}

QVariantMap batch(const QVariantList& objects)
{
    return { { QStringLiteral("objects"), objects } };
}

} // namespace

class TestObjectStore : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        // The build sandbox has no writable home: keep settings in a temp dir.
        QVERIFY(m_settingsDir.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settingsDir.path());
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("ObjectStoreTest"));
        QCoreApplication::setApplicationName(QStringLiteral("ObjectStoreTest"));
    }
    void init() { QSettings().clear(); m_now = 1'000'000; }

    void testPublishSetsHostFields();
    void testHostFieldsCannotBeSupplied();
    void testUnknownFieldsAreDropped();
    void testValidationRefusesBadShapes_data();
    void testValidationRefusesBadShapes();
    void testBatchIsAllOrNothing();
    void testCustomTypeOnlyForItsOwnApp();
    void testSameKeyReplacesAndKeepsSeen();
    void testIdsAreStablePerAppAndKey();
    void testRetractByKeysAndAll();
    void testRetractRefusesBadShapes();
    void testOverFiveHundredEvictsOldest();
    void testBatchOverTheLimitFails();
    void testFieldsOverTwoKilobytesFails();
    void testRateLimitPerApp();
    void testSwitchOffDeletesAndDrops();
    void testSwitchIsRemembered();
    void testExpiredObjectsAreDropped();
    void testRetainAppsForgetsUninstalled();
    void testListFilters();
    void testCountsOfType();
    void testSignalsCarryTheApp();
    void testEnvelopesAreCanonicalPayloads();
    void testIconFullBleedIsStampedPerApp();
    void testChangesSinceFollowsEveryChange();
    void testChangesSinceResetsWhenItCannotTell();

private:
    ObjectStore* makeStore()
    {
        auto* store = new ObjectStore(this);
        store->setClock([this]() { return m_now; });
        store->setIconResolver([](const QString& app) { return QStringLiteral("file:///icons/") + app; });
        return store;
    }

    QTemporaryDir m_settingsDir;
    qint64 m_now = 1'000'000;
};

void TestObjectStore::testPublishSetsHostFields()
{
    std::unique_ptr<ObjectStore> store(makeStore());
    QStringList ids;
    QCOMPARE(store->publish(kWallet, batch({ tx(QStringLiteral("tx:1")) }), nullptr, &ids), QString());
    QCOMPARE(ids.size(), 1);
    QCOMPARE(store->count(kWallet), 1);

    const QVariantMap o = store->get(ids.first());
    QCOMPARE(o.value(QStringLiteral("id")).toString(), ObjectStore::idFor(kWallet, QStringLiteral("tx:1")));
    QCOMPARE(o.value(QStringLiteral("app")).toString(), kWallet);
    QCOMPARE(o.value(QStringLiteral("icon")).toString(), QStringLiteral("file:///icons/wallet_ui"));
    QCOMPARE(o.value(QStringLiteral("updated")).toLongLong(), m_now);
    QCOMPARE(o.value(QStringLiteral("time")).toLongLong(), m_now);   // defaults to publish time
    QCOMPARE(o.value(QStringLiteral("seen")).toBool(), false);
    QCOMPARE(o.value(QStringLiteral("kind")).toString(), QStringLiteral("item"));
    QCOMPARE(o.value(QStringLiteral("fields")).toMap().value(QStringLiteral("unit")).toString(),
             QStringLiteral("BTC"));
    QCOMPARE(store->types(), QStringList{ QStringLiteral("transaction") });
}

void TestObjectStore::testHostFieldsCannotBeSupplied()
{
    std::unique_ptr<ObjectStore> store(makeStore());
    QVariantMap o = tx(QStringLiteral("tx:1"));
    o[QStringLiteral("id")] = QStringLiteral("forged");
    o[QStringLiteral("app")] = kChat;
    o[QStringLiteral("seen")] = true;
    o[QStringLiteral("icon")] = QStringLiteral("https://evil.example/x.png");
    o[QStringLiteral("iconFullBleed")] = true;
    QStringList ids;
    QCOMPARE(store->publish(kWallet, batch({ o }), nullptr, &ids), QString());
    const QVariantMap stored = store->get(ids.first());
    QCOMPARE(stored.value(QStringLiteral("app")).toString(), kWallet);
    QVERIFY(stored.value(QStringLiteral("id")).toString() != QStringLiteral("forged"));
    QCOMPARE(stored.value(QStringLiteral("seen")).toBool(), false);
    QCOMPARE(stored.value(QStringLiteral("icon")).toString(), QStringLiteral("file:///icons/wallet_ui"));
    QCOMPARE(stored.value(QStringLiteral("iconFullBleed")).toBool(), false);
    QCOMPARE(store->count(kChat), 0);
}

// A card draws the publisher's icon with the shared app tile, which needs to
// know whether that icon fills the tile (basecamp-experiments#40). The host
// says so per app; without a resolver, or for an app it says no to, false.
void TestObjectStore::testIconFullBleedIsStampedPerApp()
{
    std::unique_ptr<ObjectStore> store(makeStore());
    QStringList ids;
    QCOMPARE(store->publish(kWallet, batch({ tx(QStringLiteral("tx:1")) }), nullptr, &ids), QString());
    QCOMPARE(store->get(ids.first()).value(QStringLiteral("iconFullBleed")).toBool(), false);

    store->setIconFullBleedResolver([](const QString& app) { return app == kWallet; });
    QCOMPARE(store->publish(kWallet, batch({ tx(QStringLiteral("tx:2")) }), nullptr, &ids), QString());
    QCOMPARE(store->get(ids.last()).value(QStringLiteral("iconFullBleed")).toBool(), true);
    QCOMPARE(store->publish(kChat, batch({ tx(QStringLiteral("tx:3")) }), nullptr, &ids), QString());
    QCOMPARE(store->get(ids.last()).value(QStringLiteral("iconFullBleed")).toBool(), false);
}

void TestObjectStore::testUnknownFieldsAreDropped()
{
    std::unique_ptr<ObjectStore> store(makeStore());
    QVariantMap o = tx(QStringLiteral("tx:1"));
    o[QStringLiteral("colour")] = QStringLiteral("red");                // unknown envelope field
    QVariantMap fields = o.value(QStringLiteral("fields")).toMap();
    fields[QStringLiteral("memo")] = QStringLiteral("for lunch");       // not a transaction field
    o[QStringLiteral("fields")] = fields;
    QStringList ids;
    QCOMPARE(store->publish(kWallet, batch({ o }), nullptr, &ids), QString());
    const QVariantMap stored = store->get(ids.first());
    QVERIFY(!stored.contains(QStringLiteral("colour")));
    QVERIFY(!stored.value(QStringLiteral("fields")).toMap().contains(QStringLiteral("memo")));
    QVERIFY(stored.value(QStringLiteral("fields")).toMap().contains(QStringLiteral("status")));
}

void TestObjectStore::testValidationRefusesBadShapes_data()
{
    QTest::addColumn<QVariantMap>("patch");      // merged over a good object
    QTest::addColumn<QString>("remove");         // a field to take out first

    auto row = [](const char* name, QVariantMap patch, QString remove = {}) {
        QTest::newRow(name) << patch << remove;
    };
    row("no key", {}, QStringLiteral("key"));
    row("empty key", { { QStringLiteral("key"), QString() } });
    row("long key", { { QStringLiteral("key"), QString(129, QLatin1Char('k')) } });
    row("no type", {}, QStringLiteral("type"));
    row("unknown type", { { QStringLiteral("type"), QStringLiteral("spaceship") } });
    row("reserved type", { { QStringLiteral("type"), QStringLiteral("nft") } });
    row("host type", { { QStringLiteral("type"), QStringLiteral("app") } });
    row("bad kind", { { QStringLiteral("kind"), QStringLiteral("banner") } });
    row("no title", {}, QStringLiteral("title"));
    row("blank title", { { QStringLiteral("title"), QStringLiteral("   ") } });
    row("long title", { { QStringLiteral("title"), QString(81, QLatin1Char('t')) } });
    row("title not text", { { QStringLiteral("title"), 42 } });
    row("long subtitle", { { QStringLiteral("subtitle"), QString(121, QLatin1Char('s')) } });
    row("fields not object", { { QStringLiteral("fields"), QStringLiteral("x") } });
    row("bad enum", { { QStringLiteral("fields"), QVariantMap{ { QStringLiteral("status"), QStringLiteral("lost") } } } });
    row("wrong field type", { { QStringLiteral("fields"), QVariantMap{ { QStringLiteral("direction"), true } } } });
    row("17 keywords", { { QStringLiteral("keywords"), QStringList(17, QStringLiteral("k")) } });
    row("long keyword", { { QStringLiteral("keywords"), QStringList{ QString(33, QLatin1Char('k')) } } });
    row("9 subjects", { { QStringLiteral("subjects"), QStringList(9, QStringLiteral("logos:pk:1")) } });
    row("9 refs", { { QStringLiteral("refs"), QVariantList(9, QVariantMap{ { QStringLiteral("type"), QStringLiteral("contact") }, { QStringLiteral("key"), QStringLiteral("c1") } }) } });
    row("ref without key", { { QStringLiteral("refs"), QVariantList{ QVariantMap{ { QStringLiteral("type"), QStringLiteral("contact") } } } } });
    const QVariantMap action{ { QStringLiteral("id"), QStringLiteral("view") }, { QStringLiteral("label"), QStringLiteral("View") } };
    row("4 actions", { { QStringLiteral("actions"), QVariantList{
        QVariantMap{ { QStringLiteral("id"), QStringLiteral("a") }, { QStringLiteral("label"), QStringLiteral("A") } },
        QVariantMap{ { QStringLiteral("id"), QStringLiteral("b") }, { QStringLiteral("label"), QStringLiteral("B") } },
        QVariantMap{ { QStringLiteral("id"), QStringLiteral("c") }, { QStringLiteral("label"), QStringLiteral("C") } },
        QVariantMap{ { QStringLiteral("id"), QStringLiteral("d") }, { QStringLiteral("label"), QStringLiteral("D") } } } } });
    row("duplicate action", { { QStringLiteral("actions"), QVariantList{ action, action } } });
    row("action named open", { { QStringLiteral("actions"), QVariantList{ QVariantMap{ { QStringLiteral("id"), QStringLiteral("open") }, { QStringLiteral("label"), QStringLiteral("Open") } } } } });
    row("action input not text", { { QStringLiteral("actions"), QVariantList{ QVariantMap{ { QStringLiteral("id"), QStringLiteral("r") }, { QStringLiteral("label"), QStringLiteral("Reply") }, { QStringLiteral("input"), QStringLiteral("html") } } } } });
    row("progress over 100", { { QStringLiteral("progress"), 101 } });
    row("negative progress", { { QStringLiteral("progress"), -1 } });
    row("time not number", { { QStringLiteral("time"), QStringLiteral("today") } });
    // Past the largest JS Date: converting it to qint64 would be undefined (on
    // x86, 1e20 becomes INT64_MIN and the object expires on arrival).
    row("time past a JS Date", { { QStringLiteral("time"), 1e19 } });
    row("expires past a JS Date", { { QStringLiteral("expires"), 1e20 } });
    // NaN slips past every `< 0` / `> 100` comparison: refused as a number.
    row("NaN time", { { QStringLiteral("time"), std::nan("") } });
    row("NaN expires", { { QStringLiteral("expires"), std::nan("") } });
    row("NaN progress", { { QStringLiteral("progress"), std::nan("") } });
    row("infinite expires", { { QStringLiteral("expires"), std::numeric_limits<double>::infinity() } });
}

void TestObjectStore::testValidationRefusesBadShapes()
{
    QFETCH(QVariantMap, patch);
    QFETCH(QString, remove);
    std::unique_ptr<ObjectStore> store(makeStore());
    QVariantMap o = tx(QStringLiteral("tx:1"));
    if (!remove.isEmpty()) o.remove(remove);
    for (auto it = patch.cbegin(); it != patch.cend(); ++it)
        o[it.key()] = it.value();
    int bad = -2;
    QCOMPARE(store->publish(kWallet, batch({ tx(QStringLiteral("ok:0")), o }), &bad), QStringLiteral("bad_request"));
    QCOMPARE(bad, 1);
    QCOMPARE(store->count(), 0);
}

void TestObjectStore::testBatchIsAllOrNothing()
{
    std::unique_ptr<ObjectStore> store(makeStore());
    QVariantMap broken = tx(QStringLiteral("tx:3"));
    broken.remove(QStringLiteral("title"));
    int bad = -2;
    QCOMPARE(store->publish(kWallet, batch({ tx(QStringLiteral("tx:1")), tx(QStringLiteral("tx:2")), broken }), &bad),
             QStringLiteral("bad_request"));
    QCOMPARE(bad, 2);
    QCOMPARE(store->count(kWallet), 0);

    // The batch itself malformed: index -1.
    QCOMPARE(store->publish(kWallet, { { QStringLiteral("objects"), QStringLiteral("nope") } }, &bad),
             QStringLiteral("bad_request"));
    QCOMPARE(bad, -1);
    QCOMPARE(store->publish(kWallet, {}, &bad), QStringLiteral("bad_request"));
}

void TestObjectStore::testCustomTypeOnlyForItsOwnApp()
{
    std::unique_ptr<ObjectStore> store(makeStore());
    QVariantMap o{ { QStringLiteral("key"), QStringLiteral("q1") },
                   { QStringLiteral("type"), QStringLiteral("x.wallet_ui.quote") },
                   { QStringLiteral("title"), QStringLiteral("BTC/USD") },
                   { QStringLiteral("fields"), QVariantMap{ { QStringLiteral("anything"), 1 } } } };
    QStringList ids;
    QCOMPARE(store->publish(kWallet, batch({ o }), nullptr, &ids), QString());
    // An app's own type keeps its fields whole: there is no list to check.
    QCOMPARE(store->get(ids.first()).value(QStringLiteral("fields")).toMap().value(QStringLiteral("anything")).toInt(), 1);
    // …and is that app's alone.
    QCOMPARE(store->publish(kChat, batch({ o })), QStringLiteral("bad_request"));
    o[QStringLiteral("type")] = QStringLiteral("x.wallet_ui.");
    QCOMPARE(store->publish(kWallet, batch({ o })), QStringLiteral("bad_request"));
}

void TestObjectStore::testSameKeyReplacesAndKeepsSeen()
{
    std::unique_ptr<ObjectStore> store(makeStore());
    QStringList ids;
    QVERIFY(store->publish(kWallet, batch({ tx(QStringLiteral("tx:1")) }), nullptr, &ids).isEmpty());
    store->markSeen(ids);
    QVERIFY(store->get(ids.first()).value(QStringLiteral("seen")).toBool());

    m_now += 500;
    QVariantMap update = tx(QStringLiteral("tx:1"));
    update[QStringLiteral("progress")] = 60;
    update[QStringLiteral("title")] = QStringLiteral("Sent to Ada");
    QStringList again;
    QVERIFY(store->publish(kWallet, batch({ update }), nullptr, &again).isEmpty());
    QCOMPARE(again, ids);
    QCOMPARE(store->count(kWallet), 1);
    const QVariantMap o = store->get(ids.first());
    QCOMPARE(o.value(QStringLiteral("title")).toString(), QStringLiteral("Sent to Ada"));
    QCOMPARE(o.value(QStringLiteral("progress")).toDouble(), 60.0);
    QCOMPARE(o.value(QStringLiteral("updated")).toLongLong(), m_now);
    // An update in place is not news.
    QVERIFY(o.value(QStringLiteral("seen")).toBool());

    // The same key from another app is another object.
    QVERIFY(store->publish(kChat, batch({ tx(QStringLiteral("tx:1")) })).isEmpty());
    QCOMPARE(store->count(), 2);
}

void TestObjectStore::testIdsAreStablePerAppAndKey()
{
    QCOMPARE(ObjectStore::idFor(kWallet, QStringLiteral("a")), ObjectStore::idFor(kWallet, QStringLiteral("a")));
    QVERIFY(ObjectStore::idFor(kWallet, QStringLiteral("a")) != ObjectStore::idFor(kChat, QStringLiteral("a")));
    QVERIFY(ObjectStore::idFor(kWallet, QStringLiteral("a")) != ObjectStore::idFor(kWallet, QStringLiteral("b")));
}

void TestObjectStore::testRetractByKeysAndAll()
{
    std::unique_ptr<ObjectStore> store(makeStore());
    QVERIFY(store->publish(kWallet, batch({ tx(QStringLiteral("a")), tx(QStringLiteral("b")), tx(QStringLiteral("c")) })).isEmpty());
    QVERIFY(store->publish(kChat, batch({ tx(QStringLiteral("a")) })).isEmpty());

    QCOMPARE(store->retract(kWallet, { { QStringLiteral("keys"), QStringList{ QStringLiteral("a"), QStringLiteral("zzz") } } }),
             QString());   // an unknown key is not an error
    QCOMPARE(store->count(kWallet), 2);
    QCOMPARE(store->count(kChat), 1);   // never another app's

    QCOMPARE(store->retract(kWallet, { { QStringLiteral("all"), true } }), QString());
    QCOMPARE(store->count(kWallet), 0);
    QCOMPARE(store->count(kChat), 1);
}

void TestObjectStore::testRetractRefusesBadShapes()
{
    std::unique_ptr<ObjectStore> store(makeStore());
    QCOMPARE(store->retract(kWallet, {}), QStringLiteral("bad_request"));
    QCOMPARE(store->retract(kWallet, { { QStringLiteral("keys"), QStringLiteral("a") } }), QStringLiteral("bad_request"));
    QCOMPARE(store->retract(kWallet, { { QStringLiteral("all"), QStringLiteral("yes") } }), QStringLiteral("bad_request"));
}

void TestObjectStore::testOverFiveHundredEvictsOldest()
{
    std::unique_ptr<ObjectStore> store(makeStore());
    // 10 requests of 50, each object a distinct `time`; the earliest batch is oldest.
    for (int b = 0; b < 10; ++b) {
        QVariantList objects;
        for (int i = 0; i < 50; ++i)
            objects << tx(QStringLiteral("tx:%1").arg(b * 50 + i), 10'000 + b * 50 + i);
        QVERIFY(store->publish(kWallet, batch(objects)).isEmpty());
    }
    QCOMPARE(store->count(kWallet), 500);

    m_now += ObjectStore::kRateWindowMs;   // a fresh window for the next request
    QVERIFY(store->publish(kWallet, batch({ tx(QStringLiteral("tx:new"), 99'999),
                                            tx(QStringLiteral("tx:ancient"), 1) })).isEmpty());
    QCOMPARE(store->count(kWallet), 500);
    // The oldest by `time` went: the one just published as ancient, then tx:0.
    QVERIFY(store->get(ObjectStore::idFor(kWallet, QStringLiteral("tx:ancient"))).isEmpty());
    QVERIFY(store->get(ObjectStore::idFor(kWallet, QStringLiteral("tx:0"))).isEmpty());
    QVERIFY(!store->get(ObjectStore::idFor(kWallet, QStringLiteral("tx:1"))).isEmpty());
    QVERIFY(!store->get(ObjectStore::idFor(kWallet, QStringLiteral("tx:new"))).isEmpty());
}

void TestObjectStore::testBatchOverTheLimitFails()
{
    std::unique_ptr<ObjectStore> store(makeStore());
    QVariantList objects;
    for (int i = 0; i < ObjectStore::kMaxObjectsPerApp + 1; ++i)
        objects << tx(QStringLiteral("tx:%1").arg(i));
    QCOMPARE(store->publish(kWallet, batch(objects)), QStringLiteral("failed"));
    QCOMPARE(store->count(), 0);
}

void TestObjectStore::testFieldsOverTwoKilobytesFails()
{
    std::unique_ptr<ObjectStore> store(makeStore());
    QVariantMap o{ { QStringLiteral("key"), QStringLiteral("m1") },
                   { QStringLiteral("type"), QStringLiteral("x.wallet_ui.blob") },
                   { QStringLiteral("title"), QStringLiteral("Big") },
                   { QStringLiteral("fields"), QVariantMap{ { QStringLiteral("a"), QString(1000, QLatin1Char('a')) },
                                                            { QStringLiteral("b"), QString(1000, QLatin1Char('b')) },
                                                            { QStringLiteral("c"), QString(100, QLatin1Char('c')) } } } };
    int bad = -2;
    QCOMPARE(store->publish(kWallet, batch({ o }), &bad), QStringLiteral("failed"));
    QCOMPARE(bad, 0);
    QCOMPARE(store->count(), 0);
}

void TestObjectStore::testRateLimitPerApp()
{
    std::unique_ptr<ObjectStore> store(makeStore());
    for (int i = 0; i < ObjectStore::kRateRequests; ++i) {
        QCOMPARE(store->publish(kWallet, batch({ tx(QStringLiteral("tx:%1").arg(i)) })), QString());
        m_now += 100;
    }
    QCOMPARE(store->publish(kWallet, batch({ tx(QStringLiteral("tx:over")) })), QStringLiteral("failed"));
    QVERIFY(store->get(ObjectStore::idFor(kWallet, QStringLiteral("tx:over"))).isEmpty());
    // Per app: another app is not affected.
    QCOMPARE(store->publish(kChat, batch({ tx(QStringLiteral("tx:1")) })), QString());
    // The window slides: once the first request is 10 s old, one more fits.
    m_now = 1'000'000 + ObjectStore::kRateWindowMs;
    QCOMPARE(store->publish(kWallet, batch({ tx(QStringLiteral("tx:later")) })), QString());
    QCOMPARE(store->publish(kWallet, batch({ tx(QStringLiteral("tx:later2")) })), QStringLiteral("failed"));
}

void TestObjectStore::testSwitchOffDeletesAndDrops()
{
    std::unique_ptr<ObjectStore> store(makeStore());
    QVERIFY(store->publish(kWallet, batch({ tx(QStringLiteral("a")), tx(QStringLiteral("b")) })).isEmpty());
    QVERIFY(store->publish(kChat, batch({ tx(QStringLiteral("a")) })).isEmpty());

    QSignalSpy disabledSpy(store.get(), &ObjectStore::disabledAppsChanged);
    store->setEnabled(kWallet, false);
    QCOMPARE(disabledSpy.count(), 1);
    QCOMPARE(store->count(kWallet), 0);   // deleted at once
    QCOMPARE(store->count(kChat), 1);
    QVERIFY(!store->isEnabled(kWallet));

    // Still answered ok, with ids, but nothing is kept.
    QStringList ids;
    QCOMPARE(store->publish(kWallet, batch({ tx(QStringLiteral("c")) }), nullptr, &ids), QString());
    QCOMPARE(ids.size(), 1);
    QCOMPARE(store->count(kWallet), 0);
    QCOMPARE(store->retract(kWallet, { { QStringLiteral("all"), true } }), QString());
    // A bad batch is still bad: answering differently would reveal the switch.
    QVariantMap broken = tx(QStringLiteral("d"));
    broken.remove(QStringLiteral("key"));
    QCOMPARE(store->publish(kWallet, batch({ broken })), QStringLiteral("bad_request"));

    store->setEnabled(kWallet, true);
    QVERIFY(store->publish(kWallet, batch({ tx(QStringLiteral("e")) })).isEmpty());
    QCOMPARE(store->count(kWallet), 1);   // nothing from while it was off comes back
}

void TestObjectStore::testSwitchIsRemembered()
{
    {
        std::unique_ptr<ObjectStore> store(makeStore());
        store->setEnabled(kWallet, false);
    }
    std::unique_ptr<ObjectStore> store(makeStore());
    QVERIFY(!store->isEnabled(kWallet));
    QVERIFY(store->isEnabled(kChat));
    QCOMPARE(store->disabledApps(), QStringList{ kWallet });
}

void TestObjectStore::testExpiredObjectsAreDropped()
{
    std::unique_ptr<ObjectStore> store(makeStore());
    QVariantMap soon = tx(QStringLiteral("soon"));
    soon[QStringLiteral("expires")] = m_now + 1000;
    QVariantMap past = tx(QStringLiteral("past"));
    past[QStringLiteral("expires")] = m_now - 1;
    QVERIFY(store->publish(kWallet, batch({ soon, past, tx(QStringLiteral("forever")) })).isEmpty());
    QCOMPARE(store->count(kWallet), 2);   // already expired: never kept

    // "Never", spelled as the largest JS Date, is a far future, not a wrapped
    // negative that expires on arrival.
    QVariantMap never = tx(QStringLiteral("never"));
    never[QStringLiteral("expires")] = 8.64e15;
    QVERIFY(store->publish(kChat, batch({ never })).isEmpty());
    QCOMPARE(store->count(kChat), 1);
    QCOMPARE(store->get(ObjectStore::idFor(kChat, QStringLiteral("never")))
                 .value(QStringLiteral("expires")).toLongLong(), qint64(8'640'000'000'000'000));
    QCOMPARE(store->retract(kChat, { { QStringLiteral("all"), true } }), QString());

    m_now += 1000;
    QCOMPARE(store->count(kWallet), 1);
    QVERIFY(!store->get(ObjectStore::idFor(kWallet, QStringLiteral("forever"))).isEmpty());

    // And without anyone reading: the store's own timer fires.
    m_now = QDateTime::currentMSecsSinceEpoch();
    store->setClock({});
    QVariantMap later = tx(QStringLiteral("later"));
    later[QStringLiteral("expires")] = QDateTime::currentMSecsSinceEpoch() + 100;
    QVERIFY(store->publish(kChat, batch({ later })).isEmpty());
    QSignalSpy changed(store.get(), &ObjectStore::objectsChanged);
    QTRY_COMPARE_WITH_TIMEOUT(store->total(), 1, 2000);
    QCOMPARE(changed.count(), 1);
    QCOMPARE(changed.first().first().toString(), kChat);
}

void TestObjectStore::testRetainAppsForgetsUninstalled()
{
    std::unique_ptr<ObjectStore> store(makeStore());
    QVERIFY(store->publish(kWallet, batch({ tx(QStringLiteral("a")) })).isEmpty());
    QVERIFY(store->publish(kChat, batch({ tx(QStringLiteral("a")) })).isEmpty());
    store->retainApps({ kChat });
    QCOMPARE(store->count(kWallet), 0);
    QCOMPARE(store->count(kChat), 1);
}

// The apps home names under a type pill (basecamp-experiments#36): who holds
// objects of that type, how many, and never a switched-off app.
void TestObjectStore::testCountsOfType()
{
    std::unique_ptr<ObjectStore> store(makeStore());
    const QString explorer = QStringLiteral("explorer_ui");
    QVariantMap msg{ { QStringLiteral("key"), QStringLiteral("m1") },
                     { QStringLiteral("type"), QStringLiteral("message") },
                     { QStringLiteral("title"), QStringLiteral("Ada: lunch?") } };
    QVERIFY(store->publish(kWallet, batch({ tx(QStringLiteral("t1")), tx(QStringLiteral("t2")) })).isEmpty());
    QVERIFY(store->publish(explorer, batch({ tx(QStringLiteral("e1")) })).isEmpty());
    QVERIFY(store->publish(kChat, batch({ msg })).isEmpty());

    QHash<QString, int> txs = store->countsOfType(QStringLiteral("transaction"));
    QCOMPARE(txs.size(), 2);
    QCOMPARE(txs.value(kWallet), 2);
    QCOMPARE(txs.value(explorer), 1);
    QCOMPARE(store->countsOfType(QStringLiteral("message")), (QHash<QString, int>{ { kChat, 1 } }));
    QVERIFY(store->countsOfType(QStringLiteral("news")).isEmpty());
    QVERIFY(store->countsOfType(QString()).isEmpty());

    // Retracted: no longer related.
    QVERIFY(store->retract(explorer, { { QStringLiteral("all"), true } }).isEmpty());
    QCOMPARE(store->countsOfType(QStringLiteral("transaction")), (QHash<QString, int>{ { kWallet, 2 } }));
    // Switched off in Settings -> Search: gone.
    store->setEnabled(kWallet, false);
    QVERIFY(store->countsOfType(QStringLiteral("transaction")).isEmpty());
}

void TestObjectStore::testListFilters()
{
    std::unique_ptr<ObjectStore> store(makeStore());
    QVariantMap msg{ { QStringLiteral("key"), QStringLiteral("m1") },
                     { QStringLiteral("type"), QStringLiteral("message") },
                     { QStringLiteral("title"), QStringLiteral("Ada: lunch?") },
                     { QStringLiteral("subjects"), QStringList{ QStringLiteral("logos:pk:ada") } },
                     { QStringLiteral("time"), 5 } };
    QVariantMap summary = tx(QStringLiteral("sum"), 9);
    summary[QStringLiteral("kind")] = QStringLiteral("summary");
    QVERIFY(store->publish(kChat, batch({ msg })).isEmpty());
    QVERIFY(store->publish(kWallet, batch({ tx(QStringLiteral("t1"), 7), summary })).isEmpty());

    const QVariantList all = store->list();
    QCOMPARE(all.size(), 3);
    // Newest `time` first.
    QCOMPARE(all.at(0).toMap().value(QStringLiteral("key")).toString(), QStringLiteral("sum"));
    QCOMPARE(all.at(2).toMap().value(QStringLiteral("key")).toString(), QStringLiteral("m1"));

    QCOMPARE(store->list({ { QStringLiteral("types"), QStringList{ QStringLiteral("message") } } }).size(), 1);
    QCOMPARE(store->list({ { QStringLiteral("apps"), QStringList{ kWallet } } }).size(), 2);
    QCOMPARE(store->list({ { QStringLiteral("kinds"), QStringList{ QStringLiteral("summary") } } }).size(), 1);
    QCOMPARE(store->list({ { QStringLiteral("subjects"), QStringList{ QStringLiteral("logos:pk:ada") } } }).size(), 1);

    store->markSeen({ ObjectStore::idFor(kChat, QStringLiteral("m1")) });
    QCOMPARE(store->list({ { QStringLiteral("unseenOnly"), true } }).size(), 2);
    QCOMPARE(store->types(), (QStringList{ QStringLiteral("message"), QStringLiteral("transaction") }));
}

void TestObjectStore::testSignalsCarryTheApp()
{
    std::unique_ptr<ObjectStore> store(makeStore());
    QSignalSpy spy(store.get(), &ObjectStore::objectsChanged);
    QVERIFY(store->publish(kWallet, batch({ tx(QStringLiteral("a")) })).isEmpty());
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.last().first().toString(), kWallet);
    QVERIFY(store->retract(kWallet, { { QStringLiteral("keys"), QStringList{ QStringLiteral("a") } } }).isEmpty());
    QCOMPARE(spy.count(), 2);
    // Nothing changed: nothing said.
    QVERIFY(store->retract(kWallet, { { QStringLiteral("keys"), QStringList{ QStringLiteral("a") } } }).isEmpty());
    QCOMPARE(spy.count(), 2);
    // A switched-off app's publish changes nothing either.
    store->setEnabled(kWallet, false);
    const int before = spy.count();
    QVERIFY(store->publish(kWallet, batch({ tx(QStringLiteral("b")) })).isEmpty());
    QCOMPARE(spy.count(), before);
}

// The envelopes may cross into another QML engine (the search role, a view),
// so they must pass the broker's payload rules. A QStringList does not: that
// is the shape that made a publish answer `failed` in the first headless run.
void TestObjectStore::testEnvelopesAreCanonicalPayloads()
{
    std::unique_ptr<ObjectStore> store(makeStore());
    QVariantMap o = tx(QStringLiteral("a"));
    o[QStringLiteral("keywords")] = QStringList{ QStringLiteral("lunch") };
    o[QStringLiteral("subjects")] = QStringList{ QStringLiteral("logos:pk:ada") };
    o[QStringLiteral("refs")] = QVariantList{ QVariantMap{ { QStringLiteral("type"), QStringLiteral("contact") },
                                                           { QStringLiteral("key"), QStringLiteral("c1") } } };
    o[QStringLiteral("actions")] = QVariantList{ QVariantMap{ { QStringLiteral("id"), QStringLiteral("reply") },
                                                              { QStringLiteral("label"), QStringLiteral("Reply") },
                                                              { QStringLiteral("input"), QStringLiteral("text") } } };
    o[QStringLiteral("progress")] = 40;
    o[QStringLiteral("expires")] = m_now + 100000;
    QVERIFY(store->publish(kWallet, batch({ o })).isEmpty());
    const QVariantList all = store->list();
    QCOMPARE(all.size(), 1);
    QVERIFY(logos::intent::isCanonicalValue(QVariant(all)));
}

namespace {
QStringList idsOf(const QList<QVariantMap>& objects)
{
    QStringList out;
    for (const QVariantMap& o : objects) out << o.value(QStringLiteral("id")).toString();
    out.sort();
    return out;
}
QStringList sortedList(QStringList l)
{
    l.sort();
    return l;
}
} // namespace

// basecamp-experiments#27: the search role keeps a copy and asks only for
// what changed. Every kind of change must show up, or its copy goes stale.
void TestObjectStore::testChangesSinceFollowsEveryChange()
{
    std::unique_ptr<ObjectStore> store(makeStore());
    QCOMPARE(store->version(), quint64(0));
    QVERIFY(store->publish(kWallet, batch({ tx(QStringLiteral("a"), 10), tx(QStringLiteral("b"), 20) })).isEmpty());
    const QString a = ObjectStore::idFor(kWallet, QStringLiteral("a"));
    const QString b = ObjectStore::idFor(kWallet, QStringLiteral("b"));

    // From nothing: everything, as a reset.
    ObjectStore::Changes c = store->changesSince(0);
    QVERIFY(c.reset);
    QCOMPARE(idsOf(c.changed), sortedList({ a, b }));
    QVERIFY(c.removed.isEmpty());
    quint64 v = c.version;
    QCOMPARE(v, store->version());

    // Nothing since: nothing.
    c = store->changesSince(v);
    QVERIFY(!c.reset);
    QVERIFY(c.changed.isEmpty());
    QVERIFY(c.removed.isEmpty());
    QCOMPARE(c.version, v);

    // A replace (same key) is a change, with the new envelope.
    QVariantMap a2 = tx(QStringLiteral("a"), 10);
    a2[QStringLiteral("title")] = QStringLiteral("Sent to Ada");
    QVERIFY(store->publish(kWallet, batch({ a2 })).isEmpty());
    c = store->changesSince(v);
    QVERIFY(!c.reset);
    QCOMPARE(idsOf(c.changed), QStringList{ a });
    QCOMPARE(c.changed.first().value(QStringLiteral("title")).toString(), QStringLiteral("Sent to Ada"));
    v = c.version;

    // Marking seen is a change (it moves the ranking).
    store->markSeen({ b });
    c = store->changesSince(v);
    QCOMPARE(idsOf(c.changed), QStringList{ b });
    QVERIFY(c.changed.first().value(QStringLiteral("seen")).toBool());
    v = c.version;
    // Already seen: no change.
    store->markSeen({ b });
    QCOMPARE(store->changesSince(v).changed.size(), 0);

    // Retract: removed.
    QVERIFY(store->retract(kWallet, { { QStringLiteral("keys"), QStringList{ QStringLiteral("a") } } }).isEmpty());
    c = store->changesSince(v);
    QVERIFY(c.changed.isEmpty());
    QCOMPARE(c.removed, QStringList{ a });
    v = c.version;

    // Removed, then published again: a change, not a removal.
    QVERIFY(store->publish(kWallet, batch({ tx(QStringLiteral("a"), 30) })).isEmpty());
    c = store->changesSince(v);
    QCOMPARE(idsOf(c.changed), QStringList{ a });
    QVERIFY(c.removed.isEmpty());
    v = c.version;

    // Expiry: removed.
    QVariantMap e = tx(QStringLiteral("e"), 40);
    e[QStringLiteral("expires")] = m_now + 1000;
    QVERIFY(store->publish(kChat, batch({ e })).isEmpty());
    const QString eid = ObjectStore::idFor(kChat, QStringLiteral("e"));
    c = store->changesSince(v);
    QCOMPARE(idsOf(c.changed), QStringList{ eid });
    v = c.version;
    m_now += 2000;
    c = store->changesSince(v);
    QCOMPARE(c.removed, QStringList{ eid });
    v = c.version;

    // The user's switch: everything that app published is removed.
    store->setEnabled(kWallet, false);
    c = store->changesSince(v);
    QCOMPARE(sortedList(c.removed), sortedList({ a, b }));
    v = c.version;
    store->setEnabled(kWallet, true);

    // Eviction past 500: the evicted are removed, the new ones changed.
    // (Batches of 100 so the rate limit is not what stops them.)
    for (int n = 0; n < 6; ++n) {
        QVariantList objs;
        for (int i = 0; i < 100; ++i)
            objs << tx(QStringLiteral("f%1").arg(n * 100 + i), 100 + n * 100 + i);
        QVERIFY(store->publish(kWallet, batch(objs)).isEmpty());
    }
    c = store->changesSince(v);
    QCOMPARE(c.changed.size(), 500);
    // The 100 oldest were published since `v` and then evicted: removed.
    QCOMPARE(c.removed.size(), 100);
    QVERIFY(c.removed.contains(ObjectStore::idFor(kWallet, QStringLiteral("f0"))));

    // A copy kept from these changes equals the store.
    QHash<QString, QVariantMap> copy;
    for (const QVariantMap& o : store->changesSince(0).changed) copy.insert(o.value(QStringLiteral("id")).toString(), o);
    QCOMPARE(copy.size(), store->total());
}

void TestObjectStore::testChangesSinceResetsWhenItCannotTell()
{
    std::unique_ptr<ObjectStore> store(makeStore());
    QVERIFY(store->publish(kWallet, batch({ tx(QStringLiteral("a"), 10) })).isEmpty());
    const quint64 v = store->version();
    // A version this store never gave out.
    QVERIFY(store->changesSince(v + 5).reset);
    QCOMPARE(store->changesSince(v + 5).changed.size(), 1);

    // More removals than it remembers: a reader from before them starts over.
    m_now += 60 * 1000;
    int published = 0;
    while (store->version() < v + 2 * ObjectStore::kMaxTombstones + 10) {
        QVariantList objs;
        for (int i = 0; i < 400; ++i, ++published)
            objs << tx(QStringLiteral("x%1").arg(published), 1000 + published);
        m_now += 10 * 1000;   // past the rate window
        QVERIFY(store->publish(kChat, batch(objs)).isEmpty());
        QVERIFY(store->retract(kChat, { { QStringLiteral("all"), true } }).isEmpty());
    }
    const ObjectStore::Changes c = store->changesSince(v);
    QVERIFY(c.reset);
    QCOMPARE(idsOf(c.changed), QStringList{ ObjectStore::idFor(kWallet, QStringLiteral("a")) });
    // A recent reader is still told precisely.
    const quint64 now = store->version();
    QVERIFY(store->retract(kWallet, { { QStringLiteral("all"), true } }).isEmpty());
    const ObjectStore::Changes d = store->changesSince(now);
    QVERIFY(!d.reset);
    QCOMPARE(d.removed.size(), 1);
}

QTEST_GUILESS_MAIN(TestObjectStore)
#include "object_store_test.moc"
