#pragma once

#include <QObject>
#include <QPointer>
#include <QTimer>
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
    // Settings → Search: apps that publish objects, { name, displayName, icon,
    // enabled, count } (SHELL-OBJECTS-SPEC §8).
    Q_PROPERTY(QVariantList searchApps READ searchApps NOTIFY searchChanged)
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
    // Home is shown as the desktop's Activity sidebar: only the search, the
    // pills and the cards (basecamp-experiments#44).
    Q_PROPERTY(bool feedOnly READ feedOnly NOTIFY feedOnlyChanged)
    // Recent apps: recently closed and recently installed, newest first, in
    // installedApps' row shape plus recentKind ("closed" | "installed").
    // Empty on a fresh profile; home then shows the role apps.
    Q_PROPERTY(QVariantList recentApps READ recentApps NOTIFY recentAppsChanged)
    // What home saved of its own with saveHomeSettings (its widgets and their
    // layout, basecamp-experiments#51); {} when nothing. This app's only:
    // another home app starts from nothing.
    Q_PROPERTY(QVariantMap homeSettings READ homeSettings NOTIFY homeSettingsChanged)

    // ── keychain (basecamp-experiments#55, #56) ──────────────────────────
    // Every registered key, the user's label and note included, grouped by
    // owner (Keychain::allKeys rows plus ownerIcon and ownerFullBleed, served
    // by the host).
    Q_PROPERTY(QVariantList keychainKeys READ keychainKeys NOTIFY keychainChanged)
    // Open signing requests' ids, oldest first.
    Q_PROPERTY(QStringList keychainRequests READ keychainRequests NOTIFY keychainRequestsChanged)

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
    QVariantList searchApps() const;
    QObject* prefs() const;
    QString pendingSection() const;

    QObject* appsModel() const;
    bool appsLoading() const;
    QString pendingSearch() const;

    QVariantList installedApps() const;
    QStringList loadingModules() const;

    bool searchInBar() const;
    QString homeQuery() const;
    bool feedOnly() const;
    QVariantList recentApps() const;
    QVariantMap homeSettings() const;
    QVariantList keychainKeys() const;
    QStringList keychainRequests() const;

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
    Q_INVOKABLE void setSearchEnabled(const QString& app, bool enabled);
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
    // home: the apps matching `query` for its Apps block (basecamp-experiments#32),
    // installed and catalog ones, best first (AppSearch.h): [{ name, title,
    // icon, fullBleed, state }], state "running" | "installed" | "installing" |
    // "available". icon is served by the host and only for a local file, ""
    // otherwise (draw letters). The first query fetches the catalog; asked
    // again on appResultsChanged.
    Q_INVOKABLE QVariantList findApps(const QString& query);
    // home: the installed apps that hold objects of `type` (a type pill,
    // basecamp-experiments#36), most first, in findApps' row shape; apps
    // switched off in Settings -> Search are left out. Read again on
    // relatedAppsChanged and on appResultsChanged.
    Q_INVOKABLE QVariantList relatedApps(const QString& type);
    // home: install an app found by findApps, through the shell's own install
    // dialog (nothing is installed without it). The host picks the repository.
    // An installed app opens instead.
    Q_INVOKABLE void installApp(const QString& name);
    // home: what the shell's bar reads while it searches this page, naming
    // the active filter ("Search transactions"); "" gives the shell's own back.
    // Plain text, one line, at most 60 characters.
    Q_INVOKABLE void setBarPlaceholder(const QString& text);
    // home: keeps `settings` (a JSON object, at most 64 KB) across restarts,
    // in place of what was saved. False when refused. Every view of the same
    // app sees homeSettingsChanged.
    Q_INVOKABLE bool saveHomeSettings(const QVariantMap& settings);

    // ── search (SHELL-OBJECTS-SPEC §5) ───────────────────────────────────
    // The envelopes `filter` allows ({ types?, apps?, kinds?, subjects?,
    // unseenOnly? }), newest first.
    Q_INVOKABLE QVariantList objects(const QVariantMap& filter = QVariantMap()) const;
    // What changed since `version` (0: everything), for a holder that keeps
    // its own copy: { version, reset, objects, removed, ids? } (ObjectSearch.h).
    Q_INVOKABLE QVariantMap objectsSince(double version, const QVariantMap& filter = QVariantMap()) const;
    // Ranked ids for a searchRequested: at most 200; unknown ids, ids outside
    // the request's filter and repeats are dropped. false for a stale request.
    Q_INVOKABLE bool answer(const QString& requestId, const QVariantList& ids);

    // ── home and launcher: views over objects ────────────────────────────
    // Live results (ObjectResults) for `query` and `filter`, owned by the
    // caller's JS. Keep one and call its update(query, filter). null when this
    // app is not a view.
    Q_INVOKABLE QObject* search(const QString& query, const QVariantMap& filter = QVariantMap());
    // `open` or one of the object's actions, `text` for an `input: "text"` one.
    // Only objects in this app's results. Returns the request id, "" if refused.
    Q_INVOKABLE QString raise(const QString& id, const QString& action, const QString& text = QString());
    // Only objects in this app's results; returns the ids marked.
    Q_INVOKABLE QStringList markSeen(const QVariantList& ids);
    // How many objects match `query`, uncapped (a results model stops at
    // 200): { total, types: [{ type, count }], apps: [{ app, count }] }, for
    // pills. Read when asked, never pushed.
    Q_INVOKABLE QVariantMap counts(const QString& query) const;

    // ── keychain ─────────────────────────────────────────────────────────
    // The user's own words for a key, never shown to any other app. "" clears;
    // false for an unknown key or over 40 (label) / 500 (note) characters.
    Q_INVOKABLE bool setKeyLabel(const QString& ownerApp, const QString& keyId, const QString& label);
    Q_INVOKABLE bool setKeyNote(const QString& ownerApp, const QString& keyId, const QString& note);
    // The entry leaves the directory; the signer app keeps its private key.
    Q_INVOKABLE bool removeKey(const QString& ownerApp, const QString& keyId);
    // One open request as the Keychain shows it (KeychainBroker::
    // requestForKeychain): who asks, their words, the kind, the keys that can
    // sign it and the one this requester used last. {} when unknown.
    Q_INVOKABLE QVariantMap keychainRequest(const QString& requestId) const;
    // Continue with this key (it goes to its signer app), or Cancel.
    Q_INVOKABLE bool chooseKey(const QString& requestId, const QString& ownerApp, const QString& keyId);
    Q_INVOKABLE bool cancelSignatureRequest(const QString& requestId);
    // Bring forward the signer app an open request is waiting for.
    Q_INVOKABLE bool openSigner(const QString& requestId);

    // any role app: open whatever fills another role (navigation only).
    Q_INVOKABLE bool openRole(const QString& role);

signals:
    void rolesChanged();
    void repositoriesChanged();
    void repositoryOperationCompleted(const QString& operation, const QString& url,
                                      bool success, const QString& error);
    void modulesChanged();
    void notificationsChanged();
    void searchChanged();
    void prefsChanged();
    void sectionRequested(const QString& section);
    void appsChanged();
    void launcherSearchRequested(const QString& query);
    void homeQueryChanged();
    void feedOnlyChanged();
    void recentAppsChanged();
    void homeSettingsChanged();
    // home: findApps would answer differently (an app opened, closed,
    // installed or removed, the catalog arrived). Coalesced.
    void appResultsChanged();
    // home: relatedApps would answer differently (some app published,
    // retracted or was switched off). Coalesced.
    void relatedAppsChanged();
    // Enter in the shell's bar while home is shown: open the first result.
    void homeQueryAccepted();
    // search: a view wants results; answer(requestId, ids) within 500 ms or
    // the host ranks instead.
    void searchRequested(const QString& requestId, const QString& query, const QVariantMap& filter);
    // search: what `app` published changed (the host asks again by itself).
    void objectsChanged(const QString& app);
    // keychain: a key was registered, changed or removed.
    void keychainChanged();
    void keychainRequestsChanged();
    // keychain: a signing request is waiting for the user.
    void keychainRequestOpened(const QString& requestId);

private:
    bool holds(const char* role) const;
    bool holdsAny() const;
    bool isView() const { return holds("home") || holds("launcher"); }

    void scheduleAppResults();

    QPointer<MainUIBackend> m_backend;
    QString m_app;
    QTimer* m_appResultsTimer = nullptr;
    QTimer* m_relatedTimer = nullptr;
};
