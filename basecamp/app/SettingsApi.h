#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

class MainUIBackend;
class QQuickImageProvider;

// ── SettingsApi ──────────────────────────────────────────────────────────────
//
// What the app filling the `settings` role may do (SHELL-ROLES-SPEC §4,
// ADR-067). Injected as the context property `shell` into that app's QML
// engine, and into no other.
//
// The grant is re-checked on every read and call, not only at injection: if
// the user gives the role to another app, this one's object goes inert (empty
// reads, calls ignored) without needing a reload.
//
// Deliberately a narrow, named surface over MainUIBackend rather than the
// backend itself, which can install and uninstall without asking. Everything
// here is something Basecamp's own Settings screen already did.
class SettingsApi : public QObject {
    Q_OBJECT

    // Package repositories
    Q_PROPERTY(QVariantList repositories READ repositories NOTIFY repositoriesChanged)
    Q_PROPERTY(bool repositoriesLoading READ repositoriesLoading NOTIFY repositoriesChanged)
    Q_PROPERTY(QString downloadSource READ downloadSource NOTIFY repositoriesChanged)
    Q_PROPERTY(bool repositoryManagement READ repositoryManagement CONSTANT)

    // Inspectors (developer views): live Qt models, owned by the backend.
    Q_PROPERTY(QObject* uiModulesModel READ uiModulesModel CONSTANT)
    Q_PROPERTY(QObject* coreModulesModel READ coreModulesModel CONSTANT)
    Q_PROPERTY(bool modulesLoading READ modulesLoading NOTIFY modulesChanged)

    // About
    Q_PROPERTY(QString buildVersion READ buildVersion CONSTANT)
    Q_PROPERTY(bool isPortableBuild READ isPortableBuild CONSTANT)
    Q_PROPERTY(QVariantList buildCommits READ buildCommits CONSTANT)

    // Notifications
    Q_PROPERTY(QVariantList notificationApps READ notificationApps NOTIFY notificationsChanged)
    Q_PROPERTY(bool desktopNotifications READ desktopNotifications WRITE setDesktopNotifications NOTIFY notificationsChanged)

    // Roles (Default apps)
    Q_PROPERTY(QVariantMap roles READ roles NOTIFY rolesChanged)

    // The shell's own preferences (layout, shell, wallpaper, restart, file
    // picker), handed over by the shell. null under a shell that has none.
    Q_PROPERTY(QObject* prefs READ prefs NOTIFY prefsChanged)

    // A section someone asked Settings to show ("repositories"), consumed by
    // takePendingSection(); also announced through sectionRequested().
    Q_PROPERTY(QString pendingSection READ pendingSection NOTIFY sectionRequested)

public:
    SettingsApi(MainUIBackend* backend, const QString& appName, QObject* parent = nullptr);

    // `image://basecamp/…` for the same app: other apps' icons
    // (appicon/<name>) and the wallpaper previews (wallpaper/builtin|custom).
    // The sandbox keeps an app from reading files outside its own folder, so
    // the host reads these few on its behalf. Owned by the engine it is added to.
    static QQuickImageProvider* createImageProvider(MainUIBackend* backend, const QString& appName);

    QVariantList repositories() const;
    bool repositoriesLoading() const;
    QString downloadSource() const;
    bool repositoryManagement() const;
    QObject* uiModulesModel() const;
    QObject* coreModulesModel() const;
    bool modulesLoading() const;
    QString buildVersion() const;
    bool isPortableBuild() const;
    QVariantList buildCommits() const;
    QVariantList notificationApps() const;
    bool desktopNotifications() const;
    void setDesktopNotifications(bool on);
    QVariantMap roles() const;
    QObject* prefs() const;
    QString pendingSection() const;

    Q_INVOKABLE void refreshRepositories();
    Q_INVOKABLE void addRepository(const QString& url);
    Q_INVOKABLE void removeRepository(const QString& url);
    Q_INVOKABLE void setRepositoryEnabled(const QString& url, bool enabled);
    Q_INVOKABLE void refreshDownloadSource();
    Q_INVOKABLE void setDownloadSource(const QString& source);

    Q_INVOKABLE void refreshUiModules();
    Q_INVOKABLE void loadUiModule(const QString& name);
    Q_INVOKABLE void unloadUiModule(const QString& name);
    Q_INVOKABLE void refreshCoreModules();
    Q_INVOKABLE void loadCoreModule(const QString& name);
    Q_INVOKABLE void unloadCoreModule(const QString& name);
    Q_INVOKABLE QString getCoreModuleMethods(const QString& name);
    Q_INVOKABLE QString getCoreModuleEvents(const QString& name);
    Q_INVOKABLE QString callCoreModuleMethod(const QString& name, const QString& method,
                                             const QString& argsJson);

    Q_INVOKABLE void setNotificationsEnabled(const QString& app, bool enabled);

    Q_INVOKABLE QVariantList roleCandidates(const QString& role) const;
    Q_INVOKABLE bool assignRole(const QString& role, const QString& app);
    Q_INVOKABLE void resetRole(const QString& role);
    Q_INVOKABLE bool openRole(const QString& role);

    Q_INVOKABLE QString takePendingSection();

signals:
    void repositoriesChanged();
    void repositoryOperationCompleted(const QString& operation, const QString& url,
                                      bool success, const QString& error);
    void modulesChanged();
    void notificationsChanged();
    void rolesChanged();
    void prefsChanged();
    void sectionRequested(const QString& section);

private:
    bool allowed() const;
    QVariant read(const char* property) const;
    void call(const char* method);
    void call(const char* method, const QString& a);

    QPointer<MainUIBackend> m_backend;
    QString m_app;
};
