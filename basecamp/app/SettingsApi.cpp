#include "SettingsApi.h"

#include "MainUIBackend.h"
#include "UIPluginManager.h"

#include <QImage>
#include <QQuickImageProvider>
#include <QUrl>

namespace {

QString localPathFor(const QString& url)
{
    if (url.startsWith(QStringLiteral("file:"))) return QUrl(url).toLocalFile();
    if (url.startsWith(QStringLiteral("qrc:")))  return url.mid(3);   // ":/…"
    return url;   // already a path (or ":/…")
}

class BasecampImageProvider : public QQuickImageProvider {
public:
    BasecampImageProvider(MainUIBackend* backend, const QString& app)
        : QQuickImageProvider(QQuickImageProvider::Image), m_backend(backend), m_app(app) {}

    QImage requestImage(const QString& id, QSize* size, const QSize& requested) override
    {
        QImage image;
        if (m_backend && m_backend->roleHolder(QStringLiteral("settings")) == m_app) {
            QString path;
            if (id.startsWith(QStringLiteral("appicon/"))) {
                const QString name = id.mid(8);
                if (UIPluginManager* plugins = m_backend->uiPluginManager())
                    path = localPathFor(plugins->pluginIconUrl(name));
            } else if (id.startsWith(QStringLiteral("wallpaper/"))) {
                QObject* prefs = m_backend->shellPreferences();
                const char* key = id.startsWith(QStringLiteral("wallpaper/custom"))
                    ? "customWallpaperUrl" : "builtinWallpaperUrl";
                if (prefs) path = localPathFor(prefs->property(key).toUrl().toString());
            }
            if (!path.isEmpty()) image.load(path);
        }
        if (!image.isNull() && requested.isValid())
            image = image.scaled(requested, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        if (size) *size = image.size();
        return image;
    }

private:
    QPointer<MainUIBackend> m_backend;
    QString m_app;
};

QVariantList withServedIcons(QVariantList rows)
{
    for (QVariant& v : rows) {
        QVariantMap row = v.toMap();
        const QString name = row.value(QStringLiteral("name")).toString();
        if (!name.isEmpty())
            row.insert(QStringLiteral("icon"), QStringLiteral("image://basecamp/appicon/") + name);
        v = row;
    }
    return rows;
}

} // namespace

QQuickImageProvider* SettingsApi::createImageProvider(MainUIBackend* backend, const QString& appName)
{
    return new BasecampImageProvider(backend, appName);
}

SettingsApi::SettingsApi(MainUIBackend* backend, const QString& appName, QObject* parent)
    : QObject(parent)
    , m_backend(backend)
    , m_app(appName)
{
    if (!backend) return;
    connect(backend, &MainUIBackend::repositoriesChanged, this, &SettingsApi::repositoriesChanged);
    connect(backend, &MainUIBackend::repositoriesLoadingChanged, this, &SettingsApi::repositoriesChanged);
    connect(backend, &MainUIBackend::downloadSourceChanged, this, &SettingsApi::repositoriesChanged);
    connect(backend, &MainUIBackend::repositoryOperationCompleted, this,
            [this](const QString& op, const QString& url, bool ok, const QString& err) {
                if (allowed()) emit repositoryOperationCompleted(op, url, ok, err);
            });
    connect(backend, &MainUIBackend::modulesLoadingChanged, this, &SettingsApi::modulesChanged);
    connect(backend, &MainUIBackend::notificationAppsChanged, this, &SettingsApi::notificationsChanged);
    if (QObject* center = backend->notifications())
        connect(center, SIGNAL(desktopChanged()), this, SIGNAL(notificationsChanged()));
    // A role change can also revoke this object, so every bound property re-reads.
    connect(backend, &MainUIBackend::rolesChanged, this, [this]() {
        emit rolesChanged();
        emit repositoriesChanged();
        emit modulesChanged();
        emit notificationsChanged();
        emit prefsChanged();
    });
    connect(backend, &MainUIBackend::shellPreferencesChanged, this, &SettingsApi::prefsChanged);
    connect(backend, &MainUIBackend::settingsSectionRequested, this, [this](const QString& section) {
        if (allowed()) emit sectionRequested(section);
    });
}

bool SettingsApi::allowed() const
{
    return m_backend && m_backend->roleHolder(QStringLiteral("settings")) == m_app;
}

QVariant SettingsApi::read(const char* property) const
{
    return allowed() ? m_backend->property(property) : QVariant();
}

void SettingsApi::call(const char* method)
{
    if (allowed()) QMetaObject::invokeMethod(m_backend, method);
}

void SettingsApi::call(const char* method, const QString& a)
{
    if (allowed()) QMetaObject::invokeMethod(m_backend, method, Q_ARG(QString, a));
}

QVariantList SettingsApi::repositories() const     { return read("repositories").toList(); }
bool SettingsApi::repositoriesLoading() const      { return read("repositoriesLoading").toBool(); }
QString SettingsApi::downloadSource() const        { return read("downloadSource").toString(); }
bool SettingsApi::repositoryManagement() const     { return m_backend && m_backend->repositoryManagement(); }
QObject* SettingsApi::uiModulesModel() const       { return allowed() ? m_backend->property("uiModulesModel").value<QObject*>() : nullptr; }
QObject* SettingsApi::coreModulesModel() const     { return allowed() ? m_backend->property("coreModulesModel").value<QObject*>() : nullptr; }
bool SettingsApi::modulesLoading() const           { return read("modulesLoading").toBool(); }
QString SettingsApi::buildVersion() const          { return m_backend ? m_backend->property("buildVersion").toString() : QString(); }
bool SettingsApi::isPortableBuild() const          { return m_backend && m_backend->property("isPortableBuild").toBool(); }
QVariantList SettingsApi::buildCommits() const     { return m_backend ? m_backend->property("buildCommits").toList() : QVariantList(); }
QVariantList SettingsApi::notificationApps() const { return withServedIcons(read("notificationApps").toList()); }
QVariantMap SettingsApi::roles() const             { return m_backend ? m_backend->roles() : QVariantMap(); }
QObject* SettingsApi::prefs() const                { return allowed() ? m_backend->shellPreferences() : nullptr; }
QString SettingsApi::pendingSection() const        { return allowed() ? m_backend->pendingSettingsSection() : QString(); }

bool SettingsApi::desktopNotifications() const
{
    QObject* center = allowed() ? m_backend->notifications() : nullptr;
    return center && center->property("desktop").toBool();
}

void SettingsApi::setDesktopNotifications(bool on)
{
    if (QObject* center = allowed() ? m_backend->notifications() : nullptr)
        center->setProperty("desktop", on);
}

void SettingsApi::refreshRepositories()                  { call("refreshRepositories"); }
void SettingsApi::addRepository(const QString& url)      { call("addRepository", url); }
void SettingsApi::removeRepository(const QString& url)   { call("removeRepository", url); }
void SettingsApi::refreshDownloadSource()                { call("refreshDownloadSource"); }
void SettingsApi::setDownloadSource(const QString& s)    { call("setDownloadSource", s); }
void SettingsApi::refreshUiModules()                     { call("refreshUiModules"); }
void SettingsApi::loadUiModule(const QString& name)      { call("loadUiModule", name); }
void SettingsApi::unloadUiModule(const QString& name)    { call("unloadUiModule", name); }
void SettingsApi::refreshCoreModules()                   { call("refreshCoreModules"); }
void SettingsApi::loadCoreModule(const QString& name)    { call("loadCoreModule", name); }
void SettingsApi::unloadCoreModule(const QString& name)  { call("unloadCoreModule", name); }

void SettingsApi::setRepositoryEnabled(const QString& url, bool enabled)
{
    if (allowed())
        QMetaObject::invokeMethod(m_backend, "setRepositoryEnabled",
                                  Q_ARG(QString, url), Q_ARG(bool, enabled));
}

QString SettingsApi::getCoreModuleMethods(const QString& name)
{
    return allowed() ? m_backend->getCoreModuleMethods(name) : QString();
}

QString SettingsApi::getCoreModuleEvents(const QString& name)
{
    return allowed() ? m_backend->getCoreModuleEvents(name) : QString();
}

QString SettingsApi::callCoreModuleMethod(const QString& name, const QString& method,
                                          const QString& argsJson)
{
    return allowed() ? m_backend->callCoreModuleMethod(name, method, argsJson) : QString();
}

void SettingsApi::setNotificationsEnabled(const QString& app, bool enabled)
{
    if (allowed()) m_backend->setNotificationsEnabled(app, enabled);
}

QVariantList SettingsApi::roleCandidates(const QString& role) const
{
    return allowed() ? withServedIcons(m_backend->roleCandidates(role)) : QVariantList();
}

bool SettingsApi::assignRole(const QString& role, const QString& app)
{
    return allowed() && m_backend->assignRole(role, app);
}

void SettingsApi::resetRole(const QString& role)
{
    if (allowed()) m_backend->resetRole(role);
}

// Opening another role's app is navigation any app may already ask for
// (basecamp.apps.launch); allowed for the holder without a second check.
bool SettingsApi::openRole(const QString& role)
{
    return allowed() && m_backend->openRole(role);
}

QString SettingsApi::takePendingSection()
{
    return allowed() ? m_backend->takePendingSettingsSection() : QString();
}
