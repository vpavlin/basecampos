#include "DesktopArea.h"

#include <QCloseEvent>
#include <QIcon>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QVBoxLayout>

#include <ThemeColors.h>

namespace {

QColor tok(const char* name, const char* fallback)
{
    return Logos::themeColor(QString::fromLatin1(name), QColor(QString::fromLatin1(fallback)));
}

} // namespace

// ── Title bar ───────────────────────────────────────────────────────────────

// Icon and name on the left, minimise / maximise / close circles on the right.
// Drag to move the window, double-click to maximise.
class WindowTitleBar : public QWidget
{
public:
    static constexpr int kHeight = 36;

    enum Button { None = -1, Minimize = 0, Maximize = 1, Close = 2 };

    WindowTitleBar(AppWindow* window, const QString& title, const QString& iconPath)
        : QWidget(nullptr), m_window(window), m_title(title)
    {
        setFixedHeight(kHeight);
        setMouseTracking(true);
        const QIcon icon(iconPath);
        if (!iconPath.isEmpty() && !icon.isNull())
            m_icon = icon.pixmap(QSize(18, 18), devicePixelRatioF());
    }

    void setActive(bool active) { m_active = active; update(); }

protected:
    QRect buttonRect(int b) const
    {
        const int d = 14, gap = 10, right = 14;
        const int x = width() - right - d - (2 - b) * (d + gap);
        return QRect(x, (height() - d) / 2, d, d);
    }

    int buttonAt(const QPoint& p) const
    {
        for (int b = Minimize; b <= Close; ++b)
            if (buttonRect(b).adjusted(-4, -4, 4, 4).contains(p))
                return b;
        return None;
    }

    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), m_active ? tok("backgroundSecondary", "#262626")
                                    : tok("background", "#171717"));

        int x = 12;
        if (!m_icon.isNull()) {
            p.drawPixmap(QRect(x, (height() - 18) / 2, 18, 18), m_icon);
            x += 26;
        }
        QFont f = font();
        f.setPixelSize(13);
        f.setWeight(QFont::Medium);
        p.setFont(f);
        p.setPen(m_active ? tok("text", "#ffffff") : tok("textSecondary", "#a0a0a0"));
        const QRect textRect(x, 0, buttonRect(Minimize).left() - x - 12, height());
        p.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft,
                   p.fontMetrics().elidedText(m_title, Qt::ElideRight, textRect.width()));

        for (int b = Minimize; b <= Close; ++b) {
            const QRectF r = buttonRect(b);
            const bool hot = m_hover == b;
            QColor fill = tok("surfaceRaised", "#333333");
            QColor glyph = tok("textSecondary", "#a0a0a0");
            if (hot) {
                fill = b == Close ? tok("error", "#e5484d") : tok("accentOrange", "#ff8800");
                glyph = tok("accentForeground", "#171717");
            }
            p.setPen(Qt::NoPen);
            p.setBrush(fill);
            p.drawEllipse(r);
            p.setPen(QPen(glyph, 1.4, Qt::SolidLine, Qt::RoundCap));
            p.setBrush(Qt::NoBrush);
            const QPointF c = r.center();
            if (b == Minimize) {
                p.drawLine(QPointF(c.x() - 3.5, c.y()), QPointF(c.x() + 3.5, c.y()));
            } else if (b == Maximize) {
                p.drawRect(QRectF(c.x() - 3, c.y() - 3, 6, 6));
            } else {
                p.drawLine(QPointF(c.x() - 3, c.y() - 3), QPointF(c.x() + 3, c.y() + 3));
                p.drawLine(QPointF(c.x() + 3, c.y() - 3), QPointF(c.x() - 3, c.y() + 3));
            }
        }
    }

    void mouseMoveEvent(QMouseEvent* e) override
    {
        if (m_dragging) {
            if (m_window->isMaximizedOnDesktop()) {
                // Pull a maximised window out under the cursor, keeping the
                // grab point in proportion.
                const qreal ratio = qreal(e->position().x()) / qMax(1, width());
                m_window->setMaximizedOnDesktop(false);
                m_grab = QPoint(int(ratio * m_window->width()), int(e->position().y()));
            }
            const QPoint parentPos = m_window->parentWidget()
                ? m_window->parentWidget()->mapFromGlobal(e->globalPosition().toPoint())
                : e->globalPosition().toPoint();
            QPoint to = parentPos - m_grab;
            to.setY(qMax(0, to.y()));
            m_window->move(to);
            return;
        }
        const int h = buttonAt(e->position().toPoint());
        if (h != m_hover) {
            m_hover = h;
            setCursor(h == None ? Qt::ArrowCursor : Qt::PointingHandCursor);
            update();
        }
    }

    void leaveEvent(QEvent*) override
    {
        m_hover = None;
        update();
    }

    void mousePressEvent(QMouseEvent* e) override
    {
        if (e->button() != Qt::LeftButton)
            return;
        m_pressed = buttonAt(e->position().toPoint());
        if (m_pressed == None) {
            m_dragging = true;
            m_grab = e->position().toPoint();
        }
        if (QMdiArea* area = m_window->mdiArea())
            area->setActiveSubWindow(m_window);
        m_window->raise();
    }

    void mouseReleaseEvent(QMouseEvent* e) override
    {
        m_dragging = false;
        const int b = buttonAt(e->position().toPoint());
        if (b != None && b == m_pressed) {
            if (b == Minimize)
                emit m_window->minimizeRequested(m_window->key());
            else if (b == Maximize)
                m_window->toggleMaximized();
            else
                emit m_window->closeRequested(m_window->key());
        }
        m_pressed = None;
    }

    void mouseDoubleClickEvent(QMouseEvent* e) override
    {
        if (buttonAt(e->position().toPoint()) == None)
            m_window->toggleMaximized();
    }

private:
    AppWindow* m_window;
    QString m_title;
    QPixmap m_icon;
    bool m_active = false;
    bool m_dragging = false;
    QPoint m_grab;
    int m_hover = None;
    int m_pressed = None;
};

// ── Window frame ────────────────────────────────────────────────────────────

// A thin frame around the title bar and the app: the window's border, and the
// handle for resizing from any edge or corner.
class WindowFrame : public QWidget
{
public:
    static constexpr int kMargin = 4;      // frame thickness
    static constexpr int kCorner = 14;     // corner grab size along each edge

    explicit WindowFrame(AppWindow* window) : QWidget(nullptr), m_window(window)
    {
        setMouseTracking(true);
    }

    void setActive(bool active) { m_active = active; update(); }

protected:
    Qt::Edges edgesAt(const QPoint& p) const
    {
        const int m = kMargin + 2;
        const bool nearL = p.x() < m, nearR = p.x() >= width() - m;
        const bool nearT = p.y() < m, nearB = p.y() >= height() - m;
        // Near a corner, either edge band grabs both edges.
        const bool inL = p.x() < kCorner, inR = p.x() >= width() - kCorner;
        const bool inT = p.y() < kCorner, inB = p.y() >= height() - kCorner;
        Qt::Edges e;
        if (nearL || ((nearT || nearB) && inL)) e |= Qt::LeftEdge;
        if (nearR || ((nearT || nearB) && inR)) e |= Qt::RightEdge;
        if (nearT || ((nearL || nearR) && inT)) e |= Qt::TopEdge;
        if (nearB || ((nearL || nearR) && inB)) e |= Qt::BottomEdge;
        return e;
    }

    static Qt::CursorShape cursorFor(Qt::Edges e)
    {
        const bool l = e & Qt::LeftEdge, r = e & Qt::RightEdge, t = e & Qt::TopEdge, b = e & Qt::BottomEdge;
        if ((l && t) || (r && b)) return Qt::SizeFDiagCursor;
        if ((r && t) || (l && b)) return Qt::SizeBDiagCursor;
        if (l || r) return Qt::SizeHorCursor;
        if (t || b) return Qt::SizeVerCursor;
        return Qt::ArrowCursor;
    }

    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.fillRect(rect(), m_active ? tok("backgroundSecondary", "#262626") : tok("background", "#171717"));
        p.setPen(m_active ? tok("borderStrong", "#4a4a4a") : tok("border", "#333333"));
        p.drawRect(rect().adjusted(0, 0, -1, -1));
    }

    void mousePressEvent(QMouseEvent* e) override
    {
        if (e->button() != Qt::LeftButton)
            return;
        m_edges = edgesAt(e->position().toPoint());
        if (!m_edges)
            return;
        if (m_window->isMaximizedOnDesktop())
            m_window->setMaximizedOnDesktop(false);
        m_pressGlobal = e->globalPosition().toPoint();
        m_startGeometry = m_window->geometry();
        if (QMdiArea* area = m_window->mdiArea())
            area->setActiveSubWindow(m_window);
    }

    void mouseMoveEvent(QMouseEvent* e) override
    {
        if (!m_edges) {
            setCursor(cursorFor(edgesAt(e->position().toPoint())));
            return;
        }
        const QPoint d = e->globalPosition().toPoint() - m_pressGlobal;
        QRect g = m_startGeometry;
        const int minW = 420, minH = 260;
        if (m_edges & Qt::LeftEdge)   g.setLeft(qMin(g.left() + d.x(), g.right() - minW));
        if (m_edges & Qt::RightEdge)  g.setRight(qMax(g.right() + d.x(), g.left() + minW));
        if (m_edges & Qt::TopEdge)    g.setTop(qMax(0, qMin(g.top() + d.y(), g.bottom() - minH)));
        if (m_edges & Qt::BottomEdge) g.setBottom(qMax(g.bottom() + d.y(), g.top() + minH));
        m_window->setGeometry(g);
    }

    void mouseReleaseEvent(QMouseEvent*) override { m_edges = {}; }

    void leaveEvent(QEvent*) override
    {
        if (!m_edges)
            unsetCursor();
    }

private:
    AppWindow* m_window;
    bool m_active = false;
    Qt::Edges m_edges;
    QPoint m_pressGlobal;
    QRect m_startGeometry;
};

// ── AppWindow ───────────────────────────────────────────────────────────────

AppWindow::AppWindow(const QString& key, const QString& title, const QString& iconPath,
                     QWidget* content, QWidget* parent)
    : QMdiSubWindow(parent, Qt::FramelessWindowHint)
    , m_key(key)
    , m_content(content)
{
    setAttribute(Qt::WA_DeleteOnClose, false);
    setWindowTitle(title);

    // The frame (border + resize handle), then the title bar and the app.
    m_frame = new WindowFrame(this);
    auto* layout = new QVBoxLayout(m_frame);
    const int m = WindowFrame::kMargin;
    layout->setContentsMargins(m, m, m, m);
    layout->setSpacing(0);
    m_titleBar = new WindowTitleBar(this, title, iconPath);
    layout->addWidget(m_titleBar);
    if (content) {
        content->setParent(m_frame);
        content->show();
        layout->addWidget(content, 1);
    }
    setWidget(m_frame);

    connect(Logos::ThemeNotifier::instance(), &Logos::ThemeNotifier::currentChanged,
            this, [this]() { m_frame->update(); m_titleBar->update(); });
}

void AppWindow::setActiveLook(bool active)
{
    m_active = active;
    m_titleBar->setActive(active);
    m_frame->setActive(active);
}

void AppWindow::setMaximizedOnDesktop(bool on)
{
    if (on == m_maximized)
        return;
    auto* area = qobject_cast<DesktopArea*>(mdiArea());
    if (on) {
        m_normalGeometry = geometry();
        m_maximized = true;
        if (area)
            setGeometry(area->maximizedRect());
    } else {
        m_maximized = false;
        if (m_normalGeometry.isValid())
            setGeometry(m_normalGeometry);
    }
}

QWidget* AppWindow::releaseContent()
{
    QWidget* c = m_content;
    if (c) {
        c->hide();
        c->setParent(nullptr);
    }
    m_content = nullptr;
    return c;
}

void AppWindow::closeEvent(QCloseEvent* event)
{
    // Ctrl+F4 and friends: the owner decides (an app unloads, a system app hides).
    event->ignore();
    emit closeRequested(m_key);
}

void AppWindow::resizeEvent(QResizeEvent* event)
{
    QMdiSubWindow::resizeEvent(event);
}

// ── DesktopArea ─────────────────────────────────────────────────────────────

DesktopArea::DesktopArea(QWidget* parent)
    : QMdiArea(parent)
{
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setActivationOrder(QMdiArea::ActivationHistoryOrder);
    connect(this, &QMdiArea::subWindowActivated, this, &DesktopArea::onActivated);
    connect(Logos::ThemeNotifier::instance(), &Logos::ThemeNotifier::currentChanged,
            this, [this]() { viewport()->update(); });
}

void DesktopArea::setWallpaper(const QString& path)
{
    m_wallpaper = path.isEmpty() ? QPixmap() : QPixmap(path);
    m_scaled = QPixmap();
    viewport()->update();
}

void DesktopArea::setBottomReserve(int px)
{
    m_reserve = px;
    for (AppWindow* w : appWindows())
        if (w->isMaximizedOnDesktop())
            w->setGeometry(maximizedRect());
}

QRect DesktopArea::maximizedRect() const
{
    const QSize s = viewport()->size();
    return QRect(0, 0, s.width(), qMax(200, s.height() - m_reserve));
}

QList<AppWindow*> DesktopArea::appWindows() const
{
    QList<AppWindow*> out;
    for (QMdiSubWindow* w : subWindowList())
        if (auto* aw = qobject_cast<AppWindow*>(w))
            out << aw;
    return out;
}

AppWindow* DesktopArea::window(const QString& key) const
{
    for (AppWindow* w : appWindows())
        if (w->key() == key)
            return w;
    return nullptr;
}

AppWindow* DesktopArea::windowFor(const QWidget* content) const
{
    for (AppWindow* w : appWindows())
        if (w->content() == content)
            return w;
    return nullptr;
}

AppWindow* DesktopArea::openWindow(const QString& key, const QString& title,
                                   const QString& iconPath, QWidget* content)
{
    if (AppWindow* existing = window(key)) {
        restoreAndRaise(existing);
        return existing;
    }
    auto* w = new AppWindow(key, title, iconPath, content);
    addSubWindow(w);
    connect(w, &AppWindow::closeRequested, this, &DesktopArea::closeRequested);
    connect(w, &AppWindow::minimizeRequested, this, [this, w]() { minimize(w); });

    // About 70 % of the desktop, centred, each next one 32 px further on.
    const QRect area = maximizedRect();
    // Capped, so a very wide screen still gets a window-sized window.
    const QSize size(qMin(int(area.width() * 0.7), 1280), qMin(int(area.height() * 0.78), 860));
    QPoint pos((area.width() - size.width()) / 2, (area.height() - size.height()) / 2);
    pos += QPoint(32, 32) * (m_cascade % 6) - QPoint(80, 60);
    pos.setX(qMax(0, pos.x()));
    pos.setY(qMax(0, pos.y()));
    ++m_cascade;
    w->setGeometry(QRect(pos, size));
    w->show();
    setActiveSubWindow(w);
    emit windowsChanged();
    return w;
}

void DesktopArea::restoreAndRaise(AppWindow* w)
{
    if (!w)
        return;
    w->show();
    w->raise();
    setActiveSubWindow(w);
    emit windowsChanged();
}

void DesktopArea::minimize(AppWindow* w)
{
    if (!w)
        return;
    w->hide();
    // The next window in history becomes active, as on a desktop.
    for (QMdiSubWindow* other : subWindowList(QMdiArea::ActivationHistoryOrder))
        if (other != w && other->isVisible())
            setActiveSubWindow(other);
    emit windowsChanged();
}

void DesktopArea::removeWindow(AppWindow* w)
{
    if (!w)
        return;
    w->releaseContent();
    removeSubWindow(w);
    w->deleteLater();
    emit windowsChanged();
}

QStringList DesktopArea::openKeys() const
{
    QStringList keys;
    for (AppWindow* w : appWindows())
        keys << w->key();
    return keys;
}

QString DesktopArea::activeKey() const
{
    auto* w = qobject_cast<AppWindow*>(activeSubWindow());
    return w && w->isVisible() ? w->key() : QString();
}

void DesktopArea::onActivated(QMdiSubWindow* active)
{
    for (AppWindow* w : appWindows())
        w->setActiveLook(w == active);
    emit windowsChanged();
}

void DesktopArea::paintEvent(QPaintEvent*)
{
    QPainter p(viewport());
    const QSize s = viewport()->size();
    if (m_wallpaper.isNull()) {
        p.fillRect(viewport()->rect(), tok("background", "#171717"));
        return;
    }
    // Cover: fill the screen, crop the overflow, keep the centre.
    if (m_scaled.isNull() || m_scaled.deviceIndependentSize().toSize() != s) {
        const QPixmap big = m_wallpaper.scaled(s * devicePixelRatioF(), Qt::KeepAspectRatioByExpanding,
                                               Qt::SmoothTransformation);
        const QSize target = s * devicePixelRatioF();
        m_scaled = big.copy((big.width() - target.width()) / 2, (big.height() - target.height()) / 2,
                            target.width(), target.height());
        m_scaled.setDevicePixelRatio(devicePixelRatioF());
    }
    p.drawPixmap(0, 0, m_scaled);
}

void DesktopArea::resizeEvent(QResizeEvent* event)
{
    QMdiArea::resizeEvent(event);
    m_scaled = QPixmap();
    for (AppWindow* w : appWindows())
        if (w->isMaximizedOnDesktop())
            w->setGeometry(maximizedRect());
}

// ── DesktopStatusBar ────────────────────────────────────────────────────────

#include <QDateTime>
#include <QTimer>

DesktopStatusBar::DesktopStatusBar(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(kHeight);
    setMouseTracking(true);
    auto* tick = new QTimer(this);
    tick->setInterval(1000);
    connect(tick, &QTimer::timeout, this, qOverload<>(&QWidget::update));
    tick->start();
    connect(Logos::ThemeNotifier::instance(), &Logos::ThemeNotifier::currentChanged,
            this, [this]() { m_lambda = QPixmap(); update(); });
}

QRect DesktopStatusBar::searchRect() const
{
    // Left of the clock, sized for "Search  Ctrl K".
    return QRect(width() - 16 - 150 - 16 - 120, 4, 120, height() - 8);
}

void DesktopStatusBar::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), tok("backgroundSecondary", "#262626"));
    p.setPen(tok("borderHairline", "#2a2a2a"));
    p.drawLine(0, height() - 1, width(), height() - 1);

    // The λ in the text colour (the asset is white).
    const QColor text = tok("text", "#ffffff");
    if (m_lambda.isNull()) {
        const qreal dpr = devicePixelRatioF();
        QPixmap src = QIcon(QStringLiteral(":/qt/qml/Basecamp/Icons/Basecamp/Icons/logos-lambda.svg"))
                          .pixmap(QSize(12, 16), dpr);
        if (!src.isNull()) {
            QPainter tp(&src);
            tp.setCompositionMode(QPainter::CompositionMode_SourceIn);
            tp.fillRect(src.rect(), text);
        }
        m_lambda = src;
    }
    int x = 16;
    if (!m_lambda.isNull()) {
        p.drawPixmap(QRect(x, (height() - 16) / 2, 12, 16), m_lambda);
        x += 22;
    }
    QFont f = font();
    f.setPixelSize(13);
    f.setWeight(QFont::DemiBold);
    p.setFont(f);
    p.setPen(text);
    p.drawText(QRect(x, 0, 300, height()), Qt::AlignVCenter | Qt::AlignLeft, tr("Logos Basecamp"));

    // Search hint.
    const QRect sr = searchRect();
    if (m_searchHover) {
        p.setPen(Qt::NoPen);
        p.setBrush(tok("surfaceRaised", "#333333"));
        p.drawRoundedRect(sr, 6, 6);
    }
    f.setWeight(QFont::Normal);
    f.setPixelSize(12);
    p.setFont(f);
    p.setPen(tok("textSecondary", "#a0a0a0"));
    p.drawText(sr, Qt::AlignCenter, tr("Search   Ctrl K"));

    // Clock.
    f.setWeight(QFont::Medium);
    f.setPixelSize(13);
    p.setFont(f);
    p.setPen(text);
    p.drawText(QRect(width() - 16 - 150, 0, 150, height()), Qt::AlignVCenter | Qt::AlignRight,
               QLocale().toString(QDateTime::currentDateTime(), QStringLiteral("ddd d MMM  HH:mm")));
}

void DesktopStatusBar::mouseMoveEvent(QMouseEvent* e)
{
    const bool hover = searchRect().contains(e->position().toPoint());
    if (hover != m_searchHover) {
        m_searchHover = hover;
        setCursor(hover ? Qt::PointingHandCursor : Qt::ArrowCursor);
        update();
    }
}

void DesktopStatusBar::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton && searchRect().contains(e->position().toPoint()))
        emit searchClicked();
}

void DesktopStatusBar::leaveEvent(QEvent*)
{
    m_searchHover = false;
    update();
}
