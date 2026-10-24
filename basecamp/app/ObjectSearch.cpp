#include "ObjectSearch.h"

#include "ObjectRanker.h"
#include "ObjectResults.h"
#include "ObjectStore.h"

#include <QDateTime>
#include <QDebug>
#include <QSet>
#include <QTimer>

#include <algorithm>
#include <utility>

const QString& ObjectSearch::searchRole()
{
    static const QString s = QStringLiteral("search");
    return s;
}

const QString& ObjectSearch::fallbackRanker()
{
    static const QString s = QStringLiteral("fallback");
    return s;
}

const QString& ObjectSearch::shellView()
{
    static const QString s = QStringLiteral("main_ui");
    return s;
}

ObjectSearch::ObjectSearch(ObjectStore* store, HolderFn holderOf, QObject* parent)
    : QObject(parent)
    , m_store(store)
    , m_holderOf(std::move(holderOf))
{
    m_debounce = new QTimer(this);
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(kRefreshDebounceMs);
    connect(m_debounce, &QTimer::timeout, this, [this]() {
        refreshFor(std::exchange(m_changedApps, {}));
    });
    if (m_store) {
        connect(m_store, &ObjectStore::objectsChanged, this, [this](const QString& app) {
            emit objectsChanged(app);
            m_changedApps.insert(app);
            if (!m_debounce->isActive())
                m_debounce->start();
        });
    }
}

ObjectSearch::~ObjectSearch()
{
    // Results the views still hold stop following a search that is gone.
    for (const QPointer<ObjectResults>& r : std::as_const(m_live))
        if (r) r->detach();
}

void ObjectSearch::setTimings(int answerTimeoutMs, int refreshDebounceMs)
{
    m_answerTimeoutMs = answerTimeoutMs;
    m_debounce->setInterval(refreshDebounceMs);
}

qint64 ObjectSearch::now() const
{
    return m_now ? m_now() : QDateTime::currentMSecsSinceEpoch();
}

QString ObjectSearch::holder() const
{
    return m_holderOf ? m_holderOf(searchRole()) : QString();
}

bool ObjectSearch::isView(const QString& app) const
{
    if (app.isEmpty()) return false;
    if (app == shellView()) return true;
    return m_holderOf && (m_holderOf(QStringLiteral("home")) == app
                          || m_holderOf(QStringLiteral("launcher")) == app);
}

QVariantMap ObjectSearch::sanitizeFilter(const QVariantMap& filter)
{
    QVariantMap out;
    for (const char* key : { "types", "apps", "kinds", "subjects" }) {
        const QString k = QLatin1String(key);
        const QVariant v = filter.value(k);
        if (!v.isValid() || v.isNull()) continue;
        QStringList list = v.toStringList();
        list.removeAll(QString());
        if (!list.isEmpty())
            out.insert(k, list);
    }
    if (filter.value(QStringLiteral("unseenOnly")).toBool())
        out.insert(QStringLiteral("unseenOnly"), true);
    return out;
}

ObjectResults* ObjectSearch::search(const QString& viewApp, const QString& query,
                                    const QVariantMap& filter)
{
    if (!isView(viewApp)) {
        qWarning().noquote() << "Object search:" << viewApp << "is not a view (home or launcher) — refused";
        return nullptr;
    }
    auto* results = new ObjectResults(this, viewApp, viewApp != shellView());

    int mine = 0;
    for (int i = int(m_live.size()) - 1; i >= 0; --i) {
        ObjectResults* r = m_live.at(i);
        if (!r) { m_live.removeAt(i); continue; }
        if (r->viewApp() == viewApp && ++mine >= kMaxLivePerView) {
            forget(r);
            r->detach();
        }
    }
    m_live.append(results);
    results->update(query, filter);
    return results;
}

bool ObjectSearch::inResultsOf(const QString& viewApp, const QString& id) const
{
    for (const QPointer<ObjectResults>& r : m_live)
        if (r && r->viewApp() == viewApp && r->contains(id))
            return true;
    return false;
}

QStringList ObjectSearch::markSeen(const QString& viewApp, const QStringList& ids)
{
    QStringList allowed;
    for (const QString& id : ids)
        if (!allowed.contains(id) && inResultsOf(viewApp, id))
            allowed << id;
    if (m_store && !allowed.isEmpty())
        m_store->markSeen(allowed);
    return allowed;
}

QVariantMap ObjectSearch::counts(const QString& viewApp, const QString& query) const
{
    if (!m_store || !isView(viewApp))
        return {};
    const QStringList tokens = ObjectRanker::tokens(query);
    QHash<QString, int> byType, byApp;
    int total = 0;
    for (const QVariant& v : m_store->list({})) {
        const QVariantMap o = v.toMap();
        if (!tokens.isEmpty() && ObjectRanker::score(o, tokens, 0) < 0)
            continue;
        ++total;
        ++byType[o.value(QStringLiteral("type")).toString()];
        ++byApp[o.value(QStringLiteral("app")).toString()];
    }
    auto sorted = [](const QHash<QString, int>& counts, const QString& key) {
        QList<QPair<QString, int>> list;
        for (auto it = counts.cbegin(); it != counts.cend(); ++it)
            if (!it.key().isEmpty()) list.append({ it.key(), it.value() });
        std::sort(list.begin(), list.end(), [](const auto& a, const auto& b) {
            return a.second != b.second ? a.second > b.second : a.first < b.first;
        });
        QVariantList out;
        for (const auto& p : std::as_const(list))
            out << QVariantMap{ { key, p.first }, { QStringLiteral("count"), p.second } };
        return out;
    };
    return { { QStringLiteral("total"), total },
             { QStringLiteral("types"), sorted(byType, QStringLiteral("type")) },
             { QStringLiteral("apps"), sorted(byApp, QStringLiteral("app")) } };
}

QVariantList ObjectSearch::objects(const QString& app, const QVariantMap& filter) const
{
    if (!m_store || app.isEmpty() || app != holder())
        return {};
    return m_store->list(sanitizeFilter(filter));
}

bool ObjectSearch::answer(const QString& app, const QString& requestId, const QVariantList& ids)
{
    if (app.isEmpty() || app != holder())
        return false;
    const auto it = m_open.constFind(requestId);
    if (it == m_open.cend() || it->holder != app || !it->results
        || it->results->requestId != requestId)
        return false;   // stale, answered, or asked of someone else
    ObjectResults* results = it->results;
    m_open.erase(it);
    results->requestId.clear();

    QSet<QString> allowed;
    if (m_store)
        for (const QVariant& v : m_store->list(results->filter()))
            allowed.insert(v.toMap().value(QStringLiteral("id")).toString());
    QStringList kept;
    QSet<QString> seen;
    int dropped = 0;
    for (const QVariant& v : ids) {
        const QString id = v.typeId() == QMetaType::QString ? v.toString() : QString();
        if (id.isEmpty() || !allowed.contains(id) || seen.contains(id)) {
            ++dropped;
            continue;
        }
        if (kept.size() >= kMaxAnswer) {
            ++dropped;
            continue;
        }
        seen.insert(id);
        kept << id;
    }
    if (dropped > 0)
        qWarning().noquote() << "Object search:" << app << "answer" << requestId << "—"
                             << dropped << "ids dropped (unknown, outside the filter, repeated or past"
                             << kMaxAnswer << ")";
    qInfo().noquote() << "Object search" << requestId << "answered by" << app << "—"
                      << kept.size() << "results";
    fillWith(results, kept, app);
    return true;
}

QStringList ObjectSearch::rankWithFallback(const QString& query, const QVariantMap& filter) const
{
    if (!m_store) return {};
    return ObjectRanker::rank(m_store->list(sanitizeFilter(filter)), query, now());
}

void ObjectSearch::request(ObjectResults* results, bool byView)
{
    if (!results || !results->attached()) return;
    if (!results->requestId.isEmpty())
        m_open.remove(std::exchange(results->requestId, QString()));   // superseded: stale

    // Only what the view asked for is pending: a host refresh keeps the rows
    // (and, when they come back the same, says nothing).
    if (byView)
        results->setPending(true);
    const QString who = holder();
    if (who.isEmpty()) {
        fallback(results);
        return;
    }
    const QString id = QStringLiteral("search-%1").arg(++m_nextRequest);
    results->requestId = id;
    m_open.insert(id, Open{ results, who });
    emit searchRequested(who, id, results->query(), results->filter());

    if (!m_open.contains(id))
        return;   // answered synchronously
    QPointer<ObjectResults> guard(results);
    QTimer::singleShot(m_answerTimeoutMs, this, [this, guard, id, who]() {
        if (!guard || guard->requestId != id || !m_open.contains(id))
            return;
        m_open.remove(id);
        guard->requestId.clear();
        qInfo().noquote() << "Object search" << id << ":" << who << "did not answer within"
                          << m_answerTimeoutMs << "ms — the host ranker answered";
        fallback(guard);
    });
}

void ObjectSearch::fallback(ObjectResults* results)
{
    fillWith(results, rankWithFallback(results->query(), results->filter()), fallbackRanker());
}

void ObjectSearch::fillWith(ObjectResults* results, const QStringList& ids, const QString& ranker)
{
    QList<QVariantMap> envelopes;
    QStringList present;
    for (const QString& id : ids) {
        const QVariantMap o = m_store ? m_store->get(id) : QVariantMap();
        if (o.isEmpty()) continue;
        envelopes << o;
        present << id;
    }
    results->fill(present, envelopes, ranker);
    setLastRanker(ranker);
}

void ObjectSearch::forget(ObjectResults* results)
{
    for (auto it = m_open.begin(); it != m_open.end();) {
        if (it->results == results || !it->results) it = m_open.erase(it);
        else ++it;
    }
    m_live.removeAll(results);
}

void ObjectSearch::refreshAll()
{
    const QList<QPointer<ObjectResults>> live = m_live;
    for (const QPointer<ObjectResults>& r : live)
        if (r) request(r, false);
}

void ObjectSearch::refreshFor(const QSet<QString>& apps)
{
    if (apps.isEmpty()) return;
    const QList<QPointer<ObjectResults>> live = m_live;
    for (const QPointer<ObjectResults>& r : live) {
        if (!r) continue;
        const QStringList only = r->filter().value(QStringLiteral("apps")).toStringList();
        // A filter naming its apps cannot change with another app's objects.
        if (!only.isEmpty()
            && std::none_of(only.cbegin(), only.cend(),
                            [&apps](const QString& a) { return apps.contains(a); }))
            continue;
        request(r, false);
    }
}

void ObjectSearch::rolesChanged()
{
    const QList<QPointer<ObjectResults>> live = m_live;
    for (const QPointer<ObjectResults>& r : live) {
        if (!r) continue;
        if (!isView(r->viewApp())) {
            // The view is not one any more: it stops, and is emptied.
            forget(r);
            r->detach();
            r->fill({}, {}, QString());
        }
    }
    refreshAll();
}

void ObjectSearch::setLastRanker(const QString& ranker)
{
    if (ranker.isEmpty() || ranker == m_lastRanker) return;
    m_lastRanker = ranker;
    emit lastRankerChanged();
}
