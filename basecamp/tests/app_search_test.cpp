// srcdeps: AppSearch.cpp ObjectRanker.cpp
//
// Unit tests for the apps in home's search results (app/AppSearch.h,
// basecamp-experiments#32): which installed and catalog apps a query finds,
// each one's state, and the order.
//
// Run: nix build .#unit-tests -L

#include "../app/AppSearch.h"
#include "InstallEnums.h"

#include <QtTest/QtTest>

using AppSearch::Candidate;

namespace {
QVariantMap installed(const QString& name, const QString& title, bool running = false)
{
    return { { "name", name }, { "displayName", title }, { "isLoaded", running },
             { "iconPath", "file:///apps/" + name + "/icon.png" } };
}

QVariantMap catalog(const QString& name, const QString& title, bool installable = true,
                    int stage = InstallStage::None, const QString& type = "ui_qml")
{
    return { { "name", name }, { "displayName", title }, { "type", type },
             { "iconUrl", "https://example.org/" + name + ".png" },
             { "hasInstallableVersion", installable }, { "installStage", stage } };
}

QStringList names(const QList<Candidate>& list)
{
    QStringList out;
    for (const Candidate& c : list) out << c.name;
    return out;
}

QString stateOf(const QList<Candidate>& list, const QString& name)
{
    for (const Candidate& c : list)
        if (c.name == name) return c.state;
    return {};
}
}

class AppSearchTest : public QObject {
    Q_OBJECT

private slots:
    void emptyQueryFindsNothing()
    {
        const auto all = AppSearch::candidates({ installed("lez_wallet", "LEZ Wallet") }, {}, {});
        QVERIFY(AppSearch::match(all, "").isEmpty());
        QVERIFY(AppSearch::match(all, "  ,. ").isEmpty());
    }

    // "wal" starts a word of the title; "allet" starts none.
    void prefixOfAWord()
    {
        const auto all = AppSearch::candidates(
            { installed("lez_wallet", "LEZ Wallet"), installed("chat", "Chat") }, {}, {});
        QCOMPARE(names(AppSearch::match(all, "wal")), QStringList{ "lez_wallet" });
        QCOMPARE(names(AppSearch::match(all, "WAL")), QStringList{ "lez_wallet" });
        QVERIFY(AppSearch::match(all, "allet").isEmpty());
    }

    // The name's words count too, split like the title's.
    void matchesTheNameToo()
    {
        const auto all = AppSearch::candidates({ installed("objects_radio", "Booth") }, {}, {});
        QCOMPARE(names(AppSearch::match(all, "radio")), QStringList{ "objects_radio" });
    }

    void everyTokenMustMatch()
    {
        const auto all = AppSearch::candidates(
            { installed("lez_wallet", "LEZ Wallet"), installed("eth_wallet", "Ethereum Wallet") }, {}, {});
        QCOMPARE(names(AppSearch::match(all, "wallet eth")), QStringList{ "eth_wallet" });
        QVERIFY(AppSearch::match(all, "wallet xyz").isEmpty());
    }

    void statesFromInstalledAndCatalog()
    {
        const auto all = AppSearch::candidates(
            { installed("lez_wallet", "LEZ Wallet", true), installed("eth_wallet", "Ethereum Wallet") },
            { catalog("monero_wallet", "Monero Wallet"),
              catalog("zcash_wallet", "Zcash Wallet", true, InstallStage::Downloading) },
            {});
        const auto found = AppSearch::match(all, "wallet");
        QCOMPARE(found.size(), 4);
        QCOMPARE(stateOf(found, "lez_wallet"), QString("running"));
        QCOMPARE(stateOf(found, "eth_wallet"), QString("installed"));
        QCOMPARE(stateOf(found, "monero_wallet"), QString("available"));
        QCOMPARE(stateOf(found, "zcash_wallet"), QString("installing"));
    }

    // An installed app's catalog row adds nothing; one row per app.
    void installedWinsOverCatalog()
    {
        const auto all = AppSearch::candidates(
            { installed("lez_wallet", "LEZ Wallet") },
            { catalog("lez_wallet", "LEZ Wallet"), catalog("chat", "Chat"), catalog("chat", "Chat") },
            {});
        QCOMPARE(all.size(), 2);
        QCOMPARE(stateOf(all, "lez_wallet"), QString("installed"));
        QCOMPARE(all[0].icon, QString("file:///apps/lez_wallet/icon.png"));
    }

    // Two repositories offer it and one is installing it: that one shows.
    void installingRowWins()
    {
        const auto all = AppSearch::candidates(
            {}, { catalog("chat", "Chat"), catalog("chat", "Chat", true, InstallStage::Installing) }, {});
        QCOMPARE(all.size(), 1);
        QCOMPARE(all[0].state, QString("installing"));
    }

    void skipsWhatCannotBeOffered()
    {
        const auto all = AppSearch::candidates(
            { installed("basecamp_home", "Home") },
            { catalog("wallet_core", "Wallet core", true, InstallStage::None, "core"),
              catalog("old_wallet", "Old Wallet", false),
              catalog("basecamp_search", "Search") },
            { "basecamp_home", "basecamp_search" });
        QVERIFY(all.isEmpty());
    }

    // All tokens in the title first, then a leading word, then running,
    // installed, installing, available, then by title.
    void order()
    {
        const auto all = AppSearch::candidates(
            { installed("a_notes", "Notes Wallet"), installed("b", "Wallet Two"),
              installed("c", "Wallet One", true), installed("wallet_d", "Keys") },
            { catalog("e", "Wallet Alpha"), catalog("f", "Wallet Beta", true, InstallStage::Queued) },
            {});
        QCOMPARE(names(AppSearch::match(all, "wallet")),
                 (QStringList{ "c", "b", "f", "e", "a_notes", "wallet_d" }));
    }

    void capped()
    {
        QVariantList rows;
        for (int i = 0; i < 10; ++i) rows << installed(QString("app%1").arg(i), QString("App %1").arg(i));
        const auto all = AppSearch::candidates(rows, {}, {});
        QCOMPARE(AppSearch::match(all, "app", 3).size(), 3);
        QCOMPARE(AppSearch::match(all, "app").size(), 10);
    }

    void titleFallsBackToName()
    {
        const auto all = AppSearch::candidates({ QVariantMap{ { "name", "radio" } } },
                                               { catalog("zine", "") }, {});
        QCOMPARE(all[0].title, QString("radio"));
        QCOMPARE(all[1].title, QString("zine"));
    }

    // Related apps under a type pill (#36): installed apps holding objects of
    // the type, most first; catalog rows and apps without any never.
    void relatedInstalledOnly()
    {
        const auto all = AppSearch::candidates(
            { installed("objects_wallet", "Stub Wallet"), installed("objects_explorer", "Stub Explorer", true),
              installed("objects_chat", "Stub Chat") },
            { catalog("eth_wallet_ui", "ETH Wallet") }, {});
        const QHash<QString, int> counts{ { "objects_wallet", 8 }, { "objects_explorer", 5 },
                                          { "eth_wallet_ui", 3 }, { "gone_app", 2 } };
        const auto found = AppSearch::related(all, counts);
        QCOMPARE(names(found), (QStringList{ "objects_wallet", "objects_explorer" }));
        QCOMPARE(stateOf(found, "objects_explorer"), QString("running"));
        QVERIFY(AppSearch::related(all, {}).isEmpty());
        QCOMPARE(AppSearch::related(all, counts, 1).size(), 1);
    }

    void relatedTiesRunningThenTitle()
    {
        const auto all = AppSearch::candidates(
            { installed("b_app", "Beta"), installed("a_app", "Alpha"), installed("c_app", "Gamma", true) }, {}, {});
        const QHash<QString, int> counts{ { "a_app", 2 }, { "b_app", 2 }, { "c_app", 2 } };
        QCOMPARE(names(AppSearch::related(all, counts)), (QStringList{ "c_app", "a_app", "b_app" }));
    }
};

QTEST_GUILESS_MAIN(AppSearchTest)
#include "app_search_test.moc"
