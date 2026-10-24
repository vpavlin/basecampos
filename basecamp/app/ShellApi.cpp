#include "ShellApi.h"

#include "IntentRegistry.h"
#include "MainUIBackend.h"
#include "UIPluginManager.h"

#include "BasecampModelRoles.h"

#include <QAbstractItemModel>
#include <QCoreApplication>
#include <QFileInfo>
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

bool holdsAnyRole(MainUIBackend* backend, const QString& app)
{
    if (!backend || app.isEmpty()) return false;
    for (const QString& role : IntentRegistry::knownRoles())
        if (backend->roleHolder(role) == app) return true;
    return false;
}

// What the host will draw for a role app, chosen by the host: an app's icon by
// name (its installed file, or the URL the catalog gives for it) or one of the
// shell's two wallpapers. The app names the item, never the address.
QString iconSourceFor(MainUIBackend* backend, const QString& name)
{
    if (UIPluginManager* plugins = backend->uiPluginManager()) {
        const QString installed = plugins->pluginIconUrl(name);
        if (!installed.isEmpty() && !installed.startsWith(QStringLiteral("qrc:"))
            && QFileInfo::exists(localPathFor(QUrl(installed).adjusted(QUrl::RemoveQuery).toString())))
            return QUrl(installed).adjusted(QUrl::RemoveQuery).toString();
    }
    // Not installed: the catalog's icon, which the package downloader resolves
    // to a release asset URL.
    if (QAbstractItemModel* model = backend->appsModel()) {
        for (int i = 0; i < model->rowCount(); ++i) {
            const QModelIndex ix = model->index(i, 0);
            if (ix.data(AppsModelRoles::NameRole).toString() == name) {
                const QString url = ix.data(AppsModelRoles::IconUrlRole).toString();
                if (!url.isEmpty()) return url;
            }
        }
    }
    if (UIPluginManager* plugins = backend->uiPluginManager())
        return plugins->pluginIconUrl(name);   // qrc: default, or ""
    return {};
}

class BasecampImageResponse : public QQuickImageResponse {
public:
    BasecampImageResponse(QPointer<MainUIBackend> backend, QString app, QString id, QSize requested)
        : m_requested(requested)
    {
        // Model lookups and the network live on the GUI thread. Moved there and
        // called through itself, so a response QML drops first is never touched.
        moveToThread(qApp->thread());
        QMetaObject::invokeMethod(this, [this, backend, app, id]() { start(backend, app, id); },
                                  Qt::QueuedConnection);
    }

    QQuickTextureFactory* textureFactory() const override
    {
        return QQuickTextureFactory::textureFactoryForImage(m_image);
    }

private:
    void start(QPointer<MainUIBackend> backend, const QString& app, const QString& id)
    {
        if (!holdsAnyRole(backend, app)) { finish({}); return; }
        QString source;
        if (id.startsWith(QStringLiteral("appicon/"))) {
            source = iconSourceFor(backend, id.mid(8));
        } else if (id.startsWith(QStringLiteral("wallpaper/"))
                   && backend->roleHolder(QStringLiteral("settings")) == app) {
            if (QObject* prefs = backend->shellPreferences())
                source = prefs->property(id.startsWith(QStringLiteral("wallpaper/custom"))
                                             ? "customWallpaperUrl" : "builtinWallpaperUrl")
                             .toUrl().toString();
        }
        // Local files only. A catalog icon for an app that isn't installed is a
        // GitHub URL (logos-package-downloader resolves the index's icon path
        // against the index URL), fetched with no storage-network source and no
        // sha256 check; the host doesn't repeat that on an app's behalf. Such
        // tiles show a placeholder until the catalog offers a storage source.
        if (source.contains(QStringLiteral("://")) && !source.startsWith(QStringLiteral("file:"))) {
            finish({});
            return;
        }
        QImage image;
        if (!source.isEmpty()) image.load(localPathFor(source));
        finish(image);
    }

    void finish(QImage image)
    {
        if (!image.isNull() && m_requested.isValid())
            image = image.scaled(m_requested, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_image = image;
        emit finished();
    }

    QSize m_requested;
    QImage m_image;
};

class BasecampImageProvider : public QQuickAsyncImageProvider {
public:
    BasecampImageProvider(MainUIBackend* backend, const QString& app) : m_backend(backend), m_app(app) {}

    QQuickImageResponse* requestImageResponse(const QString& id, const QSize& requested) override
    {
        return new BasecampImageResponse(m_backend, m_app, id, requested);
    }

private:
    QPointer<MainUIBackend> m_backend;
    QString m_app;
};

// Rows whose icon is a file outside the app's sandbox: point them at the host.
QVariantList withServedIcons(QVariantList rows)
{
    for (QVariant& v : rows) {
        QVariantMap row = v.toMap();
        const QString name = row.value(QStringLiteral("name")).toString();
        if (!name.isEmpty()) {
            const QString served = QStringLiteral("image://basecamp/appicon/") + name;
            // Only where there is an icon: an empty one means "draw a placeholder".
            for (const char* key : { "icon", "iconPath" })
                if (!row.value(QLatin1String(key)).toString().isEmpty())
                    row.insert(QLatin1String(key), served);
            if (!row.contains(QStringLiteral("icon")))
                row.insert(QStringLiteral("icon"), served);
        }
        v = row;
    }
    return rows;
}

} // namespace

QQuickAsyncImageProvider* ShellApi::createImageProvider(MainUIBackend* backend, const QString& appName)
{
    return new BasecampImageProvider(backend, appName);
}

ShellApi::ShellApi(MainUIBackend* backend, const QString& appName, QObject* parent)
    : QObject(parent)
    , m_backend(backend)
    , m_app(appName)
{
    if (!backend) return;
    connect(backend, &MainUIBackend::repositoriesChanged, this, &ShellApi::repositoriesChanged);
    connect(backend, &MainUIBackend::repositoriesLoadingChanged, this, &ShellApi::repositoriesChanged);
    connect(backend, &MainUIBackend::downloadSourceChanged, this, &ShellApi::repositoriesChanged);
    connect(backend, &MainUIBackend::repositoryOperationCompleted, this,
            [this](const QString& op, const QString& url, bool ok, const QString& err) {
                if (holds("settings")) emit repositoryOperationCompleted(op, url, ok, err);
            });
    connect(backend, &MainUIBackend::modulesLoadingChanged, this, &ShellApi::modulesChanged);
    connect(backend, &MainUIBackend::notificationAppsChanged, this, &ShellApi::notificationsChanged);
    // Gated like the event signals below, not like the plain re-read signals
    // above: `shell` is handed to any app that merely CLAIMS a role, and even
    // debounced (MainUIBackend::refreshSearchApps) this changes with other
    // apps' object counts — timing a non-holder has no business observing.
    connect(backend, &MainUIBackend::searchAppsChanged, this, [this]() {
        if (holds("settings")) emit searchChanged();
    });
    if (QObject* center = backend->notifications())
        connect(center, SIGNAL(desktopChanged()), this, SIGNAL(notificationsChanged()));
    connect(backend, &MainUIBackend::launcherAppsChanged, this, &ShellApi::appsChanged);
    connect(backend, &MainUIBackend::loadingModulesChanged, this, &ShellApi::appsChanged);
    connect(backend, &MainUIBackend::appsLoadingChanged, this, &ShellApi::appsChanged);
    // A role change can grant or revoke any member, so every bound property re-reads.
    connect(backend, &MainUIBackend::rolesChanged, this, [this]() {
        emit rolesChanged();
        emit repositoriesChanged();
        emit modulesChanged();
        emit notificationsChanged();
        emit searchChanged();
        emit prefsChanged();
        emit appsChanged();
        emit homeQueryChanged();
    });
    connect(backend, &MainUIBackend::shellPreferencesChanged, this, &ShellApi::prefsChanged);
    connect(backend, &MainUIBackend::settingsSectionRequested, this, [this](const QString& section) {
        if (holds("settings")) emit sectionRequested(section);
    });
    connect(backend, &MainUIBackend::launcherSearchRequested, this, [this](const QString& query) {
        if (holds("launcher")) emit launcherSearchRequested(query);
    });
    connect(backend, &MainUIBackend::homeQueryChanged, this, [this]() {
        if (holds("home")) emit homeQueryChanged();
    });
    connect(backend, &MainUIBackend::homeQueryAccepted, this, [this]() {
        if (holds("home")) emit homeQueryAccepted();
    });
}

bool ShellApi::holds(const char* role) const
{
    return m_backend && m_backend->roleHolder(QLatin1String(role)) == m_app;
}

bool ShellApi::holdsAny() const
{
    return holdsAnyRole(m_backend, m_app);
}

QVariantMap ShellApi::roles() const { return m_backend ? m_backend->roles() : QVariantMap(); }

// ── settings ─────────────────────────────────────────────────────────────────

bool ShellApi::repositoriesLoading() const { return holds("settings") && m_backend->property("repositoriesLoading").toBool(); }
QString ShellApi::downloadSource() const   { return holds("settings") ? m_backend->property("downloadSource").toString() : QString(); }
bool ShellApi::repositoryManagement() const { return m_backend && m_backend->repositoryManagement(); }
QObject* ShellApi::uiModulesModel() const  { return holds("settings") ? m_backend->property("uiModulesModel").value<QObject*>() : nullptr; }
QObject* ShellApi::coreModulesModel() const { return holds("settings") ? m_backend->property("coreModulesModel").value<QObject*>() : nullptr; }
bool ShellApi::modulesLoading() const      { return holds("settings") && m_backend->property("modulesLoading").toBool(); }
QString ShellApi::buildVersion() const     { return m_backend ? m_backend->property("buildVersion").toString() : QString(); }
bool ShellApi::isPortableBuild() const     { return m_backend && m_backend->property("isPortableBuild").toBool(); }
QVariantList ShellApi::buildCommits() const { return m_backend ? m_backend->property("buildCommits").toList() : QVariantList(); }
QVariantList ShellApi::notificationApps() const
{
    return holds("settings") ? withServedIcons(m_backend->notificationApps()) : QVariantList();
}
QVariantList ShellApi::searchApps() const
{
    return holds("settings") ? withServedIcons(m_backend->searchApps()) : QVariantList();
}
QObject* ShellApi::prefs() const           { return holds("settings") ? m_backend->shellPreferences() : nullptr; }
QString ShellApi::pendingSection() const   { return holds("settings") ? m_backend->pendingSettingsSection() : QString(); }

bool ShellApi::desktopNotifications() const
{
    QObject* center = holds("settings") ? m_backend->notifications() : nullptr;
    return center && center->property("desktop").toBool();
}

void ShellApi::setDesktopNotifications(bool on)
{
    if (QObject* center = holds("settings") ? m_backend->notifications() : nullptr)
        center->setProperty("desktop", on);
}

void ShellApi::refreshRepositories()               { if (holds("settings") || holds("launcher")) m_backend->refreshRepositories(); }
void ShellApi::addRepository(const QString& url)    { if (holds("settings")) m_backend->addRepository(url); }
void ShellApi::removeRepository(const QString& url) { if (holds("settings")) m_backend->removeRepository(url); }
void ShellApi::setRepositoryEnabled(const QString& url, bool enabled) { if (holds("settings")) m_backend->setRepositoryEnabled(url, enabled); }
void ShellApi::refreshDownloadSource()              { if (holds("settings")) m_backend->refreshDownloadSource(); }
void ShellApi::setDownloadSource(const QString& s)  { if (holds("settings")) m_backend->setDownloadSource(s); }
void ShellApi::refreshUiModules()                   { if (holds("settings")) m_backend->refreshUiModules(); }
void ShellApi::loadUiModule(const QString& name)    { if (holds("settings")) m_backend->loadUiModule(name); }
void ShellApi::unloadUiModule(const QString& name)  { if (holds("settings")) m_backend->unloadUiModule(name); }
void ShellApi::refreshCoreModules()                 { if (holds("settings")) m_backend->refreshCoreModules(); }
void ShellApi::loadCoreModule(const QString& name)  { if (holds("settings")) m_backend->loadCoreModule(name); }
void ShellApi::unloadCoreModule(const QString& name){ if (holds("settings")) m_backend->unloadCoreModule(name); }

QString ShellApi::getCoreModuleMethods(const QString& name)
{
    return holds("settings") ? m_backend->getCoreModuleMethods(name) : QString();
}

QString ShellApi::getCoreModuleEvents(const QString& name)
{
    return holds("settings") ? m_backend->getCoreModuleEvents(name) : QString();
}

QString ShellApi::callCoreModuleMethod(const QString& name, const QString& method,
                                       const QString& argsJson)
{
    return holds("settings") ? m_backend->callCoreModuleMethod(name, method, argsJson) : QString();
}

void ShellApi::setNotificationsEnabled(const QString& app, bool enabled)
{
    if (holds("settings")) m_backend->setNotificationsEnabled(app, enabled);
}

void ShellApi::setSearchEnabled(const QString& app, bool enabled)
{
    if (holds("settings")) m_backend->setSearchEnabled(app, enabled);
}

QVariantList ShellApi::roleCandidates(const QString& role) const
{
    return holds("settings") ? withServedIcons(m_backend->roleCandidates(role)) : QVariantList();
}

bool ShellApi::assignRole(const QString& role, const QString& app)
{
    return holds("settings") && m_backend->assignRole(role, app);
}

void ShellApi::resetRole(const QString& role)
{
    if (holds("settings")) m_backend->resetRole(role);
}

QString ShellApi::takePendingSection()
{
    return holds("settings") ? m_backend->takePendingSettingsSection() : QString();
}

// ── settings and launcher ────────────────────────────────────────────────────

QVariantList ShellApi::repositories() const
{
    return (holds("settings") || holds("launcher")) ? m_backend->property("repositories").toList()
                                                    : QVariantList();
}

// ── launcher ─────────────────────────────────────────────────────────────────

QObject* ShellApi::appsModel() const     { return holds("launcher") ? m_backend->property("appsModel").value<QObject*>() : nullptr; }
bool ShellApi::appsLoading() const       { return holds("launcher") && m_backend->appsLoading(); }
QString ShellApi::pendingSearch() const  { return holds("launcher") ? m_backend->pendingLauncherSearch() : QString(); }

void ShellApi::requestCatalog()    { if (holds("launcher")) m_backend->requestCatalog(); }
void ShellApi::refreshAppCatalog() { if (holds("launcher")) m_backend->refreshAppCatalog(); }

void ShellApi::openApp(const QString& name, const QString& repositoryUrl, bool fast)
{
    if (holds("launcher")) m_backend->openApp(name, repositoryUrl, QVariantMap(), fast);
}

void ShellApi::uninstallApp(const QString& name, const QString& repositoryUrl)
{
    if (holds("launcher")) m_backend->uninstallApp(name, repositoryUrl);
}

void ShellApi::showRepositories()
{
    if (holds("launcher") || holds("settings"))
        m_backend->requestSettingsSection(QStringLiteral("repositories"));
}

QString ShellApi::takePendingSearch()
{
    return holds("launcher") ? m_backend->takePendingLauncherSearch() : QString();
}

// ── home and launcher ────────────────────────────────────────────────────────

QVariantList ShellApi::installedApps() const
{
    return (holds("home") || holds("launcher")) ? withServedIcons(m_backend->launcherApps())
                                                : QVariantList();
}

QStringList ShellApi::loadingModules() const
{
    return (holds("home") || holds("launcher")) ? m_backend->loadingModules() : QStringList();
}

// ── home ─────────────────────────────────────────────────────────────────────

bool ShellApi::searchInBar() const { return holds("home") && m_backend->homeSearchInBar(); }
QString ShellApi::homeQuery() const { return holds("home") ? m_backend->homeQuery() : QString(); }

void ShellApi::launch(const QString& name)
{
    if (holds("home")) emit m_backend->homeLaunchRequested(name);
}

// Navigation any app could already ask for (basecamp.apps.launch). From home,
// the shell also turns the New tab into whatever opens.
bool ShellApi::openRole(const QString& role)
{
    if (!holdsAny()) return false;
    if (holds("home")) {
        emit m_backend->homeRoleRequested(role);
        return true;
    }
    return m_backend->openRole(role);
}
