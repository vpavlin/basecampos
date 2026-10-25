// What the search matched, marked (basecamp-experiments#46), as a Text draws
// it: the StyledText that src/Basecamp/Objects/ObjectHighlight.js makes from
// publisher text lays out exactly as the plain text does, so nothing an app
// sent is read as markup (ADR-072: no bold, no image, no link), and marking a
// match moves nothing (colour only). The strings themselves are checked in
// object_cards_test; this runs them through a real QtQuick Text.

#include <QDir>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QUrl>
#include <QtTest>

#include <memory>

class ObjectHighlightTest : public QObject {
    Q_OBJECT

    QQmlEngine m_engine;
    std::unique_ptr<QObject> m_root;
    QQuickItem* m_plain = nullptr;
    QQuickItem* m_styled = nullptr;

    // Lays `text` out as plain text and as mark(text, query) in StyledText.
    void layOut(const QString& text, const QString& query)
    {
        QMetaObject::invokeMethod(m_root.get(), "show", Q_ARG(QVariant, text), Q_ARG(QVariant, query));
    }

    void sameAsPlain(const QString& text, const QString& query)
    {
        layOut(text, query);
        const QByteArray what = (text + QStringLiteral(" / ") + query).toUtf8();
        QVERIFY2(m_plain->implicitWidth() > 0, what.constData());
        QVERIFY2(qFuzzyCompare(m_styled->implicitWidth(), m_plain->implicitWidth()), what.constData());
        QVERIFY2(qFuzzyCompare(m_styled->implicitHeight(), m_plain->implicitHeight()), what.constData());
        QCOMPARE(m_styled->property("lineCount").toInt(), m_plain->property("lineCount").toInt());
    }

private slots:
    void initTestCase()
    {
        const QString js = QFINDTESTDATA("../src/Basecamp/Objects/ObjectHighlight.js");
        QVERIFY(!js.isEmpty());
        QQmlComponent c(&m_engine);
        c.setData(QStringLiteral(
            "import QtQuick\n"
            "import \"%1\" as H\n"
            "Item {\n"
            "    property alias plain: p\n"
            "    property alias styled: s\n"
            "    function show(text, query) {\n"
            "        p.text = text\n"
            "        s.text = H.mark(text, query, \"#ed7b58\")\n"
            "    }\n"
            "    Text { id: p; textFormat: Text.PlainText; font.pixelSize: 15 }\n"
            "    Text { id: s; textFormat: Text.StyledText; font.pixelSize: 15 }\n"
            "}\n").arg(QUrl::fromLocalFile(js).toString()).toUtf8(),
            QUrl::fromLocalFile(QDir::current().absoluteFilePath(QStringLiteral("highlight.qml"))));
        m_root.reset(c.create());
        QVERIFY2(m_root, qPrintable(c.errorString()));
        m_plain = m_root->property("plain").value<QQuickItem*>();
        m_styled = m_root->property("styled").value<QQuickItem*>();
        QVERIFY(m_plain && m_styled);
    }

    void publisherMarkupStaysText_data()
    {
        QTest::addColumn<QString>("text");
        QTest::newRow("bold") << QStringLiteral("<b>Ada</b> Morrow");
        QTest::newRow("entity") << QStringLiteral("Fish &amp; chips & co");
        QTest::newRow("image") << QStringLiteral("Hi <img src=\"file:///nowhere.png\" width=400 height=400> Ada");
        QTest::newRow("link") << QStringLiteral("<a href=\"https://example.com\">Wallet</a>");
        QTest::newRow("font") << QStringLiteral("<font color=\"red\" size=\"7\">Ada</font>");
        QTest::newRow("breaks") << QStringLiteral("Ada\nMorrow\nthree");
        QTest::newRow("spaces") << QStringLiteral("Ada   Morrow");
    }
    void publisherMarkupStaysText()
    {
        QFETCH(QString, text);
        sameAsPlain(text, QString());
        sameAsPlain(text, QStringLiteral("ada"));
        sameAsPlain(text, QStringLiteral("img wal font b"));
    }

    // The issue's cases draw no wider or narrower when marked.
    void markingMovesNothing()
    {
        sameAsPlain(QStringLiteral("Sent 12.00 TST to Ada Morrow"), QStringLiteral("ada"));
        sameAsPlain(QStringLiteral("Stub Wallet"), QStringLiteral("wal"));
        sameAsPlain(QStringLiteral("LEZ Wallet"), QStringLiteral("wal"));
    }
};

QTEST_MAIN(ObjectHighlightTest)
#include "object_highlight_test.moc"
