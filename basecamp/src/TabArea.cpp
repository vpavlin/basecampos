#include "TabArea.h"

#include <QApplication>
#include <QTimer>
#include <QLabel>
#include <QStyle>
#include <QHBoxLayout>
#include <QIcon>
#include <QMenu>
#include <QCursor>
#include <QMouseEvent>
#include <QStackedWidget>
#include <QToolButton>
#include <QUrl>
#include <QShowEvent>
#include <QVBoxLayout>
#include <QWindow>

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

QIcon iconFor(const QString& path)
{
    if (path.isEmpty())
        return QIcon(QStringLiteral(":/qt/qml/Basecamp/Icons/Basecamp/Icons/logos-lambda.svg"));
    QString p = path;
    if (p.startsWith(QStringLiteral("file:")))
        p = QUrl(p).toLocalFile();
    else if (p.startsWith(QStringLiteral("qrc:")))
        p = p.mid(3);
    return QIcon(p);
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
    void setIcon(const QIcon& icon) { m_icon->setPixmap(icon.pixmap(16, 16)); }
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
    void elide()
    {
        const int w = qMax(0, width() - 4 - 16 - 8);
        m_label->setText(m_label->fontMetrics().elidedText(m_text, Qt::ElideRight, w));
    }
    QLabel* m_icon = nullptr;
    QLabel* m_label = nullptr;
    QString m_text;
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
    QTabBar::mousePressEvent(event);
}

void TabStrip::mouseMoveEvent(QMouseEvent* event)
{
    // Well outside the strip (above or below, or past the ends): tear the tab off.
    const QPoint p = event->position().toPoint();
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

// ── TabArea ─────────────────────────────────────────────────────────────────

TabArea::TabArea(QWidget* newTabPage, QWidget* parent)
    : QWidget(parent)
    , m_newTabPage(newTabPage)
{
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
    m_plus->setText(QStringLiteral("+"));
    m_plus->setToolTip(tr("New tab (Ctrl+T)"));
    m_plus->setFixedSize(36, 32);
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
    column->addWidget(m_stack, 1);

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

    m_bar->setStyleSheet(QStringLiteral(
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
        "QTabBar QToolButton { background: %1; }")
        .arg(css(bar), css(page), css(hover), css(muted), css(text), css(border), css(danger)));
    QPalette p = m_stack->palette();
    p.setColor(QPalette::Window, page);
    m_stack->setPalette(p);
    m_stack->setAutoFillBackground(true);
}

void TabArea::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateStripWidth();
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
    setLabel(m_tabs, i, iconFor(QString()), tr("New tab"));
    m_tabs->setTabToolTip(i, tr("New tab"));
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
    setLabel(m_tabs, i, iconFor(iconPath), title);
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
    QWidget* content = contentFor(key);
    if (!content)
        return;
    m_stack->removeWidget(content);
    m_tabs->removeTab(index);
    updateStripWidth();
    onCurrentChanged(m_tabs->currentIndex());

    // A plain top-level window (system title bar) owned by this area, so it
    // goes when Basecamp's window goes.
    auto* win = new QWidget(this, Qt::Window);
    win->setObjectName(QStringLiteral("basecampium.detached"));
    win->setProperty(kKeyProp, key);
    win->setWindowTitle(m_titles.value(key, key));
    win->setWindowIcon(iconFor(m_icons.value(key)));
    auto* lay = new QVBoxLayout(win);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(content);
    content->show();
    QPalette p = win->palette();
    p.setColor(QPalette::Window, tok("background", "#171717"));
    win->setPalette(p);
    win->setAutoFillBackground(true);
    const QSize size = window()->size() * 0.7;
    win->resize(size.expandedTo(QSize(720, 480)));
    win->move(globalPos - QPoint(win->width() / 3, 16));
    win->installEventFilter(this);
    m_detached.insert(key, win);
    win->show();
    emit currentKeyChanged(currentKey());
}

QWidget* TabArea::takeTab(const QString& key)
{
    if (QWidget* win = m_detached.take(key)) {
        QWidget* content = contentFor(key);
        if (!content)
            content = win->findChild<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
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
    QWidget* content = contentFor(key);
    if (content) {
        m_stack->removeWidget(content);
        content->setProperty(kKeyProp, QVariant());
    }
    m_tabs->removeTab(i);
    updateStripWidth();
    if (m_tabs->count() == 0)
        addNewTab();
    else
        onCurrentChanged(m_tabs->currentIndex());
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
    if (QWidget* win = m_detached.value(key)) {
        for (QWidget* c : win->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly))
            if (c->property(kKeyProp).toString() == key)
                return c;
    }
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
    if (index < 0)
        return;
    const QString key = m_tabs->tabData(index).toString();
    if (QWidget* w = contentFor(key)) {
        m_stack->setCurrentWidget(w);
        for (int i = 0; i < m_stack->count(); ++i)
            if (QWidget* other = m_stack->widget(i); other != w && other->isVisible())
                other->hide();
        if (w == m_newTabPage)
            emit newTabShown();
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
    QAction* others = menu.addAction(tr("Close other tabs"));
    QAction* right = menu.addAction(tr("Close tabs to the right"));
    menu.addSeparator();
    QAction* undock = menu.addAction(tr("Move to new window"));
    undock->setEnabled(!isNewTabKey(key) && m_tabs->count() > 1);
    others->setEnabled(m_tabs->count() > 1);
    right->setEnabled(index < m_tabs->count() - 1);
    QAction* chosen = menu.exec(globalPos);
    if (!chosen)
        return;
    if (chosen == undock) {
        detachTab(index, QCursor::pos());
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
