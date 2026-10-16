#pragma once

#include <QMdiArea>
#include <QMdiSubWindow>
#include <QPixmap>
#include <QPointer>

class WindowFrame;
class WindowTitleBar;

// One app on the desktop layout: a frameless QMdiSubWindow with a Basecamp
// title bar (icon, name, minimise / maximise / close) drawn from theme tokens.
// `key` names it: a module name for an app, or "sys:<name>" for the shell's
// own Applications / Package Manager / Settings.
class AppWindow : public QMdiSubWindow
{
    Q_OBJECT

public:
    AppWindow(const QString& key, const QString& title, const QString& iconPath,
              QWidget* content, QWidget* parent = nullptr);

    QString key() const { return m_key; }
    QWidget* content() const { return m_content; }

    bool isMaximizedOnDesktop() const { return m_maximized; }
    // Maximised fills the desktop above the dock, not the whole area, so it
    // is tracked here rather than with Qt's own window state.
    void setMaximizedOnDesktop(bool on);
    void toggleMaximized() { setMaximizedOnDesktop(!m_maximized); }
    void setActiveLook(bool active);

    // Gives the content back to its owner (the host deletes app widgets
    // itself) so deleting this window doesn't take the widget with it.
    QWidget* releaseContent();

signals:
    void closeRequested(const QString& key);
    void minimizeRequested(const QString& key);

protected:
    void closeEvent(QCloseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    QString m_key;
    QPointer<QWidget> m_content;
    WindowFrame* m_frame = nullptr;
    WindowTitleBar* m_titleBar = nullptr;
    bool m_maximized = false;
    bool m_active = false;
    QRect m_normalGeometry;
};

// The desktop: a wallpaper with app windows on it. The dock floats above it
// (MainContainer owns that); `bottomReserve` keeps maximised windows clear of it.
class DesktopArea : public QMdiArea
{
    Q_OBJECT

public:
    explicit DesktopArea(QWidget* parent = nullptr);

    // An image file or qrc path; empty = plain (the theme's background colour).
    void setWallpaper(const QString& path);
    void setBottomReserve(int px);
    QRect maximizedRect() const;

    AppWindow* window(const QString& key) const;
    AppWindow* windowFor(const QWidget* content) const;
    QList<AppWindow*> appWindows() const;

    // Opens a new window for `content`, or returns the existing one for `key`.
    AppWindow* openWindow(const QString& key, const QString& title,
                          const QString& iconPath, QWidget* content);
    void restoreAndRaise(AppWindow* w);
    void minimize(AppWindow* w);
    // Removes the window, detaching its content first (see releaseContent).
    void removeWindow(AppWindow* w);

    QStringList openKeys() const;
    QString activeKey() const;

signals:
    void windowsChanged();
    void closeRequested(const QString& key);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void onActivated(QMdiSubWindow* w);

    QPixmap m_wallpaper;
    QPixmap m_scaled;
    int m_reserve = 0;
    int m_cascade = 0;
};

// The desktop's top bar: the λ and "Logos Basecamp" on the left; a search
// hint (opens Spotlight) and the clock on the right.
class DesktopStatusBar : public QWidget
{
    Q_OBJECT

public:
    static constexpr int kHeight = 30;
    explicit DesktopStatusBar(QWidget* parent = nullptr);

signals:
    void searchClicked();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QRect searchRect() const;
    QPixmap m_lambda;
    bool m_searchHover = false;
};
