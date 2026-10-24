#include "ObjectResults.h"

#include "ObjectSearch.h"

ObjectResults::ObjectResults(ObjectSearch* search, const QString& viewApp, bool serveIcons,
                             QObject* parent)
    : QAbstractListModel(parent)
    , m_search(search)
    , m_viewApp(viewApp)
    , m_serveIcons(serveIcons)
{
}

ObjectResults::~ObjectResults()
{
    if (m_search)
        m_search->forget(this);
}

int ObjectResults::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant ObjectResults::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};
    const QVariantMap& o = m_rows.at(index.row());
    switch (role) {
    case IdRole:       return o.value(QStringLiteral("id"));
    case AppRole:      return o.value(QStringLiteral("app"));
    case KeyRole:      return o.value(QStringLiteral("key"));
    case TypeRole:     return o.value(QStringLiteral("type"));
    case KindRole:     return o.value(QStringLiteral("kind"));
    case TitleRole:    return o.value(QStringLiteral("title"));
    case SubtitleRole: return o.value(QStringLiteral("subtitle"), QString());
    case FieldsRole:   return o.value(QStringLiteral("fields"), QVariantMap());
    case ActionsRole:  return o.value(QStringLiteral("actions"), QVariantList());
    case ProgressRole: return o.value(QStringLiteral("progress"));   // undefined when absent
    case TimeRole:     return o.value(QStringLiteral("time"));
    case UpdatedRole:  return o.value(QStringLiteral("updated"));
    case SeenRole:     return o.value(QStringLiteral("seen"));
    case IconRole:     return o.value(QStringLiteral("icon"), QString());
    case SubjectsRole: return o.value(QStringLiteral("subjects"), QVariantList());
    case RefsRole:     return o.value(QStringLiteral("refs"), QVariantList());
    case ObjectRole:   return o;
    default:           return {};
    }
}

QHash<int, QByteArray> ObjectResults::roleNames() const
{
    return {
        { IdRole, "id" },             { AppRole, "app" },       { KeyRole, "key" },
        { TypeRole, "type" },         { KindRole, "kind" },     { TitleRole, "title" },
        { SubtitleRole, "subtitle" }, { FieldsRole, "fields" }, { ActionsRole, "actions" },
        { ProgressRole, "progress" }, { TimeRole, "time" },     { UpdatedRole, "updated" },
        { SeenRole, "seen" },         { IconRole, "icon" },     { SubjectsRole, "subjects" },
        { RefsRole, "refs" },         { ObjectRole, "object" },
    };
}

void ObjectResults::update(const QString& query, const QVariantMap& filter)
{
    setRequest(query, filter);
    if (m_search)
        m_search->request(this, true);
}

QVariantMap ObjectResults::get(int row) const
{
    return row >= 0 && row < m_rows.size() ? m_rows.at(row) : QVariantMap();
}

void ObjectResults::setRequest(const QString& query, const QVariantMap& filter)
{
    const QVariantMap clean = ObjectSearch::sanitizeFilter(filter);
    if (query == m_query && clean == m_filter)
        return;
    m_query = query;
    m_filter = clean;
    emit requestChanged();
}

void ObjectResults::setPending(bool pending)
{
    if (m_pending == pending) return;
    m_pending = pending;
    emit pendingChanged();
}

// A role app's sandbox cannot read the publisher's icon file: point it at the
// host's image provider (ShellApi::createImageProvider), which serves it by
// app name. The shell's own views read the file as is.
QVariantMap ObjectResults::served(QVariantMap o) const
{
    if (m_serveIcons) {
        const QString app = o.value(QStringLiteral("app")).toString();
        o[QStringLiteral("icon")] = o.value(QStringLiteral("icon")).toString().isEmpty() || app.isEmpty()
            ? QString() : QStringLiteral("image://basecamp/appicon/") + app;
    }
    return o;
}

void ObjectResults::fill(const QStringList& ids, const QList<QVariantMap>& envelopes,
                         const QString& ranker)
{
    QList<QVariantMap> rows;
    rows.reserve(envelopes.size());
    for (const QVariantMap& o : envelopes)
        rows << served(o);

    // What the view asked for is answered even when it comes back the same;
    // a host refresh that changes nothing says nothing (the view would learn
    // when other apps publish, ObjectSearch.h).
    const bool answersView = m_pending;
    const bool sameOrder = ids == m_ids;
    const int before = int(m_rows.size());
    bool changed = ranker != m_ranker;
    if (sameOrder) {
        if (rows != m_rows) {
            m_rows = rows;
            emit dataChanged(index(0), index(int(m_rows.size()) - 1));
            changed = true;
        }
    } else {
        beginResetModel();
        m_ids = ids;
        m_rows = rows;
        endResetModel();
        changed = true;
    }
    m_ranker = ranker;
    setPending(false);
    if (before != m_rows.size())
        emit countChanged();
    if (changed || answersView)
        emit resultsChanged();
}
