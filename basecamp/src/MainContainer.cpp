#include "MainContainer.h"
#include "ShellSections.h"
#include "ShellDevQml.h"
#include "AppsFilterProxy.h"
#include "InstallEnums.h"
#include "ShortcutBridge.h"
#include "WorkspaceArea.h"
#include "DesktopArea.h"
#include "ShellPrefs.h"
#include "TabArea.h"

#include <QQuickWidget>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickStyle>
#include <qqml.h>
#include <QVBoxLayout>
#include <QLabel>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QQuickItem>
#include <QVariantMap>
#include <QColor>
#include <QPalette>
#include <QEvent>
#include <QTimer>
#include <QShortcut>
#include <QShowEvent>
#include <QKeyEvent>
#include <QApplication>
#include <QUrl>
#include <QCursor>
#include <QSettings>
#include <QWindow>
#include <QMouseEvent>

#include <ThemeColors.h>

namespace {
constexpr int kAppsStackIndex     = 0;  // WorkspaceArea (QDockWidget-based)
constexpr int kContentStackIndex  = 1;  // ContentViews.qml (App Manager + Settings)
constexpr int kModulesStackIndex  = 2;  // package_manager_ui (sandboxed QQuickWidget)

// The Package Manager page before package_manager_ui arrives -- and again after
// it goes away. Built in two places, so it lives here: the stack must keep all
// three pages at all times, because every section switch below indexes into it
// by constant.
QWidget* makePmuiPlaceholder(QWidget* parent)
{
    QWidget* ph = new QWidget(parent);
    ph->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QVBoxLayout* phLayout = new QVBoxLayout(ph);
    phLayout->setAlignment(Qt::AlignCenter);
    QLabel* loadingLabel = new QLabel(QStringLiteral("Loading Package Manager…"), ph);
    loadingLabel->setAlignment(Qt::AlignCenter);
    loadingLabel->setStyleSheet(QStringLiteral("color: #a0a0a0; font-size: 14px;"));
    phLayout->addWidget(loadingLabel);
    return ph;
}

} // namespace

MainContainer::MainContainer(IShellHost* host, QWidget* parent)
    : QWidget(parent)
    , m_host(host)
    , m_sidebarWidget(nullptr)
    , m_contentStack(nullptr)
    , m_workspaceArea(nullptr)
    , m_contentWidget(nullptr)
    , m_overlayWidget(nullptr)
{
    // Not a degraded mode — every host operation below goes through this
    // pointer, so a null one would surface as a null dereference in a click
    // handler rather than here.
    if (!m_host) {
        qFatal("MainContainer requires an IShellHost");
    }

    // Set QML style
    QQuickStyle::setStyle("Basic");

    m_prefs = new ShellPrefs(this);
    m_desktopLayout = m_prefs->desktopLayout();
#ifdef BASECAMPIUM
    // Basecampium is this shell built with a tab frame instead of the
    // sidebar / desktop: always tabs, whatever the saved layout says.
    m_tabsLayout = true;
    m_desktopLayout = false;
#endif

    setupUi();

    // Provider lets the bridge scan the front-most dock's shortcuts
    // when Workspace is the current section.
    m_shortcutBridge = new ShortcutBridge(this, m_contentStack, [this]() -> QQuickWidget* {
        if (m_tabArea) {
            QWidget* w = m_tabArea->contentFor(m_tabArea->currentKey());
            if (auto* qw = qobject_cast<QQuickWidget*>(w)) return qw;
            return w ? w->findChild<QQuickWidget*>() : nullptr;
        }
        if (!m_workspaceArea) return nullptr;
        if (QQuickWidget* dock = m_workspaceArea->activeDockWidget()) return dock;
        return m_workspaceArea->welcomePageWidget();
    });
    // Tab-switching inside the workspace changes the dock without
    // touching m_contentStack — force a rebind so the new dock's
    // shortcuts become live.
    if (m_workspaceArea) {
        connect(m_workspaceArea, &WorkspaceArea::activeAppChanged,
                m_shortcutBridge,
                [this](const QString&) { m_shortcutBridge->rebindDeferred(); });
    }

    // Subscribe to the host. These five arrive as IShellObserver virtual calls
    // rather than signals: a signal/slot connection would make both sides agree
    // on a metaobject, which is the coupling this boundary exists to avoid.
    // Detached again in detachFromHost().
    m_host->setObserver(this);

    // When user closes a plugin tab (× button), notify backend to unload.
    connect(m_workspaceArea, &WorkspaceArea::pluginClosed,
            this, [this](const QString& moduleName) {
        m_host->unloadUiModule(moduleName);
    });

    // Keep the sidebar's active-app highlight in sync with the front-most
    // dock — QDockWidget tab clicks bypass onAppLauncherClicked, so without
    // this the sidebar stays stuck on whichever app was last launched.
    connect(m_workspaceArea, &WorkspaceArea::activeAppChanged,
            this, [this](const QString& moduleName) {
        m_host->setCurrentVisibleApp(moduleName);
    });

    // WelcomePage's two navigation blocks → the views that own those flows.
    connect(m_workspaceArea, &WorkspaceArea::discoverApplicationsClicked, this, [this]() {
        m_host->setCurrentSectionIndex(ShellSection::AppManager);
    });
    connect(m_workspaceArea, &WorkspaceArea::managePackagesClicked, this, [this]() {
        m_host->setCurrentSectionIndex(ShellSection::PackageManager);
    });
    connect(m_workspaceArea, SIGNAL(reopenAppRequested(QString)),
            m_host->backendObject(), SLOT(onAppLauncherClicked(QString)),
            Qt::QueuedConnection);

    connect(m_workspaceArea, &WorkspaceArea::appActivated, this,
            [this](const QString& name, const QString& repositoryUrl) {
        invokeOpenApp(name, repositoryUrl);
    });
    connect(m_workspaceArea, &WorkspaceArea::packageActivated, this,
            [this](const QString& name) {
        QMetaObject::invokeMethod(m_host->backendObject(),
                                  "showPackageDetails",
                                  Q_ARG(QString, name));
    });
    connect(m_workspaceArea, &WorkspaceArea::packageInstallRequested, this,
            [this](const QString& name) {
        QMetaObject::invokeMethod(m_host->backendObject(),
                                  "requestPackageInstall",
                                  Q_ARG(QString, name));
    });
    connect(m_host->backendObject(),
            SIGNAL(requestOpenAddApplicationDialog(QVariantMap)),
            this, SLOT(onAddApplicationDialogRequested(QVariantMap)));
    connect(m_workspaceArea, &WorkspaceArea::showAllResultsRequested, this,
            [this](const QString& typeValue, const QString& query) {
        m_host->setCurrentSectionIndex(typeValue == QStringLiteral("core")
                                           ? ShellSection::PackageManager
                                           : ShellSection::AppManager);
        if (typeValue != QStringLiteral("core"))
            applyAppManagerSearch(query);
    });

    qDebug() << "MainContainer created";
}

// Connects to SidebarPanel's signals; runs again after each shell hot reload.
void MainContainer::wireSidebar(QObject* sidebarRoot)
{
    connect(sidebarRoot, SIGNAL(launchUIModule(QString)),
            m_host->backendObject(), SLOT(onAppLauncherClicked(QString)),
            Qt::QueuedConnection);
    connect(sidebarRoot, SIGNAL(updateLauncherIndex(int)),
            m_host->backendObject(), SLOT(setCurrentActiveSectionIndex(int)));
    connect(sidebarRoot, SIGNAL(tooltipRequested(QString, qreal)),
            this, SLOT(onSidebarTooltipRequested(QString, qreal)));
}

void MainContainer::onAddApplicationDialogRequested(const QVariantMap& metadata)
{
    if (m_host->currentSectionIndex() == ShellSection::AppManager) return;

    m_host->setCurrentSectionIndex(ShellSection::AppManager);
    applyAppManagerSearch(metadata.value(QStringLiteral("name")).toString());
}

void MainContainer::invokeOpenApp(const QString& name,
                                  const QString& repositoryUrl)
{
    if (!QMetaObject::invokeMethod(m_host->backendObject(), "openApp",
                                   Q_ARG(QString, name),
                                   Q_ARG(QString, repositoryUrl),
                                   Q_ARG(QVariantMap, QVariantMap()),
                                   // allowFastLaunch: launch when installed,
                                   // fall through to the install dialog if not.
                                   Q_ARG(bool, true))) {
        qCritical() << "openApp(QString,QString,QVariantMap,bool) not found on"
                    << "the backend — welcome-page app tiles will do nothing.";
    }
}

void MainContainer::applyAppManagerSearch(const QString& query)
{
    if (!m_contentWidget) return;
    if (QObject* contentRoot = m_contentWidget->rootObject()) {
        QMetaObject::invokeMethod(contentRoot, "applyAppSearch",
                                  Q_ARG(QVariant, QVariant(query)));
    }
}

MainContainer::~MainContainer()
{
    // Belt-and-braces: MainShellView::destroyShell() detaches before deleting,
    // but this covers the path where Qt's parent-child teardown destroys the
    // central widget first. Idempotent.
    detachFromHost();
    qDebug() << "MainContainer destroyed";
}

void MainContainer::detachFromHost()
{
    if (!m_host) {
        return;
    }
    m_host->setObserver(nullptr);
}

void MainContainer::setupUi()
{
    const QColor bgColor = Logos::themeColor(QStringLiteral("background"), QColor("#171717"));
    // set background color
    setAutoFillBackground(true);
    QPalette p = palette();
    p.setColor(QPalette::Window, bgColor);
    setPalette(p);

    // The host window (app/window.cpp) fills #171717 around us and doesn't link
    // the design system; recolour it (and us) from here, now and on every
    // live theme switch.
    auto applyBackground = [this]() {
        const QColor bg = Logos::themeColor(QStringLiteral("background"), QColor("#171717"));
        QPalette own = palette();
        own.setColor(QPalette::Window, bg);
        setPalette(own);
        if (m_sidebarWidget)
            m_sidebarWidget->setClearColor(bg);
        if (m_contentWidget && !m_desktopLayout)
            m_contentWidget->setClearColor(bg);
        QWidget* top = window();
        if (top && top != this) {
            QPalette tp = top->palette();
            tp.setColor(QPalette::Window, bg);
            top->setPalette(tp);
        }
    };
    QTimer::singleShot(0, this, applyBackground);
    connect(Logos::ThemeNotifier::instance(), &Logos::ThemeNotifier::currentChanged,
            this, applyBackground);

    // Create main horizontal layout
    m_mainLayout = new QHBoxLayout(this);
    m_mainLayout->setSpacing(0);
    // Even margin around the sidebar so it doesn't touch the title bar.
    m_mainLayout->setContentsMargins(8, 8, 4, 8);

    // === SIDEBAR (QML) ===
    m_sidebarWidget = new QQuickWidget(this);
    m_sidebarWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    m_sidebarWidget->rootContext()->setContextProperty("backend", m_host->backendObject());
    ShellDevQml::load(m_sidebarWidget,
        QUrl(QStringLiteral("qrc:/qt/qml/Basecamp/Sidebar/Basecamp/Sidebar/SidebarPanel.qml")),
        [this](QObject* root) { wireSidebar(root); });
    m_sidebarWidget->setMinimumWidth(80);
    m_sidebarWidget->setMaximumWidth(80);
    // set clear color to sidebar so that rounded corners don't show white
    m_sidebarWidget->setClearColor(bgColor);

    // === CONTENT AREA (vertical layout with stack + app launcher) ===
    QWidget* contentArea = new QWidget(this);
    QVBoxLayout* contentLayout = new QVBoxLayout(contentArea);
    contentLayout->setSpacing(0);
    // Top 1 + the main layout's 8 keeps the content where it was (9).
    contentLayout->setContentsMargins(4, 1, 4, 0);
    // Create content stack
    m_contentStack = new QStackedWidget(contentArea);
    m_contentStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    // Index 0: WorkspaceArea (QDockWidget-based)
    m_workspaceArea = new WorkspaceArea(m_host->backendObject(), m_contentStack);
    m_contentStack->addWidget(m_workspaceArea);
    
    // Index 1: QML content views (Dashboard, Modules, PackageManager, Settings)
    m_contentWidget = new QQuickWidget(m_contentStack);
    m_contentWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    m_contentWidget->setClearColor(bgColor);
    m_contentWidget->rootContext()->setContextProperty("backend", m_host->backendObject());
    m_contentWidget->rootContext()->setContextProperty("shellPrefs", m_prefs);
    ShellDevQml::load(m_contentWidget,
        QUrl(QStringLiteral("qrc:/qt/qml/Basecamp/Shell/Basecamp/Shell/ContentViews.qml")),
        [this](QObject* root) {
            // Desktop layout: this instance is the Applications window, and
            // Settings is a second instance (see systemAppContent).
            if (!m_desktopLayout && !m_tabsLayout)
                return;
            root->setProperty("pinnedSection", int(ShellSection::AppManager));
            connect(root, SIGNAL(repositoriesRequested()), this, SLOT(openSettingsRepositories()));
        });
    m_contentStack->addWidget(m_contentWidget);

    // Index 2: placeholder for package_manager_ui — shows a centered
    // "Loading…" label until PMUI's QQuickWidget arrives via the
    // pluginWindowRequested intercept
    m_contentStack->addWidget(makePmuiPlaceholder(m_contentStack));

    // Content stack fills the content area — the version footer that
    // used to live in a QML bottom toolbar is now inside SidebarPanel.qml.
    contentLayout->addWidget(m_contentStack, 1);

    // Add widgets to main layout
    if (m_desktopLayout) {
        // The sidebar and content stack still exist (their wiring is shared
        // with the sidebar layout) but stay hidden; the desktop takes the
        // window and the content views move into app windows on demand.
        m_mainLayout->setContentsMargins(0, 0, 0, 0);
        m_sidebarWidget->hide();
        contentArea->hide();
        m_desktopRoot = new DesktopRoot(this);
        auto* desktopColumn = m_desktopRoot;
        auto* column = new QVBoxLayout(desktopColumn);
        column->setContentsMargins(0, 0, 0, 0);
        column->setSpacing(0);
        m_statusBar = new DesktopStatusBar(desktopColumn);
        column->addWidget(m_statusBar);
        m_desktop = new DesktopArea(desktopColumn);
        column->addWidget(m_desktop, 1);
        m_mainLayout->addWidget(desktopColumn, 1);
    } else if (m_tabsLayout) {
        // Basecampium: the sidebar and content stack stay hidden (their wiring
        // is shared); the tab frame takes the window.
        m_mainLayout->setContentsMargins(0, 0, 0, 0);
        m_sidebarWidget->hide();
        contentArea->hide();
        m_newTabWidget = new QQuickWidget(this);
        m_newTabWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
        m_newTabWidget->setClearColor(bgColor);
        m_newTabWidget->rootContext()->setContextProperty("backend", m_host->backendObject());
        ShellDevQml::load(m_newTabWidget,
            QUrl(QStringLiteral("qrc:/qt/qml/Basecamp/Shell/Basecamp/Shell/NewTabPage.qml")),
            [this](QObject* root) {
                connect(root, SIGNAL(openSystemApp(QString)), this, SLOT(tabOpenSystem(QString)));
                connect(root, SIGNAL(launchApp(QString)), this, SLOT(tabLaunchApp(QString)));
            });
        m_tabArea = new TabArea(m_newTabWidget, this);
        m_mainLayout->addWidget(m_tabArea, 1);
    } else {
        m_mainLayout->addWidget(m_sidebarWidget);
        m_mainLayout->addWidget(contentArea, 1);
    }

    // === OVERLAY DIALOGS (QML) ===
    // Child of `this` but deliberately NOT added to m_mainLayout — we
    // want it to float across the whole window, overlapping sidebar +
    // content. resizeEvent keeps its geometry in sync with the parent.
    m_overlayWidget = new QQuickWidget(this);
    m_overlayWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    // Transparent clear so the sidebar + content stay visible through
    // the overlay. The dialog itself paints its own opaque background.
    m_overlayWidget->setAttribute(Qt::WA_AlwaysStackOnTop);
    m_overlayWidget->setAttribute(Qt::WA_TranslucentBackground);
    m_overlayWidget->setClearColor(Qt::transparent);
    // Start transparent-to-input so the user can interact with the
    // normal UI; flipped off in onOverlayActiveChanged while a dialog
    // is visible so the dialog itself can receive clicks.
    m_overlayWidget->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_overlayWidget->rootContext()->setContextProperty("backend", m_host->backendObject());
    ShellDevQml::load(m_overlayWidget,
        QUrl(QStringLiteral("qrc:/qt/qml/Basecamp/Shell/Basecamp/Shell/OverlayDialogs.qml")),
        [this](QObject* root) {
            connect(root, SIGNAL(overlayActiveChanged(bool)),
                    this, SLOT(onOverlayActiveChanged(bool)));
        });
    m_overlayWidget->setVisible(false);

    // Watch for the mouse leaving the sidebar so we can hide the tooltip
    // overlay.
    if (m_sidebarWidget) m_sidebarWidget->installEventFilter(this);

    // Set initial state — Apps section (workspace) visible by default.
    m_contentStack->setCurrentIndex(kAppsStackIndex);

    // Set reasonable minimum size
    setMinimumSize(800, 600);

    if (m_desktopLayout)
        setupDesktopUi();
    if (m_tabsLayout)
        setupTabsUi();
}

void MainContainer::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    positionDock();
    if (m_spotlight && m_spotlight->isVisible())
        m_spotlight->setGeometry(rect());
    if (m_overlayWidget) {
        m_overlayWidget->setGeometry(0, 0, width(), height());
        // Qt re-stacks siblings on resize in some cases; keep the
        // overlay on top explicitly.
        m_overlayWidget->raise();
    }
}

void MainContainer::onOverlayActiveChanged(bool active)
{
    if (!m_overlayWidget) return;
    // When a dialog is open, the overlay must catch the click on the
    // Cancel/Continue buttons — so make it opaque to input. When no
    // dialog is showing, pass every click through to the sidebar /
    // content behind it.
    m_overlayWidget->setAttribute(Qt::WA_TransparentForMouseEvents, !active);
    m_overlayWidget->setVisible(active);
    if (active) m_overlayWidget->raise();
}

void MainContainer::onSidebarTooltipRequested(const QString& text, qreal y)
{
    if (!m_overlayWidget) return;
    QObject* overlayRoot = m_overlayWidget->rootObject();
    if (!overlayRoot) return;
    overlayRoot->setProperty("sidebarTooltipText", text);
    overlayRoot->setProperty("sidebarTooltipY", y);
    if (!m_overlayWidget->isVisible()) {
        m_overlayWidget->setVisible(true);
        m_overlayWidget->raise();
    }
}

bool MainContainer::eventFilter(QObject* watched, QEvent* event)
{
    if (m_tabsLayout && tabsEdgeResize(watched, event))
        return true;
    if (m_tabsLayout && event->type() == QEvent::KeyPress
        && tabsKey(static_cast<QKeyEvent*>(event)))
        return true;
    if (m_desktopLayout && m_spotlight && event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        const auto mods = ke->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier
                                             | Qt::AltModifier | Qt::MetaModifier);
        auto* w = qobject_cast<QWidget*>(watched);
        if (ke->key() == Qt::Key_K && mods == Qt::ControlModifier && w && w->window() == window()) {
            toggleSpotlight();
            return true;
        }
    }
    if (watched == m_dockWidget && event->type() == QEvent::Resize)
        QTimer::singleShot(0, this, [this]() { positionDock(); });
    if (watched == m_sidebarWidget && event->type() == QEvent::Leave) {
        if (m_overlayWidget) {
            QObject* overlayRoot = m_overlayWidget->rootObject();
            if (overlayRoot) {
                overlayRoot->setProperty("sidebarTooltipText", QString());
                const bool dialogUp = overlayRoot->property("anyDialogOpen").toBool();
                if (!dialogUp) m_overlayWidget->setVisible(false);
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

// A backend without availableSections (shell-preview) offers every section.
bool MainContainer::offersSection(int section) const
{
    if (QObject* backend = m_host->backendObject()) {
        const QVariant available = backend->property("availableSections");
        if (available.isValid())
            return available.toList().contains(QVariant(section));
    }
    return true;
}

void MainContainer::onSectionIndexChanged(int index)
{
    if (!offersSection(index)) {
        m_host->setCurrentSectionIndex(ShellSection::Workspace);
        return;
    }

    const int sectionIndex = index;

    // Desktop layout: a section is an app window (Basecampium: a tab). Open
    // it, then park the section back on Workspace so asking for the same one
    // again re-opens it.
    if (m_desktopLayout || m_tabsLayout) {
        switch (sectionIndex) {
        case ShellSection::AppManager:     openSystemApp(QStringLiteral("sys:applications")); break;
        case ShellSection::PackageManager: openSystemApp(QStringLiteral("sys:packages")); break;
        case ShellSection::Settings:       openSystemApp(QStringLiteral("sys:settings")); break;
        default: return;
        }
        QTimer::singleShot(0, this, [this]() {
            m_host->setCurrentSectionIndex(ShellSection::Workspace);
        });
        return;
    }

    qDebug() << "MainContainer: Active section index changed to" << sectionIndex;

    //   0 (Workspace)        → WorkspaceArea (QDockWidget-based)
    //   1 (Applications)     → ContentViews.qml (App Manager view)
    //   2 (Package Manager)  → package_manager_ui (preloaded in background)
    //   3 (Settings)         → ContentViews.qml (StackLayout picks the page)
    switch (sectionIndex) {
    case ShellSection::Workspace:
        m_contentStack->setCurrentIndex(kAppsStackIndex);
        break;
    case ShellSection::AppManager:
        m_contentStack->setCurrentIndex(kContentStackIndex);
        break;
    case ShellSection::PackageManager:
        if (!m_pmuiWidget) {
            m_host->loadUiModule(QStringLiteral("package_manager_ui"));
        }
        m_contentStack->setCurrentIndex(kModulesStackIndex);
        break;
    case ShellSection::Settings:
        m_contentStack->setCurrentIndex(kContentStackIndex);
        break;
    default: break;
    }
}

void MainContainer::onNavigateToApps()
{
    // Suppressed exactly once after package_manager_ui is folded into the
    // content stack — that path installs the pane itself and must not also
    // bounce the user to the Apps view.
    if (m_suppressNextNavToApps) {
        m_suppressNextNavToApps = false;
        return;
    }

    // This is called when an app is loaded and we need to switch to Apps view.
    // The desktop has no such view: the app's window is already in front.
    if (m_desktopLayout || m_tabsLayout)
        return;
    m_host->setCurrentSectionIndex(ShellSection::Workspace);
}

void MainContainer::onPluginWindowRequested(QWidget* widget, const QString& title)
{
    if ((m_desktopLayout || m_tabsLayout) && widget) {
        if (title == QStringLiteral("package_manager_ui") && !m_pmuiWidget
            && offersSection(ShellSection::PackageManager)) {
            // Preloaded in the background: keep it until Package Manager is opened.
            m_pmuiWidget = widget;
            widget->setParent(this);
            widget->hide();
            m_suppressNextNavToApps = true;
            if (m_packagesPending)
                openSystemApp(QStringLiteral("sys:packages"));
            return;
        }
        const QVariantMap entry = launcherEntry(title);
        const QString resolved = m_host->displayNameFor(title);
        QString icon = entry.value(QStringLiteral("iconPath")).toString();
        if (icon.startsWith(QStringLiteral("file:")))
            icon = QUrl(icon).toLocalFile();
        else if (icon.startsWith(QStringLiteral("qrc:")))
            icon = icon.mid(3);
        if (m_tabArea) {
            openAppTab(title, resolved.isEmpty() ? title : resolved, entry.value(QStringLiteral("iconPath")).toString(), widget);
            return;
        }
        m_desktop->openWindow(title, resolved.isEmpty() ? title : resolved, icon, widget);
        updateDock();
        return;
    }

    // package_manager_ui is not a dock: it becomes the Package Manager page of
    // the content stack, replacing the placeholder that sits there at startup.
    // Only where that section exists — a host without it (standalone mode)
    // docks it like any other app.
    if (title == QStringLiteral("package_manager_ui") && !m_pmuiWidget
        && offersSection(ShellSection::PackageManager)) {
        m_pmuiWidget = widget;
        QWidget* placeholder = m_contentStack->widget(kModulesStackIndex);
        widget->setParent(m_contentStack);
        m_contentStack->insertWidget(kModulesStackIndex, widget);
        if (placeholder) {
            m_contentStack->removeWidget(placeholder);
            placeholder->deleteLater();
        }
        // A new pane's QML just materialised — ask the bridge to pick up any
        // Shortcut { } blocks inside it. Deferred, so the QML tree has time to
        // instantiate.
        if (m_shortcutBridge) m_shortcutBridge->rebindDeferred();
        m_suppressNextNavToApps = true;
        // Standalone mode opening it as the app under development: show it.
        if (QObject* backend = m_host->backendObject();
            backend && backend->property("currentVisibleApp").toString() == title)
            m_host->setCurrentSectionIndex(ShellSection::PackageManager);
        return;
    }

    if (m_workspaceArea && widget) {
        const QString resolved = m_host->displayNameFor(title);
        const QString label = resolved.isEmpty() ? title : resolved;
        m_workspaceArea->addPluginDock(widget, title, label);
        qDebug() << "MainContainer: Added plugin dock to WorkspaceArea:"
                 << label << "(module:" << title << ")";
    }
}

void MainContainer::onPluginWindowRemoveRequested(QWidget* widget)
{
    if (m_tabArea) {
        const QString key = m_tabArea->keyFor(widget);
        if (!key.isEmpty())
            m_tabArea->takeTab(key);
        if (widget && widget == m_pmuiWidget)
            m_pmuiWidget = nullptr;
        saveTabs();
        return;
    }
    if (m_desktopLayout) {
        if (AppWindow* w = m_desktop->windowFor(widget))
            m_desktop->removeWindow(w);
        if (widget && widget == m_pmuiWidget)
            m_pmuiWidget = nullptr;
        updateDock();
        return;
    }
    if (widget && widget == m_pmuiWidget) {
        // package_manager_ui is the Package Manager PAGE, not a dock, so it must
        // not reach removePluginDock() -- but it cannot simply be left in the
        // stack either: the host deleteLater()s it the moment this returns.
        // Taking a page out without putting one back leaves a two-page stack
        // that every `setCurrentIndex(kModulesStackIndex)` then indexes past the
        // end of, and the section is dead for the rest of the session.
        m_contentStack->removeWidget(widget);
        m_contentStack->insertWidget(kModulesStackIndex,
                                     makePmuiPlaceholder(m_contentStack));
        m_pmuiWidget = nullptr;
        return;
    }
    if (m_workspaceArea && widget)
        m_workspaceArea->removePluginDock(widget);
}

void MainContainer::onPresentAppRequested(QWidget* widget)
{
    if (!widget) return;

    if (m_tabArea) {
        if (widget == m_pmuiWidget) {
            openSystemApp(QStringLiteral("sys:packages"));
        } else {
            const QString key = m_tabArea->keyFor(widget);
            if (!key.isEmpty()) {
                m_tabArea->activate(key);
                dropPendingNewTab();
            }
        }
        return;
    }

    if (m_desktopLayout) {
        if (widget == m_pmuiWidget)
            openSystemApp(QStringLiteral("sys:packages"));
        else if (AppWindow* w = m_desktop->windowFor(widget))
            m_desktop->restoreAndRaise(w);
        updateDock();
        return;
    }

    // Hoisted into the content stack, not docked, so "to the front" means
    // selecting its section rather than raising a tab.
    if (widget == m_pmuiWidget) {
        m_host->setCurrentSectionIndex(ShellSection::PackageManager);
        return;
    }

    if (!m_workspaceArea) return;

    // Switch section before raising the tab, or the tab becomes current behind
    // whatever the user is actually looking at.
    onNavigateToApps();
    m_workspaceArea->activatePluginDock(widget);
}

// ── Desktop layout ──────────────────────────────────────────────────────────

void MainContainer::setupDesktopUi()
{
    m_desktopRoot->setWallpaper(m_prefs->wallpaperPath(), m_prefs->wallpaperFit());
    connect(m_prefs, &ShellPrefs::wallpaperChanged, this, [this]() {
        m_desktopRoot->setWallpaper(m_prefs->wallpaperPath(), m_prefs->wallpaperFit());
    });
    connect(m_statusBar, &DesktopStatusBar::tileClicked, this, [this]() {
        m_desktop->setTiling(!m_desktop->tiling());
    });
    connect(m_desktop, &DesktopArea::tilingChanged, m_statusBar, &DesktopStatusBar::setTiling);

    // The shell's own windows (Applications, Settings) draw frosted glass
    // themselves (see GlassInfo).
    m_contentWidget->engine()->addImageProvider(QStringLiteral("backdrop"),
                                                new BackdropProvider(m_desktopRoot));
    m_contentWidget->rootContext()->setContextProperty(
        "glass", new GlassInfo(m_desktopRoot, m_contentWidget, m_contentWidget));
    connect(m_desktop, &DesktopArea::windowsChanged, this, &MainContainer::updateDock);
    connect(m_desktop, &DesktopArea::closeRequested, this, &MainContainer::onDesktopCloseRequested);

    // The dock floats over the desktop, like the overlay: a translucent
    // QQuickWidget sized to its content and kept centred at the bottom.
    m_dockWidget = new QQuickWidget(this);
    m_dockWidget->setResizeMode(QQuickWidget::SizeViewToRootObject);
    m_dockWidget->setAttribute(Qt::WA_AlwaysStackOnTop);
    m_dockWidget->setAttribute(Qt::WA_TranslucentBackground);
    m_dockWidget->setClearColor(Qt::transparent);
    m_dockWidget->rootContext()->setContextProperty("backend", m_host->backendObject());
    ShellDevQml::load(m_dockWidget,
        QUrl(QStringLiteral("qrc:/qt/qml/Basecamp/Shell/Basecamp/Shell/DockPanel.qml")),
        [this](QObject* root) {
            connect(root, SIGNAL(openSystemApp(QString)), this, SLOT(openSystemApp(QString)));
            connect(root, SIGNAL(launchApp(QString)), this, SLOT(launchFromDock(QString)));
            updateDock();
        });
    m_dockWidget->installEventFilter(this);
    m_dockWidget->show();
    // Panel 72 + 12 from the edge + a little air: maximised windows stop above it.
    m_desktop->setBottomReserve(92);

    // Spotlight: the workspace's search page (search, Recently Closed,
    // results) in spotlight mode, over the whole desktop. Ctrl+K toggles it.
    m_spotlight = new QQuickWidget(this);
    m_spotlight->setResizeMode(QQuickWidget::SizeRootObjectToView);
    m_spotlight->setAttribute(Qt::WA_AlwaysStackOnTop);
    m_spotlight->setAttribute(Qt::WA_TranslucentBackground);
    m_spotlight->setClearColor(Qt::transparent);
    m_spotlight->rootContext()->setContextProperty("backend", m_host->backendObject());
    m_spotlight->engine()->addImageProvider(QStringLiteral("backdrop"), new BackdropProvider(m_desktopRoot));
    m_spotlight->rootContext()->setContextProperty(
        "glass", new GlassInfo(m_desktopRoot, m_spotlight, m_spotlight));
    ShellDevQml::load(m_spotlight,
        QUrl(QStringLiteral("qrc:/qt/qml/Basecamp/Shell/Basecamp/Shell/WelcomePage.qml")),
        [this](QObject* root) {
            root->setProperty("spotlight", true);
            root->setProperty("searchShortcutEnabled", false);
            connect(root, SIGNAL(dismissRequested()), this, SLOT(hideSpotlight()));
            connect(root, SIGNAL(reopenAppRequested(QString)), this, SLOT(spotlightReopen(QString)));
            connect(root, SIGNAL(appActivated(QString,QString)), this, SLOT(spotlightOpenApp(QString,QString)));
            connect(root, SIGNAL(packageActivated(QString)), this, SLOT(spotlightPackage(QString)));
            connect(root, SIGNAL(packageInstallRequested(QString)), this, SLOT(spotlightInstall(QString)));
            connect(root, SIGNAL(showAllResultsRequested(QString,QString)),
                    this, SLOT(spotlightShowAll(QString,QString)));
        });
    m_spotlight->hide();
    if (m_workspaceArea && m_workspaceArea->welcomePageWidget()
        && m_workspaceArea->welcomePageWidget()->rootObject())
        m_workspaceArea->welcomePageWidget()->rootObject()->setProperty("searchShortcutEnabled", false);
    auto* search = new QShortcut(QKeySequence(QStringLiteral("Ctrl+K")), this);
    search->setContext(Qt::ApplicationShortcut);
    auto toggleSpotlight = [this]() { this->toggleSpotlight(); };
    connect(search, &QShortcut::activated, this, toggleSpotlight);
    // A focused text field takes Ctrl+K first (it means "delete to end of
    // line" in Qt text inputs), so the shortcut never sees it; catch the key
    // for the whole window too (eventFilter).
    qApp->installEventFilter(this);
    // Apps and the hidden Applications / Package Manager views have their
    // own Ctrl+K, which makes this one ambiguous; on the desktop the shell's wins.
    connect(search, &QShortcut::activatedAmbiguously, this, [toggleSpotlight]() {
        qDebug() << "MainContainer: Ctrl+K ambiguous; Spotlight takes it";
        toggleSpotlight();
    });
    connect(m_statusBar, &DesktopStatusBar::searchClicked, this, &MainContainer::showSpotlight);

    // Full screen is the desktop's natural state; F11 leaves and re-enters it.
    auto* fullScreen = new QShortcut(QKeySequence(Qt::Key_F11), this);
    fullScreen->setContext(Qt::ApplicationShortcut);
    connect(fullScreen, &QShortcut::activated, this, [this]() {
        QWidget* top = window();
        if (top->isFullScreen())
            top->showNormal();
        else
            top->showFullScreen();
    });
}

void MainContainer::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (m_desktopLayout && !m_wentFullScreen) {
        m_wentFullScreen = true;
        QTimer::singleShot(0, this, [this]() { window()->showFullScreen(); });
    }
}

void MainContainer::positionDock()
{
    if (!m_dockWidget)
        return;
    m_dockWidget->move((width() - m_dockWidget->width()) / 2,
                       height() - m_dockWidget->height() - 12);
    m_dockWidget->raise();
    if (m_overlayWidget && m_overlayWidget->isVisible())
        m_overlayWidget->raise();
}

void MainContainer::updateDock()
{
    if (!m_dockWidget || !m_desktop)
        return;
    QObject* root = m_dockWidget->rootObject();
    if (!root)
        return;
    root->setProperty("openKeys", QVariant(m_desktop->openKeys()));
    const QString active = m_desktop->activeKey();
    root->setProperty("activeKey", active);
    if (!active.isEmpty() && !active.startsWith(QStringLiteral("sys:")))
        m_host->setCurrentVisibleApp(active);
}

QVariantMap MainContainer::launcherEntry(const QString& name) const
{
    if (QObject* backend = m_host->backendObject()) {
        for (const QVariant& v : backend->property("launcherApps").toList()) {
            const QVariantMap m = v.toMap();
            if (m.value(QStringLiteral("name")).toString() == name)
                return m;
        }
    }
    return {};
}

QWidget* MainContainer::systemAppContent(const QString& key)
{
    if (key == QStringLiteral("sys:applications"))
        return m_contentWidget;
    if (key == QStringLiteral("sys:packages"))
        return m_pmuiWidget;
    if (key == QStringLiteral("sys:settings")) {
        if (!m_settingsWidget) {
            m_settingsWidget = new QQuickWidget(this);
            m_settingsWidget->hide();
            m_settingsWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
            m_settingsWidget->setClearColor(
                Logos::themeColor(QStringLiteral("background"), QColor("#171717")));
            if (m_desktopRoot) {
                m_settingsWidget->engine()->addImageProvider(QStringLiteral("backdrop"),
                                                             new BackdropProvider(m_desktopRoot));
                m_settingsWidget->rootContext()->setContextProperty(
                    "glass", new GlassInfo(m_desktopRoot, m_settingsWidget, m_settingsWidget));
            }
            m_settingsWidget->rootContext()->setContextProperty("backend", m_host->backendObject());
            m_settingsWidget->rootContext()->setContextProperty("shellPrefs", m_prefs);
            ShellDevQml::load(m_settingsWidget,
                QUrl(QStringLiteral("qrc:/qt/qml/Basecamp/Shell/Basecamp/Shell/ContentViews.qml")),
                [](QObject* root) {
                    root->setProperty("pinnedSection", int(ShellSection::Settings));
                    root->setProperty("handlesIntents", false);
                });
        }
        return m_settingsWidget;
    }
    return nullptr;
}

void MainContainer::openSystemApp(const QString& key)
{
    if (m_tabArea) {
        openSystemTab(key);
        return;
    }
    if (!m_desktop)
        return;
    if (AppWindow* w = m_desktop->window(key)) {
        m_desktop->restoreAndRaise(w);
        return;
    }
    static const QHash<QString, std::pair<QString, QString>> meta = {
        { QStringLiteral("sys:applications"),
          { tr("Applications"), QStringLiteral(":/qt/qml/Basecamp/Icons/Basecamp/Icons/dashboard.svg") } },
        { QStringLiteral("sys:packages"),
          { tr("Package Manager"), QStringLiteral(":/qt/qml/Basecamp/Icons/Basecamp/Icons/module.svg") } },
        { QStringLiteral("sys:settings"),
          { tr("Settings"), QStringLiteral(":/qt/qml/Basecamp/Icons/Basecamp/Icons/settings.svg") } },
    };
    if (!meta.contains(key))
        return;
    QWidget* content = systemAppContent(key);
    if (!content) {
        // Package Manager arrives from the host; open it when it does.
        if (key == QStringLiteral("sys:packages")) {
            m_packagesPending = true;
            m_host->loadUiModule(QStringLiteral("package_manager_ui"));
        }
        return;
    }
    if (key == QStringLiteral("sys:packages")) {
        m_packagesPending = false;
        applyGlass(content);
    }
    const auto& [title, icon] = meta.value(key);
    m_desktop->openWindow(key, title, icon, content);
    if (key == QStringLiteral("sys:applications"))
        QMetaObject::invokeMethod(m_host->backendObject(), "requestCatalog");
    updateDock();
}

void MainContainer::openSettingsRepositories()
{
    openSystemApp(QStringLiteral("sys:settings"));
    if (m_settingsWidget && m_settingsWidget->rootObject())
        QMetaObject::invokeMethod(m_settingsWidget->rootObject(), "showRepositories");
}

void MainContainer::launchFromDock(const QString& name)
{
    if (AppWindow* w = m_desktop ? m_desktop->window(name) : nullptr) {
        m_desktop->restoreAndRaise(w);
        return;
    }
    QMetaObject::invokeMethod(m_host->backendObject(), "onAppLauncherClicked",
                              Qt::QueuedConnection, Q_ARG(QString, name));
}

void MainContainer::onDesktopCloseRequested(const QString& key)
{
    AppWindow* w = m_desktop->window(key);
    if (!w)
        return;
    if (key.startsWith(QStringLiteral("sys:"))) {
        // The shell's own views are kept for the next open.
        QWidget* content = w->releaseContent();
        m_desktop->removeWindow(w);
        if (content) {
            content->setParent(this);
            content->hide();
        }
        updateDock();
        return;
    }
    // An app's window goes when the host unloads it (onPluginWindowRemoveRequested).
    m_host->unloadUiModule(key);
}

void MainContainer::showSpotlight()
{
    if (!m_spotlight)
        return;
    m_spotlight->setGeometry(rect());
    m_spotlight->show();
    m_spotlight->raise();
    m_spotlight->setFocus();
    if (QObject* root = m_spotlight->rootObject())
        QMetaObject::invokeMethod(root, "focusSearch");
}

void MainContainer::hideSpotlight()
{
    if (!m_spotlight || !m_spotlight->isVisible())
        return;
    if (QObject* root = m_spotlight->rootObject())
        QMetaObject::invokeMethod(root, "clearSearch");
    m_spotlight->hide();
}

void MainContainer::spotlightReopen(const QString& name)
{
    hideSpotlight();
    launchFromDock(name);
}

void MainContainer::spotlightOpenApp(const QString& name, const QString& repositoryUrl)
{
    hideSpotlight();
    if (AppWindow* w = m_desktop->window(name))
        m_desktop->restoreAndRaise(w);
    else
        invokeOpenApp(name, repositoryUrl);
}

void MainContainer::spotlightPackage(const QString& name)
{
    hideSpotlight();
    QMetaObject::invokeMethod(m_host->backendObject(), "showPackageDetails", Q_ARG(QString, name));
}

void MainContainer::spotlightInstall(const QString& name)
{
    hideSpotlight();
    QMetaObject::invokeMethod(m_host->backendObject(), "requestPackageInstall", Q_ARG(QString, name));
}

void MainContainer::spotlightShowAll(const QString& typeValue, const QString& query)
{
    hideSpotlight();
    if (typeValue == QStringLiteral("core")) {
        openSystemApp(QStringLiteral("sys:packages"));
    } else {
        openSystemApp(QStringLiteral("sys:applications"));
        applyAppManagerSearch(query);
    }
}


// Frosted glass for an app we don't own the QML of (Package Manager): make
// its root fill transparent and slide a glass layer underneath its content.
void MainContainer::applyGlass(QWidget* widget)
{
    auto* qw = qobject_cast<QQuickWidget*>(widget);
    if (!qw)
        qw = widget ? widget->findChild<QQuickWidget*>() : nullptr;
    if (!qw || qw->property("_bcGlass").toBool())
        return;
    auto* root = qobject_cast<QQuickItem*>(qw->rootObject());
    if (!root || !root->property("color").isValid())
        return;
    QColor tint = Logos::themeColor(QStringLiteral("background"), QColor("#171717"));
    tint.setAlphaF(0.9);
    new GlassItem(m_desktopRoot, qw, tint, root);
    root->setProperty("color", QColor(Qt::transparent));
    qw->setProperty("_bcGlass", true);
}

void MainContainer::toggleSpotlight()
{
    if (m_spotlight && m_spotlight->isVisible())
        hideSpotlight();
    else
        showSpotlight();
}


// ── Basecampium: tabs ───────────────────────────────────────────────────────

bool MainContainer::event(QEvent* event)
{
#ifndef Q_OS_MAC
    // Basecampium draws its own bar (tabs + window buttons), so the window
    // loses its system title bar as soon as we are placed in it (before it is
    // first shown). macOS is already frameless; Basecamp draws the traffic lights.
    if (m_tabsLayout && event->type() == QEvent::ParentChange) {
        QWidget* top = window();
        if (top && top != this && !(top->windowFlags() & Qt::FramelessWindowHint))
            top->setWindowFlag(Qt::FramelessWindowHint, true);
    }
#endif
    return QWidget::event(event);
}

void MainContainer::setupTabsUi()
{
    connect(m_tabArea, &TabArea::closeRequested, this, &MainContainer::closeTab);
    connect(m_tabArea, &TabArea::currentKeyChanged, this, [this](const QString& key) {
        if (!key.isEmpty() && !key.startsWith(QStringLiteral("sys:")) && !TabArea::isNewTabKey(key))
            m_host->setCurrentVisibleApp(key);
        if (m_shortcutBridge)
            m_shortcutBridge->rebindDeferred();
        saveTabs();
    });
    connect(m_tabArea, &TabArea::newTabShown, this, [this]() {
        if (m_newTabWidget && m_newTabWidget->rootObject()) {
            QMetaObject::invokeMethod(m_newTabWidget->rootObject(), "clearSearch");
            m_newTabWidget->setFocus();
            QMetaObject::invokeMethod(m_newTabWidget->rootObject(), "focusSearch");
        }
    });

    // Keys are caught for the whole window in eventFilter (tabsKey): a focused
    // text field inside an app or Settings would otherwise take Ctrl+W / Ctrl+T
    // before a QShortcut sees them.
    if (m_workspaceArea && m_workspaceArea->welcomePageWidget()
        && m_workspaceArea->welcomePageWidget()->rootObject())
        m_workspaceArea->welcomePageWidget()->rootObject()->setProperty("searchShortcutEnabled", false);

    // Frameless window: resize from the edges ourselves.
    qApp->installEventFilter(this);

    QTimer::singleShot(0, this, &MainContainer::restoreTabs);
}

void MainContainer::openSystemTab(const QString& key)
{
    static const QHash<QString, std::pair<QString, QString>> meta = {
        { QStringLiteral("sys:applications"),
          { tr("Applications"), QStringLiteral(":/qt/qml/Basecamp/Icons/Basecamp/Icons/dashboard.svg") } },
        { QStringLiteral("sys:packages"),
          { tr("Package Manager"), QStringLiteral(":/qt/qml/Basecamp/Icons/Basecamp/Icons/module.svg") } },
        { QStringLiteral("sys:settings"),
          { tr("Settings"), QStringLiteral(":/qt/qml/Basecamp/Icons/Basecamp/Icons/settings.svg") } },
    };
    if (!meta.contains(key))
        return;
    if (m_tabArea->hasKey(key)) {
        m_tabArea->activate(key);
        dropPendingNewTab();
        return;
    }
    QWidget* content = systemAppContent(key);
    if (!content) {
        // Package Manager arrives from the host; opened when it does.
        if (key == QStringLiteral("sys:packages")) {
            m_packagesPending = true;
            m_host->loadUiModule(QStringLiteral("package_manager_ui"));
        }
        return;
    }
    if (key == QStringLiteral("sys:packages"))
        m_packagesPending = false;
    const auto& [title, icon] = meta.value(key);
    const QString replace = m_pendingNewTab;
    m_pendingNewTab.clear();
    m_tabArea->openContent(key, title, icon, content, replace);
    if (key == QStringLiteral("sys:applications"))
        QMetaObject::invokeMethod(m_host->backendObject(), "requestCatalog");
}

void MainContainer::openAppTab(const QString& module, const QString& label,
                               const QString& icon, QWidget* widget)
{
    const QString replace = m_tabArea->hasKey(m_pendingNewTab) ? m_pendingNewTab : QString();
    m_pendingNewTab.clear();
    m_tabArea->openContent(module, label, icon, widget, replace);
    if (m_shortcutBridge)
        m_shortcutBridge->rebindDeferred();
}

void MainContainer::tabOpenSystem(const QString& key)
{
    const QString current = m_tabArea->currentKey();
    m_pendingNewTab = TabArea::isNewTabKey(current) ? current : QString();
    openSystemTab(key);
}

void MainContainer::tabLaunchApp(const QString& name)
{
    const QString current = m_tabArea->currentKey();
    m_pendingNewTab = TabArea::isNewTabKey(current) ? current : QString();
    if (m_tabArea->hasKey(name)) {
        m_tabArea->activate(name);
        dropPendingNewTab();
        return;
    }
    QMetaObject::invokeMethod(m_host->backendObject(), "onAppLauncherClicked",
                              Qt::QueuedConnection, Q_ARG(QString, name));
}

void MainContainer::dropPendingNewTab()
{
    // The app was already open in another tab: the New tab it was picked
    // from goes, as in a browser switching to an open tab.
    if (!m_pendingNewTab.isEmpty() && m_tabArea->hasKey(m_pendingNewTab)
        && m_tabArea->currentKey() != m_pendingNewTab)
        m_tabArea->takeTab(m_pendingNewTab);
    m_pendingNewTab.clear();
}

void MainContainer::closeTab(const QString& key)
{
    if (key.isEmpty())
        return;
    if (TabArea::isNewTabKey(key)) {
        m_tabArea->takeTab(key);
        return;
    }
    m_closedKeys.removeAll(key);
    m_closedKeys.append(key);
    if (key.startsWith(QStringLiteral("sys:"))) {
        // The shell's own views are kept for the next open.
        if (QWidget* content = m_tabArea->takeTab(key)) {
            content->setParent(this);
            content->hide();
        }
        saveTabs();
        return;
    }
    // An app's tab goes when the host unloads it (onPluginWindowRemoveRequested).
    m_host->unloadUiModule(key);
}

void MainContainer::reopenClosedTab()
{
    if (m_closedKeys.isEmpty())
        return;
    const QString key = m_closedKeys.takeLast();
    m_pendingNewTab.clear();
    if (key.startsWith(QStringLiteral("sys:")))
        openSystemTab(key);
    else
        QMetaObject::invokeMethod(m_host->backendObject(), "onAppLauncherClicked",
                                  Qt::QueuedConnection, Q_ARG(QString, key));
}

void MainContainer::saveTabs()
{
    if (!m_tabArea || m_restoringTabs)
        return;
    QStringList keep;
    for (const QString& k : m_tabArea->keys())
        if (!TabArea::isNewTabKey(k))
            keep << k;
    QSettings().setValue(QStringLiteral("BasecampiumShell/tabs"), keep);
}

void MainContainer::restoreTabs()
{
    const QStringList saved = QSettings().value(QStringLiteral("BasecampiumShell/tabs")).toStringList();
    m_restoringTabs = true;
    for (const QString& key : saved) {
        if (key.startsWith(QStringLiteral("sys:")))
            openSystemTab(key);
        else
            QMetaObject::invokeMethod(m_host->backendObject(), "onAppLauncherClicked",
                                      Qt::QueuedConnection, Q_ARG(QString, key));
    }
    m_restoringTabs = false;
    // Nothing to restore: start on a New tab. Apps arrive asynchronously; if
    // none of them comes back, fall back to a New tab rather than an empty bar.
    if (saved.isEmpty() || m_tabArea->keys().isEmpty()) {
        if (saved.isEmpty())
            m_tabArea->addNewTab();
        else
            QTimer::singleShot(5000, this, [this]() {
                if (m_tabArea->keys().isEmpty())
                    m_tabArea->addNewTab();
            });
    }
}

bool MainContainer::tabsEdgeResize(QObject* watched, QEvent* event)
{
    const auto type = event->type();
    if (type != QEvent::MouseMove && type != QEvent::MouseButtonPress)
        return false;
    auto* w = qobject_cast<QWidget*>(watched);
    QWidget* top = window();
    if (!w || !top || w->window() != top || top == this)
        return false;
    if (top->isMaximized() || top->isFullScreen())
        return false;
    auto* me = static_cast<QMouseEvent*>(event);
    const QPoint p = top->mapFromGlobal(me->globalPosition().toPoint());
    constexpr int m = 6;
    Qt::Edges edges;
    if (p.x() <= m) edges |= Qt::LeftEdge;
    if (p.x() >= top->width() - m) edges |= Qt::RightEdge;
    if (p.y() <= m) edges |= Qt::TopEdge;
    if (p.y() >= top->height() - m) edges |= Qt::BottomEdge;

    if (type == QEvent::MouseMove && me->buttons() == Qt::NoButton) {
        const int code = int(edges);
        if (code != m_edgeCursor) {
            m_edgeCursor = code;
            if (!edges) {
                top->unsetCursor();
            } else if ((edges & (Qt::LeftEdge | Qt::TopEdge)) == (Qt::LeftEdge | Qt::TopEdge)
                       || (edges & (Qt::RightEdge | Qt::BottomEdge)) == (Qt::RightEdge | Qt::BottomEdge)) {
                top->setCursor(Qt::SizeFDiagCursor);
            } else if ((edges & (Qt::RightEdge | Qt::TopEdge)) == (Qt::RightEdge | Qt::TopEdge)
                       || (edges & (Qt::LeftEdge | Qt::BottomEdge)) == (Qt::LeftEdge | Qt::BottomEdge)) {
                top->setCursor(Qt::SizeBDiagCursor);
            } else if (edges & (Qt::LeftEdge | Qt::RightEdge)) {
                top->setCursor(Qt::SizeHorCursor);
            } else {
                top->setCursor(Qt::SizeVerCursor);
            }
        }
        return false;
    }
    if (type == QEvent::MouseButtonPress && me->button() == Qt::LeftButton && edges) {
        if (QWindow* handle = top->windowHandle()) {
            handle->startSystemResize(edges);
            return true;
        }
    }
    return false;
}

bool MainContainer::tabsKey(QKeyEvent* e)
{
    if (!m_tabArea || !isVisible())
        return false;
    const auto mods = e->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier | Qt::MetaModifier);
    const int key = e->key();
    if (key == Qt::Key_F11 && mods == Qt::NoModifier) {
        QWidget* top = window();
        if (top->isFullScreen()) top->showNormal(); else top->showFullScreen();
        return true;
    }
    const bool ctrl = mods == Qt::ControlModifier;
    const bool ctrlShift = mods == (Qt::ControlModifier | Qt::ShiftModifier);
    if (!ctrl && !ctrlShift)
        return false;
    if (e->isAutoRepeat() && (key == Qt::Key_W || key == Qt::Key_T))
        return true;
    if (ctrl && key == Qt::Key_T) { m_tabArea->addNewTab(); return true; }
    if (ctrl && key == Qt::Key_W) { closeTab(m_tabArea->currentKey()); return true; }
    if (ctrl && (key == Qt::Key_Tab || key == Qt::Key_PageDown)) { m_tabArea->nextTab(1); return true; }
    if ((ctrlShift && (key == Qt::Key_Backtab || key == Qt::Key_Tab)) || (ctrl && key == Qt::Key_PageUp)) {
        m_tabArea->nextTab(-1); return true;
    }
    if (ctrl && key >= Qt::Key_1 && key <= Qt::Key_8) { m_tabArea->goToTab(key - Qt::Key_1); return true; }
    if (ctrl && key == Qt::Key_9) { m_tabArea->goToTab(-1); return true; }
    if (ctrl && key == Qt::Key_K) {
        if (TabArea::isNewTabKey(m_tabArea->currentKey()) && m_newTabWidget->rootObject())
            QMetaObject::invokeMethod(m_newTabWidget->rootObject(), "focusSearch");
        else
            m_tabArea->addNewTab();
        return true;
    }
    if (ctrl && key == Qt::Key_Comma) { openSystemApp(QStringLiteral("sys:settings")); return true; }
    if (ctrlShift && key == Qt::Key_T) { reopenClosedTab(); return true; }
    return false;
}
