// The object cards' and home's pure JS (SHELL-OBJECTS-SPEC §3, §4, §7):
// src/Basecamp/Objects/ObjectFormat.js (what a card writes for a type, a
// time, an amount, a status, which actions it draws) and
// src/Basecamp/Objects/ObjectHighlight.js (what the search matched, marked,
// basecamp-experiments#46),
// apps/basecamp_home/qml/BasecampHomeWidgets.js (widgets as saved filters,
// pills, the feed's order and "N new", rows that keep their places, the grid
// layout, where the arrow keys take the focus). Each file runs in a bare
// QJSEngine, as QML would run a `.pragma library` script; the QML around
// them is checked headlessly.

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
    QJSEngine m_mark;

    QJSValue f(const QString& expr) { return m_format.evaluate(expr); }
    QJSValue h(const QString& expr) { return m_home.evaluate(expr); }
    QString fs(const QString& expr) { return f(expr).toString(); }
    QString hs(const QString& expr) { return h(expr).toString(); }
    QString ms(const QString& expr) { return m_mark.evaluate(expr).toString(); }

    // Where an arrow takes the focus: "where:index", or "-" when the key is
    // not home's.
    // `lines`: the rows' visual lines as a JS array ("" for none).
    QString nav(const QString& from, const QString& key, int tiles, int rows, int lastTile = 0,
                const QString& lines = QString())
    {
        return hs(QStringLiteral("(function(){ var t = navMove(%1, '%2', %3, %4, %5, %6);"
                                 " return t ? t.where + (t.where === 'search' ? '' : ':' + t.index) : '-' })()")
                      .arg(from, key).arg(tiles).arg(rows).arg(lastTile)
                      .arg(lines.isEmpty() ? QStringLiteral("undefined") : lines));
    }

private slots:
    void initTestCase()
    {
        QVERIFY(load(m_format, QFINDTESTDATA("../src/Basecamp/Objects/ObjectFormat.js")));
        QVERIFY(load(m_home, QFINDTESTDATA("../apps/basecamp_home/qml/BasecampHomeWidgets.js")));
        QVERIFY(load(m_mark, QFINDTESTDATA("../src/Basecamp/Objects/ObjectHighlight.js")));
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
        // Enter on a card (#48): the first action, else View; a compact row
        // draws no actions, so View.
        QCOMPARE(fs("primaryAction(actions({ actions: [ { id: 'pay', label: 'Pay' }, { id: 'no' } ] }), false).id"),
                 QStringLiteral("pay"));
        QCOMPARE(fs("primaryAction(actions({ actions: [ { id: 'reply', input: 'text' } ] }), false).input"),
                 QStringLiteral("text"));
        QCOMPARE(fs("primaryAction(actions({ actions: [ { id: 'pay' } ] }), true).id"), QStringLiteral("open"));
        QCOMPARE(fs("primaryAction(actions({}), false).id"), QStringLiteral("open"));
        QCOMPARE(fs("primaryAction(actions({ actions: [ { id: 'open' } ] }), false).id"), QStringLiteral("open"));
    }

    void timeOf()
    {
        QCOMPARE(f("timeOf({ time: 5, updated: 9 })").toInt(), 5);
        QCOMPARE(f("timeOf({ updated: 9 })").toInt(), 9);
        QCOMPARE(f("timeOf({ time: 'now' })").toInt(), 0);
    }

    // ── ObjectHighlight.js ──────────────────────────────────────────────

    // The issue's own cases: the start of each word a query word begins,
    // case-insensitive, as the rankers match.
    void markWordPrefixes()
    {
        const QString o = QStringLiteral("<font color=\"#ed7b58\">");
        const QString c = QStringLiteral("</font>");
        QCOMPARE(ms("mark('Sent 12.00 TST to Ada Morrow', 'ada', '#ed7b58')"),
                 QStringLiteral("Sent 12.00 TST to ") + o + "Ada" + c + " Morrow");
        QCOMPARE(ms("mark('Stub Wallet', 'wal', '#ed7b58')"), QStringLiteral("Stub ") + o + "Wal" + c + "let");
        QCOMPARE(ms("mark('LEZ Wallet', 'WAL', '#ed7b58')"), QStringLiteral("LEZ ") + o + "Wal" + c + "let");
        // Every query word; the longest one a word starts with.
        QCOMPARE(ms("mark('Ada Morrow', 'mo ada mor', '#ed7b58')"),
                 o + "Ada" + c + " " + o + "Mor" + c + "row");
        // Word starts only: "ada" is not in "Canada"; punctuation splits words.
        QCOMPARE(ms("mark('Canada', 'ada', '#ed7b58')"), QStringLiteral("Canada"));
        QCOMPARE(ms("mark('re:ada-morrow', 'mor', '#ed7b58')"),
                 QStringLiteral("re:ada-") + o + "mor" + c + "row");
        // A query word longer than the word marks the word, not past it.
        QCOMPARE(ms("ranges('Ada', 'adam').length").toInt(), 0);
        QCOMPARE(ms("JSON.stringify(ranges('Ada Ada', 'a'))"), QStringLiteral("[[0,1],[4,5]]"));
        // No query, no match or no valid colour: the text, escaped, unmarked.
        QCOMPARE(ms("mark('Ada', '', '#ed7b58')"), QStringLiteral("Ada"));
        QCOMPARE(ms("mark('Ada', '  ', '#ed7b58')"), QStringLiteral("Ada"));
        QCOMPARE(ms("mark('Ada', 'ada', 'red\\\"><img src=x>')"), QStringLiteral("Ada"));
        QCOMPARE(ms("mark(42, 'ada', '#ed7b58')"), QString());
        QCOMPARE(ms("mark('', 'ada', '#ed7b58')"), QString());
    }

    // Publisher text stays plain (ADR-072): escaped before the one tag is
    // added, matched or not.
    void markEscapesPublisherText()
    {
        const QString o = QStringLiteral("<font color=\"#ed7b58\">");
        const QString c = QStringLiteral("</font>");
        QCOMPARE(ms("mark('<b>Ada</b> &amp; <img src=\"http://x/a.png\">', 'ada', '#ed7b58')"),
                 QStringLiteral("&lt;b&gt;") + o + "Ada" + c
                 + "&lt;/b&gt; &amp;amp; &lt;img src=&quot;http://x/a.png&quot;&gt;");
        // A match inside what looks like a tag is still only text.
        QCOMPARE(ms("mark('<img src=x>', 'img', '#ed7b58')"),
                 QStringLiteral("&lt;") + o + "img" + c + " src=x&gt;");
        QCOMPARE(ms("mark('<b>bold</b>', '', '#ed7b58')"), QStringLiteral("&lt;b&gt;bold&lt;/b&gt;"));
        // Reads as the plain text would: line breaks, runs of spaces.
        QCOMPARE(ms("escape('a\\nb\\r\\nc')"), QStringLiteral("a<br>b<br>c"));
        QCOMPARE(ms("escape('a   b')"), QStringLiteral("a &nbsp;&nbsp;b"));
        QCOMPARE(ms("mark('x  Ada', 'ada', '#ed7b58')"), QStringLiteral("x &nbsp;") + o + "Ada" + c);
        QCOMPARE(ms("escape({})"), QString());
    }

    // ── BasecampHomeWidgets.js ──────────────────────────────────────────

    void defaultWidgets()
    {
        QCOMPARE(hs("defaultWidgets().map(m => m.name).join(',')"),
                 QStringLiteral("Transactions,Messages,Communities,Wallet,News"));
        // The defaults leave room for the pills' results within the 8 live
        // results a view keeps.
        QVERIFY(h("defaultWidgets().length < kMaxWidgets").toBool());
        QCOMPARE(hs("defaultWidgets().map(m => sanitizeWidget(m).size).join(',')"),
                 QStringLiteral("large,medium,medium,small,row"));
    }

    void sanitize()
    {
        const QJSValue m = h("sanitizeWidget({ id: 'a', name: '  Mine  ', size: 'huge', pinned: 'yes',"
                             " filter: { types: ['message', 3, 'message'], apps: 'x', evil: 1 },"
                             " extra: 1 })");
        QCOMPARE(m.property("name").toString(), QStringLiteral("Mine"));
        QCOMPARE(m.property("size").toString(), QStringLiteral("medium"));
        QCOMPARE(m.property("pinned").toBool(), false);
        QCOMPARE(m.property("filter").property("types").property("length").toInt(), 1);
        QVERIFY(m.property("filter").property("apps").isUndefined());
        QVERIFY(m.property("extra").isUndefined());
        // A widget that came back through a model: its lists are array-like.
        QCOMPARE(hs("JSON.stringify(effectiveFilter({ types: { 0: 'transaction', length: 1 } }, null))"),
                 QStringLiteral("{\"types\":[\"transaction\"]}"));
    }

    void orderAndEdit()
    {
        h("var ws = defaultWidgets().map(sanitizeWidget)");
        h("ws = updateWidget(ws, 'news', { pinned: true, size: 'small' })");
        QCOMPARE(hs("ordered(ws)[0].id"), QStringLiteral("news"));
        QCOMPARE(hs("ordered(ws)[0].size"), QStringLiteral("small"));
        h("ws = removeWidget(ws, 'wallet')");
        QCOMPARE(h("ws.length").toInt(), 4);
    }

    void addCustomWidget()
    {
        h("var base = defaultWidgets().map(sanitizeWidget)");
        h("var added = addWidget(base, customName('btc', 'Transactions'), 'btc', { type: 'transaction', app: '' })");
        QCOMPARE(h("added.length").toInt(), 6);
        QCOMPARE(hs("added[5].name"), QStringLiteral("Transactions: btc"));
        QCOMPARE(hs("added[5].id"), QStringLiteral("custom-1"));
        QCOMPARE(hs("added[5].filter.types.join()"), QStringLiteral("transaction"));
        QCOMPARE(hs("added[5].query"), QStringLiteral("btc"));
        QVERIFY(h("added[5].custom && added[5].notify").toBool());
        // The same query and pill again: refused.
        QVERIFY(h("addWidget(added, 'other name', 'btc', { type: 'transaction' })").isNull());
        // A name already used: refused.
        QVERIFY(h("addWidget(added, 'Messages', 'x', null)").isNull());
        // No name: refused.
        QVERIFY(h("addWidget(base, customName('', ''), '', null)").isNull());
        // Full: refused.
        h("var full = addWidget(added, 'two', 'q2', null)");
        QCOMPARE(h("full.length").toInt(), 7);
        QVERIFY(h("addWidget(full, 'three', 'q3', null)").isNull());
        QCOMPARE(hs("customName('', 'News')"), QStringLiteral("News"));
        QCOMPARE(hs("customName(' ada ', '')"), QStringLiteral("ada"));
    }

    void effectiveFilters()
    {
        // The pill narrows a widget's filter.
        QCOMPARE(hs("JSON.stringify(effectiveFilter({ types: ['community', 'post'] }, { type: 'post', app: '' }))"),
                 QStringLiteral("{\"types\":[\"post\"]}"));
        // Excluded: a Messages widget under the Transactions pill.
        QVERIFY(h("effectiveFilter({ types: ['message'] }, { type: 'transaction' })").isNull());
        // An app pill adds an app to a widget without one.
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

    // ── cards fit their block (basecamp-experiments#53) ─────────────────

    // How many items fit whole, given their heights: a line of `columns` is
    // as tall as its tallest, spacing between lines, nothing cut off.
    void fitCount()
    {
        auto fit = [this](const char* heights, int columns, int available) {
            return h(QStringLiteral("fitCount(%1, %2, %3, 8)").arg(QLatin1String(heights)).arg(columns)
                         .arg(available)).toInt();
        };
        // One column: 100 + 8 + 100 = 208 fits two, 316 three.
        QCOMPARE(fit("[100, 100, 100]", 1, 207), 1);
        QCOMPARE(fit("[100, 100, 100]", 1, 208), 2);
        QCOMPARE(fit("[100, 100, 100]", 1, 316), 3);
        QCOMPARE(fit("[100, 100, 100]", 1, 9999), 3);
        // Not even one.
        QCOMPARE(fit("[300]", 1, 200), 0);
        QCOMPARE(fit("[]", 1, 200), 0);
        // Two columns: lines of two, the taller sets the line (150 + 8 + 100).
        QCOMPARE(fit("[100, 150, 100, 60]", 2, 257), 2);
        QCOMPARE(fit("[100, 150, 100, 60]", 2, 258), 4);
        // A last line with one item counts it.
        QCOMPARE(fit("[100, 100, 100]", 2, 208), 3);
        // Compact rows (44 px) in a two-row block: 172 px holds three.
        QCOMPARE(fit("[44, 44, 44, 44, 44]", 1, 172), 3);
        // An item not measured yet stops the count.
        QCOMPARE(fit("[100, 0, 100]", 1, 9999), 1);
        QCOMPARE(fit("[0]", 3, 9999), 0);
    }

    // Width decides the columns (each at least 300 px, 1 to 3), height
    // whether the rows are one-line, and how many a widget builds at most.
    void fitColumnsAndForm()
    {
        QCOMPARE(h("itemColumns(299, 8)").toInt(), 1);
        QCOMPARE(h("itemColumns(356, 8)").toInt(), 1);
        QCOMPARE(h("itemColumns(608, 8)").toInt(), 2);
        QCOMPARE(h("itemColumns(760, 8)").toInt(), 2);
        QCOMPARE(h("itemColumns(916, 8)").toInt(), 3);
        QCOMPARE(h("itemColumns(4000, 8)").toInt(), 3);
        QCOMPARE(h("itemColumns(0, 8)").toInt(), 1);
        QVERIFY(h("compactRows(2)").toBool());
        QVERIFY(!h("compactRows(3)").toBool());
        QVERIFY(!h("compactRows(0)").toBool());
        // A two-row block's 172 px: three one-line rows a column.
        QCOMPARE(h("candidateCount(172, 1, 8)").toInt(), 3);
        QCOMPARE(h("candidateCount(172, 3, 8)").toInt(), 9);
        QCOMPARE(h("candidateCount(43, 1, 8)").toInt(), 0);
        QCOMPARE(h("candidateCount(-20, 2, 8)").toInt(), 0);
    }

    // ── the cell grid (basecamp-experiments#49) ─────────────────────────

    // A layout as "id@x,y wxh" in reading order.
    QString cells(const QString& layout)
    {
        return hs(QStringLiteral("readingOrder((%1).blocks).map(b => b.id + '@' + b.x + ',' + b.y"
                                 " + ' ' + b.w + 'x' + b.h).join(' | ')").arg(layout));
    }

    void gridColumns()
    {
        // By the page's (window's) width, not the grid's: the side margins
        // never cost a column (a 1800 px window has 5).
        QCOMPARE(h("columnsFor(366)").toInt(), 1);
        QCOMPARE(h("columnsFor(390)").toInt(), 1);
        QCOMPARE(h("columnsFor(599)").toInt(), 1);
        QCOMPARE(h("columnsFor(600)").toInt(), 2);
        QCOMPARE(h("columnsFor(999)").toInt(), 2);
        QCOMPARE(h("columnsFor(1000)").toInt(), 3);
        QCOMPARE(h("columnsFor(1280)").toInt(), 3);
        QCOMPARE(h("columnsFor(1399)").toInt(), 3);
        QCOMPARE(h("columnsFor(1400)").toInt(), 4);
        QCOMPARE(h("columnsFor(1799)").toInt(), 4);
        // Full width (no cap): a column more every 400 px, at most eight.
        QCOMPARE(h("columnsFor(1800)").toInt(), 5);
        QCOMPARE(h("columnsFor(2199)").toInt(), 5);
        QCOMPARE(h("columnsFor(2200)").toInt(), 6);
        QCOMPARE(h("columnsFor(2599)").toInt(), 6);
        QCOMPARE(h("columnsFor(2600)").toInt(), 7);
        QCOMPARE(h("columnsFor(2999)").toInt(), 7);
        QCOMPARE(h("columnsFor(3000)").toInt(), 8);
        QCOMPARE(h("columnsFor(5000)").toInt(), 8);
        QCOMPARE(h("kMaxColumns").toInt(), 8);
        QCOMPARE(h("columnsFor(0)").toInt(), 1);
        QCOMPARE(h("columnsFor(NaN)").toInt(), 1);
    }

    // The five defaults, from their old size and pin.
    void gridMigration()
    {
        h("var ws = defaultWidgets().map(sanitizeWidget)");
        QCOMPARE(cells("migrateLayout(ws, 4)"),
                 QStringLiteral("transactions@0,0 2x4 | messages@2,0 1x3 | communities@3,0 1x3"
                                " | wallet@2,3 1x2 | news@0,5 4x2"));
        QCOMPARE(cells("migrateLayout(ws, 2)"),
                 QStringLiteral("transactions@0,0 2x4 | messages@0,4 1x3 | communities@1,4 1x3"
                                " | wallet@0,7 1x2 | news@0,9 2x2"));
        // Five and six columns (wide screens): the strip spans them all.
        QCOMPARE(cells("migrateLayout(ws, 5)"),
                 QStringLiteral("transactions@0,0 2x4 | messages@2,0 1x3 | communities@3,0 1x3"
                                " | wallet@4,0 1x2 | news@0,4 5x2"));
        QCOMPARE(cells("migrateLayout(ws, 6)"),
                 QStringLiteral("transactions@0,0 2x4 | messages@2,0 1x3 | communities@3,0 1x3"
                                " | wallet@4,0 1x2 | news@0,4 6x2"));
        QCOMPARE(hs("blockFor(migrateLayout(ws, 8), 'news').w"), QStringLiteral("8"));
        // Never arranged at 6: layoutFor derives it from the widest saved
        // (here 4) and it stays inside the grid.
        h("var from4 = { 4: place(migrateLayout(ws, 4), 'messages', 0, 0) }");
        QCOMPARE(cells("layoutFor(from4, 6, ws)"), cells("fitTo(from4[4], 6)"));
        QCOMPARE(hs("layoutFor(from4, 6, ws).blocks.every(b => b.x + b.w <= 6)"), QStringLiteral("true"));
        // A saved 6-column layout is kept for 6 and is the widest to derive from.
        h("var from6 = { 4: from4[4], 6: place(migrateLayout(ws, 6), 'wallet', 5, 0) }");
        QCOMPARE(cells("layoutFor(from6, 6, ws)"), cells("from6[6]"));
        QCOMPARE(cells("layoutFor(from6, 5, ws)"), cells("fitTo(from6[6], 5)"));
        QCOMPARE(cells("migrateLayout(ws, 1)"),
                 QStringLiteral("transactions@0,0 1x4 | messages@0,4 1x3 | communities@0,7 1x3"
                                " | wallet@0,10 1x2 | news@0,12 1x2"));
        // Pinned first.
        h("var pinned = updateWidget(ws, 'news', { pinned: true })");
        QCOMPARE(hs("readingOrder(migrateLayout(pinned, 4).blocks)[0].id"), QStringLiteral("news"));
        QCOMPARE(hs("readingOrder(migrateLayout(pinned, 4).blocks)[0].w"), QStringLiteral("4"));
        QCOMPARE(h("rowsOf(migrateLayout(ws, 4))").toInt(), 7);
        QCOMPARE(h("rowsOf({ columns: 2, blocks: [] })").toInt(), 0);
    }

    void gridMoveAndResize()
    {
        h("var ws = defaultWidgets().map(sanitizeWidget)");
        h("var l4 = migrateLayout(ws, 4)");
        // Messages to the top left: Transactions is pushed below it, the
        // rest move up into the room it left.
        QCOMPARE(cells("place(l4, 'messages', 0, 0)"),
                 QStringLiteral("messages@0,0 1x3 | wallet@2,0 1x2 | communities@3,0 1x3"
                                " | transactions@0,3 2x4 | news@0,7 4x2"));
        // Dropped far below: it rises to under the last card, News moving
        // up into the room it left.
        QCOMPARE(cells("place(l4, 'wallet', 3, 40)"),
                 QStringLiteral("transactions@0,0 2x4 | messages@2,0 1x3 | communities@3,0 1x3"
                                " | news@0,4 4x2 | wallet@3,6 1x2"));
        // Past the right edge: clamped in.
        QCOMPARE(hs("blockFor(place(l4, 'transactions', 9, 0), 'transactions').x"), QStringLiteral("2"));
        // Resize: clamped to the columns and the minimum rows; what it now
        // covers is pushed down.
        h("var r = resize(l4, 'wallet', 9, 1)");
        QCOMPARE(hs("JSON.stringify(blockFor(r, 'wallet'))"),
                 QStringLiteral("{\"id\":\"wallet\",\"x\":0,\"y\":3,\"w\":4,\"h\":2}"));
        QCOMPARE(hs("blockFor(r, 'transactions').y"), QStringLiteral("5"));
        QCOMPARE(hs("blockFor(resize(l4, 'messages', 0, 0), 'messages').w"), QStringLiteral("1"));
        QCOMPARE(hs("blockFor(resize(l4, 'messages', 0, 0), 'messages').h"), QStringLiteral("2"));
        // Wider than the room right of it: moved left to fit.
        QCOMPARE(hs("JSON.stringify(blockFor(resize(l4, 'communities', 2, 3), 'communities'))"),
                 QStringLiteral("{\"id\":\"communities\",\"x\":2,\"y\":0,\"w\":2,\"h\":3}"));
        // An unknown id changes nothing.
        QCOMPARE(cells("place(l4, 'nope', 0, 0)"), cells("l4"));
        // compact lifts a floating card and settles overlaps.
        QCOMPARE(cells("compact({ columns: 2, blocks: [{ id: 'a', x: 0, y: 5, w: 1, h: 2 },"
                       " { id: 'b', x: 0, y: 0, w: 2, h: 2 }, { id: 'c', x: 0, y: 1, w: 1, h: 3 }] })"),
                 QStringLiteral("b@0,0 2x2 | c@0,2 1x3 | a@0,5 1x2"));
    }

    // No overlap, nothing out of the grid, nothing lost, after any of a few
    // hundred random moves, resizes and width changes.
    void gridNeverOverlaps()
    {
        const QString result = hs(R"JS((function () {
            var seed = 49
            function rnd(n) { seed = (seed * 1103515245 + 12345) % 2147483648; return seed % n }
            var ws = defaultWidgets().map(sanitizeWidget)
            ws = addWidget(ws, 'one', 'q1', null)
            ws = addWidget(ws, 'two', 'q2', null)
            var ids = ws.map(function (m) { return m.id })
            var l = migrateLayout(ws, 4)
            function check(l, step) {
                if (l.blocks.length !== ids.length) return step + ': lost a card'
                for (var i = 0; i < l.blocks.length; ++i) {
                    var a = l.blocks[i]
                    if (a.x < 0 || a.y < 0 || a.w < 1 || a.h < kMinRows || a.x + a.w > l.columns)
                        return step + ': ' + a.id + ' out of the grid'
                    for (var j = i + 1; j < l.blocks.length; ++j)
                        if (overlaps(a, l.blocks[j])) return step + ': ' + a.id + ' overlaps ' + l.blocks[j].id
                }
                // Compact: nothing can move up.
                var c = compact(l)
                for (var k = 0; k < l.blocks.length; ++k)
                    if (c.blocks[k].y !== l.blocks[k].y) return step + ': not compact'
                return ''
            }
            for (var step = 0; step < 400; ++step) {
                var id = ids[rnd(ids.length)]
                var op = rnd(3)
                if (op === 0) l = place(l, id, rnd(10) - 1, rnd(20) - 2)
                else if (op === 1) l = resize(l, id, rnd(10), rnd(7))
                else l = fitTo(l, 1 + rnd(8))
                var bad = check(l, step)
                if (bad) return bad
            }
            return 'ok'
        })())JS");
        QCOMPARE(result, QStringLiteral("ok"));
    }

    void gridFitTo()
    {
        h("var ws = defaultWidgets().map(sanitizeWidget)");
        h("var l4 = place(migrateLayout(ws, 4), 'news', 0, 0)");
        h("function ids(l) { return readingOrder(l.blocks).map(b => b.id).join(',') }");
        const QString order = hs("ids(l4)");
        QCOMPARE(order, QStringLiteral("news,transactions,messages,communities,wallet"));
        h("var l2 = fitTo(l4, 2), l1 = fitTo(l2, 1), back = fitTo(l1, 4)");
        QCOMPARE(hs("ids(l2)"), order);
        QCOMPARE(hs("ids(l1)"), order);
        QCOMPARE(hs("ids(back)"), order);
        // Widths clamped to the columns.
        QCOMPARE(hs("blockFor(l2, 'news').w"), QStringLiteral("2"));
        QCOMPARE(hs("l1.blocks.every(b => b.w === 1 && b.x === 0)"), QStringLiteral("true"));
        QCOMPARE(h("back.columns").toInt(), 4);
    }

    void gridLayoutFor()
    {
        h("var ws = defaultWidgets().map(sanitizeWidget)");
        // Nothing saved: the migrated layout, at every width.
        QCOMPARE(cells("layoutFor({}, 3, ws)"), cells("migrateLayout(ws, 3)"));
        QCOMPARE(cells("layoutFor(null, 1, ws)"), cells("migrateLayout(ws, 1)"));
        // A saved layout wins over a derived one.
        h("var mine = place(migrateLayout(ws, 2), 'wallet', 0, 0)");
        h("var saved = { 2: mine, 4: place(migrateLayout(ws, 4), 'news', 0, 0) }");
        QCOMPARE(cells("layoutFor(saved, 2, ws)"), cells("mine"));
        // None for 3: derived from the widest saved (4).
        QCOMPARE(cells("layoutFor(saved, 3, ws)"), cells("fitTo(saved[4], 3)"));
        QCOMPARE(hs("readingOrder(layoutFor(saved, 1, ws).blocks)[0].id"), QStringLiteral("news"));
        // A widget removed since drops out; one added gets 1x3 at the end.
        h("var fewer = removeWidget(ws, 'messages')");
        QVERIFY(h("blockFor(layoutFor(saved, 2, fewer), 'messages') === null").toBool());
        h("var more = addWidget(ws, 'mine', 'q', null)");
        h("var withNew = layoutFor(saved, 2, more)");
        QCOMPARE(hs("JSON.stringify(blockFor(withNew, 'custom-1'))"),
                 QStringLiteral("{\"id\":\"custom-1\",\"x\":0,\"y\":11,\"w\":1,\"h\":3}"));
        QCOMPARE(hs("readingOrder(withNew.blocks).pop().id"), QStringLiteral("custom-1"));
        // Done after a remove saves no block for the removed widget.
        h("var pruned = pruneLayouts(saved, fewer)");
        QVERIFY(h("blockFor(pruned[2], 'messages') === null && blockFor(pruned[4], 'messages') === null").toBool());
        QVERIFY(h("blockFor(pruned[2], 'wallet') !== null").toBool());
        QCOMPARE(hs("JSON.stringify(pruneLayouts({ 2: null, 3: { columns: 3 } }, ws))"), QStringLiteral("{}"));
        // Layouts read back from the settings (lists from C++, not JS arrays)
        // are kept by Done, not dropped.
        h("var fromCpp = { 3: { columns: 3, blocks: { length: 2, 0: { id: 'news', x: 0, y: 0, w: 1, h: 4 },"
          " 1: { id: 'messages', x: 1, y: 0, w: 2, h: 2 } } } }");
        QCOMPARE(hs("JSON.stringify(pruneLayouts(fromCpp, fewer)[3].blocks.map(function (b) { return b.id }))"),
                 QStringLiteral("[\"news\"]"));
        // A broken saved layout is cleaned: bad blocks clamped, duplicates
        // and unknown ids dropped, overlaps settled.
        h("var junk = { 2: { columns: 2, blocks: [{ id: 'news', x: -3, y: 0, w: 9, h: 0 },"
          " { id: 'news', x: 1, y: 1, w: 1, h: 2 }, { id: 'ghost', x: 0, y: 0, w: 1, h: 2 },"
          " { id: 'wallet', x: 0, y: 0, w: 1, h: 2 }, 7, null] } }");
        QCOMPARE(cells("layoutFor(junk, 2, ws)"),
                 QStringLiteral("news@0,0 2x2 | wallet@0,2 1x2 | transactions@1,2 1x3"
                                " | messages@0,4 1x3 | communities@1,5 1x3"));
        // Absurd rows and heights (a hand edit, a bad sync): clamped, and
        // compact lifts the card at once instead of a row at a time.
        h("var far = { 3: { columns: 3, blocks: [{ id: 'transactions', x: 0, y: 1e17, w: 1, h: 1e9 },"
          " { id: 'news', x: 1, y: 1e300, w: 2, h: -1e9 }] } }");
        QCOMPARE(cells("layoutFor(far, 3, ws)"),
                 QStringLiteral("transactions@0,0 1x8 | news@1,0 2x2 | messages@1,2 1x3 | communities@2,2 1x3"
                                " | wallet@1,5 1x3"));
        QCOMPARE(hs("JSON.stringify(clampBlock({ id: 'a', x: 0, y: 1e17, w: 1, h: 1e9 }, 3))"),
                 QStringLiteral("{\"id\":\"a\",\"x\":0,\"y\":256,\"w\":1,\"h\":8}"));
        // Hidden cards give up their cells; the rest close up in reading
        // order. None hidden: the layout as it is.
        QCOMPARE(cells("withoutHidden(migrateLayout(ws, 4), ['transactions', 'wallet'])"),
                 QStringLiteral("messages@0,0 1x3 | communities@1,0 1x3 | news@0,3 4x2"));
        QCOMPARE(cells("withoutHidden(migrateLayout(ws, 4), ['messages', 'gone'])"),
                 QStringLiteral("transactions@0,0 2x4 | communities@2,0 1x3 | wallet@3,0 1x2 | news@0,4 4x2"));
        QCOMPARE(cells("withoutHidden(mine, [])"), cells("mine"));
    }

    // ── edit mode (basecamp-experiments#51) ─────────────────────────────

    // A card dragged below its neighbour stays below it: the neighbour
    // moves up past it, where a plain compact would lift the card back.
    void gridDragDown()
    {
        h("var ws = defaultWidgets().map(sanitizeWidget)");
        h("var l1 = migrateLayout(ws, 1)");
        QCOMPARE(cells("place(l1, 'transactions', 0, 4)"),
                 QStringLiteral("messages@0,0 1x3 | transactions@0,3 1x4 | communities@0,7 1x3"
                                " | wallet@0,10 1x2 | news@0,12 1x2"));
        // Dragged past two: both move up.
        QCOMPARE(hs("readingOrder(place(l1, 'transactions', 0, 7).blocks).map(b => b.id).join(',')"),
                 QStringLiteral("messages,communities,transactions,wallet,news"));
        // Not yet past the next one's top: it lands back where it was.
        QCOMPARE(cells("place(l1, 'transactions', 0, 2)"), cells("l1"));
        // Up works as before: Messages to the top pushes Transactions down.
        QCOMPARE(hs("readingOrder(place(l1, 'messages', 0, 0).blocks).map(b => b.id).join(',')"),
                 QStringLiteral("messages,transactions,communities,wallet,news"));
        // On four columns, News (a strip) dragged to the top and back down.
        h("var l4 = migrateLayout(ws, 4)");
        h("var top = place(l4, 'news', 0, 0)");
        QCOMPARE(hs("readingOrder(top.blocks)[0].id"), QStringLiteral("news"));
        QCOMPARE(cells("place(top, 'news', 0, 6)"), cells("l4"));
        // closeUpAround keeps the held card where it is.
        QCOMPARE(cells("closeUpAround({ columns: 1, blocks: [{ id: 'a', x: 0, y: 4, w: 1, h: 2 },"
                       " { id: 'b', x: 0, y: 7, w: 1, h: 3 }] }, 'a')"),
                 QStringLiteral("b@0,0 1x3 | a@0,4 1x2"));
    }

    void gridDropCell()
    {
        // pitch 400 x 136: a card held at (390, 300) lands on cell (1, 2).
        QCOMPARE(hs("JSON.stringify(dropCell(390, 300, 400, 136, 3, 1))"), QStringLiteral("{\"x\":1,\"y\":2}"));
        // Clamped in: never left of 0, never past the right for its width.
        QCOMPARE(hs("JSON.stringify(dropCell(-90, -40, 400, 136, 3, 1))"), QStringLiteral("{\"x\":0,\"y\":0}"));
        QCOMPARE(hs("JSON.stringify(dropCell(1190, 0, 400, 136, 3, 2))"), QStringLiteral("{\"x\":1,\"y\":0}"));
        // One column: up and down only.
        QCOMPARE(hs("JSON.stringify(dropCell(250, 700, 366, 136, 1, 1))"), QStringLiteral("{\"x\":0,\"y\":5}"));
    }

    void gridAutoScroll()
    {
        QCOMPARE(h("autoScrollStep(400, 800, 16)").toInt(), 0);
        QVERIFY(h("autoScrollStep(10, 800, 16)").toInt() < 0);
        QVERIFY(h("autoScrollStep(790, 800, 16)").toInt() > 0);
        // Faster the closer to the edge, at most maxStep.
        QVERIFY(h("autoScrollStep(5, 800, 16)").toInt() < h("autoScrollStep(50, 800, 16)").toInt());
        QCOMPARE(h("autoScrollStep(-40, 800, 16)").toInt(), -16);
        QCOMPARE(h("autoScrollStep(900, 800, 16)").toInt(), 16);
        // A short view still has a middle that does not scroll.
        QCOMPARE(h("autoScrollStep(50, 100, 16)").toInt(), 0);
    }

    // What home saves through the shell, and what it makes of what comes back.
    void homeSettingsRoundTrip()
    {
        h("var ws = defaultWidgets().map(sanitizeWidget)");
        h("var mine = { 4: place(migrateLayout(ws, 4), 'messages', 0, 0) }");
        h("var back = fromSettings(JSON.parse(JSON.stringify(settingsOf(removeWidget(ws, 'news'), mine))))");
        QCOMPARE(hs("back.widgets.map(m => m.id).join(',')"),
                 QStringLiteral("transactions,messages,communities,wallet"));
        QCOMPARE(cells("layoutFor(back.layouts, 4, back.widgets)"),
                 cells("layoutFor(mine, 4, removeWidget(ws, 'news'))"));
        // Nothing saved, another version or junk: the defaults, no layouts.
        for (const char* junk : { "undefined", "null", "7", "({})", "({ version: 2, widgets: [] })" }) {
            QCOMPARE(hs(QStringLiteral("fromSettings(%1).widgets.length").arg(junk)), QStringLiteral("5"));
            QCOMPARE(hs(QStringLiteral("Object.keys(fromSettings(%1).layouts).length").arg(junk)), QStringLiteral("0"));
        }
        // Every widget removed stays so.
        QCOMPARE(hs("fromSettings({ version: 1, widgets: [] }).widgets.length"), QStringLiteral("0"));
        // Bad widgets and layouts are dropped: no id, a duplicate, past
        // kMaxWidgets, a column count home never has.
        h("var many = []; for (var i = 0; i < 12; ++i) many.push({ id: 'm' + i, name: 'M' + i })");
        QCOMPARE(hs("fromSettings({ version: 1, widgets: many }).widgets.length"), QStringLiteral("7"));
        QCOMPARE(hs("fromSettings({ version: 1, widgets: [{ name: 'x' }, { id: 'a', name: 'A' },"
                    " { id: 'a', name: 'B' }, 3] }).widgets.map(m => m.name).join(',')"),
                 QStringLiteral("A"));
        QCOMPARE(hs("Object.keys(fromSettings({ version: 1, layouts: { 2: { columns: 2, blocks: [] },"
                    " 9: {}, x: {}, 3: 'no' } }).layouts).join(',')"),
                 QStringLiteral("2"));
    }

    // Settings saved while home's widgets were called modules: the list under
    // the old key loads as the same widgets and layout; the next save has
    // `widgets` only.
    void homeSettingsOldKey()
    {
        h("var ws = removeWidget(defaultWidgets().map(sanitizeWidget), 'wallet')");
        h("var mine = { 3: place(migrateLayout(ws, 3), 'news', 1, 0) }");
        h("var old = JSON.parse(JSON.stringify({ version: 1, modules: ws, layouts: mine }))");
        h("var fromOld = fromSettings(old)");
        QCOMPARE(hs("JSON.stringify(fromOld.widgets)"), hs("JSON.stringify(ws)"));
        QCOMPARE(cells("layoutFor(fromOld.layouts, 3, fromOld.widgets)"),
                 cells("layoutFor(mine, 3, ws)"));
        QCOMPARE(hs("kOldWidgetsKey"), QStringLiteral("modules"));
        // Saving writes the new key and not the old one.
        h("var saved = settingsOf(fromOld.widgets, fromOld.layouts)");
        QCOMPARE(hs("Object.keys(saved).sort().join(',')"), QStringLiteral("layouts,version,widgets"));
        QCOMPARE(hs("JSON.stringify(fromSettings(JSON.parse(JSON.stringify(saved))))"),
                 hs("JSON.stringify(fromOld)"));
        // `widgets` wins over a stale old list, even an empty one.
        QCOMPARE(hs("fromSettings({ version: 1, widgets: [], modules: ws }).widgets.length"),
                 QStringLiteral("0"));
    }

    // ── resize handles (basecamp-experiments#52) ────────────────────────

    void gridSizeLimits()
    {
        // 3 columns of 392 px: one is wide enough, the right edge stops it.
        QCOMPARE(hs("JSON.stringify(sizeLimits({ x: 1, y: 0, w: 1, h: 3 }, 3, 392, 12))"),
                 QStringLiteral("{\"minW\":1,\"maxW\":2,\"minH\":2,\"maxH\":8}"));
        // Columns narrower than a readable card: two at least, unless the
        // card is narrower already (then it may stay so).
        QCOMPARE(hs("JSON.stringify(sizeLimits({ x: 0, y: 0, w: 3, h: 2 }, 4, 200, 12))"),
                 QStringLiteral("{\"minW\":2,\"maxW\":4,\"minH\":2,\"maxH\":8}"));
        QCOMPARE(hs("sizeLimits({ x: 0, y: 0, w: 1, h: 2 }, 4, 200, 12).minW"), QStringLiteral("1"));
        // Never taller than kMaxRows (a block saved taller is cut to it).
        QCOMPARE(hs("sizeLimits({ x: 0, y: 0, w: 1, h: 10 }, 1, 366, 12).maxH"), QStringLiteral("8"));
    }

    void gridSnapSize()
    {
        h("var lim = { minW: 1, maxW: 3, minH: 2, maxH: 8 }");
        // pitch 404 x 132 (cells 392 x 120, gap 12): to the nearest cell.
        QCOMPARE(hs("JSON.stringify(snapSize(392, 384, 404, 132, 12, lim))"), QStringLiteral("{\"w\":1,\"h\":3}"));
        QCOMPARE(hs("JSON.stringify(snapSize(620, 470, 404, 132, 12, lim))"), QStringLiteral("{\"w\":2,\"h\":4}"));
        // Within the limits.
        QCOMPARE(hs("JSON.stringify(snapSize(10, 10, 404, 132, 12, lim))"), QStringLiteral("{\"w\":1,\"h\":2}"));
        QCOMPARE(hs("JSON.stringify(snapSize(9000, 9000, 404, 132, 12, lim))"), QStringLiteral("{\"w\":3,\"h\":8}"));
    }

    // The issue's cases on the model: Transactions 2 wide to 1, the next
    // card moves up beside it; News 1x3 to 2x5 pushes what it covers down.
    void gridResizeCases()
    {
        h("var ws = defaultWidgets().map(sanitizeWidget)");
        h("var l2 = migrateLayout(ws, 2)");
        QCOMPARE(cells("resize(l2, 'transactions', 1, 4)"),
                 QStringLiteral("transactions@0,0 1x4 | communities@1,0 1x3 | messages@0,4 1x3"
                                " | wallet@0,7 1x2 | news@0,9 2x2"));
        // At 3 columns Messages is already beside it: narrower, it pulls
        // Messages into the column it freed, and Communities moves up, so
        // no column is left empty.
        h("var l3 = migrateLayout(ws, 3)");
        QCOMPARE(cells("resize(l3, 'transactions', 1, 4)"),
                 QStringLiteral("transactions@0,0 1x4 | messages@1,0 1x3 | communities@2,0 1x3"
                                " | wallet@0,4 1x2 | news@0,6 3x2"));
        // Shorter only: nothing is pulled.
        QCOMPARE(cells("resize(l3, 'transactions', 2, 2)"),
                 QStringLiteral("transactions@0,0 2x2 | messages@2,0 1x3 | wallet@0,2 1x2"
                                " | communities@2,3 1x3 | news@0,6 3x2"));
        h("var l4 = place(resize(migrateLayout(ws, 4), 'news', 1, 3), 'news', 0, 0)");
        QCOMPARE(cells("l4"), QStringLiteral("news@0,0 1x3 | messages@2,0 1x3 | communities@3,0 1x3"
                                             " | transactions@0,3 2x4 | wallet@2,3 1x2"));
        h("var big = resize(l4, 'news', 2, 5)");
        // Transactions, under it, moves down; the others stay.
        QCOMPARE(cells("big"), QStringLiteral("news@0,0 2x5 | messages@2,0 1x3 | communities@3,0 1x3"
                                              " | wallet@2,3 1x2 | transactions@0,5 2x4"));
        QCOMPARE(hs("big.blocks.every((a, i) => big.blocks.every((b, j) => i === j || !overlaps(a, b)))"),
                 QStringLiteral("true"));
    }

    // ── keyboard in edit mode (basecamp-experiments#54) ─────────────────

    // An arrow moves a card a step: past the card above or below it (the
    // others close up, so one cell may not be a change), along the columns,
    // never off the grid. Shift and an arrow resizes it a cell.
    void gridKeyMove()
    {
        h("var ws = defaultWidgets().map(sanitizeWidget)");
        h("var l3 = migrateLayout(ws, 3)");
        QCOMPARE(cells("l3"), QStringLiteral("transactions@0,0 2x4 | messages@2,0 1x3 | communities@2,3 1x3"
                                             " | wallet@0,4 1x2 | news@0,6 3x2"));
        // Down twice: below Communities, then below News.
        h("var d1 = keyMove(l3, 'messages', 0, 1)");
        QCOMPARE(cells("d1"), QStringLiteral("transactions@0,0 2x4 | communities@2,0 1x3 | messages@2,3 1x3"
                                             " | wallet@0,4 1x2 | news@0,6 3x2"));
        h("var d2 = keyMove(d1, 'messages', 0, 1)");
        QCOMPARE(cells("d2"), QStringLiteral("transactions@0,0 2x4 | communities@2,0 1x3 | wallet@0,4 1x2"
                                             " | news@0,6 3x2 | messages@2,8 1x3"));
        // Up twice: back where it was.
        QCOMPARE(cells("keyMove(keyMove(d2, 'messages', 0, -1), 'messages', 0, -1)"), cells("l3"));
        // At the top, the right edge or the bottom: nothing changes.
        QVERIFY(h("keyMove(l3, 'messages', 0, -1) === l3").toBool());
        QVERIFY(h("keyMove(l3, 'messages', 1, 0) === l3").toBool());
        QVERIFY(h("keyMove(d2, 'messages', 0, 1) === d2").toBool());
        QVERIFY(h("keyMove(l3, 'nope', 0, 1) === l3").toBool());
        // Left: into Transactions' columns, which moves down.
        QCOMPARE(cells("keyMove(l3, 'messages', -1, 0)"),
                 QStringLiteral("messages@1,0 1x3 | communities@2,0 1x3 | transactions@0,3 2x4"
                                " | wallet@0,7 1x2 | news@0,9 3x2"));
        // One column: Up swaps it with the card above, Left and Right do nothing.
        h("var l1 = migrateLayout(ws, 1)");
        QCOMPARE(cells("keyMove(l1, 'messages', 0, -1)"),
                 QStringLiteral("messages@0,0 1x3 | transactions@0,3 1x4 | communities@0,7 1x3"
                                " | wallet@0,10 1x2 | news@0,12 1x2"));
        QVERIFY(h("keyMove(l1, 'messages', -1, 0) === l1").toBool());
    }

    void gridKeyResize()
    {
        h("var ws = defaultWidgets().map(sanitizeWidget)");
        h("var l3 = migrateLayout(ws, 3)");
        h("var lim = function (l, id) { return sizeLimits(blockFor(l, id), 3, 380, 16) }");
        // Taller and shorter by a row, the others making room or closing up.
        QCOMPARE(hs("JSON.stringify(blockFor(keyResize(l3, 'messages', 0, 1, lim(l3, 'messages')), 'messages'))"),
                 QStringLiteral("{\"id\":\"messages\",\"x\":2,\"y\":0,\"w\":1,\"h\":4}"));
        QCOMPARE(hs("blockFor(keyResize(l3, 'messages', 0, -1, lim(l3, 'messages')), 'communities').y"),
                 QStringLiteral("2"));
        // At the right edge it grows to the left; Transactions moves down.
        QCOMPARE(cells("keyResize(l3, 'messages', 1, 0, lim(l3, 'messages'))"),
                 QStringLiteral("messages@1,0 2x3 | transactions@0,3 2x4 | communities@2,3 1x3"
                                " | wallet@0,7 1x2 | news@0,9 3x2"));
        // Wider than the grid, narrower than one column, shorter than two
        // rows: nothing changes.
        h("var full = keyResize(l3, 'news', 1, 0, lim(l3, 'news'))");
        QVERIFY(h("full === l3").toBool());
        QVERIFY(h("keyResize(l3, 'messages', -1, 0, lim(l3, 'messages')) === l3").toBool());
        QVERIFY(h("keyResize(l3, 'wallet', 0, -1, lim(l3, 'wallet')) === l3").toBool());
        // Shrinking Transactions a column.
        QCOMPARE(hs("blockFor(keyResize(l3, 'transactions', -1, 0, lim(l3, 'transactions')), 'transactions').w"),
                 QStringLiteral("1"));
    }

    // ── keyboard (basecamp-experiments#48) ──────────────────────────────

    void keyboardNavigation()
    {
        const QString search = QStringLiteral("{ where: 'search' }");
        auto tile = [](int i) { return QStringLiteral("{ where: 'tile', index: %1 }").arg(i); };
        auto row = [](int i) { return QStringLiteral("{ where: 'row', index: %1 }").arg(i); };

        // Down from the search: the first Recent app, else the first row
        // (a query hides Recent apps: "wal", Down lands on the first app).
        QCOMPARE(nav(search, "down", 3, 5), QStringLiteral("tile:0"));
        QCOMPARE(nav(search, "down", 0, 5), QStringLiteral("row:0"));
        QCOMPARE(nav(search, "down", 0, 0), QStringLiteral("-"));
        // Up, Left and Right in the search stay the field's own.
        QCOMPARE(nav(search, "up", 3, 5), QStringLiteral("-"));
        QCOMPARE(nav(search, "left", 3, 5), QStringLiteral("-"));

        // Along Recent apps, stopping at both ends.
        QCOMPARE(nav(tile(0), "right", 3, 5), QStringLiteral("tile:1"));
        QCOMPARE(nav(tile(2), "right", 3, 5), QStringLiteral("tile:2"));
        QCOMPARE(nav(tile(1), "left", 3, 5), QStringLiteral("tile:0"));
        QCOMPARE(nav(tile(0), "left", 3, 5), QStringLiteral("tile:0"));
        // Up from the row: the search; Down: the first row, or stay.
        QCOMPARE(nav(tile(2), "up", 3, 5), QStringLiteral("search"));
        QCOMPARE(nav(tile(2), "down", 3, 5), QStringLiteral("row:0"));
        QCOMPARE(nav(tile(2), "down", 3, 0), QStringLiteral("tile:2"));

        // Down the rows (app rows, then cards), stopping at the last.
        QCOMPARE(nav(row(0), "down", 3, 5), QStringLiteral("row:1"));
        QCOMPARE(nav(row(4), "down", 3, 5), QStringLiteral("row:4"));
        QCOMPARE(nav(row(3), "up", 3, 5), QStringLiteral("row:2"));
        // Up from the first row: back to the tile it came from, or the search.
        QCOMPARE(nav(row(0), "up", 3, 5, 2), QStringLiteral("tile:2"));
        QCOMPARE(nav(row(0), "up", 3, 5, 9), QStringLiteral("tile:2"));
        QCOMPARE(nav(row(0), "up", 0, 5), QStringLiteral("search"));
        // Left and Right in a column are not home's (a card's own buttons).
        QCOMPARE(nav(row(1), "left", 3, 5), QStringLiteral("-"));
        QCOMPARE(nav(row(1), "right", 3, 5), QStringLiteral("-"));
        // Rows that shrank under the focus: clamped.
        QCOMPARE(nav(row(7), "down", 0, 3), QStringLiteral("row:2"));
        QCOMPARE(nav(QStringLiteral("null"), "down", 3, 5), QStringLiteral("-"));
    }

    // Cards side by side (a widget's sideways strip, a large widget's two
    // columns) share a line: Left and Right go along it, Up and Down leave it
    // for the line above or below, to the card in the same place or the
    // nearest (basecamp-experiments#48 review).
    void keyboardNavigationByLine()
    {
        auto row = [](int i) { return QStringLiteral("{ where: 'row', index: %1 }").arg(i); };
        // Widgets view: an app row (0), a strip of four (1-4), a widget below (5).
        const QString strip = QStringLiteral("[0, 1, 1, 1, 1, 2]");
        QCOMPARE(nav(row(1), "right", 0, 6, 0, strip), QStringLiteral("row:2"));
        QCOMPARE(nav(row(4), "right", 0, 6, 0, strip), QStringLiteral("row:4"));
        QCOMPARE(nav(row(2), "left", 0, 6, 0, strip), QStringLiteral("row:1"));
        QCOMPARE(nav(row(1), "left", 0, 6, 0, strip), QStringLiteral("row:1"));
        // Down from anywhere in the strip: the widget below, not the next card.
        QCOMPARE(nav(row(3), "down", 0, 6, 0, strip), QStringLiteral("row:5"));
        QCOMPARE(nav(row(1), "down", 0, 6, 0, strip), QStringLiteral("row:5"));
        // Up from the strip: the line above; from below: into the strip.
        QCOMPARE(nav(row(4), "up", 0, 6, 0, strip), QStringLiteral("row:0"));
        QCOMPARE(nav(row(5), "up", 0, 6, 0, strip), QStringLiteral("row:1"));
        QCOMPARE(nav(row(0), "down", 0, 6, 0, strip), QStringLiteral("row:1"));
        // A line of one: Left and Right stay the card's own.
        QCOMPARE(nav(row(5), "right", 0, 6, 0, strip), QStringLiteral("-"));
        QCOMPARE(nav(row(0), "left", 0, 6, 0, strip), QStringLiteral("-"));
        // The last line: Down stays; the first: Up goes to the tiles or the search.
        QCOMPARE(nav(row(5), "down", 0, 6, 0, strip), QStringLiteral("row:5"));
        QCOMPARE(nav(row(0), "up", 3, 6, 1, strip), QStringLiteral("tile:1"));
        QCOMPARE(nav(row(2), "up", 0, 3, 0, QStringLiteral("[0, 0, 0]")), QStringLiteral("search"));

        // A large widget's two columns: pairs, and an odd one out at the end.
        const QString pairs = QStringLiteral("[0, 0, 1, 1, 2]");
        QCOMPARE(nav(row(1), "down", 0, 5, 0, pairs), QStringLiteral("row:3"));
        QCOMPARE(nav(row(0), "down", 0, 5, 0, pairs), QStringLiteral("row:2"));
        QCOMPARE(nav(row(3), "down", 0, 5, 0, pairs), QStringLiteral("row:4"));
        QCOMPARE(nav(row(4), "up", 0, 5, 0, pairs), QStringLiteral("row:2"));
        QCOMPARE(nav(row(3), "up", 0, 5, 0, pairs), QStringLiteral("row:1"));
        QCOMPARE(nav(row(2), "right", 0, 5, 0, pairs), QStringLiteral("row:3"));
        QCOMPARE(nav(row(3), "left", 0, 5, 0, pairs), QStringLiteral("row:2"));
        // Lines that do not match the rows (a stale list) are not used.
        QCOMPARE(nav(row(1), "down", 0, 5, 0, QStringLiteral("[0, 0]")), QStringLiteral("row:2"));
        // The focused row past the end (rows shrank): clamped, then moved.
        QCOMPARE(nav(row(9), "up", 0, 5, 0, pairs), QStringLiteral("row:2"));
    }

    void typingGoesToTheSearch()
    {
        const int shift = 0x02000000, ctrl = 0x04000000, alt = 0x08000000, meta = 0x10000000;
        // A script error would read as true below: every answer must be a bool.
        QVERIFY(h("typeof typesText('w', 0) === 'boolean' && typeof typesText('\\r', 0) === 'boolean'").toBool());
        QVERIFY(h("typesText('w', 0)").toBool());
        QVERIFY(h(QStringLiteral("typesText('W', %1)").arg(shift)).toBool());
        QVERIFY(h("typesText(' ', 0)").toBool());
        QVERIFY(h("typesText('\\u00e9', 0)").toBool());
        QVERIFY(!h(QStringLiteral("typesText('k', %1)").arg(ctrl)).toBool());
        QVERIFY(!h(QStringLiteral("typesText('k', %1)").arg(alt)).toBool());
        QVERIFY(!h(QStringLiteral("typesText('k', %1)").arg(meta)).toBool());
        // Arrows, Enter, Esc, Tab, Backspace and Delete type nothing.
        QVERIFY(!h("typesText('', 0)").toBool());
        QVERIFY(!h("typesText('\\r', 0)").toBool());
        QVERIFY(!h("typesText('\\u001b', 0)").toBool());
        QVERIFY(!h("typesText('\\t', 0)").toBool());
        QVERIFY(!h("typesText('\\b', 0)").toBool());
        QVERIFY(!h("typesText('\\u007f', 0)").toBool());
        QVERIFY(!h("typesText(undefined, 0)").toBool());
    }
};

QTEST_GUILESS_MAIN(ObjectCardsTest)
#include "object_cards_test.moc"
