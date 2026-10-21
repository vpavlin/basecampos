#pragma once

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <deque>
#include <functional>

// Notifications: what apps and Basecamp itself tell the user
// (docs/NOTIFICATIONS-SPEC.md on the notifications branch).
//
// Host-side and shell-agnostic: every shell renders the same list and reacts to
// `posted` for toasts. Apps reach it only through the shell intent
// `basecamp.notifications.post`; Basecamp's own notices (an app's process
// stopped) are posted directly with app == kBasecampApp.
class NotificationCenter : public QObject {
    Q_OBJECT
    // Newest first. Each item: id, app, title, body, level, tag, time (ms since
    // epoch), read, and optionally action { label, launch }.
    Q_PROPERTY(QVariantList items READ items NOTIFY itemsChanged)
    Q_PROPERTY(int unreadCount READ unreadCount NOTIFY itemsChanged)
    Q_PROPERTY(QStringList mutedApps READ mutedApps NOTIFY mutedAppsChanged)
    Q_PROPERTY(bool doNotDisturb READ doNotDisturb WRITE setDoNotDisturb NOTIFY doNotDisturbChanged)

public:
    static constexpr int kMaxItems = 50;
    static constexpr int kMaxTitle = 80;
    static constexpr int kMaxBody = 300;
    static constexpr int kMaxTag = 64;
    static constexpr int kRatePerMinute = 5;
    static inline const QString kBasecampApp = QStringLiteral("basecamp");

    explicit NotificationCenter(QObject* parent = nullptr);

    QVariantList items() const { return m_items; }
    int unreadCount() const;
    QStringList mutedApps() const { return QStringList(m_muted.begin(), m_muted.end()); }
    bool doNotDisturb() const { return m_dnd; }
    void setDoNotDisturb(bool on);

    // Validate and store a notification from `app`. Returns the id; on failure
    // returns an empty string and sets *error to an intent error code
    // ("bad_request" or "failed"). A muted app still gets an id, so a sender
    // cannot learn the user's settings, but nothing is stored or shown.
    QString post(const QString& app, const QVariantMap& params, QString* error = nullptr);

    // Basecamp's own notice, with an optional action button.
    QString postSystem(const QString& title, const QString& body, const QString& level,
                       const QVariantMap& action = {});

    // Who draws each sender: returns an icon URL for an app name ("" = none).
    void setIconResolver(std::function<QString(const QString&)> resolver) { m_iconFor = std::move(resolver); }

    Q_INVOKABLE void dismiss(const QString& id);
    Q_INVOKABLE void clearAll();
    Q_INVOKABLE void markAllRead();
    Q_INVOKABLE void setMuted(const QString& app, bool muted);
    // The user clicked a notification (or its button, when `useAction`).
    Q_INVOKABLE void activate(const QString& id, bool useAction);

signals:
    void itemsChanged();
    void mutedAppsChanged();
    void doNotDisturbChanged();
    // A new or replacing notification to show as a toast. Not emitted for
    // muted apps; emitted under Do not disturb with `quiet` set, so a shell can
    // skip the toast and still update its bell.
    void posted(const QVariantMap& notification, bool quiet);
    // Open the sender (`launch` empty) or run an action that launches an app.
    void activated(const QString& app, const QString& launch);

private:
    QString store(QVariantMap n);
    bool allow(const QString& app);

    QVariantList m_items;
    QSet<QString> m_muted;
    QHash<QString, std::deque<qint64>> m_recent;   // app -> post times (ms)
    bool m_dnd = false;
    std::function<QString(const QString&)> m_iconFor;
    quint64 m_next = 0;
};
