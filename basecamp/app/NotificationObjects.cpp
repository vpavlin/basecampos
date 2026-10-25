#include "NotificationObjects.h"

#include "NotificationCenter.h"
#include "ObjectStore.h"

#include <QDebug>
#include <QStringList>
#include <QVariantList>

namespace {

const QString kPrefix = QStringLiteral("basecamp.notification:");
const QString kType = QStringLiteral("notification");

} // namespace

NotificationObjects::NotificationObjects(NotificationCenter* center, ObjectStore* store,
                                         QObject* parent)
    : QObject(parent)
    , m_center(center)
    , m_store(store)
{
    connect(m_center, &NotificationCenter::posted, this,
            [this](const QVariantMap& n, bool) { mirror(n); });
    connect(m_center, &NotificationCenter::mutedAppsChanged, this, &NotificationObjects::dropMuted);
}

QString NotificationObjects::keyFor(const QVariantMap& n)
{
    const QString tag = n.value(QStringLiteral("tag")).toString();
    return tag.isEmpty() ? kPrefix + n.value(QStringLiteral("id")).toString()
                         : kPrefix + QStringLiteral("tag:") + tag;
}

bool NotificationObjects::isMirrorKey(const QString& key)
{
    return key.startsWith(kPrefix);
}

QVariantMap NotificationObjects::envelopeFor(const QVariantMap& n)
{
    QVariantMap fields{ { QStringLiteral("level"), n.value(QStringLiteral("level")).toString() } };
    const QString body = n.value(QStringLiteral("body")).toString();
    if (!body.isEmpty())
        fields[QStringLiteral("body")] = body;
    QVariantMap o{
        { QStringLiteral("key"), keyFor(n) },
        { QStringLiteral("type"), kType },
        { QStringLiteral("title"), n.value(QStringLiteral("title")).toString() },
        { QStringLiteral("fields"), fields },
    };
    const qint64 time = n.value(QStringLiteral("time")).toLongLong();
    if (time > 0)
        o[QStringLiteral("time")] = time;
    return o;
}

void NotificationObjects::mirror(const QVariantMap& n)
{
    const QString app = n.value(QStringLiteral("app")).toString();
    if (app.isEmpty() || app == NotificationCenter::kBasecampApp)
        return;
    const QString error = m_store->publishFromHost(
        app, QVariantMap{ { QStringLiteral("objects"), QVariantList{ envelopeFor(n) } } });
    if (!error.isEmpty()) {
        qWarning().noquote() << "NotificationObjects:" << app << "notification not mirrored:" << error;
        return;
    }
    trim(app);
}

void NotificationObjects::trim(const QString& app)
{
    // Untagged notifications each make a card; without a cap they would push
    // out what the app publishes itself (the store's per-app limit).
    const QVariantList mine = m_store->list(QVariantMap{
        { QStringLiteral("apps"), QStringList{ app } },
        { QStringLiteral("types"), QStringList{ kType } } });
    QStringList excess;
    int kept = 0;
    for (const QVariant& v : mine) {   // newest first
        const QString key = v.toMap().value(QStringLiteral("key")).toString();
        if (!isMirrorKey(key))
            continue;
        if (++kept > kMaxPerApp)
            excess << key;
    }
    if (!excess.isEmpty())
        m_store->retract(app, QVariantMap{ { QStringLiteral("keys"), excess } });
}

void NotificationObjects::dropMuted()
{
    for (const QString& app : m_center->mutedApps()) {
        QStringList keys;
        const QVariantList mine = m_store->list(QVariantMap{
            { QStringLiteral("apps"), QStringList{ app } },
            { QStringLiteral("types"), QStringList{ kType } } });
        for (const QVariant& v : mine) {
            const QString key = v.toMap().value(QStringLiteral("key")).toString();
            if (isMirrorKey(key))
                keys << key;
        }
        if (!keys.isEmpty())
            m_store->retract(app, QVariantMap{ { QStringLiteral("keys"), keys } });
    }
}
