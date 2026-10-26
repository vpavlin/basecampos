#pragma once

#include <QHash>
#include <QStringList>
#include <QTabBar>
#include <QWidget>

class QStackedWidget;
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
    // Puts a torn-off window's app back as a tab ("Back to tabs").
    void attachTab(const QString& key);

    void nextTab(int delta);
    void goToTab(int index);  // -1 = last

signals:
    void closeRequested(const QString& key);
    void currentKeyChanged(const QString& key);
    void newTabShown();

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

    QWidget* m_bar = nullptr;
    QToolButton* m_plus = nullptr;
    TabStrip* m_tabs = nullptr;
    QWidget* m_drag = nullptr;
    QWidget* m_buttons = nullptr;
    QToolButton* m_maxButton = nullptr;
    QStackedWidget* m_stack = nullptr;
    QWidget* m_newTabPage = nullptr;
    int m_newCounter = 0;
    QHash<QString, QWidget*> m_detached;    // key -> its own window
    QHash<QString, QString> m_titles;       // key -> tab title
    QHash<QString, QString> m_icons;        // key -> icon path
    bool m_watchingWindow = false;
};
