// srcdeps: RecentlyClosedStore.cpp
//
// Unit tests for home's Recent apps (app/RecentApps.h, basecamp-experiments#30):
// the recently closed and recently installed lists merged newest first, and
// the watch that decides an app was just installed.
//
// Run: nix build .#unit-tests -L

#include "../app/RecentApps.h"

#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QTimeZone>

using RecentApps::InstallWatch;
using RecentApps::Item;
using Entry = RecentlyClosedStore::Entry;

namespace {
QDateTime at(int minute)
{
    return QDateTime(QDate(2026, 10, 25), QTime(12, minute), QTimeZone::UTC);
}

QStringList names(const QList<Item>& items)
{
    QStringList out;
    for (const Item& i : items) out << i.name;
    return out;
}
}

class RecentAppsTest : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;

private slots:
    void emptyWhenNothingHappened()
    {
        QVERIFY(RecentApps::merge({}, {}).isEmpty());
    }

    // Closed and installed interleave by time, newest first.
    void mergesNewestFirst()
    {
        const QList<Entry> closed{{"wallet", at(30)}, {"chat", at(10)}};
        const QList<Entry> installed{{"radio", at(20)}, {"notes", at(5)}};
        const QList<Item> out = RecentApps::merge(closed, installed);
        QCOMPARE(names(out), (QStringList{"wallet", "radio", "chat", "notes"}));
        QCOMPARE(out.at(0).kind, QStringLiteral("closed"));
        QCOMPARE(out.at(1).kind, QStringLiteral("installed"));
    }

    // Installed, then closed later: one row, the close.
    void oneRowPerAppLatestWins()
    {
        const QList<Item> out = RecentApps::merge({{"radio", at(40)}}, {{"radio", at(20)}});
        QCOMPARE(out.size(), 1);
        QCOMPARE(out.at(0).kind, QStringLiteral("closed"));

        const QList<Item> back = RecentApps::merge({{"radio", at(20)}}, {{"radio", at(40)}});
        QCOMPARE(back.size(), 1);
        QCOMPARE(back.at(0).kind, QStringLiteral("installed"));
    }

    // A hand-edited file with no time keeps the app, after the dated ones.
    void undatedSortLast()
    {
        const QList<Item> out = RecentApps::merge({{"old", QDateTime()}, {"chat", at(1)}},
                                                  {{"radio", at(2)}});
        QCOMPARE(names(out), (QStringList{"radio", "chat", "old"}));
    }

    void blankNamesDropped()
    {
        QCOMPARE(names(RecentApps::merge({{"", at(5)}}, {{"radio", at(1)}})),
                 QStringList{"radio"});
    }

    // Everything in the first list was installed before this run.
    void firstListOnlyPrimes()
    {
        InstallWatch w;
        QVERIFY(w.update({"wallet", "chat"}).isEmpty());
        QCOMPARE(w.update({"wallet", "chat", "radio"}), QStringList{"radio"});
        QVERIFY(w.update({"wallet", "chat", "radio"}).isEmpty());
    }

    // A refresh that briefly sees nothing does not make every app new.
    void emptyListIgnored()
    {
        InstallWatch w;
        QVERIFY(w.update({}).isEmpty());          // not primed by an empty list
        QVERIFY(w.update({"wallet"}).isEmpty());  // this one primes
        QVERIFY(w.update({}).isEmpty());
        QVERIFY(w.update({"wallet"}).isEmpty());
    }

    // Uninstalled, then installed again: new again.
    void reinstallCountsAgain()
    {
        InstallWatch w;
        w.update({"wallet", "radio"});
        QVERIFY(w.update({"wallet"}).isEmpty());
        QCOMPARE(w.update({"wallet", "radio"}), QStringList{"radio"});
    }

    // The installed list is the same store under its own key: both can share
    // a directory (or a file) without reading each other.
    void installedStoreKeepsItsOwnKey()
    {
        QVERIFY(m_dir.isValid());
        const QString file = m_dir.filePath("shared.ini");
        {
            RecentlyClosedStore closed(file);
            RecentlyClosedStore installed(file, QStringLiteral("recentlyInstalled"));
            closed.record("wallet");
            installed.record("radio");
        }
        QCOMPARE(RecentlyClosedStore(file).names(), QStringList{"wallet"});
        QCOMPARE(RecentlyClosedStore(file, QStringLiteral("recentlyInstalled")).names(),
                 QStringList{"radio"});
    }
};

QTEST_MAIN(RecentAppsTest)
#include "recent_apps_test.moc"
