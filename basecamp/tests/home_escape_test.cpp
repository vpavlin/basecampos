// Unit tests for Esc on the desktop layout's Basecamp overlay and Activity
// sidebar (src/HomeEscape.h, basecamp-experiments#47): the shell asks home's
// QML root to clear its search first and closes only when home had nothing
// to clear; a key that went outside home's view closes at once; a dialog
// over home takes the key; a home app without
// escapeSearch() (or with an odd answer) gets the old close.
//
// Run: nix build .#unit-tests -L

#include "HomeEscape.h"

#include <QDir>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QUrl>
#include <QtTest/QtTest>

#include <memory>

class HomeEscapeTest : public QObject {
    Q_OBJECT

    QQmlEngine m_engine;

    // A stand-in home root: `text` is its search, escapeSearch() as in
    // apps/basecamp_home/qml (clear and say so, or say there was nothing).
    std::unique_ptr<QObject> home(const QString& body)
    {
        QQmlComponent c(&m_engine);
        c.setData(QStringLiteral("import QtQml\nQtObject {\n%1\n}\n").arg(body).toUtf8(),
                  QUrl::fromLocalFile(QDir::current().absoluteFilePath(QStringLiteral("home.qml"))));
        std::unique_ptr<QObject> o(c.create());
        if (!o)
            qWarning() << c.errorString();
        return o;
    }

    std::unique_ptr<QObject> realHome(const QString& text)
    {
        auto o = home(QStringLiteral(
            "property string text: \"\"\n"
            "property int asked: 0\n"
            "function escapeSearch() {\n"
            "    asked++\n"
            "    if (text.length === 0) return false\n"
            "    text = \"\"\n"
            "    return true\n"
            "}"));
        if (o)
            o->setProperty("text", text);
        return o;
    }

private slots:
    void textIsClearedAndTheSurfaceStays()
    {
        auto h = realHome(QStringLiteral("ada"));
        QVERIFY(h);
        QCOMPARE(HomeEscape::onEscape(true, false, true, h.get()), HomeEscape::Action::Cleared);
        QCOMPARE(h->property("text").toString(), QString());
    }

    void secondEscCloses()
    {
        auto h = realHome(QStringLiteral("ada"));
        QVERIFY(h);
        QCOMPARE(HomeEscape::onEscape(true, false, true, h.get()), HomeEscape::Action::Cleared);
        QCOMPARE(HomeEscape::onEscape(true, false, true, h.get()), HomeEscape::Action::Close);
        QCOMPARE(h->property("asked").toInt(), 2);
    }

    void emptyFieldCloses()
    {
        auto h = realHome(QString());
        QVERIFY(h);
        QCOMPARE(HomeEscape::onEscape(true, false, true, h.get()), HomeEscape::Action::Close);
    }

    void dialogTakesTheKeyAndHomeIsNotAsked()
    {
        auto h = realHome(QStringLiteral("ada"));
        QVERIFY(h);
        QCOMPARE(HomeEscape::onEscape(true, true, true, h.get()), HomeEscape::Action::Ignore);
        QCOMPARE(h->property("asked").toInt(), 0);
        QCOMPARE(h->property("text").toString(), QStringLiteral("ada"));
    }

    void closedSurfaceIgnoresEsc()
    {
        auto h = realHome(QStringLiteral("ada"));
        QVERIFY(h);
        QCOMPARE(HomeEscape::onEscape(false, false, true, h.get()), HomeEscape::Action::Ignore);
        QCOMPARE(h->property("asked").toInt(), 0);
    }

    // The key went elsewhere (the dock, the top bar): home is not asked, and
    // whatever home holds (text, a focused result) does not keep it open.
    void keyOutsideHomeClosesAndHomeIsNotAsked()
    {
        auto h = realHome(QStringLiteral("ada"));
        QVERIFY(h);
        QCOMPARE(HomeEscape::onEscape(true, false, false, h.get()), HomeEscape::Action::Close);
        QCOMPARE(h->property("asked").toInt(), 0);
        QCOMPARE(h->property("text").toString(), QStringLiteral("ada"));
        QCOMPARE(HomeEscape::onEscape(true, true, false, h.get()), HomeEscape::Action::Ignore);
    }

    // Home is any app the user picks for the role: without the function, or
    // answering something other than true, Esc closes as it always did.
    void homeWithoutTheFunctionCloses()
    {
        auto h = home(QStringLiteral("property string text: \"ada\""));
        QVERIFY(h);
        QCOMPARE(HomeEscape::onEscape(true, false, true, h.get()), HomeEscape::Action::Close);
        QCOMPARE(HomeEscape::onEscape(true, false, true, nullptr), HomeEscape::Action::Close);
    }

    void onlyTrueCountsAsCleared()
    {
        for (const QString& answer : { QStringLiteral("1"), QStringLiteral("\"yes\""),
                                       QStringLiteral("undefined"), QStringLiteral("false") }) {
            auto h = home(QStringLiteral("function escapeSearch() { return %1 }").arg(answer));
            QVERIFY(h);
            QCOMPARE(HomeEscape::onEscape(true, false, true, h.get()), HomeEscape::Action::Close);
        }
    }

    // Phone Back (#51): home in edit mode leaves it and keeps the key, so
    // the shell does not go Back; out of edit mode, or a home app without
    // escapeEdit(), Back goes on as before.
    void backLeavesEditModeFirst()
    {
        auto h = home(QStringLiteral(
            "property bool editing: true\n"
            "function escapeEdit() { if (!editing) return false; editing = false; return true }"));
        QVERIFY(h);
        QVERIFY(HomeEscape::leaveEditInHome(h.get()));
        QCOMPARE(h->property("editing").toBool(), false);
        QVERIFY(!HomeEscape::leaveEditInHome(h.get()));
        auto plain = home(QStringLiteral("property bool editing: true"));
        QVERIFY(plain);
        QVERIFY(!HomeEscape::leaveEditInHome(plain.get()));
        QVERIFY(!HomeEscape::leaveEditInHome(nullptr));
    }
};

QTEST_MAIN(HomeEscapeTest)
#include "home_escape_test.moc"
