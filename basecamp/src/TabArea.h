#pragma once

#include <QHash>
#include <QStringList>
#include <QTabBar>
#include <QWidget>

class QStackedWidget;
class QSplitter;
class QTimer;
class QLabel;
class QLineEdit;
class QToolButton;

// Basecampium's frame: one bar with "+", the tabs, a drag area and the window
// buttons, over a stack with one page per tab. A tab is identified by a key:
// "sys:..." for the shell's own views, a module name for an app, "new:N" for a
// New tab (all New tabs share one page, the New tab widget).
class TabStrip : public QTabBar
{
    Q_OBJECT
public:
    explicit TabStrip(QWidget* parent = nullptr);
    void setAvailableWidth(int px);
    int tabWidth() const;

signals:
    // A tab was dragged well outside the strip: open it in its own window.
    void detachRequested(int index, const QPoint& globalPos);

protected:
    QSize tabSizeHint(int index) const override;
    QSize minimumTabSizeHint(int index) const override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    int m_available = 1000;
    bool m_dragging = false;
    // Touch has no right-click: a press held still opens the tab menu.
    QTimer* m_longPress = nullptr;
    QPoint m_pressPos;
};

class TabArea : public QWidget
{
    Q_OBJECT
public:
    explicit TabArea(QWidget* newTabPage, QWidget* parent = nullptr);

    // Opens (or focuses) content under `key`. If `replaceKey` names an open
    // New tab, that tab becomes this content; otherwise a tab is appended.
    void openContent(const QString& key, const QString& title, const QString& iconPath,
                     QWidget* content, const QString& replaceKey = QString());
    QString addNewTab();
    void activate(const QString& key);
    bool hasKey(const QString& key) const;
    // Removes the tab without deleting its content; returns the content.
    QWidget* takeTab(const QString& key);
    QString keyFor(QWidget* content) const;
    QWidget* contentFor(const QString& key) const;
    QString currentKey() const;
    QStringList keys() const;
    void setTitle(const QString& key, const QString& title);

    static bool isNewTabKey(const QString& key) { return key.startsWith(QStringLiteral("new:")); }

    // Moves a tab's content into its own window (drag out, or the tab menu).
    void detachTab(int index, const QPoint& globalPos);
    // Puts a torn-off window's app back as a tab ("Back to the main window").
    void attachTab(const QString& key);

    void nextTab(int delta);
    void goToTab(int index);  // -1 = last
    // Tabs (by key) whose app has unread notifications get a dot.
    void setAttention(const QStringList& keys);

    // Phone-width layout (docs/MOBILE-LAYOUT.md): below this width the strip
    // gives way to a title, a bottom bar and a tab switcher.
    static constexpr int kCompactWidth = 600;
    static constexpr int kBottomBarHeight = 60;
    bool isCompact() const { return m_compact; }
    // What the bar reads on the New tab, from the home app; "" = "Search apps…".
    void setHomePlaceholder(const QString& text);
    int bottomBarHeight() const;
    Q_INVOKABLE void showSwitcher(bool on);
    int topBarHeight() const;

    // Compact layout, one bar (docs/MOBILE-LAYOUT.md §8, after Mull): New tab
    // is the home page, not a tab of its own. Shows it, reusing an open New
    // tab; on it, focuses the bar's search.
    Q_INVOKABLE void openHome();
    // Esc / phone Back: closes the switcher, or leaves the New tab for the
    // last tab. False when there was nothing to go back from.
    Q_INVOKABLE bool handleBack();
    // Open tabs, not counting New tabs.
    int contentTabCount() const;
    void closeAllTabs();

signals:
    void closeRequested(const QString& key);
    void currentKeyChanged(const QString& key);
    void newTabShown();
    void compactChanged(bool compact);
    // One bar: the bar's search on the New tab, and its Enter.
    void homeQueryChanged(const QString& text);
    void homeQueryAccepted();
    // The system status in the bar was clicked (BasecampOS: opens System).
    void statusClicked();

protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    int indexOf(const QString& key) const;
    void applyTheme();
    void updateStripWidth();
    void showMenu(int index, const QPoint& globalPos);
    void onCurrentChanged(int index);
    void applyAttention();
    void setCompact(bool on);
    void updateCompactBar();
    void rebuildSwitcher();
    void showTabsMenu(QWidget* anchor);
    // Two apps at once (BasecampOS): one tab shown in a pane beside the
    // current one, the divider between them draggable. The tab stays in the
    // strip; picking it focuses the pane. Not in the compact layout.
    void showBeside(int index);
    void backFromSide();

    QWidget* m_bar = nullptr;
    QToolButton* m_plus = nullptr;
    TabStrip* m_tabs = nullptr;
    QWidget* m_drag = nullptr;
    QWidget* m_buttons = nullptr;
    QToolButton* m_maxButton = nullptr;
    QStackedWidget* m_stack = nullptr;
    QSplitter* m_split = nullptr;           // m_stack | m_side
    QWidget* m_side = nullptr;              // the pane beside (showBeside)
    QWidget* m_sideContent = nullptr;
    QString m_sideKey;
    QWidget* m_newTabPage = nullptr;
    int m_newCounter = 0;
    QHash<QString, QWidget*> m_detached;    // key -> its own window
    QHash<QString, QString> m_titles;       // key -> tab title
    QHash<QString, QString> m_icons;        // key -> icon path
    bool m_watchingWindow = false;
    QStringList m_attention;                // keys with unread notifications
    bool m_compact = false;
    bool m_switcherOpen = false;
    // A tab is being taken out while the switcher is up (its × or Close all):
    // the current tab may move, and the switcher stays (onCurrentChanged).
    bool m_holdSwitcher = false;
    QWidget* m_bottom = nullptr;            // compact: the one bar
    QWidget* m_pill = nullptr;              // compact: the current tab, or the search on New tab
    QLineEdit* m_pillSearch = nullptr;
    QToolButton* m_tabsButton = nullptr;
    QString m_prevKey;                      // the tab shown before the current one
    QString m_returnKey;                    // the last tab that was not a New tab
    QWidget* m_switcher = nullptr;          // compact: open apps as cards
    QWidget* m_switcherGrid = nullptr;
    QLabel* m_switcherCount = nullptr;      // "N open apps"
    QLineEdit* m_switcherSearch = nullptr;  // shown with many apps
    QWidget* m_switcherFoot = nullptr;      // holds "Close all apps", pinned below the cards
    QToolButton* m_switcherCloseAll = nullptr;
};
