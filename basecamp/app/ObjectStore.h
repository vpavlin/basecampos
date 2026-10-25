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
    //   failed       over a limit (rate, fields size, a batch over 500)
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
    void changed();
    void disabledAppsChanged();

private:
    QString publishBatch(const QString& app, const QVariantMap& params,
                         int* badIndex, QStringList* ids, bool fromHost);
    qint64 now() const;
    bool allow(const QString& app);
    // Validate one envelope. Returns the cleaned object (host fields not yet
    // set) or an empty map with *error set.
    QVariantMap validate(const QString& app, int index, const QVariantMap& in,
                         QString* error) const;
    void evict(const QString& app);
    void scheduleExpiry();
    void removeIds(const QString& app, const QStringList& ids);

    QHash<QString, QVariantMap> m_objects;         // id -> envelope
    QHash<QString, QSet<QString>> m_byApp;         // app -> ids
    QSet<QString> m_disabled;
    QHash<QString, std::deque<qint64>> m_recent;   // app -> publish times (ms)
    std::function<QString(const QString&)> m_iconFor;
    std::function<bool(const QString&)> m_iconFullBleedFor;
    std::function<qint64()> m_now;
    QTimer* m_expiry = nullptr;
};
