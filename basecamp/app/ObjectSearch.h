#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <functional>

#include "ObjectRanker.h"

class ObjectResults;
class ObjectStore;
class QTimer;

// The search protocol between views, the `search` role and the store
// (SHELL-OBJECTS-SPEC §2, §5; ADR-072).
//
//   view ──search(query, filter)──▶ ObjectSearch ──searchRequested──▶ holder
//   view ◀──ObjectResults (live)─── ObjectSearch ◀──answer(ids)────── holder
//
// * A VIEW (the `home` or `launcher` holder, or the shell itself) asks with
//   search() and gets an ObjectResults that refills on every answer.
// * The `search` HOLDER is asked with searchRequested, reads the objects the
//   filter allows with objects(), and answers ranked ids with answer(). Unknown
//   ids, ids the filter does not allow and duplicates are dropped; at most
//   kMaxAnswer are kept. Only the open request of a results model is
//   answerable: an answer to a superseded or already-answered request is stale
//   and ignored.
// * With no holder the host ranks at once (ObjectRanker); a holder that does not
//   answer within kAnswerTimeoutMs is ranked for, too, and that is logged.
// * A store change asks again, debounced, for the live results models whose
//   filter could include the app that changed. That refresh is the host's,
//   not the view's: it does not flip `pending`, and rows that come back the
//   same signal nothing, so a view cannot time other apps' publishing.
// * Marking seen does NOT ask again (basecamp-experiments#27, the flood
//   test): the results that hold a marked id take its new `seen` in place,
//   in the same order, debounced like a store change. Unseen ranks first, so
//   re-ranking would drop what a view just marked and lift the next unseen,
//   which it would mark in turn: the feed moved under the user every couple
//   of seconds and every pass ranked the whole store. The next real change
//   re-ranks. A filter with unseenOnly does ask again (its rows must go).
//
// Who may do what is decided here (each call names its app), so the rules are
// unit-tested without the rest of the host; ShellApi re-checks the role on every
// call anyway, as for its other members.
//
// QtCore only, like ObjectStore.
class ObjectSearch : public QObject {
    Q_OBJECT
public:
    using HolderFn = std::function<QString(const QString& role)>;

    static constexpr int kMaxAnswer = 200;
    static constexpr int kAnswerTimeoutMs = 500;
    static constexpr int kRefreshDebounceMs = 200;
    // Live results models per view app. A view should keep one per list and
    // call update(), and destroy() the ones it drops; past this the oldest
    // stops following the store. Twice what home uses (7 widgets, the pills
    // and the feed), so models a view dropped but the GC has not collected yet
    // do not push out ones it still shows.
    static constexpr int kMaxLivePerView = 16;

    static const QString& searchRole();      // "search"
    static const QString& fallbackRanker();  // "fallback"
    // The shell's own views (its switcher, tests through the backend).
    static const QString& shellView();       // "main_ui"

    ObjectSearch(ObjectStore* store, HolderFn holderOf, QObject* parent = nullptr);
    ~ObjectSearch() override;

    void setClock(std::function<qint64()> clock) { m_now = std::move(clock); }
    void setTimings(int answerTimeoutMs, int refreshDebounceMs);

    // The current `search` holder, "" for none.
    QString holder() const;
    // `home` or `launcher` holder, or the shell.
    bool isView(const QString& app) const;

    // ── Views ───────────────────────────────────────────────────────────
    // A live results model for `viewApp`, already asked for (and, with no
    // holder, already filled). Unparented: the caller owns it. nullptr when
    // `viewApp` is not a view.
    ObjectResults* search(const QString& viewApp, const QString& query,
                          const QVariantMap& filter = QVariantMap());
    // Whether `id` is in one of `viewApp`'s live results: the only objects a
    // view may act on or mark seen.
    bool inResultsOf(const QString& viewApp, const QString& id) const;
    // Marks the ids in `viewApp`'s results seen; returns those it marked.
    QStringList markSeen(const QString& viewApp, const QStringList& ids);
    // How many objects match `query` (the host's matching: every token a
    // prefix of a word, as ObjectRanker), uncapped, by type and by app:
    // { total, types: [{ type, count }], apps: [{ app, count }] }, most first,
    // ties by name. A results model holds at most kMaxAnswer; these are what
    // pills count. {} when `viewApp` is not a view.
    QVariantMap counts(const QString& viewApp, const QString& query) const;

    // ── The search role ─────────────────────────────────────────────────
    // The envelopes `filter` allows, newest first; empty unless `app` holds `search`.
    QVariantList objects(const QString& app, const QVariantMap& filter) const;
    // What changed in the store since `version` (basecamp-experiments#27), so
    // a holder keeps its own copy instead of asking objects() for all of it on
    // every query: { version, reset, objects: [changed envelopes], removed:
    // [ids] } (ObjectStore::changesSince), plus `ids`, the ids `filter`
    // allows, when the filter narrows anything. The same objects objects()
    // would give the holder, nothing more. {} unless `app` holds `search`.
    QVariantMap objectsSince(const QString& app, double version, const QVariantMap& filter) const;
    // false (and nothing changed) for a non-holder, or an unknown, superseded
    // or already answered request.
    bool answer(const QString& app, const QString& requestId, const QVariantList& ids);

    // ── Host ────────────────────────────────────────────────────────────
    // The host ranker, synchronously, for `filter` (tests and the backend hook).
    QStringList rankWithFallback(const QString& query, const QVariantMap& filter) const;
    // Who filled the most recent results: a holder's name or "fallback".
    QString lastRanker() const { return m_lastRanker; }
    // Roles changed hands: views that lost theirs stop, the rest ask again
    // (the holder may be someone else now).
    void rolesChanged();
    // Ask again for every live results model (the holder just started).
    void refreshAll();
    // Ask again for the live results models whose filter could include one of
    // `apps` (a store change).
    void refreshFor(const QSet<QString>& apps);
    // Only the spec's keys, as string lists, plus unseenOnly.
    static QVariantMap sanitizeFilter(const QVariantMap& filter);

signals:
    // To the holder named, and no one else.
    void searchRequested(const QString& holder, const QString& requestId,
                         const QString& query, const QVariantMap& filter);
    // Forwarded from the store for the holder: something `app` published changed.
    void objectsChanged(const QString& app);
    void lastRankerChanged();

private:
    friend class ObjectResults;
    struct Open {
        QPointer<ObjectResults> results;
        QString holder;
    };

    // Ask (again) for `results`' current query and filter. `byView`: the view
    // asked (update()), so it may see `pending`; otherwise the host refreshes.
    void request(ObjectResults* results, bool byView);
    void forget(ObjectResults* results);
    void fillWith(ObjectResults* results, const QStringList& ids, const QString& ranker);
    // The live results holding one of `ids` take their new `seen`.
    void refreshSeen(const QSet<QString>& ids);
    void fallback(ObjectResults* results);
    void setLastRanker(const QString& ranker);
    qint64 now() const;
    // The host's own prepared copy of the store, for counts() and the
    // fallback ranker: brought up to date from the store's changes before
    // each use, so a query never splits every object's words again.
    // An object's words are split when the host is idle (a few hundred per
    // turn of the event loop) or, at the latest, before a query that needs
    // them: a flood's refresh with an empty bar does not split 2500 objects'
    // words in one go.
    struct Indexed {
        ObjectRanker::Prepared prepared;
        QString type, app;
        QVariantMap envelope;   // until its words are split
        bool ready = false;
    };
    static constexpr int kPrepareChunk = 250;
    void syncIndex() const;
    // Split the words of up to `max` objects still waiting (all: -1).
    void prepareWords(int max) const;

    ObjectStore* m_store;
    HolderFn m_holderOf;
    std::function<qint64()> m_now;
    QList<QPointer<ObjectResults>> m_live;
    QHash<QString, Open> m_open;   // requestId -> the results it answers
    quint64 m_nextRequest = 0;
    int m_answerTimeoutMs = kAnswerTimeoutMs;
    QTimer* m_debounce = nullptr;
    QSet<QString> m_changedApps;   // since the debounce started
    QSet<QString> m_seenIds;       // marked seen since the debounce started
    QString m_lastRanker;
    mutable QHash<QString, Indexed> m_index;
    mutable quint64 m_indexVersion = 0;
    mutable QStringList m_unprepared;   // ids waiting for their words
    QTimer* m_prepareTimer = nullptr;
};
