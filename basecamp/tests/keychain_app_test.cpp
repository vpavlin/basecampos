// Keychain, part 3: the Keychain app (apps/basecamp_keychain,
// basecamp-experiments#57). Its pure JS, apps/basecamp_keychain/qml/
// KeychainFormat.js, runs in a bare QJSEngine as QML runs a `.pragma
// library` script (object_cards_test does the same for home): the key's
// name with the user's label first, grouping by owner, the details line,
// "Added …", the preselected key of a request, the label and note limits.
// And the app's package: it claims the `keychain` role, its two manifests
// agree, its QML files have names no other app ships. The QML around it is
// checked headlessly.

#include <QDir>
#include <QFile>
#include <QJSEngine>
#include <QJSValue>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

namespace {

constexpr qint64 kMinute = 60 * 1000;
constexpr qint64 kDay = 24 * 60 * kMinute;

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

QJsonObject readJson(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

// Two keys of one app, one of another, in registration order.
const char* kRows =
    "[ { ownerApp: 'stub_signer', ownerName: 'Stub Signer', ownerIcon: 'image://basecamp/appicon/stub_signer',"
    "    ownerFullBleed: true, keyId: 'daily', name: 'Daily key', label: '', note: '',"
    "    fingerprint: 'ABCDEF\\u2026UVWXYZ', algorithm: 'ed25519', purposes: ['message', 'login'], added: 0 },"
    "  { ownerApp: 'other_signer', ownerName: 'Other Signer', keyId: 'any', name: 'Any key',"
    "    label: 'Hiking', note: 'first line\\nsecond   line', fingerprint: '', algorithm: 'ed25519',"
    "    purposes: [], added: 0 },"
    "  { ownerApp: 'stub_signer', ownerName: 'Stub Signer', keyId: 'cold', name: 'Cold key',"
    "    label: 'Savings', note: '', fingerprint: '', algorithm: 'secp256k1',"
    "    purposes: ['transaction'], added: 0 } ]";

} // namespace

class KeychainAppTest : public QObject {
    Q_OBJECT

    QJSEngine m_js;
    QString m_appDir;

    QJSValue js(const QString& expr) { return m_js.evaluate(expr); }
    QString jss(const QString& expr) { return js(expr).toString(); }

private slots:
    void initTestCase()
    {
        m_appDir = QFINDTESTDATA("../apps/basecamp_keychain");
        QVERIFY(!m_appDir.isEmpty());
        QVERIFY(load(m_js, m_appDir + QStringLiteral("/qml/KeychainFormat.js")));
        QVERIFY(!js(QStringLiteral("var rows = %1;").arg(QString::fromUtf8(kRows))).isError());
    }

    void labelIsTheNameWhenSet()
    {
        QCOMPARE(jss("title(rows[0])"), QStringLiteral("Daily key"));
        QCOMPARE(jss("subtitle(rows[0])"), QString());
        QCOMPARE(jss("title(rows[1])"), QStringLiteral("Hiking"));
        QCOMPARE(jss("subtitle(rows[1])"), QStringLiteral("Any key"));   // the app's own name underneath
        QCOMPARE(jss("title(undefined)"), QString());
    }

    void groupedByOwnerInRegistrationOrder()
    {
        QCOMPARE(js("groups(rows).length").toInt(), 2);
        QCOMPARE(jss("groups(rows)[0].ownerApp"), QStringLiteral("stub_signer"));
        QCOMPARE(jss("groups(rows)[0].keys.map(k => k.keyId).join(',')"), QStringLiteral("daily,cold"));
        QCOMPARE(jss("groups(rows)[1].ownerName"), QStringLiteral("Other Signer"));
        QCOMPARE(js("groups(rows)[0].ownerFullBleed").toBool(), true);
        QCOMPARE(js("groups(rows)[1].ownerFullBleed").toBool(), false);
        QCOMPARE(js("groups([]).length").toInt(), 0);
        QCOMPARE(js("groups(undefined).length").toInt(), 0);
    }

    void detailsLine()
    {
        QCOMPARE(jss("details(rows[0], 'Any use')"),
                 QStringLiteral("Ed25519 · ABCDEF…UVWXYZ · Message, Login"));
        QCOMPARE(jss("details(rows[1], 'Any use')"), QStringLiteral("Ed25519 · Any use"));
        QCOMPARE(jss("details(rows[2], 'Any use')"), QStringLiteral("secp256k1 · Transaction"));
    }

    void addedAgo()
    {
        const qint64 now = 100 * kDay;
        const auto age = [&](qint64 ago) {
            return jss(QStringLiteral("(function(){ var a = age(%1, %2); return a.unit + ':' + (a.date || a.n) })()")
                           .arg(now - ago).arg(now));
        };
        QCOMPARE(age(10 * 1000), QStringLiteral("now:0"));
        QCOMPARE(age(3 * kMinute), QStringLiteral("min:3"));
        QCOMPARE(age(125 * kMinute), QStringLiteral("h:2"));
        QCOMPARE(age(3 * kDay), QStringLiteral("d:3"));
        QCOMPARE(age(30 * kDay), QStringLiteral("date:1970-03-12"));
        QCOMPARE(age(-5 * kMinute), QStringLiteral("now:0"));   // a clock ahead of ours
    }

    void requestStartsOnTheLastUsedKey()
    {
        QCOMPARE(js("preselect([], {})").toInt(), -1);
        QCOMPARE(js("preselect(rows, {})").toInt(), 0);
        QCOMPARE(js("preselect(rows, { ownerApp: 'other_signer', keyId: 'any' })").toInt(), 1);
        // Same key id, another app: not the same key.
        QCOMPARE(js("preselect(rows, { ownerApp: 'other_signer', keyId: 'daily' })").toInt(), 0);
        QCOMPARE(js("preselect(rows, undefined)").toInt(), 0);
    }

    void labelAndNoteLimits()
    {
        QCOMPARE(js("maxLabel").toInt(), 40);
        QCOMPARE(js("maxNote").toInt(), 500);
        QVERIFY(js("fits('x'.repeat(40), maxLabel)").toBool());
        QVERIFY(!js("fits('x'.repeat(41), maxLabel)").toBool());
        QVERIFY(js("fits('', maxLabel)").toBool());
        QVERIFY(js("fits('x'.repeat(500), maxNote)").toBool());
        QVERIFY(!js("fits('x'.repeat(501), maxNote)").toBool());
        QCOMPARE(jss("oneLine(rows[1].note)"), QStringLiteral("first line second line"));
    }

    void packageClaimsTheKeychainRole()
    {
        const QJsonObject meta = readJson(m_appDir + QStringLiteral("/metadata.json"));
        QCOMPARE(meta.value("name").toString(), QStringLiteral("basecamp_keychain"));
        QCOMPARE(meta.value("type").toString(), QStringLiteral("ui_qml"));
        QCOMPARE(meta.value("category").toString(), QStringLiteral("system"));
        QCOMPARE(meta.value("roles").toArray(), QJsonArray{ QStringLiteral("keychain") });
        QVERIFY(QFile::exists(m_appDir + QLatin1Char('/') + meta.value("view").toString()));
        QVERIFY(QFile::exists(m_appDir + QLatin1Char('/') + meta.value("icon").toString()));
        // The package and the bundled copy read the same file.
        QCOMPARE(readJson(m_appDir + QStringLiteral("/manifest.json")), meta);
        // It asks the shell for nothing as an app: everything goes through
        // the role-gated `shell`.
        QVERIFY(!meta.contains("uses"));
        QVERIFY(!meta.contains("provides"));
    }

    void qmlFileNamesAreItsOwn()
    {
        // Two ui_qml apps shipping a file of the same name collide in the
        // host (the second resolves the first's). Main.qml is every app's
        // entry and loaded by path.
        const QDir apps(QFINDTESTDATA("../apps"));
        QStringList ours = QDir(m_appDir + QStringLiteral("/qml")).entryList({ "*.qml", "*.js" }, QDir::Files);
        ours.removeAll(QStringLiteral("Main.qml"));
        QVERIFY(!ours.isEmpty());
        for (const QString& f : ours) QVERIFY2(f.startsWith(QStringLiteral("Keychain")), qPrintable(f));
        for (const QString& other : apps.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            if (other == QStringLiteral("basecamp_keychain")) continue;
            const QStringList theirs = QDir(apps.filePath(other) + QStringLiteral("/qml"))
                                           .entryList({ "*.qml", "*.js" }, QDir::Files);
            for (const QString& f : ours) QVERIFY2(!theirs.contains(f), qPrintable(other + "/" + f));
        }
    }
};

QTEST_GUILESS_MAIN(KeychainAppTest)
#include "keychain_app_test.moc"
