#include "NotificationCenter.h"

#include <QMetaType>
#include <QSettings>

namespace {

// The user's per-app choice outlives the session (QSettings, like the shell's
// other preferences).
constexpr const char* kMutedKey = "Notifications/muted";
constexpr const char* kDesktopKey = "Notifications/desktop";

bool isString(const QVariant& v)
{
    return v.metaType().id() == QMetaType::QString;
}

} // namespace

NotificationCenter::NotificationCenter(QObject* parent)
    : QObject(parent)
{
    const QStringList muted = QSettings().value(QLatin1String(kMutedKey)).toStringList();
    m_muted = QSet<QString>(muted.begin(), muted.end());
    m_desktop = QSettings().value(QLatin1String(kDesktopKey), true).toBool();
}

QStringList NotificationCenter::unreadApps() const
{
    QStringList apps;
    for (const QVariant& v : m_items) {
        const QVariantMap n = v.toMap();
        const QString app = n.value(QStringLiteral("app")).toString();
        if (!n.value(QStringLiteral("read")).toBool() && !apps.contains(app))
            apps.append(app);
    }
    return apps;
}

void NotificationCenter::setDesktop(bool on)
{
    if (m_desktop == on)
        return;
    m_desktop = on;
    QSettings().setValue(QLatin1String(kDesktopKey), on);
    emit desktopChanged();
}

int NotificationCenter::unreadCount() const
{
    int n = 0;
    for (const QVariant& v : m_items)
        if (!v.toMap().value(QStringLiteral("read")).toBool())
            ++n;
    return n;
}

void NotificationCenter::setDoNotDisturb(bool on)
{
    if (m_dnd == on)
        return;
    m_dnd = on;
    emit doNotDisturbChanged();
}

bool NotificationCenter::allow(const QString& app)
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    auto& times = m_recent[app];
    while (!times.empty() && now - times.front() > 60000)
        times.pop_front();
    if (int(times.size()) >= kRatePerMinute)
        return false;
    times.push_back(now);
    return true;
}

QString NotificationCenter::post(const QString& app, const QVariantMap& params, QString* error)
{
    auto fail = [error](const char* code) {
        if (error) *error = QString::fromLatin1(code);
        return QString();
    };
    if (app.isEmpty() || app == kBasecampApp)
        return fail("bad_request");

    // Plain strings only, within limits. Unknown keys are ignored so a newer
    // app can still post to an older Basecamp.
    const QVariant title = params.value(QStringLiteral("title"));
    const QVariant body = params.value(QStringLiteral("body"), QString());
    const QVariant level = params.value(QStringLiteral("level"), QStringLiteral("info"));
    const QVariant tag = params.value(QStringLiteral("tag"), QString());
    if (!isString(title) || !isString(body) || !isString(level) || !isString(tag))
        return fail("bad_request");
    const QString t = title.toString().trimmed();
    const QString l = level.toString();
    if (t.isEmpty() || t.size() > kMaxTitle || body.toString().size() > kMaxBody
        || tag.toString().size() > kMaxTag
        || !QStringList{ "info", "success", "warning", "error" }.contains(l))
        return fail("bad_request");

    if (!allow(app))
        return fail("failed");

    QVariantMap n;
    n[QStringLiteral("app")] = app;
    n[QStringLiteral("title")] = t;
    n[QStringLiteral("body")] = body.toString().trimmed();
    n[QStringLiteral("level")] = l;
    n[QStringLiteral("tag")] = tag.toString();

    if (m_muted.contains(app))
        return QStringLiteral("n%1").arg(++m_next);   // answered, not shown
    return store(n);
}

QString NotificationCenter::postSystem(const QString& title, const QString& body,
                                       const QString& level, const QVariantMap& action)
{
    QVariantMap n;
    n[QStringLiteral("app")] = kBasecampApp;
    n[QStringLiteral("title")] = title.left(kMaxTitle);
    n[QStringLiteral("body")] = body.left(kMaxBody);
    n[QStringLiteral("level")] = level;
    n[QStringLiteral("tag")] = QString();
    if (!action.isEmpty())
        n[QStringLiteral("action")] = action;
    return store(n);
}

QString NotificationCenter::store(QVariantMap n)
{
    const QString id = QStringLiteral("n%1").arg(++m_next);
    n[QStringLiteral("id")] = id;
    n[QStringLiteral("time")] = QDateTime::currentMSecsSinceEpoch();
    if (m_iconFor)
        n[QStringLiteral("icon")] = m_iconFor(n.value(QStringLiteral("app")).toString());
    n[QStringLiteral("read")] = false;

    // Same app + tag replaces the earlier one ("3 new stations", not 3 toasts).
    const QString tag = n.value(QStringLiteral("tag")).toString();
    if (!tag.isEmpty()) {
        for (int i = 0; i < m_items.size(); ++i) {
            const QVariantMap old = m_items.at(i).toMap();
            if (old.value(QStringLiteral("app")) == n.value(QStringLiteral("app"))
                && old.value(QStringLiteral("tag")).toString() == tag) {
                m_items.removeAt(i);
                break;
            }
        }
    }
    m_items.prepend(n);
    while (m_items.size() > kMaxItems)
        m_items.removeLast();
    emit itemsChanged();
    emit posted(n, m_dnd);
    return id;
}

void NotificationCenter::dismiss(const QString& id)
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).toMap().value(QStringLiteral("id")) == id) {
            m_items.removeAt(i);
            emit itemsChanged();
            return;
        }
    }
}

void NotificationCenter::clearAll()
{
    if (m_items.isEmpty())
        return;
    m_items.clear();
    emit itemsChanged();
}

void NotificationCenter::markAllRead()
{
    bool changed = false;
    for (QVariant& v : m_items) {
        QVariantMap m = v.toMap();
        if (!m.value(QStringLiteral("read")).toBool()) {
            m[QStringLiteral("read")] = true;
            v = m;
            changed = true;
        }
    }
    if (changed)
        emit itemsChanged();
}

void NotificationCenter::markAppRead(const QString& app)
{
    bool changed = false;
    for (QVariant& v : m_items) {
        QVariantMap m = v.toMap();
        if (m.value(QStringLiteral("app")) == app && !m.value(QStringLiteral("read")).toBool()) {
            m[QStringLiteral("read")] = true;
            v = m;
            changed = true;
        }
    }
    if (changed)
        emit itemsChanged();
}

void NotificationCenter::setMuted(const QString& app, bool muted)
{
    if (app.isEmpty() || app == kBasecampApp || muted == m_muted.contains(app))
        return;
    if (muted) {
        m_muted.insert(app);
        // Muting also clears what that app already posted.
        for (int i = m_items.size() - 1; i >= 0; --i)
            if (m_items.at(i).toMap().value(QStringLiteral("app")) == app)
                m_items.removeAt(i);
        emit itemsChanged();
    } else {
        m_muted.remove(app);
    }
    QSettings().setValue(QLatin1String(kMutedKey), mutedApps());
    emit mutedAppsChanged();
}

void NotificationCenter::activate(const QString& id, bool useAction)
{
    for (const QVariant& v : std::as_const(m_items)) {
        const QVariantMap n = v.toMap();
        if (n.value(QStringLiteral("id")) != id)
            continue;
        const QString app = n.value(QStringLiteral("app")).toString();
        const QString launch = useAction
            ? n.value(QStringLiteral("action")).toMap().value(QStringLiteral("launch")).toString()
            : QString();
        // A Basecamp notice with no action has nothing to open, so it stays.
        // Anything that opens an app is done once it has.
        if (useAction || app != kBasecampApp) {
            emit activated(app, launch);
            dismiss(id);
        }
        return;
    }
}
