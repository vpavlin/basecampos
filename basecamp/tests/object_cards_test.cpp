// The object cards' and home's pure JS (SHELL-OBJECTS-SPEC §3, §4, §7):
// src/Basecamp/Objects/ObjectFormat.js (what a card writes for a type, a
// time, an amount, a status, which actions it draws) and
// apps/basecamp_home/qml/BasecampHomeModules.js (modules as saved filters,
// pills, the feed's order and "N new", rows that keep their places, the grid
// layout). Each file runs in a bare QJSEngine, as QML would run a
// `.pragma library` script; the QML around them is checked headlessly.

#include <QFile>
#include <QJSEngine>
#include <QJSValue>
#include <QTest>

namespace {

constexpr qint64 kMinute = 60 * 1000;
constexpr qint64 kHour = 60 * kMinute;
constexpr qint64 kDay = 24 * kHour;

// Loads a `.pragma library` script: its functions become the engine's globals.
bool load(QJSEngine& engine, const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    QString code = QString::fromUtf8(f.readAll());
    code.replace(QStringLiteral(".pragma library"), QString());
    const QJSValue r = engine.evaluate(code, path);
    if (r.isError()) {
        qWarning().noquote() << path << r.toString();
        return false;
    }
    return true;
}

} // namespace

class ObjectCardsTest : public QObject {
    Q_OBJECT

    QJSEngine m_format;
    QJSEngine m_home;

    QJSValue f(const QString& expr) { return m_format.evaluate(expr); }
    QJSValue h(const QString& expr) { return m_home.evaluate(expr); }
    QString fs(const QString& expr) { return f(expr).toString(); }
    QString hs(const QString& expr) { return h(expr).toString(); }

private slots:
    void initTestCase()
    {
        QVERIFY(load(m_format, QFINDTESTDATA("../src/Basecamp/Objects/ObjectFormat.js")));
        QVERIFY(load(m_home, QFINDTESTDATA("../apps/basecamp_home/qml/BasecampHomeModules.js")));
    }

    // ── ObjectFormat.js ─────────────────────────────────────────────────

    void typeLabels()
    {
        QCOMPARE(fs("typeLabel('transaction', true)"), QStringLiteral("Transactions"));
        QCOMPARE(fs("typeLabel('news', true)"), QStringLiteral("News"));
        QCOMPARE(fs("typeLabel('community', false)"), QStringLiteral("Community"));
        // An app's own type reads as its last part.
        QCOMPARE(fs("typeLabel('x.objects_chat.conversation', true)"), QStringLiteral("Conversations"));
        QCOMPARE(fs("typeLabel('x.app.group_chat', false)"), QStringLiteral("Group chat"));
        QCOMPARE(fs("typeLabel('', true)"), QStringLiteral("Other"));
        QCOMPARE(fs("typeLabel({}, false)"), QStringLiteral("Item"));
    }

    void cardForType()
    {
        QCOMPARE(fs("cardFor('message')"), QStringLiteral("message"));
        QCOMPARE(fs("cardFor('x.objects_chat.conversation')"), QStringLiteral("generic"));
        QCOMPARE(fs("cardFor(42)"), QStringLiteral("generic"));
    }

    // Untrusted values never print as "[object Object]" or "undefined".
    void strOfAnything()
    {
        QCOMPARE(fs("str('a')"), QStringLiteral("a"));
        QCOMPARE(fs("str(12.5)"), QStringLiteral("12.5"));
        QCOMPARE(fs("str({ type: 'x', key: 'y' })"), QString());
        QCOMPARE(fs("str([1, 2])"), QString());
        QCOMPARE(fs("str(undefined)"), QString());
        QCOMPARE(fs("str(NaN)"), QString());
        QCOMPARE(fs("list(['friends', 3, {}, 'work'])"), QStringLiteral("friends, 3, work"));
    }

    void relativeTimes()
    {
        // Noon on a Wednesday, local time.
        const qint64 now = f("new Date(2026, 9, 21, 12, 0, 0).getTime()").toVariant().toLongLong();
        auto rel = [&](qint64 t) {
            return fs(QStringLiteral("relativeTime(%1, %2)").arg(t).arg(now));
        };
        QCOMPARE(rel(now - 10 * 1000), QStringLiteral("now"));
        QCOMPARE(rel(now + 5 * kMinute), QStringLiteral("now"));
        QCOMPARE(rel(now - 5 * kMinute), QStringLiteral("5 min"));
        QCOMPARE(rel(now - 3 * kHour), QStringLiteral("3 h"));
        QCOMPARE(rel(now - 26 * kHour), QStringLiteral("Yesterday"));
        QCOMPARE(rel(now - 3 * kDay), QStringLiteral("Sun"));
        QCOMPARE(rel(now - 9 * kDay), QStringLiteral("12 Oct"));
        QCOMPARE(fs(QStringLiteral("relativeTime(new Date(2025, 2, 4).getTime(), %1)").arg(now)),
                 QStringLiteral("4 Mar 2025"));
        QCOMPARE(fs(QStringLiteral("relativeTime('soon', %1)").arg(now)), QString());
        QCOMPARE(fs("clock(new Date(2026, 9, 21, 9, 5).getTime())"), QStringLiteral("09:05"));
    }

    void amounts()
    {
        QCOMPARE(fs("grouped(1240)"), QStringLiteral("1,240"));
        QCOMPARE(fs("grouped(-1234567)"), QStringLiteral("-1,234,567"));
        QCOMPARE(fs("amount('12.00', 'TST')"), QStringLiteral("12.00 TST"));
        QCOMPARE(fs("amount(undefined, 'TST')"), QString());
        QCOMPARE(fs("signedAmount({ amount: '40.00', unit: 'TST', direction: 'in' })"),
                 QStringLiteral("+40.00 TST"));
        QCOMPARE(fs("signedAmount({ amount: '12.00', unit: 'TST', direction: 'out' })"),
                 QStringLiteral("−12.00 TST"));
        QCOMPARE(fs("signedAmount({ unit: 'TST' })"), QString());
        QCOMPARE(fs("change(1.84)"), QStringLiteral("+1.8%"));
        QCOMPARE(fs("change(-2.4)"), QStringLiteral("−2.4%"));
        QCOMPARE(fs("change('up')"), QString());
        QCOMPARE(fs("members(1)"), QStringLiteral("1 member"));
        QCOMPARE(fs("members(1240)"), QStringLiteral("1,240 members"));
    }

    void statuses()
    {
        QCOMPARE(fs("statusLabel('done')"), QStringLiteral("Done"));
        QCOMPARE(fs("statusTone('done')"), QStringLiteral("success"));
        QCOMPARE(fs("statusTone('failed')"), QStringLiteral("error"));
        QCOMPARE(fs("statusTone('sending')"), QStringLiteral("warning"));
        QCOMPARE(fs("statusLabel('<b>hacked</b>')"), QString());
        QCOMPARE(fs("statusTone('whatever')"), QString());
    }

    // At most 3, `open` left to the card itself, an id required, `input` only
    // when it is "text", the label falling back to the id.
    void actions()
    {
        const QJSValue a = f(
            "actions({ actions: [ { id: 'open', label: 'Open' }, { label: 'no id' },"
            " { id: 'reply', label: 'Reply', input: 'text' }, { id: 'vote', input: 'number' },"
            " { id: 'like', label: 'Like' }, { id: 'share', label: 'Share' } ] })");
        QCOMPARE(a.property("length").toInt(), 3);
        QCOMPARE(a.property(0).property("id").toString(), QStringLiteral("reply"));
        QCOMPARE(a.property(0).property("input").toString(), QStringLiteral("text"));
        QCOMPARE(a.property(1).property("label").toString(), QStringLiteral("vote"));
        QCOMPARE(a.property(1).property("input").toString(), QString());
        QCOMPARE(a.property(2).property("id").toString(), QStringLiteral("like"));
        QCOMPARE(f("actions({ actions: 'join' }).length").toInt(), 0);
        // A list that came through C++ is array-like, not an Array.
        QCOMPARE(fs("actions({ actions: { 0: { id: 'vote', label: 'Vote' }, length: 1 } })[0].label"),
                 QStringLiteral("Vote"));
        QCOMPARE(fs("list({ 0: 'friends', 1: 'work', length: 2 })"), QStringLiteral("friends, work"));
        QCOMPARE(f("actions(null).length").toInt(), 0);
        // The card's own View steps aside for the app's.
        QVERIFY(f("offersView(actions({ actions: [ { id: 'view', label: 'Details' } ] }))").toBool());
        QVERIFY(f("offersView(actions({ actions: [ { id: 'show', label: 'View' } ] }))").toBool());
        QVERIFY(!f("offersView(actions({ actions: [ { id: 'join', label: 'Join' } ] }))").toBool());
    }

    void timeOf()
    {
        QCOMPARE(f("timeOf({ time: 5, updated: 9 })").toInt(), 5);
        QCOMPARE(f("timeOf({ updated: 9 })").toInt(), 9);
        QCOMPARE(f("timeOf({ time: 'now' })").toInt(), 0);
    }

    // ── BasecampHomeModules.js ──────────────────────────────────────────

    void defaultModules()
    {
        QCOMPARE(hs("defaultModules().map(m => m.name).join(',')"),
                 QStringLiteral("Transactions,Messages,Communities,Wallet,News"));
        // The defaults leave room for the pills' results within the 8 live
        // results a view keeps.
        QVERIFY(h("defaultModules().length < kMaxModules").toBool());
        QCOMPARE(hs("defaultModules().map(m => sanitizeModule(m).size).join(',')"),
                 QStringLiteral("large,medium,medium,small,row"));
    }

    void sanitize()
    {
        const QJSValue m = h("sanitizeModule({ id: 'a', name: '  Mine  ', size: 'huge', pinned: 'yes',"
                             " filter: { types: ['message', 3, 'message'], apps: 'x', evil: 1 },"
                             " extra: 1 })");
        QCOMPARE(m.property("name").toString(), QStringLiteral("Mine"));
        QCOMPARE(m.property("size").toString(), QStringLiteral("medium"));
        QCOMPARE(m.property("pinned").toBool(), false);
        QCOMPARE(m.property("filter").property("types").property("length").toInt(), 1);
        QVERIFY(m.property("filter").property("apps").isUndefined());
        QVERIFY(m.property("extra").isUndefined());
        // A module that came back through a model: its lists are array-like.
        QCOMPARE(hs("JSON.stringify(effectiveFilter({ types: { 0: 'transaction', length: 1 } }, null))"),
                 QStringLiteral("{\"types\":[\"transaction\"]}"));
    }

    void orderAndEdit()
    {
        h("var mods = defaultModules().map(sanitizeModule)");
        h("mods = updateModule(mods, 'news', { pinned: true, size: 'small' })");
        QCOMPARE(hs("ordered(mods)[0].id"), QStringLiteral("news"));
        QCOMPARE(hs("ordered(mods)[0].size"), QStringLiteral("small"));
        QCOMPARE(hs("nextSize('row')"), QStringLiteral("small"));
        QCOMPARE(hs("nextSize('small')"), QStringLiteral("medium"));
        h("mods = removeModule(mods, 'wallet')");
        QCOMPARE(h("mods.length").toInt(), 4);
    }

    void addCustomModule()
    {
        h("var base = defaultModules().map(sanitizeModule)");
        h("var added = addModule(base, customName('btc', 'Transactions'), 'btc', { type: 'transaction', app: '' })");
        QCOMPARE(h("added.length").toInt(), 6);
        QCOMPARE(hs("added[5].name"), QStringLiteral("Transactions: btc"));
        QCOMPARE(hs("added[5].id"), QStringLiteral("custom-1"));
        QCOMPARE(hs("added[5].filter.types.join()"), QStringLiteral("transaction"));
        QCOMPARE(hs("added[5].query"), QStringLiteral("btc"));
        QVERIFY(h("added[5].custom && added[5].notify").toBool());
        // The same query and pill again: refused.
        QVERIFY(h("addModule(added, 'other name', 'btc', { type: 'transaction' })").isNull());
        // A name already used: refused.
        QVERIFY(h("addModule(added, 'Messages', 'x', null)").isNull());
        // No name: refused.
        QVERIFY(h("addModule(base, customName('', ''), '', null)").isNull());
        // Full: refused.
        h("var full = addModule(added, 'two', 'q2', null)");
        QCOMPARE(h("full.length").toInt(), 7);
        QVERIFY(h("addModule(full, 'three', 'q3', null)").isNull());
        QCOMPARE(hs("customName('', 'News')"), QStringLiteral("News"));
        QCOMPARE(hs("customName(' ada ', '')"), QStringLiteral("ada"));
    }

    void effectiveFilters()
    {
        // The pill narrows a module's filter.
        QCOMPARE(hs("JSON.stringify(effectiveFilter({ types: ['community', 'post'] }, { type: 'post', app: '' }))"),
                 QStringLiteral("{\"types\":[\"post\"]}"));
        // Excluded: a Messages module under the Transactions pill.
        QVERIFY(h("effectiveFilter({ types: ['message'] }, { type: 'transaction' })").isNull());
        // An app pill adds an app to a module without one.
        QCOMPARE(hs("JSON.stringify(effectiveFilter({ types: ['message'] }, { app: 'objects_chat' }))"),
                 QStringLiteral("{\"types\":[\"message\"],\"apps\":[\"objects_chat\"]}"));
        QCOMPARE(hs("JSON.stringify(effectiveFilter({ types: ['news'], unseenOnly: true }, null))"),
                 QStringLiteral("{\"types\":[\"news\"],\"unseenOnly\":true}"));
        QCOMPARE(hs("joinQuery(' btc ', 'ada')"), QStringLiteral("btc ada"));
        QCOMPARE(hs("joinQuery('', '')"), QString());
    }

    void pills()
    {
        h("var envs = [ { id: '1', type: 'message', app: 'chat' }, { id: '2', type: 'news', app: 'news' },"
          " { id: '3', type: 'message', app: 'chat' }, { id: '4', type: 'asset', app: 'wallet' },"
          " { id: '5', type: 'transaction', app: 'wallet' } ]");
        // Most objects first, then by name.
        QCOMPARE(hs("pills(envs).types.map(p => p.type + p.count).join()"),
                 QStringLiteral("message2,asset1,news1,transaction1"));
        QCOMPARE(hs("pills(envs).apps.map(p => p.app + p.count).join()"),
                 QStringLiteral("chat2,wallet2,news1"));
        QCOMPARE(h("pills([]).types.length").toInt(), 0);
    }

    void feed()
    {
        h("var rows = [ { id: 'b', type: 'news', time: 10 }, { id: 'a', type: 'news', time: 10 },"
          " { id: 'c', type: 'message', updated: 30 }, { id: 'd', type: 'message' } ]");
        // Newest first by time (or updated); equal times by id.
        QCOMPARE(hs("feedRows(rows, null).map(e => e.id).join('')"), QStringLiteral("cabd"));
        QCOMPARE(hs("feedRows(rows, { type: 'news' }).map(e => e.id).join('')"), QStringLiteral("ab"));
    }

    // A visit's new objects: the unseen ones it showed, kept after they are
    // marked seen; "N new" counts them.
    void newSinceVisit()
    {
        h("var r1 = addUnseen({}, [ { id: 'a', seen: false }, { id: 'b', seen: true }, { id: 'c' },"
          " { id: 'a', seen: false }, null, { seen: false } ])");
        QCOMPARE(hs("r1.added.join()"), QStringLiteral("a,c"));
        // Marked seen since: still new for this visit, nothing added.
        h("var r2 = addUnseen(r1.map, [ { id: 'a', seen: true }, { id: 'c', seen: false } ])");
        QCOMPARE(h("r2.added.length").toInt(), 0);
        QVERIFY(h("r2.map === r1.map").toBool());
        QCOMPARE(h("newCount([ { id: 'a' }, { id: 'b' }, { id: 'c' } ], r2.map)").toInt(), 2);
        QCOMPARE(hs("unseenIds([ { id: 'x', seen: false }, { id: 'y', seen: true } ]).join()"),
                 QStringLiteral("x"));
    }

    // Marking seen re-ranks (the unseen boost goes); rows already shown keep
    // their places, new ones come first.
    void stableOrder()
    {
        h("var s1 = stableOrder(null, ['a', 'b', 'c'])");
        QCOMPARE(hs("s1.ids.join('')"), QStringLiteral("abc"));
        h("var s2 = stableOrder(s1.map, ['c', 'b', 'a'])");
        QCOMPARE(hs("s2.ids.join('')"), QStringLiteral("abc"));
        h("var s3 = stableOrder(s2.map, ['b', 'n', 'c', 'a'])");
        QCOMPARE(hs("s3.ids.join('')"), QStringLiteral("nabc"));
        // Gone ids go.
        QCOMPARE(hs("stableOrder(s3.map, ['c', 'n']).ids.join('')"), QStringLiteral("nc"));
    }

    void grid()
    {
        // 390: one column; 1280 minus margins: three.
        QCOMPARE(h("columnsFor(358, 340, 16, 3)").toInt(), 1);
        QCOMPARE(h("columnsFor(1184, 340, 16, 3)").toInt(), 3);
        QCOMPARE(h("columnsFor(100, 340, 16, 3)").toInt(), 1);
        QCOMPARE(h("spanFor('row', 3)").toInt(), 3);
        QCOMPARE(h("spanFor('large', 3)").toInt(), 2);
        QCOMPARE(h("spanFor('large', 1)").toInt(), 1);
        QCOMPARE(h("spanFor('small', 3)").toInt(), 1);
        QCOMPARE(h("shownCount('row')").toInt(), 10);
        QCOMPARE(h("shownCount('bogus')").toInt(), h("shownCount('medium')").toInt());

        // large (2 cols, 300), medium (100), small (50), row (3 cols, 80).
        h("var g = layout([2, 1, 1, 3], [300, 100, 50, 80], 3, 10)");
        QCOMPARE(hs("g.items.map(i => i.col + ':' + i.y).join(' ')"),
                 QStringLiteral("0:0 2:0 2:110 0:310"));
        QCOMPARE(h("g.height").toInt(), 390);
        // One column: stacked in order.
        h("var g1 = layout([1, 1], [100, 50], 1, 10)");
        QCOMPARE(hs("g1.items.map(i => i.col + ':' + i.y).join(' ')"), QStringLiteral("0:0 0:110"));
        QCOMPARE(h("g1.height").toInt(), 160);
        QCOMPARE(h("layout([], [], 3, 10).height").toInt(), 0);
    }
};

QTEST_GUILESS_MAIN(ObjectCardsTest)
#include "object_cards_test.moc"
