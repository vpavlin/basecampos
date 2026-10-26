#include "HomeSettingsStore.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace {

QJsonObject readAll(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    return doc.isObject() ? doc.object() : QJsonObject();
}

} // namespace

HomeSettingsStore::HomeSettingsStore(QString filePath) : m_path(std::move(filePath)) {}

QVariantMap HomeSettingsStore::load(const QString& app) const
{
    if (app.isEmpty()) return {};
    const QJsonValue v = readAll(m_path).value(app);
    return v.isObject() ? v.toObject().toVariantMap() : QVariantMap();
}

HomeSettingsStore::Result HomeSettingsStore::save(const QString& app, const QVariantMap& settings)
{
    if (app.isEmpty()) return Result::Refused;
    const QJsonObject mine = QJsonObject::fromVariantMap(settings);
    if (QJsonDocument(mine).toJson(QJsonDocument::Compact).size() > kMaxBytes) {
        qWarning() << "HomeSettingsStore: refused" << app << "settings over" << kMaxBytes << "bytes";
        return Result::Refused;
    }
    QJsonObject all = readAll(m_path);
    if (all.value(app) == QJsonValue(mine)) return Result::Unchanged;
    all.insert(app, mine);

    // QSaveFile: a crash mid-write leaves the old file, not half a new one.
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly)
        || file.write(QJsonDocument(all).toJson(QJsonDocument::Compact)) < 0
        || !file.commit()) {
        qWarning() << "HomeSettingsStore: failed to write" << m_path << file.errorString();
        return Result::Refused;
    }
    return Result::Saved;
}
