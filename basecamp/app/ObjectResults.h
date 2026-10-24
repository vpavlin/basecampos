#pragma once

#include <QAbstractListModel>
#include <QPointer>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

class ObjectSearch;

// One view's live search results (SHELL-OBJECTS-SPEC §5, "Views"): the
// envelopes the search role ranked for a query and a filter, in rank order.
// Created by ObjectSearch::search() for a view app; it refills itself whenever
// the ranker answers again (a store change re-asks, debounced), so a view
// binds a ListView to it once and calls update() when its bar or pills change.
//
// A view sees exactly the objects in here, never the rest of the store.
class ObjectResults : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QString query READ query NOTIFY requestChanged)
    Q_PROPERTY(QVariantMap filter READ filter NOTIFY requestChanged)
    // The ids in rank order.
    Q_PROPERTY(QStringList ids READ ids NOTIFY resultsChanged)
    // Who ranked what is shown: the search role holder's app name, or
    // "fallback" for the host's own ranker. "" before the first answer.
    Q_PROPERTY(QString ranker READ ranker NOTIFY resultsChanged)
    // A request is out and not answered yet (the rows are the previous answer).
    Q_PROPERTY(bool pending READ pending NOTIFY pendingChanged)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        AppRole,
        KeyRole,
        TypeRole,
        KindRole,
        TitleRole,
        SubtitleRole,
        FieldsRole,
        ActionsRole,
        ProgressRole,
        TimeRole,
        UpdatedRole,
        SeenRole,
        IconRole,
        SubjectsRole,
        RefsRole,
        // The whole envelope as one map, for a card that takes `object`.
        ObjectRole,
    };

    ObjectResults(ObjectSearch* search, const QString& viewApp, bool serveIcons,
                  QObject* parent = nullptr);
    ~ObjectResults() override;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return int(m_rows.size()); }
    QString query() const { return m_query; }
    QVariantMap filter() const { return m_filter; }
    QStringList ids() const { return m_ids; }
    QString ranker() const { return m_ranker; }
    bool pending() const { return m_pending; }

    // Search again with a new query and filter
    // ({ types?, apps?, kinds?, subjects?, unseenOnly? }).
    Q_INVOKABLE void update(const QString& query, const QVariantMap& filter = QVariantMap());
    // The envelope at `row` ({} out of range).
    Q_INVOKABLE QVariantMap get(int row) const;

    // ── Host side (ObjectSearch) ────────────────────────────────────────
    QString viewApp() const { return m_viewApp; }
    bool contains(const QString& id) const { return m_ids.contains(id); }
    // Stop following the store; the rows stay as they are.
    void detach() { m_search = nullptr; setPending(false); }
    bool attached() const { return !m_search.isNull(); }
    void setRequest(const QString& query, const QVariantMap& filter);
    void setPending(bool pending);
    // Replace the rows. Same ids in the same order only updates the rows'
    // data, so a view keeps its scroll position while a transfer progresses.
    void fill(const QStringList& ids, const QList<QVariantMap>& envelopes, const QString& ranker);

    QString requestId;   // the one open request, or ""

signals:
    void countChanged();
    void requestChanged();
    void resultsChanged();
    void pendingChanged();

private:
    QVariantMap served(QVariantMap envelope) const;

    QPointer<ObjectSearch> m_search;
    QString m_viewApp;
    bool m_serveIcons;
    QString m_query;
    QVariantMap m_filter;
    QStringList m_ids;
    QList<QVariantMap> m_rows;
    QString m_ranker;
    bool m_pending = false;
};
