#include "DesktopArea.h"

#include <QCloseEvent>
#include <QIcon>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QTimer>
#include <QPaintEvent>

#include <ThemeColors.h>

namespace {

QColor tok(const char* name, const char* fallback)
{
    return Logos::themeColor(QString::fromLatin1(name), QColor(QString::fromLatin1(fallback)));
}

// Window chrome over the lightly blurred wallpaper: nearly opaque, so the
// wallpaper only tints it, and a little more opaque on the active window.
QColor glassTint(bool active)
{
    QColor c = active ? tok("backgroundSecondary", "#262626") : tok("background", "#171717");
    c.setAlphaF(active ? 0.92 : 0.88);
    return c;
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
        DesktopRoot::paintGlass(p, this, rect(), glassTint(m_active));

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
        DesktopRoot::paintGlass(p, this, rect(), glassTint(m_active));
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
    // Opaque: the glass shows the wallpaper, never the windows beneath, so
    // moving a window doesn't need everything under it repainted.
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAutoFillBackground(false);
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

void AppWindow::moveEvent(QMoveEvent* event)
{
    QMdiSubWindow::moveEvent(event);
    // The glass shows what is under it, which just changed.
    m_frame->update();
    m_titleBar->update();
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
    // See-through: DesktopRoot paints the wallpaper behind us.
    setBackground(Qt::NoBrush);
    setAutoFillBackground(false);
    viewport()->setAutoFillBackground(false);
}

void DesktopArea::setTiling(bool on)
{
    if (on == m_tiling)
        return;
    m_tiling = on;
    if (on) {
        for (AppWindow* w : appWindows())
            w->setMaximizedOnDesktop(false);
        retile();
    }
    emit tilingChanged(on);
}

void DesktopArea::retileSoon()
{
    if (!m_tiling || m_retilePending)
        return;
    m_retilePending = true;
    QTimer::singleShot(0, this, [this]() {
        m_retilePending = false;
        retile();
    });
}

void DesktopArea::retile()
{
    if (!m_tiling)
        return;
    QList<AppWindow*> shown;
    for (const QString& key : m_order)
        if (AppWindow* w = window(key); w && w->isVisible())
            shown << w;
    if (shown.isEmpty())
        return;
    const int gap = 10;
    const QRect area = maximizedRect();
    const int n = shown.size();
    const int colW = (area.width() - (n + 1) * gap) / n;
    for (int i = 0; i < n; ++i) {
        shown[i]->setMaximizedOnDesktop(false);
        shown[i]->setGeometry(area.x() + gap + i * (colW + gap), area.y() + gap,
                              colW, area.height() - 2 * gap);
    }
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
    m_order << key;
    retileSoon();
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
    retileSoon();
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
    retileSoon();
    emit windowsChanged();
}

void DesktopArea::removeWindow(AppWindow* w)
{
    if (!w)
        return;
    w->releaseContent();
    m_order.removeAll(w->key());
    removeSubWindow(w);
    w->deleteLater();
    retileSoon();
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

void DesktopArea::resizeEvent(QResizeEvent* event)
{
    QMdiArea::resizeEvent(event);
    retileSoon();
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
    // The clock shows minutes, so repaint it on the minute, not every second:
    // with a Qt Quick view in the window, each repaint costs a whole-window
    // composite, and an idle desktop never stopped drawing (basecamp-theming#20).
    auto* tick = new QTimer(this);
    tick->setSingleShot(true);
    tick->setTimerType(Qt::PreciseTimer);   // a coarse timer may fire seconds early
    auto arm = [tick]() {
        const QTime now = QTime::currentTime();
        tick->start(60000 - (now.second() * 1000 + now.msec()) + 20);
    };
    connect(tick, &QTimer::timeout, this, [this, arm]() {
        update(QRect(width() - 16 - 150, 0, 150, height()));
        arm();
    });
    arm();
    connect(Logos::ThemeNotifier::instance(), &Logos::ThemeNotifier::currentChanged,
            this, [this]() { m_lambda = QPixmap(); update(); });
}

QRect DesktopStatusBar::tileRect() const
{
    const QRect sr = searchRect();
    return QRect(sr.left() - 8 - 28, sr.top(), 28, sr.height());
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
    QColor tint = tok("backgroundSecondary", "#262626");
    tint.setAlphaF(0.85);
    DesktopRoot::paintGlass(p, this, rect(), tint);
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

    // Tiling toggle: three columns; accent while on.
    const QRect tr_ = tileRect();
    if (m_tileHover || m_tiling) {
        p.setPen(Qt::NoPen);
        p.setBrush(m_tiling ? tok("accentOrange", "#ff8800") : tok("surfaceRaised", "#333333"));
        p.drawRoundedRect(tr_, 6, 6);
    }
    {
        const QColor ink = m_tiling ? tok("accentForeground", "#171717") : tok("textSecondary", "#a0a0a0");
        p.setPen(QPen(ink, 1.2));
        p.setBrush(Qt::NoBrush);
        const QRectF g(tr_.center().x() - 7.5, tr_.center().y() - 5.5, 15, 11);
        p.drawRoundedRect(g, 1.5, 1.5);
        p.drawLine(QPointF(g.left() + 5, g.top()), QPointF(g.left() + 5, g.bottom()));
        p.drawLine(QPointF(g.left() + 10, g.top()), QPointF(g.left() + 10, g.bottom()));
    }

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
    const bool tileHover = tileRect().contains(e->position().toPoint());
    if (hover != m_searchHover || tileHover != m_tileHover) {
        m_searchHover = hover;
        m_tileHover = tileHover;
        setCursor(hover || tileHover ? Qt::PointingHandCursor : Qt::ArrowCursor);
        update();
    }
}

void DesktopStatusBar::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton && searchRect().contains(e->position().toPoint()))
        emit searchClicked();
    else if (e->button() == Qt::LeftButton && tileRect().contains(e->position().toPoint()))
        emit tileClicked();
}

void DesktopStatusBar::leaveEvent(QEvent*)
{
    m_searchHover = false;
    m_tileHover = false;
    update();
}


// ── DesktopRoot ─────────────────────────────────────────────────────────────

namespace {

// A cheap, smooth blur: shrink hard, then scale back up twice.
QImage blurred(const QImage& src, int factor)
{
    if (src.isNull())
        return src;
    const QSize small(qMax(1, src.width() / factor), qMax(1, src.height() / factor));
    QImage s = src.scaled(small, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    s = s.scaled(small * 4, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    return s.scaled(src.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

} // namespace

DesktopRoot::DesktopRoot(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    connect(Logos::ThemeNotifier::instance(), &Logos::ThemeNotifier::currentChanged,
            this, [this]() { rebuild(); });
}

void DesktopRoot::setWallpaper(const QString& path, const QString& fit)
{
    m_source = path.isEmpty() ? QPixmap() : QPixmap(path);
    m_fit = fit;
    rebuild();
}

void DesktopRoot::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    rebuild();
}

void DesktopRoot::rebuild()
{
    const qreal dpr = devicePixelRatioF();
    const QSize target = size() * dpr;
    if (target.isEmpty())
        return;
    QImage out(target, QImage::Format_ARGB32_Premultiplied);
    out.fill(tok("background", "#171717"));
    if (!m_source.isNull()) {
        const QImage src = m_source.toImage();
        QPainter p(&out);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        auto cover = [&]() {
            const QImage big = src.scaled(target, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            return big.copy((big.width() - target.width()) / 2, (big.height() - target.height()) / 2,
                            target.width(), target.height());
        };
        if (m_fit == QLatin1String("stretch")) {
            p.drawImage(0, 0, src.scaled(target, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
        } else if (m_fit == QLatin1String("fill")) {
            p.drawImage(0, 0, cover());
        } else {
            // Fit: the whole image, centred; the spare room is the same image
            // blurred and dimmed, so there are no bars.
            p.drawImage(0, 0, blurred(cover(), 24));
            p.fillRect(out.rect(), QColor(0, 0, 0, 90));
            const QImage fitted = src.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            p.drawImage((target.width() - fitted.width()) / 2, (target.height() - fitted.height()) / 2, fitted);
        }
    }
    m_composed = QPixmap::fromImage(out);
    m_composed.setDevicePixelRatio(dpr);
    m_blurredImage = blurred(out, 6);
    m_blurred = QPixmap::fromImage(m_blurredImage);
    m_blurred.setDevicePixelRatio(dpr);
    ++m_revision;
    emit backdropChanged();
    update();
    // Everything frosted shows the new backdrop.
    for (QWidget* w : findChildren<QWidget*>())
        w->update();
}

void DesktopRoot::paintEvent(QPaintEvent* event)
{
    QPainter p(this);
    if (m_composed.isNull())
        p.fillRect(event->rect(), tok("background", "#171717"));
    else
        p.drawPixmap(event->rect(), m_composed,
                     QRectF(QPointF(event->rect().topLeft()) * m_composed.devicePixelRatio(),
                            QSizeF(event->rect().size()) * m_composed.devicePixelRatio()));
}

void DesktopRoot::paintGlass(QPainter& p, QWidget* w, const QRect& r, const QColor& tint)
{
    DesktopRoot* root = nullptr;
    for (QWidget* a = w; a; a = a->parentWidget())
        if ((root = qobject_cast<DesktopRoot*>(a)))
            break;
    if (root && !root->m_blurred.isNull()) {
        const QPoint at = w->mapTo(root, r.topLeft());
        const qreal dpr = root->m_blurred.devicePixelRatio();
        p.drawPixmap(r, root->m_blurred,
                     QRectF(QPointF(at) * dpr, QSizeF(r.size()) * dpr));
    }
    p.fillRect(r, tint);
}


// ── QML glass ───────────────────────────────────────────────────────────────

GlassInfo::GlassInfo(DesktopRoot* root, QWidget* widget, QObject* parent)
    : QObject(parent), m_root(root), m_widget(widget)
{
    if (root)
        connect(root, &DesktopRoot::backdropChanged, this, &GlassInfo::refresh);
    // The widget moves when its window moves; watch every ancestor.
    for (QWidget* w = widget; w; w = w->parentWidget())
        w->installEventFilter(this);
    refresh();
}

int GlassInfo::rootWidth() const { return m_root ? m_root->width() : 0; }
int GlassInfo::rootHeight() const { return m_root ? m_root->height() : 0; }
int GlassInfo::revision() const { return m_root ? m_root->revision() : 0; }

bool GlassInfo::eventFilter(QObject* watched, QEvent* event)
{
    switch (event->type()) {
    case QEvent::Move:
    case QEvent::Resize:
    case QEvent::Show:
    case QEvent::ParentChange:
        if (event->type() == QEvent::ParentChange && m_widget) {
            // Re-parented into a new window: watch the new ancestors.
            for (QWidget* w = m_widget; w; w = w->parentWidget())
                w->installEventFilter(this);
        }
        refreshSoon();
        break;
    default:
        break;
    }
    return QObject::eventFilter(watched, event);
}

// A drag sends many move events per frame (one per watched ancestor, at the
// mouse's rate); follow them once per frame.
void GlassInfo::refreshSoon()
{
    if (m_pending)
        return;
    m_pending = true;
    QTimer::singleShot(16, this, [this]() {
        m_pending = false;
        refresh();
    });
}

void GlassInfo::refresh()
{
    if (!m_root || !m_widget)
        return;
    // Works for widgets outside the root too (Spotlight floats over it).
    m_offset = m_widget->mapToGlobal(QPoint(0, 0)) - m_root->mapToGlobal(QPoint(0, 0));
    emit changed();
}

QImage BackdropProvider::requestImage(const QString&, QSize* size, const QSize&)
{
    const QImage img = m_root ? m_root->blurredImage() : QImage();
    if (size)
        *size = img.size();
    return img;
}

GlassItem::GlassItem(DesktopRoot* root, QWidget* host, const QColor& tint, QQuickItem* parent)
    : QQuickPaintedItem(parent), m_root(root), m_host(host), m_tint(tint)
{
    setZ(-1000);
    auto fit = [this]() {
        if (parentItem())
            setSize(parentItem()->size());
    };
    fit();
    connect(parent, &QQuickItem::widthChanged, this, fit);
    connect(parent, &QQuickItem::heightChanged, this, fit);
    // Repaint when the window moves or the backdrop changes.
    auto* info = new GlassInfo(root, host, this);
    connect(info, &GlassInfo::changed, this, [this]() { update(); });
}

void GlassItem::paint(QPainter* painter)
{
    const QRectF r(0, 0, width(), height());
    if (m_root && m_host && !m_root->blurredPixmap().isNull()) {
        const QPoint at = m_host->mapToGlobal(QPoint(0, 0)) - m_root->mapToGlobal(QPoint(0, 0));
        const QPixmap& pix = m_root->blurredPixmap();
        const qreal dpr = pix.devicePixelRatio();
        painter->drawPixmap(r, pix, QRectF(QPointF(at) * dpr, r.size() * dpr));
    }
    painter->fillRect(r, m_tint);
}
