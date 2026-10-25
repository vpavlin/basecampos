#include "AppSearch.h"

#include "ObjectRanker.h"
#include "InstallEnums.h"

#include <QHash>
#include <QSet>
#include <QVariantMap>

#include <algorithm>

namespace {

bool busy(int stage)
{
    return stage == InstallStage::Downloading || stage == InstallStage::Downloaded
        || stage == InstallStage::Queued || stage == InstallStage::Installing;
}

int stateOrder(const QString& state)
{
    if (state == QLatin1String("running"))    return 0;
    if (state == QLatin1String("installed"))  return 1;
    if (state == QLatin1String("installing")) return 2;
    return 3;
}

bool anyStartsWith(const QStringList& words, const QString& token)
{
    return std::any_of(words.cbegin(), words.cend(),
                       [&token](const QString& w) { return w.startsWith(token); });
}

} // namespace

namespace AppSearch {

QList<Candidate> candidates(const QVariantList& installed, const QVariantList& catalog,
                            const QStringList& hidden)
{
    QList<Candidate> out;
    QSet<QString> seen;
    for (const QVariant& v : installed) {
        const QVariantMap row = v.toMap();
        const QString name = row.value(QStringLiteral("name")).toString();
        if (name.isEmpty() || hidden.contains(name) || seen.contains(name)) continue;
        seen.insert(name);
        Candidate c;
        c.name = name;
        c.title = row.value(QStringLiteral("displayName")).toString();
        if (c.title.isEmpty()) c.title = name;
        c.icon = row.value(QStringLiteral("iconPath")).toString();
        c.fullBleed = row.value(QStringLiteral("supportsFullBleedIcon")).toBool();
        c.state = row.value(QStringLiteral("isLoaded")).toBool() ? QStringLiteral("running")
                                                                 : QStringLiteral("installed");
        out.append(c);
    }
    // A catalog app can be offered by several repositories: the first row that
    // this host can install stands for it, or one being installed wins.
    QHash<QString, int> catalogIndex;
    for (const QVariant& v : catalog) {
        const QVariantMap row = v.toMap();
        const QString name = row.value(QStringLiteral("name")).toString();
        if (name.isEmpty() || hidden.contains(name) || seen.contains(name)) continue;
        if (row.value(QStringLiteral("type")).toString() != QLatin1String("ui_qml")) continue;
        const bool installing = busy(row.value(QStringLiteral("installStage")).toInt());
        if (!installing && row.value(QStringLiteral("hasInstallableVersion"), true).toBool() == false)
            continue;
        Candidate c;
        c.name = name;
        c.title = row.value(QStringLiteral("displayName")).toString();
        if (c.title.isEmpty()) c.title = name;
        c.icon = row.value(QStringLiteral("iconUrl")).toString();
        c.fullBleed = row.value(QStringLiteral("supportsFullBleedIcon")).toBool();
        c.state = installing ? QStringLiteral("installing") : QStringLiteral("available");
        const auto it = catalogIndex.constFind(name);
        if (it == catalogIndex.cend()) {
            catalogIndex.insert(name, out.size());
            out.append(c);
        } else if (installing) {
            out[it.value()] = c;
        }
    }
    return out;
}

QList<Candidate> match(const QList<Candidate>& candidates, const QString& query, int max)
{
    const QStringList tokens = ObjectRanker::tokens(query);
    if (tokens.isEmpty() || max <= 0) return {};

    struct Hit { Candidate c; int titleTokens; bool leads; };
    QList<Hit> hits;
    for (const Candidate& c : candidates) {
        const QStringList titleWords = ObjectRanker::tokens(c.title);
        const QStringList nameWords = ObjectRanker::tokens(c.name);
        int inTitle = 0;
        bool all = true;
        for (const QString& t : tokens) {
            if (anyStartsWith(titleWords, t)) ++inTitle;
            else if (!anyStartsWith(nameWords, t)) { all = false; break; }
        }
        if (!all) continue;
        hits.append({c, inTitle, !titleWords.isEmpty() && titleWords.first().startsWith(tokens.first())});
    }
    std::sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) {
        if (a.titleTokens != b.titleTokens) return a.titleTokens > b.titleTokens;
        if (a.leads != b.leads) return a.leads;
        const int sa = stateOrder(a.c.state), sb = stateOrder(b.c.state);
        if (sa != sb) return sa < sb;
        const int byTitle = QString::compare(a.c.title, b.c.title, Qt::CaseInsensitive);
        if (byTitle != 0) return byTitle < 0;
        return a.c.name < b.c.name;
    });
    QList<Candidate> out;
    for (const Hit& h : hits) {
        if (out.size() >= max) break;
        out.append(h.c);
    }
    return out;
}

QList<Candidate> related(const QList<Candidate>& candidates, const QHash<QString, int>& counts,
                         int max)
{
    if (max <= 0) return {};
    QList<Candidate> out;
    for (const Candidate& c : candidates) {
        if (c.state != QLatin1String("running") && c.state != QLatin1String("installed")) continue;
        if (counts.value(c.name) <= 0) continue;
        out.append(c);
    }
    std::sort(out.begin(), out.end(), [&counts](const Candidate& a, const Candidate& b) {
        const int ca = counts.value(a.name), cb = counts.value(b.name);
        if (ca != cb) return ca > cb;
        const int sa = stateOrder(a.state), sb = stateOrder(b.state);
        if (sa != sb) return sa < sb;
        const int byTitle = QString::compare(a.title, b.title, Qt::CaseInsensitive);
        if (byTitle != 0) return byTitle < 0;
        return a.name < b.name;
    });
    if (out.size() > max) out = out.mid(0, max);
    return out;
}

} // namespace AppSearch
