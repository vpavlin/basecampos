// Logos responsive lab: common app layouts built from design-system controls,
// for checking how they behave across window sizes (docs/RESPONSIVE.md).
//
//   LogosResponsiveLab                          interactive, resizable window
//   LogosResponsiveLab --shot out.png --width 390 --height 844 --page settings
//                      [--density touch] [--state key=value ...]
//       renders one screen and prints a JSON report:
//         overflow: visible items that run past the window's right edge and
//                   are not clipped by an ancestor (horizontal overflow)
//         smallTargets: tappable items (buttons, mouse areas) under
//                   Theme.minTouchTarget (44) in either dimension
//       Exit code 0 always; the caller decides what fails.
//
// QML is loaded from LOGOS_LAB_QML (env), the source tree, or $out/lib/lab.

#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <QVariantMap>
#include <cstdio>

#ifndef LAB_QML_DIR
#define LAB_QML_DIR ""
#endif

namespace {

QString describe(QQuickItem* item)
{
    QString name = item->objectName();
    QString cls = QString::fromLatin1(item->metaObject()->className());
    cls = cls.section(QLatin1String("_QMLTYPE"), 0, 0).section(QLatin1String("_QML"), 0, 0);
    // A text child says more than a class name.
    QString text = item->property("text").toString();
    if (text.isEmpty())
        text = item->property("title").toString();
    if (text.isEmpty())
        text = item->property("Accessible.name").toString();
    QString out = cls;
    if (!name.isEmpty())
        out += QLatin1Char('#') + name;
    if (!text.isEmpty())
        out += QStringLiteral(" \"") + text.left(40) + QLatin1Char('"');
    return out;
}

// Something the user taps or types into. Controls whose parts are separate
// targets (a spin box's − and +, a slider's handle) are checked part by part
// in walk(); here the control itself counts.
bool interactive(QQuickItem* item)
{
    if (!item->isEnabled())
        return false;
    for (const char* cls : { "QQuickAbstractButton", "QQuickTextField", "QQuickTextArea",
                             "QQuickComboBox", "QQuickSlider", "QQuickSpinBox",
                             "QQuickRangeSlider", "QQuickDial" }) {
        if (item->inherits(cls))
            return true;
    }
    if (item->inherits("QQuickMouseArea") || item->inherits("QQuickTapHandler"))
        return item->property("enabled").toBool();
    return false;
}

// The separately tappable parts of a control: spin box − / +.
QList<QQuickItem*> tapParts(QQuickItem* item)
{
    QList<QQuickItem*> parts;
    if (item->inherits("QQuickSpinBox")) {
        for (const char* side : { "up", "down" }) {
            QObject* ind = item->property(side).value<QObject*>();
            if (ind) {
                if (auto* it = ind->property("indicator").value<QQuickItem*>())
                    parts << it;
            }
        }
    }
    return parts;
}

QJsonObject rectJson(const QRectF& r)
{
    return QJsonObject{ { "x", qRound(r.x()) }, { "y", qRound(r.y()) },
                        { "w", qRound(r.width()) }, { "h", qRound(r.height()) } };
}

struct Report {
    QJsonArray overflow;
    QJsonArray small;
};

// clip: the scene rect the item is visible within (its clipping ancestors).
void walk(QQuickItem* item, const QRectF& clip, qreal winW, int minTarget,
          bool insideInteractive, Report& rep, bool* overflowReported)
{
    if (!item->isVisible() || item->opacity() <= 0.01)
        return;
    const QRectF scene = item->mapRectToScene(QRectF(0, 0, item->width(), item->height()));
    const QRectF shown = scene.intersected(clip);
    if (shown.isEmpty() && item->width() > 0 && item->height() > 0)
        return;   // scrolled away or clipped out entirely

    // Overflow: the item runs past the right edge of what can show it (its
    // nearest clipping ancestor, or the window), so part of it is cut off.
    // Reported once, at the outermost item that overflows.
    // A bare Item with no children draws nothing (e.g. a ListView's empty
    // default highlight), so it can't visibly overflow.
    const bool drawsNothing = qstrcmp(item->metaObject()->className(), "QQuickItem") == 0
                              && item->childItems().isEmpty();
    const bool overflows = !drawsNothing && scene.width() > 0 && scene.height() > 0
                           && scene.right() > clip.right() + 1.0 && scene.left() < clip.right();
    if (overflows && !*overflowReported) {
        QStringList path;
        for (QQuickItem* a = item; a && path.size() < 4; a = a->parentItem())
            path.prepend(describe(a));
        rep.overflow.append(QJsonObject{ { "item", path.join(QStringLiteral(" > ")) }, { "rect", rectJson(scene) },
                                         { "visibleRight", qRound(clip.right()) } });
    }

    const bool tappable = interactive(item);
    for (QQuickItem* part : tapParts(item)) {
        const QRectF r = part->mapRectToScene(QRectF(0, 0, part->width(), part->height()));
        if (part->isVisible() && (r.width() < minTarget - 0.5 || r.height() < minTarget - 0.5))
            rep.small.append(QJsonObject{ { "item", describe(item) + QStringLiteral(" (part)") },
                                          { "rect", rectJson(r) } });
    }
    if (tappable && !insideInteractive && scene.width() > 0 && scene.height() > 0
        && (scene.width() < minTarget - 0.5 || scene.height() < minTarget - 0.5)) {
        rep.small.append(QJsonObject{ { "item", describe(item) }, { "rect", rectJson(scene) } });
    }

    QRectF childClip = clip;
    if (item->clip())
        childClip = clip.intersected(scene);
    bool childReported = *overflowReported || overflows;
    for (QQuickItem* c : item->childItems())
        walk(c, childClip, winW, minTarget, insideInteractive || tappable, rep, &childReported);
}

QString findQmlDir()
{
    const QString bin = QCoreApplication::applicationDirPath();
    for (const QString& p : { qEnvironmentVariable("LOGOS_LAB_QML"), QString::fromUtf8(LAB_QML_DIR),
                              bin + QStringLiteral("/../lib/lab") }) {
        if (!p.isEmpty() && QFileInfo::exists(p + QStringLiteral("/Main.qml")))
            return p;
    }
    return {};
}

} // namespace

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    app.setOrganizationName("Logos");
    app.setApplicationName("Logos Responsive Lab");
    QQuickStyle::setStyle("Basic");

    QString shot, page = QStringLiteral("mail"), density;
    int width = 1280, height = 800;
    QVariantMap state;
    const QStringList args = app.arguments();
    for (int i = 1; i < args.size(); ++i) {
        const QString a = args.at(i);
        auto next = [&]() { return i + 1 < args.size() ? args.at(++i) : QString(); };
        if (a == QLatin1String("--shot")) shot = next();
        else if (a == QLatin1String("--width")) width = next().toInt();
        else if (a == QLatin1String("--height")) height = next().toInt();
        else if (a == QLatin1String("--page")) page = next();
        else if (a == QLatin1String("--density")) density = next();
        else if (a == QLatin1String("--state")) {
            const QString kv = next();
            state.insert(kv.section(QLatin1Char('='), 0, 0), kv.section(QLatin1Char('='), 1));
        }
    }

    const QString dir = findQmlDir();
    if (dir.isEmpty()) {
        qCritical() << "LogosResponsiveLab: Main.qml not found (set LOGOS_LAB_QML)";
        return 2;
    }

    QQmlApplicationEngine engine;
    engine.addImportPath(QStringLiteral("qrc:/"));
    engine.rootContext()->setContextProperty(QStringLiteral("LabArgs"), QVariantMap{
        { QStringLiteral("page"), page },
        { QStringLiteral("density"), density },
        { QStringLiteral("shot"), !shot.isEmpty() },
        { QStringLiteral("state"), state },
        { QStringLiteral("width"), width },
        { QStringLiteral("height"), height },
    });
    engine.load(QUrl::fromLocalFile(dir + QStringLiteral("/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return 1;
    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if (!window)
        return 1;
    window->resize(width, height);

    if (shot.isEmpty())
        return app.exec();

    QTimer::singleShot(1500, &app, [&]() {
        Report rep;
        bool none = false;
        walk(window->contentItem(), QRectF(0, 0, window->width(), window->height()),
             window->width(), 44, false, rep, &none);
        const QImage img = window->grabWindow();
        const bool saved = img.save(shot);
        const QJsonObject out{
            { "page", page }, { "width", window->width() }, { "height", window->height() },
            { "density", density.isEmpty() ? QStringLiteral("default") : density },
            { "shot", saved ? shot : QString() },
            { "overflow", rep.overflow }, { "smallTargets", rep.small },
        };
        std::fputs(QJsonDocument(out).toJson(QJsonDocument::Compact).constData(), stdout);
        std::fputs("\n", stdout);
        std::fflush(stdout);
        QCoreApplication::exit(0);
    });
    return app.exec();
}
