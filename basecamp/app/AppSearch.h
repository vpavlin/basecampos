#pragma once

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantList>

// Apps in home's search results (basecamp-experiments#32): which installed and
// catalog apps match what is typed, and what each one's row offers.
//
// The rules:
//   * The query is split into tokens like an object search (ObjectRanker::
//     tokens). Every token must start a word of the app's title or of its
//     name ("wal" finds "LEZ Wallet" and lez_wallet); an empty query finds
//     nothing.
//   * One row per app name. An installed app is that row; a catalog row only
//     adds an app that is not installed, and only one this host can install.
//   * state: "running" (open), "installed", "installing" (the install
//     pipeline is busy with it) or "available" (in the catalog).
//   * Best first: every token in the title before a match on the name only,
//     then a title that starts with the first token, then running, installed,
//     installing, available, then title, then name.
//
// Pure functions of their input: unit-tested in app_search_test.cpp.
namespace AppSearch {

struct Candidate {
    QString name;
    QString title;          // display name; the name when empty
    QString icon;           // the row's icon source as the host knows it
    bool    fullBleed = false;
    QString state;          // see above
};

// `installed` (rows in launcherApps' shape) and `catalog` (AppsModel rows:
// name, displayName, type, iconUrl, hasInstallableVersion, installStage) as
// candidates. `hidden` names apps never offered (home itself, the ranker).
QList<Candidate> candidates(const QVariantList& installed, const QVariantList& catalog,
                            const QStringList& hidden);

// The candidates matching `query`, best first, at most `max`.
QList<Candidate> match(const QList<Candidate>& candidates, const QString& query, int max = 50);

// Related apps under a type pill (basecamp-experiments#36): the installed
// candidates (running or installed, never a catalog row) that hold objects of
// the type, from `counts` (app -> how many, ObjectStore::countsOfType). Most
// objects first, then running before installed, then title, then name.
QList<Candidate> related(const QList<Candidate>& candidates, const QHash<QString, int>& counts,
                         int max = 50);

} // namespace AppSearch
