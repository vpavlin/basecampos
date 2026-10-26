#pragma once

#include <QHash>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <deque>
#include <functional>

class QTimer;

// Shell objects: what apps publish for search and the home views to draw
// (docs/SHELL-OBJECTS-SPEC.md in basecamp-experiments, ADR-072).
//
// The HOST keeps them; it never ranks or draws. Apps reach it only through the
// shell intents `basecamp.objects.publish` and `basecamp.objects.retract`,
// serviced in MainUIBackend's shell endpoint, so the publishing app's name is
// always the broker's attested requester and never something in the payload.
//
// In memory only (spec §8): nothing is written to disk and everything is lost
// on restart; apps republish what is current when they start. The one thing
// that IS remembered is the user's per-app switch (Settings → Search), stored
// like the notification mutes.
//
// Kept dependency-free (QtCore only) so object_store_test.cpp can drive it
// without the rest of the host.
class ObjectStore : public QObject {
    Q_OBJECT
    // Apps the user switched off in Settings → Search.
    Q_PROPERTY(QStringList disabledApps READ disabledApps NOTIFY disabledAppsChanged)
    // Every object in the store, across apps (for tests and diagnostics).
    Q_PROPERTY(int total READ total NOTIFY changed)

public:
    // Spec §3 and §8. Per app unless stated.
    static constexpr int kMaxObjectsPerApp = 500;
    // Of those, how many may be current state (`state: true`, spec §8,
    // basecamp-experiments#63): kept when the app is over its cap, so events
    // never push them out, and capped on their own so the flag is not a way
    // round the 500.
    static constexpr int kMaxStatePerApp = 50;
    static constexpr int kRateRequests = 20;          // publish requests…
    static constexpr int kRateWindowMs = 10 * 1000;   // …per 10 s
    static constexpr int kMaxFieldsBytes = 2048;      // `fields`, serialized
    static constexpr int kMaxKey = 128;
    static constexpr int kMaxTitle = 80;
    static constexpr int kMaxSubtitle = 120;
    static constexpr int kMaxKeywords = 16;
    static constexpr int kMaxKeyword = 32;
    static constexpr int kMaxSubjects = 8;
    static constexpr int kMaxSubject = 256;
    static constexpr int kMaxRefs = 8;
    static constexpr int kMaxActions = 3;
    static constexpr int kMaxActionId = 32;
    static constexpr int kMaxActionLabel = 40;
    static constexpr int kMaxText = 1000;             // any one string in `fields`

    // The v0 types an app may publish (§4). `x.<app>.<name>` is also accepted
    // from that app, and drawn with the generic card.
    static const QStringList& knownTypes();

    explicit ObjectStore(QObject* parent = nullptr);

    // Validate and store a batch from `app`: `{ objects: [ envelope, … ] }`.
    // All or nothing. Returns "" on success, else an intent error code:
    //   bad_request  the batch, or object *badIndex (-1: the batch itself)
    //   failed       over a limit (rate, fields size, a batch over 500 or
    //                with more than 50 current-state objects)
    // A switched-off app gets "" too, so it cannot learn the user's choice, but
    // nothing is kept. `ids` receives each object's id, in batch order.
    QString publish(const QString& app, const QVariantMap& params,
                    int* badIndex = nullptr, QStringList* ids = nullptr);

    // The host publishing on `app`'s behalf: its notifications, mirrored into
    // the feed (NotificationObjects). The same checks and the same switch, but
    // not counted against the app's own rate, and every object in it is news
    // again (unseen), even when it replaces one with the same key.
    QString publishFromHost(const QString& app, const QVariantMap& params);

    // `{ keys: [ … ] }` or `{ all: true }`. Unknown keys are not an error.
    QString retract(const QString& app, const QVariantMap& params);

    // The id the host gives `app` + `key`. Stable across republish and restart.
    static QString idFor(const QString& app, const QString& key);

    // ── Reading ─────────────────────────────────────────────────────────
    // For the search role (phase 2), views' results and tests; the shell
    // decides who may call these, the store does not.
    int total() const;
    Q_INVOKABLE int count(const QString& app = QString()) const;
    // Envelopes newest `time` first. `filter`: { types?, apps?, kinds?,
    // subjects?, keys?, unseenOnly? } (lists of strings; any match passes).
    Q_INVOKABLE QVariantList list(const QVariantMap& filter = QVariantMap()) const;
    // Types that have at least one object, sorted.
    Q_INVOKABLE QStringList types() const;
    // How many objects of `type` each app holds (app -> count, no zeros),
    // switched-off apps left out: the apps home names as related to a type
    // pill (basecamp-experiments#36).
    QHash<QString, int> countsOfType(const QString& type) const;
    Q_INVOKABLE QVariantMap get(const QString& id) const;
    // get() for each of `ids`, in order ({} for an unknown one), expiring
    // once rather than per id: a search answer looks up 200.
    QList<QVariantMap> getMany(const QStringList& ids) const;
    // Whether envelope `o` passes `filter` (as list() filters).
    static bool matches(const QVariantMap& o, const QVariantMap& filter);
    // ── Changes since a version (basecamp-experiments#27) ───────────────
    // Every change to the objects (publish, replace, retract, evict, expire,
    // switch off, mark seen) moves version() on. A reader that keeps its own
    // copy asks changesSince(the version it holds) and gets only what changed:
    // the envelopes added or replaced, and the ids gone. `reset` is true when
    // it cannot be told (version 0, a version this store never gave out, or
    // one older than the removals it still remembers): `changed` is then
    // every object and the reader drops what it held. Search no longer copies
    // the whole store on every query.
    struct Changes {
        quint64 version = 0;
        bool reset = false;
        QList<QVariantMap> changed;
        QStringList removed;
    };
    quint64 version() const { return m_version; }
    Changes changesSince(quint64 since) const;
    // Removals remembered for changesSince(); past this the oldest are
    // forgotten and a reader that far behind gets a reset. Twice the store's
    // whole capacity for the five stub apps of the flood test.
    static constexpr int kMaxTombstones = 10 * kMaxObjectsPerApp;

    // Says seenChanged, not objectsChanged: marking seen is the user looking,
    // not an app publishing, and does not re-rank (ObjectSearch.h).
    Q_INVOKABLE void markSeen(const QStringList& ids);

    // ── The user's switch ───────────────────────────────────────────────
    QStringList disabledApps() const;
    bool isEnabled(const QString& app) const { return !m_disabled.contains(app); }
    // Off deletes everything `app` published, at once, and drops what it sends
    // while off. Remembered across restarts.
    Q_INVOKABLE void setEnabled(const QString& app, bool enabled);

    // Uninstalled, or no longer declaring publish: forget its objects.
    void removeApp(const QString& app);
    // removeApp() for every app with objects that is not in `apps`.
    void retainApps(const QStringList& apps);

    // Drop expired objects now. Also runs on a timer and before every read.
    void purgeExpired();

    // Who draws each publisher: an icon URL for an app name ("" = none).
    void setIconResolver(std::function<QString(const QString&)> resolver) { m_iconFor = std::move(resolver); }
    // Whether that icon fills its tile (a 0.4.0+ manifest's validated square)
    // or is a small glyph drawn inset: stamped as `iconFullBleed`.
    void setIconFullBleedResolver(std::function<bool(const QString&)> resolver) { m_iconFullBleedFor = std::move(resolver); }
    // Tests replace the clock (ms since epoch) to exercise expiry and the rate
    // window without waiting.
    void setClock(std::function<qint64()> clock) { m_now = std::move(clock); }

signals:
    // Something `app` published was added, replaced, retracted, evicted or
    // expired. The search role re-ranks on this (spec §5).
    void objectsChanged(const QString& app);
    // `ids` turned seen (markSeen).
    void seenChanged(const QStringList& ids);
    void changed();
    void disabledAppsChanged();

private:
    QString publishBatch(const QString& app, const QVariantMap& params,
                         int* badIndex, QStringList* ids, bool fromHost);
    qint64 now() const;
    // Whether `app` may publish now (20 per 10 s); records the request
    // unless `record` is false.
    bool allow(const QString& app, bool record = true);
    // Validate one envelope. Returns the cleaned object (host fields not yet
    // set) or an empty map with *error set.
    QVariantMap validate(const QString& app, int index, const QVariantMap& in,
                         QString* error) const;
    void evict(const QString& app);
    void scheduleExpiry();
    void removeIds(const QString& app, const QStringList& ids);
    // Record that `id` changed (stored, replaced, seen) or is gone.
    void touched(const QString& id);
    void dropped(const QString& id);

    quint64 m_version = 0;
    QHash<QString, quint64> m_versionOf;           // id -> version of its last change
    QHash<QString, quint64> m_gone;                // id -> version it was removed at
    quint64 m_goneFloor = 0;                       // removals before this are forgotten

    QHash<QString, QVariantMap> m_objects;         // id -> envelope
    QHash<QString, QSet<QString>> m_byApp;         // app -> ids
    QSet<QString> m_disabled;
    QHash<QString, std::deque<qint64>> m_recent;   // app -> publish times (ms)
    std::function<QString(const QString&)> m_iconFor;
    std::function<bool(const QString&)> m_iconFullBleedFor;
    std::function<qint64()> m_now;
    QTimer* m_expiry = nullptr;
};
