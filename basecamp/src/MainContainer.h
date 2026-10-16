#pragma once

#include "IShellHost.h"

#include <QWidget>
#include <QHBoxLayout>
#include <QPointer>
#include <QStackedWidget>

class QQuickWidget;
class WorkspaceArea;
class DesktopArea;
class AppWindow;
class ShellPrefs;
class DesktopStatusBar;
class DesktopRoot;
class ShortcutBridge;

// MainContainer — the UI shell. Holds exactly one host-side pointer, an
// IShellHost*: no LogosAPI*, no QtLogosCore*, no MainUIBackend*. QML reaches
// the backend as an opaque QObject* via IShellHost::backendObject(). That is
// what lets this class, and everything below it, compile against Qt alone.
class MainContainer : public QWidget, public IShellObserver
{
    Q_OBJECT

public:
    // `host` is borrowed and outlives this widget. Must not be null.
    explicit MainContainer(IShellHost* host, QWidget* parent = nullptr);
    ~MainContainer();

    WorkspaceArea* getWorkspaceArea() const { return m_workspaceArea; }

    // Stops the host from calling back into this object. Idempotent:
    // MainShellView::destroyShell() calls it before deleting, and the
    // destructor calls it again for the path where Qt's parent-child teardown
    // gets here first.
    void detachFromHost();

    // ── IShellObserver ──────────────────────────────────────────────────────
    void onSectionIndexChanged(int index) override;
    void onNavigateToApps() override;
    void onPluginWindowRequested(QWidget* widget, const QString& title) override;
    void onPluginWindowRemoveRequested(QWidget* widget) override;
    // Presentation seam. Branches on the widget pointer because this class
    // decided where each widget was mounted.
    void onPresentAppRequested(QWidget* widget) override;

protected:
    // Keeps the overlay sized to the full MainContainer: it floats over both
    // the sidebar and the content stack, so it can't sit in the HBoxLayout.
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    // Called from QML when the combined visibility of the three overlay
    // dialogs flips. Toggles mouse-event passthrough on the overlay
    // QQuickWidget: transparent when no dialog is open, so the sidebar and
    // content still receive clicks, intercepting when one is.
    void onOverlayActiveChanged(bool active);
    void onSidebarTooltipRequested(const QString& text, qreal y);
    void onAddApplicationDialogRequested(const QVariantMap& metadata);
    // Desktop layout: the dock's two signals.
    void openSystemApp(const QString& key);
    void launchFromDock(const QString& name);
    void openSettingsRepositories();
    void showSpotlight();
    void toggleSpotlight();
    void hideSpotlight();
    void spotlightReopen(const QString& name);
    void spotlightOpenApp(const QString& name, const QString& repositoryUrl);
    void spotlightPackage(const QString& name);
    void spotlightInstall(const QString& name);
    void spotlightShowAll(const QString& typeValue, const QString& query);

private:
    void wireSidebar(QObject* sidebarRoot);
    bool offersSection(int section) const;
    void applyAppManagerSearch(const QString& query);
    void invokeOpenApp(const QString& name, const QString& repositoryUrl);
    void setupUi();

    // ── Desktop layout (ShellPrefs::activeLayout() == "desktop") ──
    void setupDesktopUi();
    void updateDock();
    void positionDock();
    void onDesktopCloseRequested(const QString& key);
    QWidget* systemAppContent(const QString& key);
    QVariantMap launcherEntry(const QString& name) const;
    void applyGlass(QWidget* widget);

    ShellPrefs* m_prefs = nullptr;
    bool m_desktopLayout = false;
    DesktopArea* m_desktop = nullptr;
    QQuickWidget* m_dockWidget = nullptr;
    QQuickWidget* m_settingsWidget = nullptr;
    QQuickWidget* m_spotlight = nullptr;
    DesktopStatusBar* m_statusBar = nullptr;
    DesktopRoot* m_desktopRoot = nullptr;
    bool m_packagesPending = false;
    bool m_wentFullScreen = false;

    QHBoxLayout* m_mainLayout;

    QQuickWidget* m_sidebarWidget;

    QStackedWidget* m_contentStack;

    // Workspace (QDockWidget-based, replaces the old QMdiArea workspace)
    WorkspaceArea* m_workspaceArea;

    // QPointer, not a raw pointer: this widget is owned by the HOST side, which
    // deleteLater()s it on unload (UIPluginManager::unloadUiModuleImpl and
    // ::teardownUiPluginWidget both do). As a raw pointer it was read at four
    // sites after the widget had been freed, and `!m_pmuiWidget` stayed false
    // forever, so the Package Manager section could never reload. Same guard the
    // host applies to its own widget maps.
    QPointer<QWidget> m_pmuiWidget;
    bool m_suppressNextNavToApps = false;

    // Content views (QML for Dashboard, Modules, PackageManager, Settings)
    QQuickWidget* m_contentWidget;

    // Full-window overlay for the dependency-aware dialogs
    // (missing-deps popup, cascade-unload/uninstall confirmations).
    // Lives outside the content stack so it's visible regardless of
    // which stack page is current — previously the dialogs were
    // anchored inside m_contentWidget and never rendered while the
    // Apps/MDI screen was active.
    QQuickWidget* m_overlayWidget;

    // Not owned — Window owns it and it outlives this widget.
    IShellHost* m_host;

    ShortcutBridge* m_shortcutBridge = nullptr;
};

