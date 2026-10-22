#include "DesktopNotifier.h"

#include "NotificationCenter.h"

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QIcon>
#include <QMainWindow>
#include <QPixmap>
#include <QSet>
#include <QStandardPaths>
#include <QUrl>
#include <QWindow>

#ifdef BASECAMP_DBUS_NOTIFY
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#endif

namespace {

#ifdef BASECAMP_DBUS_NOTIFY
const QString kService = QStringLiteral("org.freedesktop.Notifications");
const QString kPath = QStringLiteral("/org/freedesktop/Notifications");
const QString kInterface = QStringLiteral("org.freedesktop.Notifications");
#endif

bool inBackground()
{
    return QGuiApplication::applicationState() != Qt::ApplicationActive;
}

} // namespace

DesktopNotifier::DesktopNotifier(NotificationCenter* center, QObject* parent)
    : QObject(parent)
    , m_center(center)
{
#ifdef BASECAMP_DBUS_NOTIFY
    QDBusConnection bus = QDBusConnection::sessionBus();
    m_available = bus.isConnected() && bus.interface()
                  && bus.interface()->isServiceRegistered(kService).value();
    if (!m_available) {
        // Most daemons are D-Bus activated: registered only once something
        // calls them. Try anyway when the service is activatable.
        m_available = bus.isConnected() && bus.interface()
                      && bus.interface()->activatableServiceNames().value().contains(kService);
    }
    if (m_available) {
        bus.connect(kService, kPath, kInterface, QStringLiteral("ActionInvoked"),
                    this, SLOT(onActionInvoked(uint,QString)));
        bus.connect(kService, kPath, kInterface, QStringLiteral("ActivationToken"),
                    this, SLOT(onActivationToken(uint,QString)));
        bus.connect(kService, kPath, kInterface, QStringLiteral("NotificationClosed"),
                    this, SLOT(onClosed(uint,uint)));
    }
#endif
    qInfo() << "Desktop notifications:" << (m_available ? "available" : "not available on this desktop");
    if (!m_available)
        return;

    connect(m_center, &NotificationCenter::posted, this, &DesktopNotifier::onPosted);
    connect(m_center, &NotificationCenter::itemsChanged, this, &DesktopNotifier::onItemsChanged);
    // Back in Basecamp, the list has everything: the desktop copies are noise.
    connect(qApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState s) {
        if (s == Qt::ApplicationActive)
            closeAll();
    });
    connect(m_center, &NotificationCenter::desktopChanged, this, [this]() {
        if (!m_center->desktop())
            closeAll();
    });
}

void DesktopNotifier::onPosted(const QVariantMap& n, bool quiet)
{
    if (quiet || !m_center->desktop() || !inBackground())
        return;
    send(n);
}

void DesktopNotifier::send(const QVariantMap& n)
{
#ifdef BASECAMP_DBUS_NOTIFY
    const QString id = n.value(QStringLiteral("id")).toString();
    const QString app = n.value(QStringLiteral("app")).toString();
    const QString name = app == NotificationCenter::kBasecampApp
        ? QStringLiteral("Basecamp")
        : (m_nameFor ? m_nameFor(app) : QString());
    const QString title = n.value(QStringLiteral("title")).toString();
    const QVariantMap action = n.value(QStringLiteral("action")).toMap();

    // "default" is a click on the notification itself.
    QStringList actions{ QStringLiteral("default"), tr("Open") };
    if (!action.isEmpty())
        actions << QStringLiteral("action") << action.value(QStringLiteral("label")).toString();

    const QString level = n.value(QStringLiteral("level")).toString();
    QVariantMap hints;
    // 0 low, 1 normal, 2 critical. Critical stays on screen until dismissed,
    // like an error in Basecamp's own list.
    hints[QStringLiteral("urgency")] = QVariant::fromValue<uchar>(level == QLatin1String("error") ? 2 : 1);
    const QString icon = iconPath(app, n.value(QStringLiteral("icon")).toString());
    if (!icon.isEmpty())
        hints[QStringLiteral("image-path")] = icon;

    QDBusMessage call = QDBusMessage::createMethodCall(kService, kPath, kInterface, QStringLiteral("Notify"));
    call << QStringLiteral("Logos Basecamp")
         << uint(0)
         << icon
         << (name.isEmpty() ? title : name + QStringLiteral(": ") + title)
         // Servers may read the body as markup; app text is plain.
         << n.value(QStringLiteral("body")).toString().toHtmlEscaped()
         << actions
         << hints
         << int(-1);
    auto* watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, id](QDBusPendingCallWatcher* w) {
        QDBusPendingReply<uint> reply = *w;
        w->deleteLater();
        if (reply.isError()) {
            qWarning() << "Desktop notification failed:" << reply.error().message();
            return;
        }
        const uint osId = reply.value();
        // Dismissed in Basecamp (or Basecamp came to the front) before the
        // desktop answered: take it straight back.
        bool live = false;
        for (const QVariant& v : m_center->items())
            if (v.toMap().value(QStringLiteral("id")) == id)
                live = true;
        if (!live || !inBackground()) {
            close(osId);
            return;
        }
        m_osIds.insert(id, osId);
    });
#else
    Q_UNUSED(n);
#endif
}

void DesktopNotifier::onItemsChanged()
{
    if (m_osIds.isEmpty())
        return;
    QSet<QString> live;
    for (const QVariant& v : m_center->items())
        live.insert(v.toMap().value(QStringLiteral("id")).toString());
    for (auto it = m_osIds.begin(); it != m_osIds.end();) {
        if (!live.contains(it.key())) {
            close(it.value());
            it = m_osIds.erase(it);
        } else {
            ++it;
        }
    }
}

void DesktopNotifier::onActionInvoked(uint osId, const QString& key)
{
    const QString id = m_osIds.key(osId);
    if (id.isEmpty())
        return;   // another app's notification
    raiseBasecamp();
    // Same as a click in Basecamp's list: open the sender (or run the button),
    // which also dismisses it.
    m_center->activate(id, key == QLatin1String("action"));
}

void DesktopNotifier::onActivationToken(uint osId, const QString& token)
{
    // Sent just before ActionInvoked. Wayland compositors only let a window
    // take focus with a token; Qt's Wayland plugin reads it from here.
    if (!m_osIds.key(osId).isEmpty() && !token.isEmpty())
        qputenv("XDG_ACTIVATION_TOKEN", token.toUtf8());
}

void DesktopNotifier::onClosed(uint osId, uint)
{
    const QString id = m_osIds.key(osId);
    if (!id.isEmpty())
        m_osIds.remove(id);   // closed on the desktop: still in Basecamp's list
}

void DesktopNotifier::close(uint osId)
{
#ifdef BASECAMP_DBUS_NOTIFY
    QDBusMessage call = QDBusMessage::createMethodCall(kService, kPath, kInterface,
                                                       QStringLiteral("CloseNotification"));
    call << osId;
    QDBusConnection::sessionBus().asyncCall(call);
#else
    Q_UNUSED(osId);
#endif
}

void DesktopNotifier::closeAll()
{
    for (uint osId : std::as_const(m_osIds))
        close(osId);
    m_osIds.clear();
}

QString DesktopNotifier::iconPath(const QString& app, const QString& iconUrl)
{
    if (m_icons.contains(app))
        return m_icons.value(app);
    // The desktop needs a file; app icons are often SVG or inside resources.
    QString source = iconUrl;
    if (source.startsWith(QStringLiteral("file:")))
        source = QUrl(source).toLocalFile();
    else if (source.startsWith(QStringLiteral("qrc:")))
        source = source.mid(3);
    QString path;
    const QPixmap pm = source.isEmpty() ? QPixmap() : QIcon(source).pixmap(128, 128);
    if (!pm.isNull()) {
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                            + QStringLiteral("/notification-icons");
        QDir().mkpath(dir);
        const QString file = dir + QLatin1Char('/') + QString(app).replace(QLatin1Char('/'), QLatin1Char('_'))
                             + QStringLiteral(".png");
        if (pm.save(file, "PNG"))
            path = file;
    }
    m_icons.insert(app, path);
    return path;
}

void DesktopNotifier::raiseBasecamp()
{
    for (QWidget* w : QApplication::topLevelWidgets()) {
        if (!qobject_cast<QMainWindow*>(w))
            continue;
        w->setWindowState(w->windowState() & ~Qt::WindowMinimized);
        w->show();
        w->raise();
        w->activateWindow();
        if (QWindow* h = w->windowHandle())
            h->requestActivate();
    }
}
