#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

class MainUIBackend;
class QQuickAsyncImageProvider;

// ── ShellApi ─────────────────────────────────────────────────────────────────
//
// What an app filling one of the shell's roles may do (SHELL-ROLES-SPEC §4,
// ADR-067). Injected as the context property `shell` into the QML engine of
// every app that claims or holds a role, and of no other app.
//
// Each member is gated by the role it belongs to, re-checked on every read and
// call: an app that only claims a role gets an inert object, and one that is
// given the role later (Settings → Default apps) starts working without a
// reload. Giving the role away makes it inert again at once.
//
// Deliberately a narrow, named surface over MainUIBackend rather than the
// backend itself, which can install and uninstall without asking. Everything
// here is something Basecamp's own Settings, Applications or New tab already
// did; install and uninstall still go through the shell's confirmation dialogs.
class ShellApi : public QObject {
    Q_OBJECT

    // Every role's current holder (readable by any role app).
    Q_PROPERTY(QVariantMap roles READ roles NOTIFY rolesChanged)

    // ── settings ─────────────────────────────────────────────────────────
    Q_PROPERTY(bool repositoriesLoading READ repositoriesLoading NOTIFY repositoriesChanged)
    Q_PROPERTY(QString downloadSource READ downloadSource NOTIFY repositoriesChanged)
    Q_PROPERTY(bool repositoryManagement READ repositoryManagement CONSTANT)
    Q_PROPERTY(QObject* uiModulesModel READ uiModulesModel NOTIFY rolesChanged)
    Q_PROPERTY(QObject* coreModulesModel READ coreModulesModel NOTIFY rolesChanged)
    Q_PROPERTY(bool modulesLoading READ modulesLoading NOTIFY modulesChanged)
    Q_PROPERTY(QString buildVersion READ buildVersion CONSTANT)
    Q_PROPERTY(bool isPortableBuild READ isPortableBuild CONSTANT)
    Q_PROPERTY(QVariantList buildCommits READ buildCommits CONSTANT)
    Q_PROPERTY(QVariantList notificationApps READ notificationApps NOTIFY notificationsChanged)
    Q_PROPERTY(bool desktopNotifications READ desktopNotifications WRITE setDesktopNotifications NOTIFY notificationsChanged)
    // The shell's own preferences (layout, shell, wallpaper, restart, file
    // picker), handed over by the shell. null under a shell that has none.
    Q_PROPERTY(QObject* prefs READ prefs NOTIFY prefsChanged)
    // A section someone asked Settings to show ("repositories").
    Q_PROPERTY(QString pendingSection READ pendingSection NOTIFY sectionRequested)

    // ── settings and launcher ────────────────────────────────────────────
    Q_PROPERTY(QVariantList repositories READ repositories NOTIFY repositoriesChanged)

    // ── launcher ─────────────────────────────────────────────────────────
    // The catalog (installed and installable apps): a live Qt model.
    Q_PROPERTY(QObject* appsModel READ appsModel NOTIFY rolesChanged)
    Q_PROPERTY(bool appsLoading READ appsLoading NOTIFY appsChanged)
    // A search handed over from elsewhere (the shell's search, a welcome page).
    Q_PROPERTY(QString pendingSearch READ pendingSearch NOTIFY launcherSearchRequested)

    // ── home and launcher ────────────────────────────────────────────────
    // Installed apps: [{ name, displayName, icon, … }], icons served by the host.
    Q_PROPERTY(QVariantList installedApps READ installedApps NOTIFY appsChanged)
    Q_PROPERTY(QStringList loadingModules READ loadingModules NOTIFY appsChanged)

    // ── home ─────────────────────────────────────────────────────────────
    // What is typed in the shell's bar while home is shown, when the bar is the
    // search (Basecampium's phone layout); home then hides its own field.
    Q_PROPERTY(bool searchInBar READ searchInBar NOTIFY homeQueryChanged)
    Q_PROPERTY(QString homeQuery READ homeQuery NOTIFY homeQueryChanged)

public:
    ShellApi(MainUIBackend* backend, const QString& appName, QObject* parent = nullptr);

    // `image://basecamp/…` for the same app: installed apps' icons
    // (appicon/<name>) and the wallpaper previews (wallpaper/builtin|custom).
    // The sandbox keeps an app from reading files outside its own folder, so
    // the host reads these few on its behalf. Owned by the engine it is added to.
    static QQuickAsyncImageProvider* createImageProvider(MainUIBackend* backend, const QString& appName);

    QVariantMap roles() const;

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
    QObject* prefs() const;
    QString pendingSection() const;

    QObject* appsModel() const;
    bool appsLoading() const;
    QString pendingSearch() const;

    QVariantList installedApps() const;
    QStringList loadingModules() const;

    bool searchInBar() const;
    QString homeQuery() const;

    // settings
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
    Q_INVOKABLE QString takePendingSection();

    // launcher
    Q_INVOKABLE void requestCatalog();
    Q_INVOKABLE void refreshAppCatalog();
    // Open an installed app, or start installing one: the shell's own install
    // dialog asks the user first unless `fast` and the app is already installed.
    Q_INVOKABLE void openApp(const QString& name, const QString& repositoryUrl, bool fast);
    // Asks the user in the shell's dialog; nothing is removed without it.
    Q_INVOKABLE void uninstallApp(const QString& name, const QString& repositoryUrl);
    Q_INVOKABLE void showRepositories();
    Q_INVOKABLE QString takePendingSearch();

    // home: turn the New tab this page is shown in into `name`.
    Q_INVOKABLE void launch(const QString& name);

    // any role app: open whatever fills another role (navigation only).
    Q_INVOKABLE bool openRole(const QString& role);

signals:
    void rolesChanged();
    void repositoriesChanged();
    void repositoryOperationCompleted(const QString& operation, const QString& url,
                                      bool success, const QString& error);
    void modulesChanged();
    void notificationsChanged();
    void prefsChanged();
    void sectionRequested(const QString& section);
    void appsChanged();
    void launcherSearchRequested(const QString& query);
    void homeQueryChanged();
    // Enter in the shell's bar while home is shown: open the first result.
    void homeQueryAccepted();

private:
    bool holds(const char* role) const;
    bool holdsAny() const;

    QPointer<MainUIBackend> m_backend;
    QString m_app;
};
