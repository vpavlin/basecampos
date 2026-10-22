#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QVariantMap>
#include <functional>

class NotificationCenter;

// Hands notifications to the desktop's own notification centre while Basecamp
// is in the background (docs/NOTIFICATIONS-SPEC.md §5, "Desktop notifications").
//
// Linux: org.freedesktop.Notifications over D-Bus (GNOME, KDE, and the
// notification daemons of other desktops). Clicking the desktop notification
// brings Basecamp to the front and opens the sender, the same as a click in
// the list. macOS and Windows: not yet (no portable Qt API); this is a no-op.
//
// What reaches the desktop is exactly what reaches the list: muted apps and
// Do not disturb are already filtered by NotificationCenter, and the user can
// turn the hand-off off (NotificationCenter::desktop).
class DesktopNotifier : public QObject {
    Q_OBJECT
public:
    explicit DesktopNotifier(NotificationCenter* center, QObject* parent = nullptr);

    // Who to name as the sender ("Radio"), from an app's module name.
    void setNameResolver(std::function<QString(const QString&)> resolver) { m_nameFor = std::move(resolver); }

    // False when there is no notification service on this desktop.
    bool available() const { return m_available; }

private slots:
    void onPosted(const QVariantMap& n, bool quiet);
    void onItemsChanged();
    void onActionInvoked(uint osId, const QString& key);
    void onActivationToken(uint osId, const QString& token);
    void onClosed(uint osId, uint reason);

private:
    void send(const QVariantMap& n);
    void close(uint osId);
    void closeAll();
    QString iconPath(const QString& app, const QString& iconUrl);
    void raiseBasecamp();

    NotificationCenter* m_center;
    std::function<QString(const QString&)> m_nameFor;
    bool m_available = false;
    QHash<QString, uint> m_osIds;        // our id -> the desktop's id
    QHash<QString, QString> m_icons;     // app -> cached PNG path
};
