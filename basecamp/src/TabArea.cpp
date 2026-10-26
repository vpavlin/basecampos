#include "TabArea.h"

#include "AppTileIcon.h"

#include <QApplication>
#include <QTimer>
#include <QElapsedTimer>
#include <QTime>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QLabel>
#include <QDebug>
#include <QLineEdit>
#include <QContextMenuEvent>
#include <QAbstractButton>
#include <QWidgetAction>
#include <QPainterPath>
#include <QStyle>
#include <QHBoxLayout>
#include <QIcon>
#include <QMenu>
#include <QKeyEvent>
#include <functional>
#include <QCursor>
#include <QMouseEvent>
#include <QStackedWidget>
#include <QSplitter>
#include <QToolButton>
#include <QFontMetrics>
#include <QPainter>
#include <QUrl>
#include <QShowEvent>
#include <QVBoxLayout>
#include <QWindow>
#include <QGridLayout>
#include <QScrollArea>

#include <ThemeColors.h>

namespace {
constexpr int kBarHeight = 40;
constexpr int kTabHeight = 34;
constexpr int kTabMax = 240;
constexpr int kTabMin = 72;
const char* kKeyProp = "_tabKey";

QColor tok(const char* name, const char* fallback)
{
    return Logos::themeColor(QString::fromLatin1(name), QColor(QString::fromLatin1(fallback)));
}

QString css(const QColor& c)
{
    return QStringLiteral("rgba(%1,%2,%3,%4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(c.alphaF());
}

// No title as well as no icon is the Basecamp tab: the λ. Anything else is an
// app's tile, rounded like the QML ones (AppTileIcon.h).
QIcon iconFor(const QString& path, const QString& title = QString())
{
    if (path.isEmpty() && title.isEmpty())
        return QIcon(QStringLiteral(":/qt/qml/Basecamp/Icons/Basecamp/Icons/logos-lambda.svg"));
    return appTileIcon(path, title, tok("surfaceRaised", "#232323"), tok("text", "#FFFFFF"));
}

// The empty part of the bar: drag to move the window, double-click to maximise.
class DragArea : public QWidget
{
public:
    using QWidget::QWidget;

protected:
    void mousePressEvent(QMouseEvent* e) override
    {
        if (e->button() == Qt::LeftButton) {
            if (QWindow* w = window()->windowHandle())
                w->startSystemMove();
        }
    }
    void mouseDoubleClickEvent(QMouseEvent* e) override
    {
        if (e->button() != Qt::LeftButton)
            return;
        QWidget* top = window();
        if (top->isMaximized())
            top->showNormal();
        else
            top->showMaximized();
    }
};

// The bar's look, shared by the main window and torn-off windows.
QString barStyleSheet()
{
    const QColor bar = tok("backgroundSecondary", "#262626");
    const QColor page = tok("background", "#171717");
    const QColor hover = tok("backgroundTertiary", "#2E2E2E");
    const QColor text = tok("text", "#FFFFFF");
    const QColor muted = tok("textSecondary", "#A3A3A3");
    const QColor border = tok("border", "#333333");
    const QColor danger = tok("error", "#E5484D");
    return QString(
        "#basecampium\\.bar { background: %1; border-bottom: 1px solid %6; }"
        "QToolButton { background: transparent; border: none; color: %4; font-size: 16px; border-radius: 6px; }"
        "QToolButton:hover { background: %3; color: %5; }"
        "QToolButton#basecampium\\.newTab { font-size: 22px; border-radius: 8px; }"
        "QToolButton#basecampium\\.close { border-radius: 0; }"
        "QToolButton#basecampium\\.close:hover { background: %7; color: white; }"
        "QToolButton#basecampium\\.minimize, QToolButton#basecampium\\.maximize { border-radius: 0; }"
        "QTabBar { background: transparent; }"
        "QTabBar::tab { background: transparent; color: %4; padding: 0 8px; margin-right: 2px;"
        "  border-top-left-radius: 8px; border-top-right-radius: 8px; text-align: left; }"
        "QTabBar::tab:hover { background: %3; color: %5; }"
        "QTabBar::tab:selected { background: %2; color: %5; }"
        "QTabBar::close-button { image: url(:/qt/qml/Basecamp/Icons/Basecamp/Icons/close.svg); subcontrol-position: right;"
        "  border-radius: 4px; margin: 2px; }"
        "QTabBar::close-button:hover { background: %3; }"
        "QLabel#basecampium\\.tabTitle { color: %4; background: transparent; }"
        "QLabel#basecampium\\.tabTitle[active=\"true\"] { color: %5; }"
        "QTabBar::scroller { width: 24px; }"
        "QTabBar QToolButton { background: %1; }"
        "QToolButton#basecampium\\.dock { border-radius: 0; }"
        "#basecampium\\.bottomBar { background: %1; border-top: 1px solid %6; }"
        "#basecampium\\.bottomBar QToolButton { font-size: 24px; border-radius: 10px; }"
        "#basecampium\\.bottomBar QToolButton:checked { background: %3; color: %5; }"
        "QLineEdit#basecampium\\.barSearch { background: transparent; border: none; color: %5; font-size: 15px;"
        "  selection-background-color: %6; }"
        "QLabel#basecampium\\.title { color: %5; background: transparent; font-size: 14px; font-weight: 600; }"
        "QLabel#basecampium\\.switcherHeader { color: %4; background: transparent; font-size: 13px; }")
        .arg(css(bar), css(page), css(hover), css(muted), css(text), css(border), css(danger));
}

// Frameless windows resize from their edges: start a system resize when a
// press lands within a few pixels of `top`'s border; show the matching cursor.
bool edgeResize(QWidget* top, QObject* watched, QEvent* event, int& cursorState)
{
    const auto type = event->type();
    if (type != QEvent::MouseMove && type != QEvent::MouseButtonPress)
        return false;
    auto* w = qobject_cast<QWidget*>(watched);
    if (!w || w->window() != top || top->isMaximized() || top->isFullScreen())
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
        if (int(edges) != cursorState) {
            cursorState = int(edges);
            if (!edges) top->unsetCursor();
            else if (edges == (Qt::LeftEdge | Qt::TopEdge) || edges == (Qt::RightEdge | Qt::BottomEdge)) top->setCursor(Qt::SizeFDiagCursor);
            else if (edges == (Qt::RightEdge | Qt::TopEdge) || edges == (Qt::LeftEdge | Qt::BottomEdge)) top->setCursor(Qt::SizeBDiagCursor);
            else if (edges & (Qt::LeftEdge | Qt::RightEdge)) top->setCursor(Qt::SizeHorCursor);
            else top->setCursor(Qt::SizeVerCursor);
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
} // namespace

// Icon + title, left-aligned and elided, as the tab's left-side widget (Qt
// centres tab text and can't left-align it from a style sheet).
class TabStrip;
class TabLabel : public QWidget
{
public:
    TabLabel(const QIcon& icon, const QString& text, QWidget* parent)
        : QWidget(parent), m_text(text)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        auto* row = new QHBoxLayout(this);
        row->setContentsMargins(4, 0, 0, 0);
        row->setSpacing(8);
        m_icon = new QLabel(this);
        m_icon->setFixedSize(16, 16);
        m_label = new QLabel(this);
        m_label->setObjectName(QStringLiteral("basecampium.tabTitle"));
        row->addWidget(m_icon);
        row->addWidget(m_label, 1);
        setIcon(icon);
    }
    void setIcon(const QIcon& icon) { m_base = icon; render(); }
    // A dot on the icon: the tab's app has notifications the user hasn't seen.
    void setDot(bool on) { if (on != m_dot) { m_dot = on; render(); } }
    void refresh() { render(); }
    void setText(const QString& t) { m_text = t; elide(); }
    void setActive(bool on)
    {
        m_label->setProperty("active", on);
        m_label->style()->unpolish(m_label);
        m_label->style()->polish(m_label);
    }

protected:
    void resizeEvent(QResizeEvent* e) override { QWidget::resizeEvent(e); elide(); }

private:
    void render()
    {
        const qreal dpr = devicePixelRatioF();
        QPixmap pm = m_base.pixmap(QSize(16, 16), dpr);
        if (m_dot) {
            if (pm.isNull()) {
                pm = QPixmap(QSize(16, 16) * dpr);
                pm.setDevicePixelRatio(dpr);
                pm.fill(Qt::transparent);
            }
            QPainter p(&pm);
            p.setRenderHint(QPainter::Antialiasing);
            // A ring in the bar's colour keeps the dot readable on any icon.
            p.setPen(QPen(tok("backgroundSecondary", "#262626"), 1.5));
            p.setBrush(tok("primary", "#ED7B58"));
            p.drawEllipse(QRectF(9.5, 0.75, 6, 6));
        }
        m_icon->setPixmap(pm);
    }
    void elide()
    {
        const int w = qMax(0, width() - 4 - 16 - 8);
        m_label->setText(m_label->fontMetrics().elidedText(m_text, Qt::ElideRight, w));
    }
    QLabel* m_icon = nullptr;
    QLabel* m_label = nullptr;
    QString m_text;
    QIcon m_base;
    bool m_dot = false;
};

TabLabel* labelAt(QTabBar* bar, int i)
{
    return static_cast<TabLabel*>(bar->tabButton(i, QTabBar::LeftSide));
}

void setLabel(TabStrip* bar, int i, const QIcon& icon, const QString& text)
{
    if (TabLabel* l = labelAt(bar, i)) {
        l->setIcon(icon);
        l->setText(text);
    } else {
        auto* nl = new TabLabel(icon, text, bar);
        bar->setTabButton(i, QTabBar::LeftSide, nl);
    }
    bar->setTabText(i, QString());
    bar->setTabIcon(i, QIcon());
    // Room for the label: the tab's width less padding and the close button.
    if (TabLabel* l = labelAt(bar, i))
        l->setFixedWidth(qMax(24, bar->tabWidth() - 44));
}

// A torn-off tab: its own bar (icon, title, drag area, "back to the main window",
// window buttons) over the app's content.
class DetachedWindow : public QWidget
{
public:
    DetachedWindow(const QString& key, const QString& title, const QIcon& icon,
                   QWidget* content, QWidget* parent, std::function<void()> onDock)
        : QWidget(parent, Qt::Window), m_content(content), m_onDock(std::move(onDock))
    {
#ifndef Q_OS_MAC
        setWindowFlag(Qt::FramelessWindowHint, true);
#endif
        setObjectName(QStringLiteral("basecampium.detached"));
        setProperty(kKeyProp, key);
        setWindowTitle(title);
        setWindowIcon(icon);
        auto* col = new QVBoxLayout(this);
        col->setContentsMargins(0, 0, 0, 0);
        col->setSpacing(0);

        m_bar = new QWidget(this);
        m_bar->setObjectName(QStringLiteral("basecampium.bar"));
        m_bar->setAttribute(Qt::WA_StyledBackground);
        m_bar->setFixedHeight(kBarHeight);
        // Same layout as the main bar: a tab strip holding this app's tab.
        auto* row = new QHBoxLayout(m_bar);
        row->setContentsMargins(8, 6, 0, 0);
        row->setSpacing(4);
#ifdef Q_OS_MAC
        row->addSpacing(70);
#endif
        m_tabs = new TabStrip(m_bar);
        m_tabs->setObjectName(QStringLiteral("basecampium.tabs"));
        m_tabs->setMovable(false);
        m_tabs->setAvailableWidth(kTabMax);
        const int t = m_tabs->addTab(QString());
        m_tabs->setTabData(t, key);
        setLabel(m_tabs, t, icon, title);
        if (TabLabel* l = labelAt(m_tabs, t))
            l->setActive(true);
        connect(m_tabs, &QTabBar::tabCloseRequested, this, [this](int) { close(); });
        row->addWidget(m_tabs, 0, Qt::AlignBottom);
        row->addWidget(new DragArea(m_bar), 1);

        auto* dock = new QToolButton(m_bar);
        dock->setObjectName(QStringLiteral("basecampium.dock"));
        dock->setIcon(QIcon(QStringLiteral(":/qt/qml/Basecamp/Icons/Basecamp/Icons/to-tab.svg")));
        dock->setIconSize(QSize(16, 16));
        // Same size and look as the window buttons beside it.
        dock->setFixedSize(46, kBarHeight - 8);
        dock->setFocusPolicy(Qt::NoFocus);
        dock->setToolTip(QObject::tr("Back to the main window (Ctrl+Shift+D)"));
        connect(dock, &QToolButton::clicked, this, [this]() { m_onDock(); });
        row->addWidget(dock, 0, Qt::AlignTop);

#ifndef Q_OS_MAC
        auto makeButton = [this, row](const QString& text, const QString& name) {
            auto* b = new QToolButton(m_bar);
            b->setObjectName(name);
            b->setText(text);
            b->setFixedSize(46, kBarHeight - 8);
            b->setFocusPolicy(Qt::NoFocus);
            row->addWidget(b, 0, Qt::AlignTop);
            return b;
        };
        auto* minB = makeButton(QStringLiteral("\u2013"), QStringLiteral("basecampium.minimize"));
        auto* maxB = makeButton(QStringLiteral("\u25A1"), QStringLiteral("basecampium.maximize"));
        auto* closeB = makeButton(QStringLiteral("\u2715"), QStringLiteral("basecampium.close"));
        connect(minB, &QToolButton::clicked, this, [this]() { showMinimized(); });
        connect(maxB, &QToolButton::clicked, this, [this]() { if (isMaximized()) showNormal(); else showMaximized(); });
        connect(closeB, &QToolButton::clicked, this, [this]() { close(); });
        qApp->installEventFilter(this);
#endif
        col->addWidget(m_bar);
        col->addWidget(content, 1);
        content->show();
        applyStyle();
    }

    ~DetachedWindow() override { qApp->removeEventFilter(this); }

    QWidget* content() const { return m_content; }

    void applyStyle()
    {
        m_bar->setStyleSheet(barStyleSheet());
        QPalette p = palette();
        p.setColor(QPalette::Window, tok("background", "#171717"));
        setPalette(p);
        setAutoFillBackground(true);
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() == QEvent::KeyPress) {
            auto* w = qobject_cast<QWidget*>(watched);
            if (w && w->window() == this) {
                auto* ke = static_cast<QKeyEvent*>(event);
                const auto mods = ke->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier);
                if (mods == (Qt::ControlModifier | Qt::ShiftModifier) && ke->key() == Qt::Key_D) {
                    m_onDock();
                    return true;
                }
                if (mods == Qt::ControlModifier && ke->key() == Qt::Key_W) {
                    close();
                    return true;
                }
            }
        }
        return edgeResize(this, watched, event, m_edgeCursor);
    }

private:
    QWidget* m_bar = nullptr;
    TabStrip* m_tabs = nullptr;
    QWidget* m_content = nullptr;
    std::function<void()> m_onDock;
    int m_edgeCursor = 0;
};


// ── TabStrip ────────────────────────────────────────────────────────────────

TabStrip::TabStrip(QWidget* parent)
    : QTabBar(parent)
{
    setExpanding(false);
    setMovable(true);
    setTabsClosable(true);
    setElideMode(Qt::ElideRight);
    setUsesScrollButtons(true);
    setDocumentMode(true);
    setDrawBase(false);
    setIconSize(QSize(16, 16));
    setFocusPolicy(Qt::NoFocus);
}

void TabStrip::setAvailableWidth(int px)
{
    if (px == m_available)
        return;
    m_available = px;
    updateGeometry();
    // Re-run the size hints for every tab.
    if (count() > 0)
        setTabText(0, tabText(0));
}

int TabStrip::tabWidth() const
{
    const int n = qMax(1, count());
    return qBound(kTabMin, m_available / n, kTabMax);
}

QSize TabStrip::tabSizeHint(int) const
{
    return QSize(tabWidth(), kTabHeight);
}

QSize TabStrip::minimumTabSizeHint(int index) const
{
    return tabSizeHint(index);
}

void TabStrip::mousePressEvent(QMouseEvent* event)
{
    m_dragging = event->button() == Qt::LeftButton && tabAt(event->position().toPoint()) >= 0;
    if (m_dragging) {
        // Held still for a moment (a finger): the tab menu, as right-click.
        m_pressPos = event->position().toPoint();
        if (!m_longPress) {
            m_longPress = new QTimer(this);
            m_longPress->setSingleShot(true);
            m_longPress->setInterval(550);
            connect(m_longPress, &QTimer::timeout, this, [this]() {
                if (!m_dragging)
                    return;
                m_dragging = false;
                // End QTabBar's own press/move-drag first.
                QMouseEvent release(QEvent::MouseButtonRelease, QPointF(m_pressPos), mapToGlobal(QPointF(m_pressPos)),
                                    Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
                QTabBar::mouseReleaseEvent(&release);
                emit customContextMenuRequested(m_pressPos);
            });
        }
        m_longPress->start();
    }
    QTabBar::mousePressEvent(event);
}

void TabStrip::mouseMoveEvent(QMouseEvent* event)
{
    // Well outside the strip (above or below, or past the ends): tear the tab off.
    const QPoint p = event->position().toPoint();
    if (m_longPress && m_longPress->isActive() && (p - m_pressPos).manhattanLength() > 12)
        m_longPress->stop();
    if (m_dragging && (event->buttons() & Qt::LeftButton)
        && !rect().adjusted(-60, -50, 60, 50).contains(p)) {
        m_dragging = false;
        const int index = currentIndex();
        // End QTabBar's own move-drag first, so the strip is left consistent.
        QMouseEvent release(QEvent::MouseButtonRelease, event->position(), event->globalPosition(),
                            Qt::LeftButton, Qt::NoButton, event->modifiers());
        QTabBar::mouseReleaseEvent(&release);
        emit detachRequested(index, event->globalPosition().toPoint());
        return;
    }
    QTabBar::mouseMoveEvent(event);
}

void TabStrip::mouseReleaseEvent(QMouseEvent* event)
{
    m_dragging = false;
    if (m_longPress)
        m_longPress->stop();
    // Middle-click closes, as in a browser.
    if (event->button() == Qt::MiddleButton) {
        const int i = tabAt(event->position().toPoint());
        if (i >= 0) {
            emit tabCloseRequested(i);
            return;
        }
    }
    QTabBar::mouseReleaseEvent(event);
}

// ── Compact (phone-width) layout ────────────────────────────────────────────

// The bottom bar's apps button: the number of open apps in a rounded square,
// as on a phone browser's tab counter, with a dot when an app other than the
// current one has unread notifications.
class TabsButton : public QToolButton
{
public:
    explicit TabsButton(QWidget* parent) : QToolButton(parent)
    {
        // Long-press (or right-click): the apps menu, as on a phone browser's
        // tab counter. The press that opened it doesn't also toggle the switcher.
        m_hold.setSingleShot(true);
        m_hold.setInterval(500);
        QObject::connect(&m_hold, &QTimer::timeout, this, [this]() {
            m_held = true;
            setDown(false);
            if (onLongPress)
                onLongPress();
        });
    }
    void setCount(int n) { if (n != m_count) { m_count = n; update(); } }
    void setDot(bool on) { if (on != m_dot) { m_dot = on; update(); } }
    std::function<void()> onLongPress;

protected:
    void mousePressEvent(QMouseEvent* e) override
    {
        m_held = false;
        if (e->button() == Qt::LeftButton)
            m_hold.start();
        QToolButton::mousePressEvent(e);
    }
    void mouseReleaseEvent(QMouseEvent* e) override
    {
        m_hold.stop();
        if (m_held) {
            m_held = false;
            setDown(false);
            e->accept();
            return;
        }
        QToolButton::mouseReleaseEvent(e);
    }
    void contextMenuEvent(QContextMenuEvent* e) override
    {
        e->accept();
        if (onLongPress)
            onLongPress();
    }

    void paintEvent(QPaintEvent* e) override
    {
        QToolButton::paintEvent(e);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QColor fg = isChecked() ? tok("text", "#FFFFFF") : tok("textSecondary", "#A3A3A3");
        const QRectF box(width() / 2.0 - 11, height() / 2.0 - 11, 22, 22);
        p.setPen(QPen(fg, 1.8));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(box, 5, 5);
        QFont f = font();
        f.setPixelSize(m_count > 99 ? 9 : 12);
        f.setBold(true);
        p.setFont(f);
        p.drawText(box, Qt::AlignCenter, m_count > 99 ? QStringLiteral(":D") : QString::number(m_count));
        if (m_dot) {
            p.setPen(QPen(tok("backgroundSecondary", "#262626"), 1.5));
            p.setBrush(tok("primary", "#ED7B58"));
            p.drawEllipse(QRectF(box.right() - 4, box.top() - 4, 8, 8));
        }
    }

private:
    int m_count = 0;
    bool m_dot = false;
    QTimer m_hold;
    bool m_held = false;
};

// The one bar's pill (compact layout), like a phone browser's address bar: the
// current tab's icon and title. Tap: New tab. Swipe sideways: the previous or
// next tab. On the New tab it holds the search field instead.
class OneBarPill : public QWidget
{
public:
    explicit OneBarPill(QWidget* parent) : QWidget(parent)
    {
        setMinimumHeight(44);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover);
        setFocusPolicy(Qt::NoFocus);
    }
    void set(const QIcon& icon, const QString& text, bool searching)
    {
        m_icon = icon;
        m_text = text;
        m_searching = searching;
        setAccessibleName(searching ? tr("Search apps") : text);
        update();
    }
    std::function<void()> onTap;
    std::function<void(int)> onSwipe;   // +1 next tab, -1 previous
    std::function<void()> onSwipeUp;    // the tab switcher (IronFox: swipe the toolbar up)

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(m_searching ? QPen(tok("primary", "#ED7B58"), 1.5) : Qt::NoPen);
        p.setBrush(underMouse() && !m_searching ? tok("border", "#333333") : tok("backgroundTertiary", "#2E2E2E"));
        const QRectF r = QRectF(rect()).adjusted(1, 1, -1, -1);
        p.drawRoundedRect(r, r.height() / 2, r.height() / 2);
        const int s = 20;
        const QRect ir(14, (height() - s) / 2, s, s);
        if (m_searching) {
            // A drawn magnifier (the shell's icon set has none).
            p.setPen(QPen(tok("textSecondary", "#A3A3A3"), 2, Qt::SolidLine, Qt::RoundCap));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(QRectF(ir.left() + 2, ir.top() + 2, 12, 12));
            p.drawLine(QPointF(ir.left() + 12.5, ir.top() + 12.5), QPointF(ir.right() - 1, ir.bottom() - 1));
            return;   // the search field draws the rest
        }
        int x = 18;
        if (!m_icon.isNull()) {
            p.drawPixmap(ir, m_icon.pixmap(QSize(s, s), devicePixelRatioF()));
            x = ir.right() + 11;
        }
        QFont f = font();
        f.setPixelSize(15);
        p.setFont(f);
        p.setPen(tok("text", "#FFFFFF"));
        const QRect tr(x, 0, width() - x - 16, height());
        p.drawText(tr, Qt::AlignVCenter | Qt::AlignLeft,
                   QFontMetrics(f).elidedText(m_text, Qt::ElideRight, tr.width()));
    }
    void mousePressEvent(QMouseEvent* e) override
    {
        if (e->button() != Qt::LeftButton)
            return;
        m_down = true;
        m_swiping = false;
        m_swipingUp = false;
        m_press = e->position().toPoint();
    }
    void mouseMoveEvent(QMouseEvent* e) override
    {
        if (!m_down || m_swiping || m_swipingUp)
            return;
        const QPoint d = e->position().toPoint() - m_press;
        if (qAbs(d.x()) > 16 && qAbs(d.x()) > 2 * qAbs(d.y()))
            m_swiping = true;
        else if (d.y() < -16 && qAbs(d.y()) > 2 * qAbs(d.x()))
            m_swipingUp = true;
    }
    void mouseReleaseEvent(QMouseEvent* e) override
    {
        if (!m_down)
            return;
        m_down = false;
        const int dx = e->position().toPoint().x() - m_press.x();
        const int dy = e->position().toPoint().y() - m_press.y();
        if (m_swipingUp) {
            m_swipingUp = false;
            if (dy <= -40 && onSwipeUp)
                QTimer::singleShot(0, this, [this]() { if (onSwipeUp) onSwipeUp(); });
            return;
        }
        // Deferred: both change the tab, which updates this pill.
        if (m_swiping) {
            m_swiping = false;
            if (qAbs(dx) >= 48 && onSwipe) {
                const int dir = dx < 0 ? 1 : -1;   // swipe left: the next tab
                QTimer::singleShot(0, this, [this, dir]() { if (onSwipe) onSwipe(dir); });
            }
            return;
        }
        if (rect().contains(e->position().toPoint()) && onTap)
            QTimer::singleShot(0, this, [this]() { if (onTap) onTap(); });
    }

private:
    QIcon m_icon;
    QString m_text;
    bool m_searching = false;
    bool m_down = false;
    bool m_swiping = false;
    bool m_swipingUp = false;
    QPoint m_press;
};

// One open tab in the tab switcher: its app's icon large, its title, a close
// button. The whole card is the touch target; the close corner is 44 px.
class TabCard : public QWidget
{
public:
    TabCard(const QIcon& icon, const QString& title, bool current, bool dot, QWidget* parent)
        : QWidget(parent), m_icon(icon), m_title(title), m_current(current), m_dot(dot)
    {
        setFixedHeight(150);
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover);
    }
    std::function<void()> onOpen;
    std::function<void()> onClose;

protected:
    QRect closeRect() const { return QRect(width() - 44, 0, 44, 44); }
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF r = QRectF(rect()).adjusted(1, 1, -1, -1);
        p.setPen(m_current ? QPen(tok("primary", "#ED7B58"), 2) : QPen(tok("border", "#333333"), 1));
        p.setBrush(underMouse() ? tok("backgroundTertiary", "#2E2E2E") : tok("backgroundSecondary", "#262626"));
        p.drawRoundedRect(r, 12, 12);

        QFont f = font();
        f.setPixelSize(13);
        f.setBold(m_current);
        p.setFont(f);
        p.setPen(tok(m_current ? "text" : "textSecondary", "#FFFFFF"));
        const QRect titleRect(12, 0, width() - 12 - 44, 44);
        p.drawText(titleRect, Qt::AlignVCenter | Qt::AlignLeft,
                   QFontMetrics(f).elidedText(m_title, Qt::ElideRight, titleRect.width()));
        p.setPen(tok("textSecondary", "#A3A3A3"));
        f.setBold(false);
        f.setPixelSize(14);
        p.setFont(f);
        p.drawText(closeRect(), Qt::AlignCenter, QStringLiteral("✕"));

        const int s = 56;
        const QRect ir((width() - s) / 2, 44 + (height() - 44 - s) / 2 - 4, s, s);
        p.drawPixmap(ir, m_icon.pixmap(QSize(s, s), devicePixelRatioF()));
        if (m_dot) {
            p.setPen(QPen(tok("backgroundSecondary", "#262626"), 2));
            p.setBrush(tok("primary", "#ED7B58"));
            p.drawEllipse(QRectF(ir.right() - 8, ir.top() - 4, 12, 12));
        }
    }
    void mouseReleaseEvent(QMouseEvent* e) override
    {
        if (e->button() != Qt::LeftButton || !rect().contains(e->position().toPoint()))
            return;
        // Deferred: both callbacks rebuild the switcher, which deletes this card.
        auto cb = closeRect().contains(e->position().toPoint()) ? onClose : onOpen;
        if (cb)
            QTimer::singleShot(0, cb);
    }

private:
    QIcon m_icon;
    QString m_title;
    bool m_current;
    bool m_dot;
};


// ── System status (BasecampOS) ──────────────────────────────────────────────

// Time, battery, network and the Shrooms mesh at the right of the bar: on a
// device where Basecamp is the whole screen, nothing else shows them. Read
// from sysfs every 10 s (no network library in the shell, no animation).
// A click opens the system app (statusClicked).
class StatusCluster : public QWidget
{
public:
    explicit StatusCluster(QWidget* parent) : QWidget(parent)
    {
        setObjectName(QStringLiteral("basecampos.status"));
        setCursor(Qt::PointingHandCursor);
        setFixedHeight(32);
        auto* t = new QTimer(this);
        t->setInterval(10000);
        connect(t, &QTimer::timeout, this, [this]() { refresh(); });
        t->start();
        refresh();
    }
    std::function<void()> onClicked;

protected:
    void mouseReleaseEvent(QMouseEvent* e) override
    {
        if (e->button() == Qt::LeftButton && rect().contains(e->position().toPoint()) && onClicked)
            onClicked();
    }

    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QColor ink = tok("textSecondary", "#A3A3A3");
        const QColor on = tok("text", "#FFFFFF");
        const QColor good = tok("success", "#3DD68C");
        const QColor bad = tok("error", "#E5484D");
        QFont f = font();
        f.setPixelSize(12);
        p.setFont(f);
        const int cy = height() / 2;
        int x = 0;

        // Shrooms mesh: a dot and "mesh".
        p.setPen(Qt::NoPen);
        p.setBrush(m_mesh ? good : ink);
        p.drawEllipse(QPointF(x + 4, cy), 3.5, 3.5);
        p.setPen(m_mesh ? on : ink);
        p.drawText(QRect(x + 12, 0, 40, height()), Qt::AlignVCenter | Qt::AlignLeft, QStringLiteral("mesh"));
        x += 52;

        // Network: Wi-Fi bars (signal), a plug for wired, a cross for none.
        if (m_net == QLatin1String("wifi")) {
            const int bars = m_wifi < 0 ? 3 : (m_wifi >= 75 ? 4 : m_wifi >= 50 ? 3 : m_wifi >= 25 ? 2 : 1);
            for (int i = 0; i < 4; ++i) {
                const int h = 4 + i * 3;
                p.setPen(Qt::NoPen);
                p.setBrush(i < bars ? on : QColor(ink.red(), ink.green(), ink.blue(), 90));
                p.drawRoundedRect(QRectF(x + i * 5, cy + 7 - h, 3.5, h), 1, 1);
            }
        } else if (m_net == QLatin1String("wired")) {
            p.setPen(QPen(on, 1.6));
            p.drawRect(QRectF(x + 2, cy - 4, 14, 9));
            p.drawLine(QPointF(x + 6, cy + 5), QPointF(x + 6, cy + 8));
            p.drawLine(QPointF(x + 12, cy + 5), QPointF(x + 12, cy + 8));
        } else {
            p.setPen(QPen(bad, 1.8));
            p.drawLine(QPointF(x + 3, cy - 5), QPointF(x + 13, cy + 5));
            p.drawLine(QPointF(x + 13, cy - 5), QPointF(x + 3, cy + 5));
        }
        x += 30;

        // Battery: outline, fill by level, a bolt while charging, and %.
        if (m_battery >= 0) {
            const QColor fill = m_battery <= 15 && !m_charging ? bad : on;
            p.setPen(QPen(ink, 1.2));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(QRectF(x, cy - 6, 22, 12), 2.5, 2.5);
            p.setPen(Qt::NoPen);
            p.setBrush(ink);
            p.drawRect(QRectF(x + 22.5, cy - 2.5, 2, 5));
            p.setBrush(fill);
            p.drawRoundedRect(QRectF(x + 2, cy - 4, 18 * qBound(0, m_battery, 100) / 100.0, 8), 1.5, 1.5);
            QString label = QString::number(m_battery) + QLatin1Char('%');
            if (m_charging)
                label.prepend(QChar(0x26A1));
            p.setPen(on);
            p.drawText(QRect(x + 29, 0, 52, height()), Qt::AlignVCenter | Qt::AlignLeft, label);
            x += 80;
        }

        // Time.
        QFont tf = f;
        tf.setPixelSize(13);
        tf.setWeight(QFont::DemiBold);
        p.setFont(tf);
        p.setPen(on);
        p.drawText(QRect(x, 0, width() - x, height()), Qt::AlignVCenter | Qt::AlignLeft,
                   QTime::currentTime().toString(QStringLiteral("HH:mm")));
    }

private:
    static QString readFile(const QString& path)
    {
        QFile f(path);
        return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()).trimmed() : QString();
    }

    void refresh()
    {
        m_battery = -1;
        m_charging = false;
        const QDir ps(QStringLiteral("/sys/class/power_supply"));
        for (const QString& e : ps.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            const QString base = ps.filePath(e);
            if (readFile(base + "/type") != QLatin1String("Battery"))
                continue;
            bool ok = false;
            const int cap = readFile(base + "/capacity").toInt(&ok);
            if (!ok)
                continue;
            m_battery = cap;
            const QString st = readFile(base + "/status");
            m_charging = st == QLatin1String("Charging") || st == QLatin1String("Full");
            break;
        }

        m_net.clear();
        m_wifi = -1;
        m_mesh = false;
        const QDir net(QStringLiteral("/sys/class/net"));
        const QString wireless = readFile(QStringLiteral("/proc/net/wireless"));
        for (const QString& iface : net.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::System)) {
            if (iface == QLatin1String("lo"))
                continue;
            const QString base = net.filePath(iface);
            const QString oper = readFile(base + "/operstate");
            const bool up = oper == QLatin1String("up")
                || (oper == QLatin1String("unknown") && readFile(base + "/carrier") == QLatin1String("1"));
            if (iface.startsWith(QLatin1String("shrooms"))) {
                m_mesh = m_mesh || up || oper == QLatin1String("unknown");
            } else if (QFileInfo::exists(base + "/wireless")) {
                if (up) {
                    m_net = QStringLiteral("wifi");
                    // /proc/net/wireless: "wlan0: 0000   60.  -50.  ..." (link quality of 70)
                    for (const QString& line : wireless.split(QLatin1Char('\n'))) {
                        if (!line.trimmed().startsWith(iface + QLatin1Char(':')))
                            continue;
                        const QStringList cols = line.simplified().split(QLatin1Char(' '));
                        if (cols.size() > 2)
                            m_wifi = qBound(0, int(cols.at(2).toDouble() * 100 / 70), 100);
                    }
                }
            } else if (up && readFile(base + "/type") == QLatin1String("1") && m_net.isEmpty()) {
                m_net = QStringLiteral("wired");
            }
        }
        QStringList tip;
        tip << (m_mesh ? tr("Shrooms mesh: connected") : tr("Shrooms mesh: not connected"));
        tip << (m_net == QLatin1String("wifi") ? tr("Wi-Fi: connected (%1%)").arg(m_wifi)
                : m_net == QLatin1String("wired") ? tr("Wired network: connected") : tr("No network"));
        if (m_battery >= 0)
            tip << tr("Battery: %1%%2").arg(m_battery).arg(m_charging ? tr(", charging") : QString());
        tip << tr("Click for System");
        setToolTip(tip.join(QLatin1Char('\n')));
        setFixedWidth(52 + 30 + (m_battery >= 0 ? 80 : 0) + 44);
        update();
    }

    int m_battery = -1;
    bool m_charging = false;
    QString m_net;
    int m_wifi = -1;
    bool m_mesh = false;
};

// ── TabArea ─────────────────────────────────────────────────────────────────

static QIcon trashIcon(const QColor& tint);

TabArea::TabArea(QWidget* newTabPage, QWidget* parent)
    : QWidget(parent)
    , m_newTabPage(newTabPage)
{
    setObjectName(QStringLiteral("basecampium.tabArea"));
    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);

    m_bar = new QWidget(this);
    m_bar->setObjectName(QStringLiteral("basecampium.bar"));
    m_bar->setAttribute(Qt::WA_StyledBackground);
    m_bar->setFixedHeight(kBarHeight);
    auto* row = new QHBoxLayout(m_bar);
    row->setContentsMargins(8, 6, 0, 0);
    row->setSpacing(4);

#ifdef Q_OS_MAC
    // Room for the traffic lights Basecamp draws over the window's top-left.
    row->addSpacing(70);
#endif

    m_plus = new QToolButton(m_bar);
    m_plus->setObjectName(QStringLiteral("basecampium.newTab"));
    // A word with the "+": it is how another app opens, and a bare "+" read
    // as decoration (BasecampOS, on the Duet).
    m_plus->setText(tr("+  Apps"));
    m_plus->setToolTip(tr("Open another app (Ctrl+T)"));
    m_plus->setFixedSize(84, 32);
    m_plus->setFocusPolicy(Qt::NoFocus);
    connect(m_plus, &QToolButton::clicked, this, [this]() { addNewTab(); });

    // Tabs, then "+" right after the last tab (it moves as tabs open and close).
    m_tabs = new TabStrip(m_bar);
    m_tabs->setObjectName(QStringLiteral("basecampium.tabs"));
    m_tabs->setContextMenuPolicy(Qt::CustomContextMenu);
    row->addWidget(m_tabs, 0, Qt::AlignBottom);
    row->addWidget(m_plus, 0, Qt::AlignVCenter);

    m_drag = new DragArea(m_bar);
    m_drag->setObjectName(QStringLiteral("basecampium.dragArea"));
    row->addWidget(m_drag, 1);

    auto* status = new StatusCluster(m_bar);
    status->onClicked = [this]() { emit statusClicked(); };
    row->addWidget(status, 0, Qt::AlignVCenter);
    row->addSpacing(8);

#ifndef Q_OS_MAC
    m_buttons = new QWidget(m_bar);
    auto* br = new QHBoxLayout(m_buttons);
    br->setContentsMargins(0, 0, 0, 6);
    br->setSpacing(0);
    auto makeButton = [this, br](const QString& text, const QString& name) {
        auto* b = new QToolButton(m_buttons);
        b->setObjectName(name);
        b->setText(text);
        b->setFixedSize(46, 32);
        b->setFocusPolicy(Qt::NoFocus);
        br->addWidget(b);
        return b;
    };
    QToolButton* minB = makeButton(QStringLiteral("–"), QStringLiteral("basecampium.minimize"));
    m_maxButton = makeButton(QStringLiteral("□"), QStringLiteral("basecampium.maximize"));
    QToolButton* closeB = makeButton(QStringLiteral("✕"), QStringLiteral("basecampium.close"));
    connect(minB, &QToolButton::clicked, this, [this]() { window()->showMinimized(); });
    connect(m_maxButton, &QToolButton::clicked, this, [this]() {
        QWidget* top = window();
        if (top->isMaximized()) top->showNormal(); else top->showMaximized();
    });
    connect(closeB, &QToolButton::clicked, this, [this]() { window()->close(); });
    row->addWidget(m_buttons, 0, Qt::AlignTop);
#endif

    column->addWidget(m_bar);

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(m_newTabPage);
    // The current tab on the left; a tab shown beside it on the right.
    m_split = new QSplitter(Qt::Horizontal, this);
    m_split->setChildrenCollapsible(false);
    m_split->setHandleWidth(8);
    m_split->addWidget(m_stack);
    m_side = new QWidget(m_split);
    auto* sideLayout = new QVBoxLayout(m_side);
    sideLayout->setContentsMargins(0, 0, 0, 0);
    sideLayout->setSpacing(0);
    m_split->addWidget(m_side);
    m_side->hide();
    column->addWidget(m_split, 1);

    // Compact layout (narrower than kCompactWidth): one bar, at the bottom
    // within thumb reach, holding everything (docs/MOBILE-LAYOUT.md §8):
    // [ current tab · search ] [ apps ]. Notifications are in the home feed
    // (the pill opens it), so there is no bell; Applications, Packages and
    // Settings are apps like any other, so there is no menu either.
    // Its gaps move the window, like the desktop bar's empty part.
    m_bottom = new DragArea(this);
    m_bottom->setObjectName(QStringLiteral("basecampium.bottomBar"));
    m_bottom->setAttribute(Qt::WA_StyledBackground);
    m_bottom->setFixedHeight(kBottomBarHeight);
    auto* bottomRow = new QHBoxLayout(m_bottom);
    bottomRow->setContentsMargins(10, 6, 6, 6);
    bottomRow->setSpacing(2);

    auto* pill = new OneBarPill(m_bottom);
    pill->setObjectName(QStringLiteral("basecampium.pill"));
    pill->onTap = [this]() { openHome(); };
    pill->onSwipe = [this](int dir) { showSwitcher(false); nextTab(dir); };
    pill->onSwipeUp = [this]() { showSwitcher(true); };
    m_pill = pill;
    // On the New tab the pill is the search: the page below shows the results.
    m_pillSearch = new QLineEdit(pill);
    m_pillSearch->setObjectName(QStringLiteral("basecampium.barSearch"));
    setHomePlaceholder(QString());
    m_pillSearch->setFrame(false);
    m_pillSearch->setVisible(false);
    auto* pillRow = new QHBoxLayout(pill);
    pillRow->setContentsMargins(44, 0, 16, 0);
    pillRow->addWidget(m_pillSearch);
    connect(m_pillSearch, &QLineEdit::textChanged, this, &TabArea::homeQueryChanged);
    connect(m_pillSearch, &QLineEdit::returnPressed, this, &TabArea::homeQueryAccepted);

    auto* tabsButton = new TabsButton(m_bottom);
    tabsButton->setObjectName(QStringLiteral("basecampium.tabSwitcher"));
    tabsButton->setToolTip(tr("Apps"));
    tabsButton->setAccessibleName(tr("Apps"));
    tabsButton->setFixedSize(48, 48);
    tabsButton->setFocusPolicy(Qt::NoFocus);
    tabsButton->setCheckable(true);
    tabsButton->onLongPress = [this, tabsButton]() { showTabsMenu(tabsButton); };
    m_tabsButton = tabsButton;
    connect(tabsButton, &QToolButton::clicked, this, [this]() { showSwitcher(!m_switcherOpen); });

    bottomRow->addWidget(pill, 1);
    bottomRow->addSpacing(4);
    bottomRow->addWidget(tabsButton);
    m_bottom->setVisible(false);
    column->addWidget(m_bottom);

    // The app switcher: a header (count, or search with many apps) over the
    // cards, and "Close all apps" pinned below them, always in reach.
    auto* switcher = new QWidget(this);
    switcher->setObjectName(QStringLiteral("basecampium.switcher"));
    switcher->setAttribute(Qt::WA_StyledBackground);
    auto* switcherColumn = new QVBoxLayout(switcher);
    switcherColumn->setContentsMargins(0, 0, 0, 0);
    switcherColumn->setSpacing(0);
    auto* head = new QWidget(switcher);
    head->setMinimumHeight(50);
    auto* headRow = new QHBoxLayout(head);
    headRow->setContentsMargins(16, 6, 16, 0);
    headRow->setSpacing(8);
    m_switcherCount = new QLabel(head);
    m_switcherCount->setObjectName(QStringLiteral("basecampium.switcherHeader"));
    m_switcherSearch = new QLineEdit(head);
    m_switcherSearch->setObjectName(QStringLiteral("basecampium.switcherSearch"));
    m_switcherSearch->setPlaceholderText(tr("Search your apps"));
    m_switcherSearch->setAccessibleName(tr("Search your apps"));
    m_switcherSearch->setClearButtonEnabled(true);
    // The clear button in the shell's own ✕, not the platform's red one.
    if (auto* clear = m_switcherSearch->findChild<QAction*>(QStringLiteral("_q_qlineeditclearaction")))
        clear->setIcon(QIcon(QStringLiteral(":/qt/qml/Basecamp/Icons/Basecamp/Icons/close.svg")));
    m_switcherSearch->setVisible(false);
    connect(m_switcherSearch, &QLineEdit::textChanged, this, [this]() { rebuildSwitcher(); });
    headRow->addWidget(m_switcherCount);
    headRow->addWidget(m_switcherSearch, 1);
    headRow->addStretch(0);
    switcherColumn->addWidget(head);
    auto* scroll = new QScrollArea(switcher);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_switcherGrid = new QWidget(scroll);
    m_switcherGrid->setAutoFillBackground(false);
    scroll->setWidget(m_switcherGrid);
    switcherColumn->addWidget(scroll, 1);
    auto* foot = new QWidget(switcher);
    auto* footRow = new QHBoxLayout(foot);
    footRow->setContentsMargins(16, 8, 16, 12);
    m_switcherCloseAll = new QToolButton(foot);
    m_switcherCloseAll->setObjectName(QStringLiteral("basecampium.switcherCloseAll"));
    m_switcherCloseAll->setText(tr("Close all apps"));
    m_switcherCloseAll->setAccessibleName(tr("Close all apps"));
    m_switcherCloseAll->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_switcherCloseAll->setIconSize(QSize(22, 22));
    m_switcherCloseAll->setFixedHeight(48);
    m_switcherCloseAll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_switcherCloseAll->setFocusPolicy(Qt::NoFocus);
    m_switcherCloseAll->setCursor(Qt::PointingHandCursor);
    connect(m_switcherCloseAll, &QToolButton::clicked, this, [this]() {
        closeAllTabs();
        // Closing happens through the shell; refresh once it has.
        QTimer::singleShot(0, this, [this]() {
            if (m_switcherOpen)
                rebuildSwitcher();
            updateCompactBar();
        });
    });
    footRow->addWidget(m_switcherCloseAll);
    m_switcherFoot = foot;
    switcherColumn->addWidget(foot);
    m_switcher = switcher;
    m_stack->addWidget(m_switcher);

    connect(m_tabs, &QTabBar::currentChanged, this, &TabArea::onCurrentChanged);
    connect(m_tabs, &QTabBar::tabCloseRequested, this, [this](int i) {
        const QString key = m_tabs->tabData(i).toString();
        if (isNewTabKey(key)) {
            takeTab(key);
            return;
        }
        emit closeRequested(key);
    });
    connect(m_tabs, &TabStrip::detachRequested, this, &TabArea::detachTab);
    connect(m_tabs, &QWidget::customContextMenuRequested, this, [this](const QPoint& p) {
        const int i = m_tabs->tabAt(p);
        if (i >= 0)
            showMenu(i, m_tabs->mapToGlobal(p));
    });

    applyTheme();
    connect(Logos::ThemeNotifier::instance(), &Logos::ThemeNotifier::currentChanged,
            this, &TabArea::applyTheme);
    // Quitting can stall (and Restart now waits for it): don't leave torn-off
    // windows on screen meanwhile.
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this]() {
        for (QWidget* w : std::as_const(m_detached))
            w->hide();
    });
}

void TabArea::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (!m_watchingWindow && window() != this) {
        window()->installEventFilter(this);
        m_watchingWindow = true;
    }
}

void TabArea::applyTheme()
{
    const QColor bar = tok("backgroundSecondary", "#262626");
    const QColor page = tok("background", "#171717");
    const QColor hover = tok("backgroundTertiary", "#2E2E2E");
    const QColor text = tok("text", "#FFFFFF");
    const QColor muted = tok("textSecondary", "#A3A3A3");
    const QColor border = tok("border", "#333333");
    const QColor danger = tok("error", "#E5484D");

    m_bar->setStyleSheet(barStyleSheet());
    m_bottom->setStyleSheet(barStyleSheet());
    m_switcherCloseAll->setIcon(trashIcon(danger));
    m_switcher->setStyleSheet(barStyleSheet()
        + QStringLiteral("#basecampium\\.switcher, QScrollArea, QScrollArea > QWidget > QWidget { background: %1; }"
                         "QToolButton { border-radius: 22px; }"
                         "QToolButton#basecampium\\.switcherCloseAll { color: %5; border: 1px solid %5; border-radius: 24px;"
                         "  font-size: 15px; padding: 0 16px; }"
                         "QToolButton#basecampium\\.switcherCloseAll:hover { background: %2; color: %5; }"
                         "QLineEdit#basecampium\\.switcherSearch { background: %2; border: 1px solid %3; border-radius: 18px;"
                         "  padding: 0 14px; min-height: 36px; color: %4; font-size: 14px; }"
                         // A phone-like thin scroll bar, not the desktop one with arrows.
                         "QScrollBar:vertical { width: 6px; background: transparent; margin: 4px 2px 4px 0; }"
                         "QScrollBar::handle:vertical { background: %3; border-radius: 2px; min-height: 32px; }"
                         "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
                         "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }")
              .arg(css(page), css(hover), css(border), css(text), css(danger)));
    for (int i = 0; i < m_tabs->count(); ++i)
        if (TabLabel* l = labelAt(m_tabs, i))
            l->refresh();
    for (QWidget* w : std::as_const(m_detached))
        static_cast<DetachedWindow*>(w)->applyStyle();
    QPalette p = m_stack->palette();
    p.setColor(QPalette::Window, page);
    m_stack->setPalette(p);
    m_stack->setAutoFillBackground(true);
}

void TabArea::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    setCompact(width() < kCompactWidth);
    updateStripWidth();
}

void TabArea::setCompact(bool on)
{
    if (on == m_compact)
        return;
    m_compact = on;
    if (on)
        backFromSide();
    // Phone layout: one bar at the bottom holds everything; the top bar goes.
    m_bar->setVisible(!on);
    m_bottom->setVisible(on);
    if (!on)
        showSwitcher(false);
    updateCompactBar();
    emit compactChanged(on);
}

int TabArea::topBarHeight() const
{
    return m_compact ? 0 : m_bar->height();
}

int TabArea::contentTabCount() const
{
    int n = 0;
    for (int i = 0; i < m_tabs->count(); ++i)
        if (!isNewTabKey(m_tabs->tabData(i).toString()))
            ++n;
    return n;
}

int TabArea::bottomBarHeight() const
{
    return m_compact ? kBottomBarHeight : 0;
}

void TabArea::updateCompactBar()
{
    if (!m_compact)
        return;
    const int i = m_tabs->currentIndex();
    const QString key = i >= 0 ? m_tabs->tabData(i).toString() : QString();
    const bool home = isNewTabKey(key) && !m_switcherOpen;
    const QIcon icon = m_switcherOpen ? QIcon()
                     : iconFor(m_icons.value(key), isNewTabKey(key) ? QString() : m_titles.value(key));
    static_cast<OneBarPill*>(m_pill)->set(icon,
                                          m_switcherOpen ? tr("Apps") : m_titles.value(key, key), home);
    if (m_pillSearch->isVisible() != home) {
        m_pillSearch->setVisible(home);
        if (!home && m_pillSearch->hasFocus())
            m_pillSearch->clearFocus();
    }
    auto* tabs = static_cast<TabsButton*>(m_tabsButton);
    // The New tab is the home page, not a tab: it isn't counted.
    tabs->setCount(contentTabCount());
    bool dot = false;
    for (int t = 0; t < m_tabs->count(); ++t)
        if (t != i && m_attention.contains(m_tabs->tabData(t).toString()))
            dot = true;
    tabs->setDot(dot);
    tabs->setChecked(m_switcherOpen);
}

void TabArea::showSwitcher(bool on)
{
    if (on && !m_compact)
        return;
    if (on == m_switcherOpen) {
        if (on)
            rebuildSwitcher();
        return;
    }
    m_switcherOpen = on;
    if (!on && !m_switcherSearch->text().isEmpty()) {
        const QSignalBlocker block(m_switcherSearch);
        m_switcherSearch->clear();
    }
    if (on) {
        rebuildSwitcher();
        m_stack->setCurrentWidget(m_switcher);
        m_switcher->show();
        // Out-of-process app views draw over siblings unless hidden.
        for (int i = 0; i < m_stack->count(); ++i)
            if (QWidget* other = m_stack->widget(i); other != m_switcher && other->isVisible())
                other->hide();
        updateCompactBar();
    } else {
        onCurrentChanged(m_tabs->currentIndex());
    }
}

void TabArea::rebuildSwitcher()
{
    delete m_switcherGrid->layout();
    for (QWidget* w : m_switcherGrid->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly))
        w->deleteLater();

    auto* grid = new QGridLayout(m_switcherGrid);
    grid->setContentsMargins(16, 8, 16, 16);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(12);
    const int open = contentTabCount();
    m_switcherCount->setText(open == 0 ? tr("No open apps")
                             : open == 1 ? tr("1 open app") : tr("%1 open apps").arg(open));
    // With many apps, a search (IronFox's "Search your tabs"); the count makes room.
    const QString query = m_switcherSearch->text().trimmed();
    const bool searching = open > 8 || !query.isEmpty();
    m_switcherSearch->setVisible(searching);
    m_switcherCount->setVisible(!searching);
    m_switcherFoot->setVisible(open > 0);

    // New tabs are the home page, reached from the bar: not listed.
    const int current = m_tabs->currentIndex();
    int slot = 0;
    for (int i = 0; i < m_tabs->count(); ++i) {
        const QString key = m_tabs->tabData(i).toString();
        const bool newTab = isNewTabKey(key);
        if (newTab)
            continue;
        if (!query.isEmpty() && !m_titles.value(key, key).contains(query, Qt::CaseInsensitive))
            continue;
        auto* card = new TabCard(newTab ? iconFor(QString()) : iconFor(m_icons.value(key), m_titles.value(key, key)),
                                 newTab ? tr("Basecamp") : m_titles.value(key, key),
                                 i == current, !newTab && m_attention.contains(key), m_switcherGrid);
        card->setObjectName(QStringLiteral("basecampium.tabCard.") + key);
        card->setAccessibleName(newTab ? tr("Basecamp") : m_titles.value(key, key));
        card->onOpen = [this, key]() {
            const int idx = indexOf(key);
            if (idx >= 0)
                m_tabs->setCurrentIndex(idx);
            showSwitcher(false);
        };
        card->onClose = [this, key]() {
            if (isNewTabKey(key))
                takeTab(key);
            else
                emit closeRequested(key);
            // Closing happens through the shell; refresh once it has.
            QTimer::singleShot(0, this, [this]() {
                if (m_switcherOpen)
                    rebuildSwitcher();
                updateCompactBar();
            });
        };
        grid->addWidget(card, slot / 2, slot % 2);
        ++slot;
    }
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    grid->setRowStretch((slot + 1) / 2, 1);
}

void TabArea::updateStripWidth()
{
    int reserved = 8 + m_plus->width() + 8 + 120;  // margins, "+", minimum drag area
    if (m_buttons)
        reserved += m_buttons->sizeHint().width();
#ifdef Q_OS_MAC
    reserved += 70;
#endif
    const int avail = qMax(kTabMin, width() - reserved);
    m_tabs->setAvailableWidth(avail);
    m_tabs->setMaximumWidth(avail);
    for (int i = 0; i < m_tabs->count(); ++i)
        if (TabLabel* l = labelAt(m_tabs, i))
            l->setFixedWidth(qMax(24, m_tabs->tabWidth() - 44));
}

bool TabArea::eventFilter(QObject* watched, QEvent* event)
{
    // Torn-off windows follow the main window: hidden with it (closed to the
    // tray, quitting), shown again with it.
    if (watched == window() && watched != this
        && (event->type() == QEvent::Hide || event->type() == QEvent::Show)) {
        const bool show = event->type() == QEvent::Show;
        for (QWidget* w : std::as_const(m_detached))
            w->setVisible(show);
    }
    if (event->type() == QEvent::Close) {
        const QString key = watched->property(kKeyProp).toString();
        if (!key.isEmpty() && m_detached.value(key) == watched) {
            event->ignore();
            emit closeRequested(key);
            return true;
        }
    }
    if (event->type() == QEvent::Show && watched != window()) {
        auto* w = qobject_cast<QWidget*>(watched);
        if (w && w->parentWidget() == m_stack && m_stack->currentWidget() != w) {
            QTimer::singleShot(0, w, [this, w]() {
                if (m_stack->currentWidget() != w)
                    w->hide();
            });
        }
    }
    if (watched == window() && event->type() == QEvent::WindowStateChange && m_maxButton)
        m_maxButton->setText(window()->isMaximized() ? QStringLiteral("❒") : QStringLiteral("□"));
    return QWidget::eventFilter(watched, event);
}

int TabArea::indexOf(const QString& key) const
{
    for (int i = 0; i < m_tabs->count(); ++i)
        if (m_tabs->tabData(i).toString() == key)
            return i;
    return -1;
}

QString TabArea::addNewTab()
{
    const QString key = QStringLiteral("new:%1").arg(++m_newCounter);
    const int i = m_tabs->addTab(QString());
    m_tabs->setTabData(i, key);
    setLabel(m_tabs, i, iconFor(QString()), tr("Basecamp"));
    m_tabs->setTabToolTip(i, tr("Basecamp"));
    updateStripWidth();
    m_tabs->setCurrentIndex(i);
    onCurrentChanged(i);
    return key;
}

void TabArea::openContent(const QString& key, const QString& title, const QString& iconPath,
                          QWidget* content, const QString& replaceKey)
{
    if (int existing = indexOf(key); existing >= 0) {
        m_tabs->setCurrentIndex(existing);
        return;
    }
    if (QWidget* w = m_detached.value(key)) {
        w->raise();
        w->activateWindow();
        return;
    }
    if (content) {
        content->setProperty(kKeyProp, key);
        if (m_stack->indexOf(content) < 0) {
            m_stack->addWidget(content);
            // An app's view can show itself when it finishes loading, after
            // the user has moved to another tab; keep it hidden unless current.
            content->installEventFilter(this);
        }
    }
    int i = isNewTabKey(replaceKey) ? indexOf(replaceKey) : -1;
    if (i < 0)
        i = m_tabs->addTab(QString());
    m_tabs->setTabData(i, key);
    setLabel(m_tabs, i, iconFor(iconPath, title), title);
    m_titles[key] = title;
    m_icons[key] = iconPath;
    m_tabs->setTabToolTip(i, title);
    updateStripWidth();
    m_tabs->setCurrentIndex(i);
    onCurrentChanged(i);
}

void TabArea::activate(const QString& key)
{
    const int i = indexOf(key);
    if (i >= 0)
        m_tabs->setCurrentIndex(i);
    else if (QWidget* w = m_detached.value(key)) {
        w->raise();
        w->activateWindow();
    }
}

bool TabArea::hasKey(const QString& key) const
{
    return indexOf(key) >= 0 || m_detached.contains(key);
}

void TabArea::detachTab(int index, const QPoint& globalPos)
{
    if (index < 0 || index >= m_tabs->count() || m_tabs->count() < 2)
        return;  // the last tab stays: that is the main window
    const QString key = m_tabs->tabData(index).toString();
    if (isNewTabKey(key))
        return;
    if (key == m_sideKey)
        backFromSide();
    QWidget* content = contentFor(key);
    if (!content)
        return;
    m_stack->removeWidget(content);
    m_tabs->removeTab(index);
    updateStripWidth();
    onCurrentChanged(m_tabs->currentIndex());

    // Owned by this area, so it goes when Basecamp's window goes.
    auto* win = new DetachedWindow(key, m_titles.value(key, key), iconFor(m_icons.value(key), m_titles.value(key, key)),
                                   content, this, [this, key]() { attachTab(key); });
    const QSize size = window()->size() * 0.7;
    win->resize(size.expandedTo(QSize(720, 480)));
    win->move(globalPos - QPoint(win->width() / 3, 16));
    win->installEventFilter(this);
    m_detached.insert(key, win);
    win->show();
    emit currentKeyChanged(currentKey());
}

void TabArea::attachTab(const QString& key)
{
    auto* win = static_cast<DetachedWindow*>(m_detached.take(key));
    if (!win)
        return;
    QWidget* content = win->content();
    win->removeEventFilter(this);
    win->hide();
    if (content) {
        content->setParent(nullptr);
        content->setProperty(kKeyProp, key);
        m_stack->addWidget(content);
    }
    win->deleteLater();
    const int i = m_tabs->addTab(QString());
    m_tabs->setTabData(i, key);
    setLabel(m_tabs, i, iconFor(m_icons.value(key), m_titles.value(key, key)), m_titles.value(key, key));
    updateStripWidth();
    m_tabs->setCurrentIndex(i);
    onCurrentChanged(i);
    window()->raise();
    window()->activateWindow();
}

QWidget* TabArea::takeTab(const QString& key)
{
    if (QWidget* win = m_detached.take(key)) {
        QWidget* content = static_cast<DetachedWindow*>(win)->content();
        if (content) {
            content->setParent(nullptr);
            content->setProperty(kKeyProp, QVariant());
        }
        win->removeEventFilter(this);
        win->hide();
        win->deleteLater();
        return content;
    }
    const int i = indexOf(key);
    if (i < 0)
        return nullptr;
    if (!m_sideKey.isEmpty() && key == m_sideKey)
        backFromSide();
    // Every New tab shows the same page: closing one must not take it out of
    // the stack, or the other New tabs (and the page shown next) go blank.
    QWidget* content = isNewTabKey(key) ? nullptr : contentFor(key);
    if (content) {
        m_stack->removeWidget(content);
        content->setProperty(kKeyProp, QVariant());
    }
    // Closed from the switcher (its × or Close all): the switcher stays and
    // only loses that card, until the last app goes; then home.
    const bool keepSwitcher = m_switcherOpen;
    m_holdSwitcher = keepSwitcher;
    m_tabs->removeTab(i);
    m_holdSwitcher = false;
    updateStripWidth();
    if (keepSwitcher && contentTabCount() > 0) {
        rebuildSwitcher();
        updateCompactBar();
    } else if (keepSwitcher) {
        // The last app went: the switcher closes on home, its search
        // cleared as showSwitcher(false) would.
        m_switcherOpen = false;
        if (!m_switcherSearch->text().isEmpty()) {
            const QSignalBlocker block(m_switcherSearch);
            m_switcherSearch->clear();
        }
        int home = -1;
        for (int t = 0; t < m_tabs->count() && home < 0; ++t)
            if (isNewTabKey(m_tabs->tabData(t).toString()))
                home = t;
        if (home < 0)
            addNewTab();
        else if (home == m_tabs->currentIndex())
            onCurrentChanged(home);
        else
            m_tabs->setCurrentIndex(home);
    } else if (m_tabs->count() == 0) {
        addNewTab();
    } else {
        onCurrentChanged(m_tabs->currentIndex());
    }
    return content;
}

QString TabArea::keyFor(QWidget* content) const
{
    return content ? content->property(kKeyProp).toString() : QString();
}

QWidget* TabArea::contentFor(const QString& key) const
{
    if (isNewTabKey(key))
        return m_newTabPage;
    if (!m_sideKey.isEmpty() && key == m_sideKey)
        return m_sideContent;
    if (QWidget* win = m_detached.value(key))
        return static_cast<DetachedWindow*>(win)->content();
    for (int i = 0; i < m_stack->count(); ++i) {
        QWidget* w = m_stack->widget(i);
        if (w != m_newTabPage && w->property(kKeyProp).toString() == key)
            return w;
    }
    return nullptr;
}

QString TabArea::currentKey() const
{
    const int i = m_tabs->currentIndex();
    return i >= 0 ? m_tabs->tabData(i).toString() : QString();
}

QStringList TabArea::keys() const
{
    QStringList out;
    for (int i = 0; i < m_tabs->count(); ++i)
        out << m_tabs->tabData(i).toString();
    return out;
}

void TabArea::setTitle(const QString& key, const QString& title)
{
    const int i = indexOf(key);
    if (i >= 0)
        if (TabLabel* l = labelAt(m_tabs, i))
            l->setText(title);
}

void TabArea::onCurrentChanged(int index)
{
    for (int i = 0; i < m_tabs->count(); ++i)
        if (TabLabel* l = labelAt(m_tabs, i))
            l->setActive(i == index);
    applyAttention();
    if (m_switcherOpen && m_holdSwitcher) {
        // An app closed from the switcher: the current tab moved under it,
        // but the user is still picking. takeTab decides what shows next.
        updateCompactBar();
        return;
    }
    if (m_switcherOpen) {
        // A tab picked while the switcher is up (shortcut, notification) wins.
        m_switcherOpen = false;
    }
    updateCompactBar();
    if (index < 0)
        return;
    const QString key = m_tabs->tabData(index).toString();
    // Compact: the New tab is transient, like a phone browser's home page.
    // Leaving it for another tab closes it.
    if (m_compact && isNewTabKey(m_prevKey) && m_prevKey != key) {
        QTimer::singleShot(0, this, [this, left = m_prevKey]() {
            if (m_compact && indexOf(left) >= 0 && currentKey() != left)
                takeTab(left);
        });
    }
    // The tab shown beside: the left keeps what it shows; the pane gets focus.
    if (!m_sideKey.isEmpty() && key == m_sideKey) {
        if (m_sideContent)
            m_sideContent->setFocus();
        emit currentKeyChanged(key);
        return;
    }
    m_prevKey = key;
    if (!isNewTabKey(key))
        m_returnKey = key;
    QElapsedTimer showTimer;
    showTimer.start();
    if (QWidget* w = contentFor(key)) {
        if (m_stack->indexOf(w) < 0)
            m_stack->addWidget(w);
        m_stack->setCurrentWidget(w);
        // The stack only shows a page when its index changes; a page hidden
        // while it stayed current (below) would otherwise stay blank.
        w->show();
        for (int i = 0; i < m_stack->count(); ++i)
            if (QWidget* other = m_stack->widget(i); other != w && other->isVisible())
                other->hide();
        if (const qint64 ms = showTimer.elapsed(); ms > 50)
            qInfo().noquote() << "tab switch to" << key << ": showing the page" << ms << "ms";
        if (w == m_newTabPage) {
            emit newTabShown();
            if (m_compact) {
                m_pillSearch->clear();
                m_pillSearch->setFocus();
            }
        }
    }
    emit currentKeyChanged(key);
}

void TabArea::nextTab(int delta)
{
    const int n = m_tabs->count();
    if (n < 2)
        return;
    m_tabs->setCurrentIndex((m_tabs->currentIndex() + delta + n) % n);
}

void TabArea::goToTab(int index)
{
    const int n = m_tabs->count();
    if (n == 0)
        return;
    m_tabs->setCurrentIndex(index < 0 || index >= n ? n - 1 : index);
}

void TabArea::showMenu(int index, const QPoint& globalPos)
{
    const QString key = m_tabs->tabData(index).toString();
    QMenu menu(this);
    QAction* close = menu.addAction(tr("Close"));
    QAction* others = menu.addAction(tr("Close other apps"));
    QAction* right = menu.addAction(tr("Close apps to the right"));
    menu.addSeparator();
    QAction* undock = menu.addAction(tr("Move to new window"));
    undock->setEnabled(!isNewTabKey(key) && m_tabs->count() > 1);
    QAction* beside = nullptr;
    if (!m_compact) {
        beside = key == m_sideKey ? menu.addAction(tr("Back to tabs"))
                                  : menu.addAction(tr("Show beside"));
        beside->setEnabled(!isNewTabKey(key) && (key == m_sideKey || contentTabCount() > 1));
    }
    others->setEnabled(m_tabs->count() > 1);
    right->setEnabled(index < m_tabs->count() - 1);
    QAction* chosen = menu.exec(globalPos);
    if (!chosen)
        return;
    if (chosen == undock) {
        detachTab(index, QCursor::pos());
        return;
    }
    if (beside && chosen == beside) {
        if (key == m_sideKey)
            backFromSide();
        else
            showBeside(index);
        return;
    }
    QStringList victims;
    if (chosen == close) {
        victims << key;
    } else {
        for (int i = 0; i < m_tabs->count(); ++i) {
            if (i == index) continue;
            if (chosen == right && i < index) continue;
            victims << m_tabs->tabData(i).toString();
        }
    }
    for (const QString& k : victims) {
        if (isNewTabKey(k))
            takeTab(k);
        else
            emit closeRequested(k);
    }
}

void TabArea::setAttention(const QStringList& keys)
{
    if (keys == m_attention)
        return;
    m_attention = keys;
    applyAttention();
    updateCompactBar();
    if (m_switcherOpen)
        rebuildSwitcher();
}

void TabArea::applyAttention()
{
    // The current tab never shows a dot: its app is what the user is looking at.
    const int current = m_tabs->currentIndex();
    for (int i = 0; i < m_tabs->count(); ++i)
        if (TabLabel* l = labelAt(m_tabs, i))
            l->setDot(i != current && m_attention.contains(m_tabs->tabData(i).toString()));
}

void TabArea::openHome()
{
    if (m_switcherOpen)
        showSwitcher(false);
    const QString current = currentKey();
    if (isNewTabKey(current)) {
        m_pillSearch->setFocus();
        return;
    }
    for (const QString& k : keys())
        if (isNewTabKey(k)) {
            activate(k);
            return;
        }
    addNewTab();
}

bool TabArea::handleBack()
{
    if (!m_compact)
        return false;
    if (m_switcherOpen) {
        showSwitcher(false);
        return true;
    }
    if (isNewTabKey(currentKey()) && indexOf(m_returnKey) >= 0) {
        activate(m_returnKey);
        return true;
    }
    return false;
}

void TabArea::closeAllTabs()
{
    for (const QString& k : keys()) {
        if (isNewTabKey(k))
            continue;
        emit closeRequested(k);
    }
}

// Menu icons: the shell's SVGs tinted, or drawn where the set has none.
static QIcon menuIcon(const QString& svg, const std::function<void(QPainter&)>& draw = {},
                      QColor tint = QColor())
{
    constexpr int s = 24;
    const qreal dpr = qApp->devicePixelRatio();
    QPixmap pm(QSize(s, s) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    if (!tint.isValid())
        tint = tok("textSecondary", "#A3A3A3");
    if (!svg.isEmpty()) {
        QIcon(svg).paint(&p, QRect(0, 0, s, s));   // these SVGs carry their own padding
        p.setCompositionMode(QPainter::CompositionMode_SourceIn);
        p.fillRect(QRect(0, 0, s, s), tint);
    } else if (draw) {
        p.setPen(QPen(tint, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        draw(p);
    }
    return QIcon(pm);
}

static QIcon plusIcon()
{
    return menuIcon({}, [](QPainter& p) {
        p.drawLine(QPointF(12, 5), QPointF(12, 19));
        p.drawLine(QPointF(5, 12), QPointF(19, 12));
    });
}

static QIcon trashIcon(const QColor& tint)
{
    return menuIcon({}, [](QPainter& p) {
        p.drawLine(QPointF(4.5, 7), QPointF(19.5, 7));          // lid
        p.drawLine(QPointF(10, 4), QPointF(14, 4));              // handle
        QPainterPath body;
        body.moveTo(6.5, 7);
        body.lineTo(7.5, 19);
        body.quadTo(7.7, 20.5, 9.2, 20.5);
        body.lineTo(14.8, 20.5);
        body.quadTo(16.3, 20.5, 16.5, 19);
        body.lineTo(17.5, 7);
        p.drawPath(body);
        p.drawLine(QPointF(10.5, 10.5), QPointF(10.5, 17));
        p.drawLine(QPointF(13.5, 10.5), QPointF(13.5, 17));
    }, tint);
}

static QIcon appIcon(const char* svg)
{
    return menuIcon(QStringLiteral(":/qt/qml/Basecamp/Icons/Basecamp/Icons/") + QLatin1String(svg));
}

// A menu row (icon, label) drawn as a widget, for what a QMenu item can't do:
// destructive actions draw in the theme's error colour.
class MenuRow : public QAbstractButton
{
public:
    MenuRow(const QIcon& icon, const QString& text, bool danger, QWidget* parent)
        : QAbstractButton(parent), m_danger(danger)
    {
        setIcon(icon);
        setText(text);
        setAccessibleName(text);
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover);
        setFocusPolicy(Qt::NoFocus);
        setFixedHeight(52);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
    QSize sizeHint() const override { return QSize(260, height()); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        p.setPen(Qt::NoPen);
        if (isDown() || underMouse()) {
            p.setBrush(tok("backgroundTertiary", "#2E2E2E"));
            p.drawRoundedRect(r, 12, 12);
        }
        QFont f = font();
        icon().paint(&p, QRect(16, (height() - 22) / 2, 22, 22));
        f.setPixelSize(15);
        p.setFont(f);
        p.setPen(m_danger ? tok("error", "#E5484D") : tok("text", "#FFFFFF"));
        const QRect tr(52, 0, width() - 64, height());
        p.drawText(tr, Qt::AlignVCenter | Qt::AlignLeft,
                   QFontMetrics(f).elidedText(text(), Qt::ElideRight, tr.width()));
    }

private:
    bool m_danger;
};

// A themed dropdown with touch-size rows (a bare QMenu is a light desktop menu).
static QMenu* styledMenu(QWidget* parent, const QString& name)
{
    auto* menu = new QMenu(parent);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->setAttribute(Qt::WA_TranslucentBackground);
    menu->setWindowFlags(menu->windowFlags() | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    menu->setObjectName(name);
    menu->setStyleSheet(QStringLiteral(
        "QMenu { background: %1; border: 1px solid %2; border-radius: 14px; padding: 6px; icon-size: 22px; }"
        "QMenu::item { color: %3; font-size: 15px; padding: 14px 20px 14px 14px; border-radius: 10px; }"
        "QMenu::item:selected { background: %4; }"
        "QMenu::icon { padding-left: 12px; }"
        "QMenu::separator { height: 1px; background: %2; margin: 6px 10px; }")
        .arg(css(tok("backgroundSecondary", "#262626")), css(tok("border", "#333333")),
             css(tok("text", "#FFFFFF")), css(tok("backgroundTertiary", "#2E2E2E"))));
    menu->setMinimumWidth(240);
    return menu;
}

static void addMenuAction(QMenu* menu, const char* name, const QIcon& icon, const QString& text,
                          std::function<void()> fn)
{
    QAction* a = menu->addAction(icon, text);
    a->setObjectName(QString::fromLatin1(name));
    QObject::connect(a, &QAction::triggered, menu, [fn]() { QTimer::singleShot(0, fn); });
}

// A destructive item: a QMenu can't colour one action, so it's a row widget.
static void addDangerAction(QMenu* menu, const char* name, const QString& text, std::function<void()> fn)
{
    auto* holder = new QWidget(menu);
    auto* l = new QVBoxLayout(holder);
    l->setContentsMargins(0, 0, 0, 0);
    auto* b = new MenuRow(trashIcon(tok("error", "#E5484D")), text, true, holder);
    b->setObjectName(QString::fromLatin1(name));
    l->addWidget(b);
    auto* wa = new QWidgetAction(menu);
    wa->setDefaultWidget(holder);
    menu->addAction(wa);
    QObject::connect(b, &QAbstractButton::clicked, menu, [menu, fn]() {
        menu->close();
        QTimer::singleShot(0, fn);
    });
}

// Test hook: a headless compositor doesn't map popups; save what was shown.
static void saveForTests(QWidget* w)
{
    const QString shot = qEnvironmentVariable("BASECAMP_SHELL_MENU_SHOT");
    if (!shot.isEmpty())
        w->grab().save(shot);
}

void TabArea::showTabsMenu(QWidget* anchor)
{
    // The app count's long-press menu: opens upward from its button,
    // right-aligned. Items that don't apply are left out, not greyed.
    QMenu* menu = styledMenu(this, QStringLiteral("basecampium.tabsMenu"));
    const QString current = currentKey();
    const bool onTab = !current.isEmpty() && !isNewTabKey(current);
    addMenuAction(menu, "menu.newTab", plusIcon(), tr("Open Basecamp"), [this]() { openHome(); });
    if (onTab)
        addMenuAction(menu, "menu.closeTab", appIcon("close.svg"), tr("Close app"), [this, current]() {
            if (indexOf(current) >= 0)
                emit closeRequested(current);
        });
    if (contentTabCount() > 1 || (contentTabCount() == 1 && !onTab))
        addDangerAction(menu, "menu.closeAll", tr("Close all apps"), [this]() { closeAllTabs(); });
    menu->ensurePolished();
    const QSize hint = menu->sizeHint();
    const QPoint corner = anchor->mapToGlobal(QPoint(anchor->width(), 0));
    menu->popup(QPoint(corner.x() - hint.width(), corner.y() - hint.height() - 6));
    qInfo() << "Basecampium: menu" << menu->objectName() << "shown" << menu->isVisible();
    saveForTests(menu);
}

void TabArea::setHomePlaceholder(const QString& text)
{
    if (!m_pillSearch) return;
    m_pillSearch->setPlaceholderText(text.isEmpty() ? tr("Search apps…") : text);
    m_pillSearch->setAccessibleName(text.isEmpty() ? tr("Search apps") : text);
}

// ── Two apps at once ────────────────────────────────────────────────────────

void TabArea::showBeside(int index)
{
    if (m_compact || index < 0 || index >= m_tabs->count())
        return;
    const QString key = m_tabs->tabData(index).toString();
    if (isNewTabKey(key) || key == m_sideKey)
        return;
    backFromSide();
    QWidget* content = contentFor(key);
    if (!content)
        return;
    // The left goes on with another app (or New tab) if this one was shown there.
    const bool wasCurrent = currentKey() == key;
    m_stack->removeWidget(content);
    m_side->layout()->addWidget(content);
    content->show();
    m_sideKey = key;
    m_sideContent = content;
    m_side->show();
    const int w = m_split->width();
    m_split->setSizes({ w / 2, w - w / 2 });
    if (wasCurrent) {
        int left = -1;
        for (int i = 0; i < m_tabs->count() && left < 0; ++i)
            if (const QString k = m_tabs->tabData(i).toString(); k != key && !isNewTabKey(k))
                left = i;
        if (left >= 0) {
            m_tabs->setCurrentIndex(left);
            onCurrentChanged(left);
        } else {
            m_stack->setCurrentWidget(m_newTabPage);
            m_newTabPage->show();
        }
    }
}

void TabArea::backFromSide()
{
    if (m_sideKey.isEmpty())
        return;
    const QString key = m_sideKey;
    QWidget* content = m_sideContent;
    m_sideKey.clear();
    m_sideContent = nullptr;
    m_side->hide();
    if (content) {
        m_side->layout()->removeWidget(content);
        m_stack->addWidget(content);
        content->hide();
    }
    // Its tab was the one picked: show it on the left now.
    if (currentKey() == key)
        onCurrentChanged(m_tabs->currentIndex());
}
