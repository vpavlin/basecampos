// srcdeps: ObjectStore.cpp ObjectRanker.cpp ObjectResults.cpp ObjectSearch.cpp
//
// Shell objects search (SHELL-OBJECTS-SPEC §2, §5; ADR-072). Covered here: the
// host's fallback ranking (match strength, recency, unseen, the empty query,
// ties), filters, the `search` holder protocol (answers reorder, unknown and
// out-of-filter ids are dropped, the 200 cap, stale requests are ignored), the
// timeout fallback, store changes asking again, and that apps without the
// right role get nothing. Roles are a plain map here; ShellRoles is tested in
// shell_roles_test.cpp. basecamp_search's BasecampSearchRanker.js follows the
// same rules: it runs here in a QJSEngine and must give the same order
// (testJsRankerMatches).

#include "ObjectRanker.h"
#include "ObjectResults.h"
#include "ObjectSearch.h"
#include "ObjectStore.h"

#include <QCoreApplication>
#include <QFile>
#include <QJSEngine>
#include <QJSValue>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <memory>
#include <vector>

namespace {

const QString kWallet = QStringLiteral("wallet_ui");
const QString kChat = QStringLiteral("chat_ui");
const QString kNews = QStringLiteral("news_ui");
const QString kHome = QStringLiteral("basecamp_home");
const QString kLauncher = QStringLiteral("basecamp_apps");
const QString kRanker = QStringLiteral("basecamp_search");
const QString kOther = QStringLiteral("some_app");

constexpr qint64 kNow = 1'700'000'000'000;
constexpr qint64 kHour = 60 * 60 * 1000;

QVariantMap obj(const QString& key, const QString& type, const QString& title,
                qint64 time, const QVariantMap& extra = {})
{
    QVariantMap o{ { QStringLiteral("key"), key },
                   { QStringLiteral("type"), type },
                   { QStringLiteral("title"), title },
                   { QStringLiteral("time"), time } };
    for (auto it = extra.cbegin(); it != extra.cend(); ++it)
        o.insert(it.key(), it.value());
    return o;
}

QVariantMap batch(const QVariantList& objects)
{
    return { { QStringLiteral("objects"), objects } };
}

// An envelope as the store hands it out, for the pure ranker tests.
QVariantMap env(const QString& id, const QString& title, qint64 time, bool seen,
                const QVariantMap& extra = {})
{
    QVariantMap o{ { QStringLiteral("id"), id }, { QStringLiteral("title"), title },
                   { QStringLiteral("time"), time }, { QStringLiteral("seen"), seen } };
    for (auto it = extra.cbegin(); it != extra.cend(); ++it)
        o.insert(it.key(), it.value());
    return o;
}

QVariantList idList(const QStringList& ids)
{
    QVariantList out;
    for (const QString& id : ids) out << id;
    return out;
}

} // namespace

class TestObjectSearch : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        // The build sandbox has no writable home: keep settings in a temp dir.
        QVERIFY(m_settingsDir.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settingsDir.path());
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("ObjectSearchTest"));
        QCoreApplication::setApplicationName(QStringLiteral("ObjectSearchTest"));
    }
    void init()
    {
        QSettings().clear();
        m_roles = { { QStringLiteral("home"), kHome }, { QStringLiteral("launcher"), kLauncher },
                    { QStringLiteral("search"), kRanker } };
    }

    // The ranker
    void testTokens();
    void testTitleBeatsKeywordsSubtitleFields();
    void testEveryTokenMustMatch();
    void testPrefixOfAWordOnly();
    void testRecencyOrdersEqualMatches();
    void testUnseenBoost();
    void testEmptyQuery();
    void testTiesById();
    void testNestedFieldStrings();
    void testCap();
    void testJsRankerMatches();

    // The protocol
    void testNoHolderAnswersAtOnce();
    void testFilters();
    void testHolderAnswerReorders();
    void testUnknownAndOutOfFilterIdsDropped();
    void testAnswerCappedAt200();
    void testStaleRequestIgnored();
    void testTimeoutFallsBack();
    void testStoreChangeAsksAgain();
    void testNonHoldersGetNothing();
    void testViewsActOnlyOnTheirResults();
    void testLosingTheViewRoleEmptiesResults();
    void testHolderChangeMovesRequests();
    void testDroppedResultsFreeTheirPlace();
    void testHostRefreshSaysNothingWhenUnchanged();
    void testCountsAreUncapped();

private:
    std::unique_ptr<ObjectStore> makeStore()
    {
        auto store = std::make_unique<ObjectStore>();
        store->setClock([]() { return kNow; });
        store->setIconResolver([](const QString& app) { return QStringLiteral("file:///icons/") + app; });
        return store;
    }
    std::unique_ptr<ObjectSearch> makeSearch(ObjectStore* store)
    {
        auto search = std::make_unique<ObjectSearch>(
            store, [this](const QString& role) { return m_roles.value(role); });
        search->setClock([]() { return kNow; });
        search->setTimings(/*answer=*/50, /*debounce=*/10);
        return search;
    }
    // Wallet: three transactions; chat: a message and a contact; news: one item.
    void seed(ObjectStore* store)
    {
        QCOMPARE(store->publish(kWallet, batch({
            obj("tx:1", "transaction", "Sent to Ada", kNow - 3 * kHour,
                { { "fields", QVariantMap{ { "direction", "out" }, { "amount", "0.5" }, { "unit", "BTC" } } },
                  { "subjects", QStringList{ "logos:pk:ada" } } }),
            obj("tx:2", "transaction", "Received from Bob", kNow - 2 * kHour,
                { { "fields", QVariantMap{ { "direction", "in" }, { "counterparty", "ada" } } } }),
            obj("tx:3", "transaction", "Weekly summary", kNow - kHour,
                { { "kind", "summary" } }),
        })), QString());
        QCOMPARE(store->publish(kChat, batch({
            obj("m:1", "message", "Ada", kNow - 30 * 60 * 1000,
                { { "fields", QVariantMap{ { "text", "lunch tomorrow?" } } },
                  { "subjects", QStringList{ "logos:pk:ada" } } }),
            obj("c:1", "contact", "Ada Lovelace", kNow - 10 * kHour),
        })), QString());
        QCOMPARE(store->publish(kNews, batch({
            obj("n:1", "news", "Logos testnet relaunch", kNow - 5 * kHour,
                { { "keywords", QStringList{ "network" } } }),
        })), QString());
    }

    QTemporaryDir m_settingsDir;
    QHash<QString, QString> m_roles;
};

// ── The ranker ───────────────────────────────────────────────────────────────

void TestObjectSearch::testTokens()
{
    QCOMPARE(ObjectRanker::tokens(QStringLiteral("  Ada's  tx:0x91… ")),
             (QStringList{ "ada", "s", "tx", "0x91…" }));
    QVERIFY(ObjectRanker::tokens(QStringLiteral(" .,; ")).isEmpty());
}

void TestObjectSearch::testTitleBeatsKeywordsSubtitleFields()
{
    // Older, seen, but a title match: ahead of everything newer and unseen
    // that only matches lower down.
    const QVariantList objects{
        env("f", "Payment", kNow, false, { { "fields", QVariantMap{ { "text", "ada" } } } }),
        env("s", "Payment", kNow, false, { { "subtitle", "To Ada" } }),
        env("k", "Payment", kNow, false, { { "keywords", QStringList{ "ada" } } }),
        env("t", "Ada", kNow - 100 * kHour, true),
    };
    QCOMPARE(ObjectRanker::rank(objects, QStringLiteral("ada"), kNow),
             (QStringList{ "t", "k", "s", "f" }));
    QCOMPARE(ObjectRanker::strength(objects.at(3).toMap(), "ada"), ObjectRanker::kTitleStrength);
    QCOMPARE(ObjectRanker::strength(objects.at(0).toMap(), "bob"), 0);
}

void TestObjectSearch::testEveryTokenMustMatch()
{
    const QVariantList objects{
        env("a", "Ada Lovelace", kNow, true),
        env("b", "Ada", kNow, true),
    };
    QCOMPARE(ObjectRanker::rank(objects, QStringLiteral("ada love"), kNow), QStringList{ "a" });
    QCOMPARE(ObjectRanker::rank(objects, QStringLiteral("ADA"), kNow).size(), 2);   // case-insensitive
}

void TestObjectSearch::testPrefixOfAWordOnly()
{
    const QVariantList objects{ env("a", "Canada trip", kNow, true) };
    QVERIFY(ObjectRanker::rank(objects, QStringLiteral("ada"), kNow).isEmpty());   // not a substring
    QCOMPARE(ObjectRanker::rank(objects, QStringLiteral("tri"), kNow), QStringList{ "a" });
}

void TestObjectSearch::testRecencyOrdersEqualMatches()
{
    const QVariantList objects{
        env("old", "Ada", kNow - 48 * kHour, true),
        env("new", "Ada", kNow - kHour, true),
        env("mid", "Ada", kNow - 24 * kHour, true),
    };
    QCOMPARE(ObjectRanker::rank(objects, QStringLiteral("ada"), kNow),
             (QStringList{ "new", "mid", "old" }));
    // Halves per day.
    QVERIFY(qAbs(ObjectRanker::score(objects.at(2).toMap(), { "ada" }, kNow)
                 - (40.0 + 2.5)) < 1e-9);
}

void TestObjectSearch::testUnseenBoost()
{
    const QVariantList objects{
        env("seen", "Ada", kNow, true),
        env("unseen", "Ada", kNow, false),
    };
    QCOMPARE(ObjectRanker::rank(objects, QStringLiteral("ada"), kNow),
             (QStringList{ "unseen", "seen" }));
    // …but never past a better match.
    const QVariantList two{
        env("unseen-field", "x", kNow, false, { { "fields", QVariantMap{ { "text", "ada" } } } }),
        env("seen-subtitle", "x", kNow - 72 * kHour, true, { { "subtitle", "ada" } }),
    };
    QCOMPARE(ObjectRanker::rank(two, QStringLiteral("ada"), kNow),
             (QStringList{ "seen-subtitle", "unseen-field" }));
}

void TestObjectSearch::testEmptyQuery()
{
    const QVariantList objects{
        env("seen-new", "a", kNow, true),
        env("unseen-old", "b", kNow - 10 * kHour, false),
        env("unseen-new", "c", kNow - kHour, false),
        env("seen-old", "d", kNow - 5 * kHour, true),
    };
    const QStringList expected{ "unseen-new", "unseen-old", "seen-new", "seen-old" };
    QCOMPARE(ObjectRanker::rank(objects, QString(), kNow), expected);
    QCOMPARE(ObjectRanker::rank(objects, QStringLiteral("  "), kNow), expected);
}

void TestObjectSearch::testTiesById()
{
    const QVariantList objects{ env("b", "Ada", kNow, true), env("a", "Ada", kNow, true),
                                env("c", "Ada", kNow, true) };
    QCOMPARE(ObjectRanker::rank(objects, QStringLiteral("ada"), kNow), (QStringList{ "a", "b", "c" }));
    QCOMPARE(ObjectRanker::rank(objects, QString(), kNow), (QStringList{ "a", "b", "c" }));
}

void TestObjectSearch::testNestedFieldStrings()
{
    const QVariantList objects{
        env("acct", "Main", kNow, true,
            { { "fields", QVariantMap{ { "balance", QVariantMap{ { "amount", "1.2" }, { "unit", "ETH" } } } } } }),
        env("num", "Other", kNow, true, { { "fields", QVariantMap{ { "members", 1234 } } } }),
    };
    QCOMPARE(ObjectRanker::rank(objects, QStringLiteral("eth"), kNow),
             ObjectRanker::kMatchContentFields ? QStringList{ "acct" } : QStringList{});
    QVERIFY(ObjectRanker::rank(objects, QStringLiteral("1234"), kNow).isEmpty());   // numbers aren't text
}

void TestObjectSearch::testCap()
{
    QVariantList objects;
    for (int i = 0; i < 250; ++i)
        objects << env(QStringLiteral("id%1").arg(i, 3, 10, QLatin1Char('0')), "Ada", kNow, true);
    QCOMPARE(ObjectRanker::rank(objects, QStringLiteral("ada"), kNow).size(), ObjectRanker::kMaxResults);
}

// The same envelopes and clock through both rankers: the same ids, in the same
// order, for every query (a change to one and not the other fails here).
void TestObjectSearch::testJsRankerMatches()
{
    QFile f(QFINDTESTDATA("../apps/basecamp_search/qml/BasecampSearchRanker.js"));
    QVERIFY(f.open(QIODevice::ReadOnly));
    QString code = QString::fromUtf8(f.readAll());
    code.replace(QStringLiteral(".pragma library"), QString());
    QJSEngine js;
    const QJSValue loaded = js.evaluate(code, f.fileName());
    QVERIFY2(!loaded.isError(), qPrintable(loaded.toString()));

    QVariantList objects{
        env("t1", "Ada Lovelace", kNow - 3 * kHour, false),
        env("t2", "Ada", kNow - 3 * kHour, false),            // ties t1 on "ada": by id
        env("t3", "Payment to Ada", kNow - 30 * kHour, true,
            { { "subtitle", "Weekly rent" }, { "keywords", QStringList{ "rent", "bills" } } }),
        env("t4", "Lunch", kNow - kHour, false,
            { { "fields", QVariantMap{ { "text", "with ada tomorrow?" }, { "count", 3 } } } }),
        env("t5", "Main account", kNow - 100 * kHour, true,
            { { "fields", QVariantMap{ { "balance", QVariantMap{ { "amount", "1.2" }, { "unit", "ETH" } } },
                                       { "tags", QVariantList{ "savings", "cold" } } } } }),
        env("t6", "Canada trip", kNow - 2 * kHour, false, { { "subtitle", "ada's idea" } }),
        env("t7", "Logos testnet relaunch", kNow - 5 * kHour, true,
            { { "keywords", QStringList{ "network" } } }),
        env("t8", "tx:0x91 sent", kNow - 7 * kHour, false),
    };
    // One with `updated` and no `time`, and one with the same time as t7.
    QVariantMap updatedOnly{ { "id", "t9" }, { "title", "Ada updated" },
                             { "updated", kNow - 4 * kHour }, { "seen", true } };
    objects << updatedOnly << env("t0", "Relaunch notes", kNow - 5 * kHour, true);

    const QStringList queries{ QString(), QStringLiteral("  "), QStringLiteral("ada"),
                               QStringLiteral("ADA love"), QStringLiteral("re"),
                               QStringLiteral("eth"), QStringLiteral("cold savings"),
                               QStringLiteral("tx:0x91"), QStringLiteral("ada's"),
                               QStringLiteral("nothing-matches"), QStringLiteral("l") };
    const QJSValue rank = js.globalObject().property(QStringLiteral("rank"));
    QVERIFY(rank.isCallable());
    for (const QString& q : queries) {
        const QJSValue r = rank.call({ js.toScriptValue(objects), QJSValue(q), QJSValue(double(kNow)) });
        QVERIFY2(!r.isError(), qPrintable(r.toString()));
        QCOMPARE(r.toVariant().toStringList(), ObjectRanker::rank(objects, q, kNow));
    }
    // And the cap.
    QVariantList many;
    for (int i = 0; i < 250; ++i)
        many << env(QStringLiteral("id%1").arg(i, 3, 10, QLatin1Char('0')), "Ada", kNow - i * kHour, i % 3 == 0);
    const QJSValue r = rank.call({ js.toScriptValue(many), QJSValue(QStringLiteral("ada")), QJSValue(double(kNow)) });
    QCOMPARE(r.toVariant().toStringList(), ObjectRanker::rank(many, QStringLiteral("ada"), kNow));
    QCOMPARE(js.globalObject().property(QStringLiteral("kMatchContentFields")).toBool(),
             ObjectRanker::kMatchContentFields);
}

// ── The protocol ─────────────────────────────────────────────────────────────

void TestObjectSearch::testNoHolderAnswersAtOnce()
{
    m_roles.remove(QStringLiteral("search"));
    auto store = makeStore();
    seed(store.get());
    auto search = makeSearch(store.get());
    QSignalSpy asked(search.get(), &ObjectSearch::searchRequested);

    std::unique_ptr<ObjectResults> results(search->search(kHome, QStringLiteral("ada")));
    QVERIFY(results);
    QCOMPARE(asked.count(), 0);
    QVERIFY(!results->pending());
    QCOMPARE(results->ranker(), ObjectSearch::fallbackRanker());
    QCOMPARE(search->lastRanker(), ObjectSearch::fallbackRanker());
    // Title matches first (newest first among them), then the field match.
    const QString m1 = ObjectStore::idFor(kChat, "m:1"), c1 = ObjectStore::idFor(kChat, "c:1");
    const QString tx1 = ObjectStore::idFor(kWallet, "tx:1"), tx2 = ObjectStore::idFor(kWallet, "tx:2");
    QCOMPARE(results->ids(), (QStringList{ m1, tx1, c1, tx2 }));
    QCOMPARE(results->ids(), search->rankWithFallback(QStringLiteral("ada"), {}));
    QCOMPARE(results->count(), 4);
    QCOMPARE(results->rowCount(), 4);

    // The model carries the envelope, icon served by the host for a role app.
    const QModelIndex first = results->index(0);
    QCOMPARE(first.data(ObjectResults::TitleRole).toString(), QStringLiteral("Ada"));
    QCOMPARE(first.data(ObjectResults::AppRole).toString(), kChat);
    QCOMPARE(first.data(ObjectResults::TypeRole).toString(), QStringLiteral("message"));
    QCOMPARE(first.data(ObjectResults::IconRole).toString(), QStringLiteral("image://basecamp/appicon/chat_ui"));
    QCOMPARE(first.data(ObjectResults::SeenRole).toBool(), false);
    QCOMPARE(first.data(ObjectResults::FieldsRole).toMap().value("text").toString(),
             QStringLiteral("lunch tomorrow?"));
    QVERIFY(!first.data(ObjectResults::ProgressRole).isValid());
    QVERIFY(results->roleNames().values().contains("subtitle"));
}

void TestObjectSearch::testFilters()
{
    m_roles.remove(QStringLiteral("search"));
    auto store = makeStore();
    seed(store.get());
    auto search = makeSearch(store.get());
    auto ids = [&](const QVariantMap& filter) {
        std::unique_ptr<ObjectResults> r(search->search(kHome, QString(), filter));
        QStringList out = r->ids();
        out.sort();
        return out;
    };
    QCOMPARE(ids({ { "types", QStringList{ "transaction" } } }).size(), 3);
    QStringList messageAndNews{ ObjectStore::idFor(kChat, "m:1"), ObjectStore::idFor(kNews, "n:1") };
    messageAndNews.sort();
    QCOMPARE(ids({ { "types", QStringList{ "message", "news" } } }), messageAndNews);
    QCOMPARE(ids({ { "apps", QStringList{ kChat } } }).size(), 2);
    QCOMPARE(ids({ { "kinds", QStringList{ "summary" } } }),
             QStringList{ ObjectStore::idFor(kWallet, "tx:3") });
    QStringList ada{ ObjectStore::idFor(kWallet, "tx:1"), ObjectStore::idFor(kChat, "m:1") };
    ada.sort();
    QCOMPARE(ids({ { "subjects", QStringList{ "logos:pk:ada" } } }), ada);

    store->markSeen({ ObjectStore::idFor(kWallet, "tx:1"), ObjectStore::idFor(kWallet, "tx:2") });
    QCOMPARE(ids({ { "unseenOnly", true }, { "apps", QStringList{ kWallet } } }),
             QStringList{ ObjectStore::idFor(kWallet, "tx:3") });

    // Only the spec's keys pass: a view can't pick objects by id or key.
    QCOMPARE(ids({ { "ids", QStringList{ ObjectStore::idFor(kNews, "n:1") } } }).size(), 6);
    QCOMPARE(ObjectSearch::sanitizeFilter({ { "keys", QStringList{ "x" } }, { "types", "news" },
                                            { "unseenOnly", false } }),
             (QVariantMap{ { "types", QStringList{ "news" } } }));
}

void TestObjectSearch::testHolderAnswerReorders()
{
    auto store = makeStore();
    seed(store.get());
    auto search = makeSearch(store.get());
    QSignalSpy asked(search.get(), &ObjectSearch::searchRequested);

    std::unique_ptr<ObjectResults> results(
        search->search(kHome, QStringLiteral("ada"), { { "apps", QStringList{ kWallet, kChat } } }));
    QCOMPARE(asked.count(), 1);
    QCOMPARE(asked.at(0).at(0).toString(), kRanker);   // to the holder, by name
    const QString requestId = asked.at(0).at(1).toString();
    QCOMPARE(asked.at(0).at(2).toString(), QStringLiteral("ada"));
    QCOMPARE(asked.at(0).at(3).toMap().value("apps").toStringList(), (QStringList{ kWallet, kChat }));
    QVERIFY(results->pending());
    QCOMPARE(results->count(), 0);

    // The holder reads what the filter allows: no news.
    const QVariantList visible = search->objects(kRanker, asked.at(0).at(3).toMap());
    QCOMPARE(visible.size(), 5);

    // Its own order wins, even one the fallback would never give.
    const QStringList mine{ ObjectStore::idFor(kChat, "c:1"), ObjectStore::idFor(kWallet, "tx:3"),
                            ObjectStore::idFor(kChat, "m:1") };
    QVERIFY(search->answer(kRanker, requestId, idList(mine)));
    QCOMPARE(results->ids(), mine);
    QCOMPARE(results->ranker(), kRanker);
    QCOMPARE(search->lastRanker(), kRanker);
    QVERIFY(!results->pending());

    // No fallback afterwards.
    QTest::qWait(120);
    QCOMPARE(results->ranker(), kRanker);
}

void TestObjectSearch::testUnknownAndOutOfFilterIdsDropped()
{
    auto store = makeStore();
    seed(store.get());
    auto search = makeSearch(store.get());
    QSignalSpy asked(search.get(), &ObjectSearch::searchRequested);
    std::unique_ptr<ObjectResults> results(
        search->search(kHome, QString(), { { "types", QStringList{ "transaction" } } }));
    const QString tx1 = ObjectStore::idFor(kWallet, "tx:1");
    QVERIFY(search->answer(kRanker, asked.at(0).at(1).toString(), QVariantList{
        "nope", tx1, ObjectStore::idFor(kNews, "n:1") /* outside the filter */,
        tx1 /* repeated */, 42 /* not a string */ }));
    QCOMPARE(results->ids(), QStringList{ tx1 });
}

void TestObjectSearch::testAnswerCappedAt200()
{
    auto store = makeStore();
    // 250 objects across apps (the store keeps 500 per app; one batch each).
    QVariantList a, b;
    for (int i = 0; i < 125; ++i) {
        a << obj(QStringLiteral("a%1").arg(i), "news", QStringLiteral("Item %1").arg(i), kNow - i);
        b << obj(QStringLiteral("b%1").arg(i), "news", QStringLiteral("Item %1").arg(i), kNow - i);
    }
    QCOMPARE(store->publish(kNews, batch(a)), QString());
    QCOMPARE(store->publish(kChat, batch(b)), QString());
    auto search = makeSearch(store.get());
    QSignalSpy asked(search.get(), &ObjectSearch::searchRequested);
    std::unique_ptr<ObjectResults> results(search->search(kHome, QString()));
    QStringList all;
    for (const QVariant& v : search->objects(kRanker, {}))
        all << v.toMap().value("id").toString();
    QCOMPARE(all.size(), 250);
    QVERIFY(search->answer(kRanker, asked.at(0).at(1).toString(), idList(all)));
    QCOMPARE(results->count(), ObjectSearch::kMaxAnswer);
    QCOMPARE(results->ids(), all.mid(0, ObjectSearch::kMaxAnswer));
}

void TestObjectSearch::testStaleRequestIgnored()
{
    auto store = makeStore();
    seed(store.get());
    auto search = makeSearch(store.get());
    QSignalSpy asked(search.get(), &ObjectSearch::searchRequested);
    std::unique_ptr<ObjectResults> results(search->search(kHome, QStringLiteral("ad")));
    const QString first = asked.at(0).at(1).toString();
    results->update(QStringLiteral("ada"));
    QCOMPARE(asked.count(), 2);
    const QString second = asked.at(1).at(1).toString();
    QVERIFY(first != second);

    const QString c1 = ObjectStore::idFor(kChat, "c:1");
    QVERIFY(!search->answer(kRanker, first, idList({ c1 })));        // superseded
    QCOMPARE(results->count(), 0);
    QVERIFY(search->answer(kRanker, second, idList({ c1 })));
    QCOMPARE(results->ids(), QStringList{ c1 });
    QVERIFY(!search->answer(kRanker, second, idList({})));           // answered already
    QCOMPARE(results->ids(), QStringList{ c1 });
    QVERIFY(!search->answer(kRanker, QStringLiteral("search-999"), idList({ c1 })));   // unknown
}

void TestObjectSearch::testTimeoutFallsBack()
{
    auto store = makeStore();
    seed(store.get());
    auto search = makeSearch(store.get());
    QSignalSpy asked(search.get(), &ObjectSearch::searchRequested);
    std::unique_ptr<ObjectResults> results(search->search(kHome, QStringLiteral("ada")));
    QCOMPARE(asked.count(), 1);
    QVERIFY(results->pending());

    QTRY_COMPARE_WITH_TIMEOUT(results->ranker(), ObjectSearch::fallbackRanker(), 2000);
    QVERIFY(!results->pending());
    QCOMPARE(results->ids(), search->rankWithFallback(QStringLiteral("ada"), {}));
    QCOMPARE(search->lastRanker(), ObjectSearch::fallbackRanker());
    // The late answer is stale: the request was closed by the fallback.
    QVERIFY(!search->answer(kRanker, asked.at(0).at(1).toString(),
                            idList({ ObjectStore::idFor(kChat, "c:1") })));
    QCOMPARE(results->ids(), search->rankWithFallback(QStringLiteral("ada"), {}));
}

void TestObjectSearch::testStoreChangeAsksAgain()
{
    auto store = makeStore();
    seed(store.get());
    auto search = makeSearch(store.get());
    QSignalSpy asked(search.get(), &ObjectSearch::searchRequested);
    QSignalSpy changed(search.get(), &ObjectSearch::objectsChanged);
    std::unique_ptr<ObjectResults> results(search->search(kHome, QStringLiteral("ada")));
    QVERIFY(search->answer(kRanker, asked.at(0).at(1).toString(), idList({})));

    // Two publishes inside the debounce: one new request.
    QCOMPARE(store->publish(kChat, batch({ obj("m:2", "message", "Ada again", kNow) })), QString());
    QCOMPARE(store->publish(kChat, batch({ obj("m:3", "message", "Ada thrice", kNow) })), QString());
    QCOMPARE(changed.count(), 2);
    QCOMPARE(changed.at(0).at(0).toString(), kChat);
    QTRY_COMPARE_WITH_TIMEOUT(asked.count(), 2, 1000);
    QTest::qWait(50);
    QCOMPARE(asked.count(), 2);
    QCOMPARE(asked.at(1).at(2).toString(), QStringLiteral("ada"));
}

void TestObjectSearch::testNonHoldersGetNothing()
{
    auto store = makeStore();
    seed(store.get());
    auto search = makeSearch(store.get());
    QSignalSpy asked(search.get(), &ObjectSearch::searchRequested);

    // Only views may search.
    QVERIFY(!search->search(kOther, QStringLiteral("ada")));
    QVERIFY(!search->search(kRanker, QStringLiteral("ada")));
    QVERIFY(!search->search(QString(), QStringLiteral("ada")));
    QCOMPARE(asked.count(), 0);
    QVERIFY(search->isView(ObjectSearch::shellView()));

    std::unique_ptr<ObjectResults> results(search->search(kLauncher, QStringLiteral("ada")));
    QVERIFY(results);
    const QString requestId = asked.at(0).at(1).toString();
    // Only the holder reads objects or answers; a view is not a holder.
    QVERIFY(search->objects(kOther, {}).isEmpty());
    QVERIFY(search->objects(kHome, {}).isEmpty());
    QVERIFY(search->objects(QString(), {}).isEmpty());
    QCOMPARE(search->objects(kRanker, {}).size(), 6);
    const QString c1 = ObjectStore::idFor(kChat, "c:1");
    QVERIFY(!search->answer(kOther, requestId, idList({ c1 })));
    QVERIFY(!search->answer(kHome, requestId, idList({ c1 })));
    QCOMPARE(results->count(), 0);
    QVERIFY(search->answer(kRanker, requestId, idList({ c1 })));

    // With nobody holding search, nobody reads across apps.
    m_roles.remove(QStringLiteral("search"));
    QVERIFY(search->objects(kRanker, {}).isEmpty());
    QVERIFY(search->objects(QString(), {}).isEmpty());
}

void TestObjectSearch::testViewsActOnlyOnTheirResults()
{
    m_roles.remove(QStringLiteral("search"));
    auto store = makeStore();
    seed(store.get());
    auto search = makeSearch(store.get());
    std::unique_ptr<ObjectResults> home(
        search->search(kHome, QString(), { { "apps", QStringList{ kChat } } }));
    const QString m1 = ObjectStore::idFor(kChat, "m:1"), n1 = ObjectStore::idFor(kNews, "n:1");
    QVERIFY(search->inResultsOf(kHome, m1));
    QVERIFY(!search->inResultsOf(kHome, n1));
    QVERIFY(!search->inResultsOf(kLauncher, m1));

    QCOMPARE(search->markSeen(kHome, { m1, n1, m1 }), QStringList{ m1 });
    QVERIFY(store->get(m1).value("seen").toBool());
    QVERIFY(!store->get(n1).value("seen").toBool());
    QVERIFY(search->markSeen(kLauncher, { m1 }).isEmpty());
    QVERIFY(search->markSeen(kOther, { n1 }).isEmpty());
    QVERIFY(!store->get(n1).value("seen").toBool());
    // The row updates after the store change re-asks.
    QTRY_VERIFY_WITH_TIMEOUT(home->index(home->ids().indexOf(m1)).data(ObjectResults::SeenRole).toBool(), 1000);
}

void TestObjectSearch::testLosingTheViewRoleEmptiesResults()
{
    m_roles.remove(QStringLiteral("search"));
    auto store = makeStore();
    seed(store.get());
    auto search = makeSearch(store.get());
    std::unique_ptr<ObjectResults> home(search->search(kHome, QString()));
    QCOMPARE(home->count(), 6);

    m_roles[QStringLiteral("home")] = kOther;
    search->rolesChanged();
    QCOMPARE(home->count(), 0);
    QVERIFY(!home->attached());
    QVERIFY(!search->inResultsOf(kHome, ObjectStore::idFor(kChat, "m:1")));
    // Detached: a store change refills nothing.
    QCOMPARE(store->publish(kChat, batch({ obj("m:2", "message", "Ada again", kNow) })), QString());
    QTest::qWait(60);
    QCOMPARE(home->count(), 0);
}

void TestObjectSearch::testHolderChangeMovesRequests()
{
    auto store = makeStore();
    seed(store.get());
    auto search = makeSearch(store.get());
    QSignalSpy asked(search.get(), &ObjectSearch::searchRequested);
    std::unique_ptr<ObjectResults> results(search->search(kHome, QStringLiteral("ada")));
    const QString first = asked.at(0).at(1).toString();

    // Another app takes the role: the open request goes to it, and the old
    // holder's answer is refused.
    m_roles[QStringLiteral("search")] = kOther;
    search->rolesChanged();
    QCOMPARE(asked.count(), 2);
    QCOMPARE(asked.at(1).at(0).toString(), kOther);
    const QString c1 = ObjectStore::idFor(kChat, "c:1");
    QVERIFY(!search->answer(kRanker, first, idList({ c1 })));
    QVERIFY(!search->answer(kRanker, asked.at(1).at(1).toString(), idList({ c1 })));
    QVERIFY(search->answer(kOther, asked.at(1).at(1).toString(), idList({ c1 })));
    QCOMPARE(results->ranker(), kOther);

    // Nobody: the host answers at once.
    m_roles.remove(QStringLiteral("search"));
    search->rolesChanged();
    QCOMPARE(asked.count(), 2);
    QCOMPARE(results->ranker(), ObjectSearch::fallbackRanker());
}

// Home rebuilds its module delegates on every module edit and frees their
// results; one it still shows (the feed's) must stay live. Results not freed
// yet (waiting for the GC) still fit under the cap with home's own.
void TestObjectSearch::testDroppedResultsFreeTheirPlace()
{
    m_roles.remove(QStringLiteral("search"));
    auto store = makeStore();
    seed(store.get());
    auto search = makeSearch(store.get());
    std::unique_ptr<ObjectResults> feed(search->search(kHome, QString()));
    for (int round = 0; round < 5; ++round) {
        std::vector<std::unique_ptr<ObjectResults>> modules;
        for (int i = 0; i < 7; ++i)
            modules.emplace_back(search->search(kHome, QString(), { { "types", QStringList{ "message" } } }));
    }   // freed, as the delegates' onDestruction does
    QVERIFY(feed->attached());

    // Seven dropped but not yet collected, seven new ones: still under the cap.
    std::vector<std::unique_ptr<ObjectResults>> lingering, current;
    for (int i = 0; i < 7; ++i) lingering.emplace_back(search->search(kHome, QString()));
    for (int i = 0; i < 7; ++i) current.emplace_back(search->search(kHome, QString()));
    QVERIFY(feed->attached());
    QVERIFY(search->inResultsOf(kHome, ObjectStore::idFor(kChat, "m:1")));

    // Past the cap the oldest stops following the store.
    std::vector<std::unique_ptr<ObjectResults>> more;
    for (int i = 0; i < ObjectSearch::kMaxLivePerView; ++i)
        more.emplace_back(search->search(kHome, QString()));
    QVERIFY(!feed->attached());
}

// A store change re-asks only the results that could include the app that
// changed, and a refresh that comes back the same tells the view nothing: a
// view must not learn when apps outside its filter publish.
void TestObjectSearch::testHostRefreshSaysNothingWhenUnchanged()
{
    auto store = makeStore();
    seed(store.get());
    auto search = makeSearch(store.get());
    QSignalSpy asked(search.get(), &ObjectSearch::searchRequested);
    std::unique_ptr<ObjectResults> wallet(
        search->search(kLauncher, QString(), { { "apps", QStringList{ kWallet } } }));
    std::unique_ptr<ObjectResults> txs(
        search->search(kLauncher, QString(), { { "types", QStringList{ "transaction" } } }));
    QCOMPARE(asked.count(), 2);
    const QStringList walletIds = search->rankWithFallback(QString(), wallet->filter());
    QVERIFY(search->answer(kRanker, asked.at(0).at(1).toString(), idList(walletIds)));
    QVERIFY(search->answer(kRanker, asked.at(1).at(1).toString(), idList(walletIds)));
    QVERIFY(!wallet->pending());

    QSignalSpy walletPending(wallet.get(), &ObjectResults::pendingChanged);
    QSignalSpy walletResults(wallet.get(), &ObjectResults::resultsChanged);
    QSignalSpy txsPending(txs.get(), &ObjectResults::pendingChanged);
    QSignalSpy txsResults(txs.get(), &ObjectResults::resultsChanged);

    // The chat app publishes: only `txs` (no app filter) is asked again.
    QCOMPARE(store->publish(kChat, batch({ obj("m:2", "message", "Ada again", kNow) })), QString());
    QTRY_COMPARE_WITH_TIMEOUT(asked.count(), 3, 1000);
    QCOMPARE(asked.at(2).at(3).toMap().value("types").toStringList(), QStringList{ "transaction" });
    QVERIFY(!txs->pending());   // the host's refresh, not the view's
    QVERIFY(search->answer(kRanker, asked.at(2).at(1).toString(), idList(walletIds)));
    QTest::qWait(30);
    QCOMPARE(asked.count(), 3);
    QCOMPARE(txsPending.count(), 0);
    QCOMPARE(txsResults.count(), 0);   // same rows: nothing to say
    // Again: still silent.
    QCOMPARE(store->publish(kChat, batch({ obj("m:3", "message", "Ada thrice", kNow) })), QString());
    QTRY_COMPARE_WITH_TIMEOUT(asked.count(), 4, 1000);
    QVERIFY(search->answer(kRanker, asked.at(3).at(1).toString(), idList(walletIds)));
    QCOMPARE(txsResults.count(), 0);
    QCOMPARE(walletPending.count(), 0);
    QCOMPARE(walletResults.count(), 0);

    // A change the view can see is told; so is an answer to the view's own ask.
    QCOMPARE(store->publish(kWallet, batch({ obj("tx:4", "transaction", "Sent to Bob", kNow) })), QString());
    QTRY_COMPARE_WITH_TIMEOUT(walletResults.count(), 1, 2000);
    QVERIFY(wallet->ids().contains(ObjectStore::idFor(kWallet, "tx:4")));
    QCOMPARE(walletPending.count(), 0);
    m_roles.remove(QStringLiteral("search"));
    const int before = int(txsResults.count());
    txs->update(QString(), txs->filter());
    QCOMPARE(txsResults.count(), before + 1);
}

void TestObjectSearch::testCountsAreUncapped()
{
    m_roles.remove(QStringLiteral("search"));
    auto store = makeStore();
    QVariantList txs, msgs;
    for (int i = 0; i < 300; ++i)
        txs << obj(QStringLiteral("tx%1").arg(i), "transaction", QStringLiteral("Payment %1").arg(i), kNow - i);
    for (int i = 0; i < 5; ++i)
        msgs << obj(QStringLiteral("m%1").arg(i), "message", QStringLiteral("Ada %1").arg(i), kNow - 1000 - i);
    QCOMPARE(store->publish(kWallet, batch(txs)), QString());
    QCOMPARE(store->publish(kChat, batch(msgs)), QString());
    auto search = makeSearch(store.get());

    std::unique_ptr<ObjectResults> all(search->search(kHome, QString()));
    QCOMPARE(all->count(), ObjectSearch::kMaxAnswer);   // the model stops at 200…
    const QVariantMap counts = search->counts(kHome, QString());
    QCOMPARE(counts.value("total").toInt(), 305);       // …the counts do not
    const QVariantList types = counts.value("types").toList();
    QCOMPARE(types.size(), 2);
    QCOMPARE(types.at(0).toMap().value("type").toString(), QStringLiteral("transaction"));
    QCOMPARE(types.at(0).toMap().value("count").toInt(), 300);
    QCOMPARE(types.at(1).toMap().value("count").toInt(), 5);
    QCOMPARE(counts.value("apps").toList().at(1).toMap().value("app").toString(), kChat);

    // With a query, only what matches (the ranker's matching).
    const QVariantMap ada = search->counts(kHome, QStringLiteral("ada"));
    QCOMPARE(ada.value("total").toInt(), 5);
    QCOMPARE(ada.value("types").toList().size(), 1);
    // Views only.
    QVERIFY(search->counts(kOther, QString()).isEmpty());
    QVERIFY(search->counts(kRanker, QString()).isEmpty());
}

QTEST_GUILESS_MAIN(TestObjectSearch)
#include "object_search_test.moc"
