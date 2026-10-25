#pragma once

#include "RecentlyClosedStore.h"

#include <QDateTime>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

#include <algorithm>

// Home's Recent apps (basecamp-experiments#30): the apps closed and the apps
// installed lately, in one list, newest first. Pure functions over the two
// stores' entries so the order and the install detection are testable without
// a running host.
namespace RecentApps {

struct Item {
    QString   name;
    QDateTime at;
    QString   kind;   // "closed" or "installed"
};

// Both lists merged by time, newest first, one row per app (its latest
// event wins). An entry without a valid time sorts after every dated one,
// keeping its place among the undated.
inline QList<Item> merge(const QList<RecentlyClosedStore::Entry>& closed,
                         const QList<RecentlyClosedStore::Entry>& installed)
{
    QList<Item> all;
    for (const auto& e : closed)    all.append({e.name, e.closedAt, QStringLiteral("closed")});
    for (const auto& e : installed) all.append({e.name, e.closedAt, QStringLiteral("installed")});
    std::stable_sort(all.begin(), all.end(), [](const Item& a, const Item& b) {
        if (a.at.isValid() != b.at.isValid()) return a.at.isValid();
        return a.at.isValid() && a.at > b.at;
    });
    QList<Item> out;
    QSet<QString> seen;
    for (const Item& it : all) {
        if (it.name.isEmpty() || seen.contains(it.name)) continue;
        seen.insert(it.name);
        out.append(it);
    }
    return out;
}

// Tracks the installed UI apps across plugin-list refreshes and names the ones
// that appeared since the last refresh: those were just installed (from the
// catalog, a local .lgx, anything that reaches the package manager). The first
// list only primes it, since everything there was installed before this run;
// an empty list is ignored, so a refresh that briefly sees nothing cannot make
// every app look new on the next one. An app that leaves the list (uninstalled)
// counts as new again when it comes back.
class InstallWatch {
public:
    QStringList update(const QStringList& installed)
    {
        if (installed.isEmpty()) return {};
        const QSet<QString> now(installed.cbegin(), installed.cend());
        QStringList added;
        if (m_primed) {
            for (const QString& name : installed)
                if (!m_known.contains(name) && !added.contains(name)) added << name;
        }
        m_known = now;
        m_primed = true;
        return added;
    }

private:
    QSet<QString> m_known;
    bool m_primed = false;
};

} // namespace RecentApps
