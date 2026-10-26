#include "AppDataStore.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace {

QVariantMap ok() { return { { QStringLiteral("ok"), true } }; }

QVariantMap fail(const char* code)
{
    return { { QStringLiteral("ok"), false }, { QStringLiteral("error"), QLatin1String(code) } };
}

// A module name as a folder name, nothing that could point outside it.
bool plainName(const QString& app)
{
    return !app.isEmpty() && !app.startsWith(QLatin1Char('.')) && !app.contains(QLatin1Char('/'))
        && !app.contains(QLatin1Char('\\')) && !app.contains(QChar(0));
}

} // namespace

AppDataStore::AppDataStore(QString baseDir, QObject* parent)
    : QObject(parent)
    , m_base(std::move(baseDir))
{
}

QString AppDataStore::fileFor(const QString& app) const
{
    return m_base + QLatin1Char('/') + app + QStringLiteral("/appdata.json");
}

QVariantMap AppDataStore::read(const QString& app)
{
    if (!plainName(app)) return fail("bad_request");
    QVariantMap data;
    const auto cached = m_cache.constFind(app);
    if (cached != m_cache.cend()) {
        data = cached.value();
    } else if (!m_base.isEmpty()) {
        QFile file(fileFor(app));
        if (file.open(QIODevice::ReadOnly))
            data = QJsonDocument::fromJson(file.read(kMaxBytes + 1)).object().toVariantMap();
        m_cache.insert(app, data);
    }
    QVariantMap r = ok();
    r.insert(QStringLiteral("data"), data);
    return r;
}

QVariantMap AppDataStore::write(const QString& app, const QVariant& value)
{
    if (!plainName(app) || value.typeId() != QMetaType::QVariantMap) return fail("bad_request");
    const QVariantMap data = value.toMap();
    const QByteArray json = QJsonDocument(QJsonObject::fromVariantMap(data)).toJson(QJsonDocument::Compact);
    if (json.size() > kMaxBytes) return fail("too_big");

    if (!m_base.isEmpty()) {
        const QString path = fileFor(app);
        if (!QDir().mkpath(QFileInfo(path).absolutePath())) return fail("failed");
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly)) return fail("failed");
        file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        file.write(json);
        if (!file.commit()) {
            qWarning().noquote() << "AppDataStore: could not write the data of" << app;
            return fail("failed");
        }
        QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    }
    m_cache.insert(app, data);
    return ok();
}

void AppDataStore::removeApp(const QString& app)
{
    if (!plainName(app)) return;
    m_cache.remove(app);
    if (!m_base.isEmpty()) QFile::remove(fileFor(app));
}
