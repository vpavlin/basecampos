#include "ShellPrefs.h"

#include <QApplication>
#include <QCoreApplication>
#include <QFileDialog>
#include <QWidget>
#include <QProcess>
#include <QProcessEnvironment>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QSettings>
#include <QStandardPaths>

namespace {
const char* kLayoutKey = "BasecampShell/layout";
const char* kWallpaperKey = "BasecampShell/wallpaper";
const char* kFitKey = "BasecampShell/wallpaperFit";
const char* kShellKey = "BasecampShell/shell";
const char* kBuiltin = ":/qt/qml/Basecamp/Icons/Basecamp/Icons/glowing-tents.jpg";

QString customPath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                        + QStringLiteral("/wallpapers");
    for (const char* ext : { "jpg", "png" }) {
        const QString p = dir + QStringLiteral("/custom.") + QLatin1String(ext);
        if (QFile::exists(p))
            return p;
    }
    return QString();
}
} // namespace

ShellPrefs::ShellPrefs(QObject* parent)
    : QObject(parent)
{
    const QString env = qEnvironmentVariable("BASECAMP_LAYOUT");
    m_activeLayout = (env == QLatin1String("desktop") || env == QLatin1String("sidebar")) ? env : layout();
}

QString ShellPrefs::layout() const
{
    const QString v = QSettings().value(QLatin1String(kLayoutKey)).toString();
    return v == QLatin1String("desktop") ? v : QStringLiteral("sidebar");
}

QUrl ShellPrefs::pickFile(const QString& title, const QString& filter)
{
    QWidget* owner = qobject_cast<QWidget*>(parent());
    QWidget* top = owner ? owner->window() : QApplication::activeWindow();
    return QFileDialog::getOpenFileUrl(top, title, QUrl(), filter);
}

void ShellPrefs::restart()
{
#ifdef Q_OS_UNIX
    // A helper waits until this process is gone, however it ends, then starts
    // Basecamp again; the two never overlap. Teardown can crash or hang, so
    // after 10 s the helper ends this process and its module hosts itself.
    QString program = QCoreApplication::applicationFilePath();
    QStringList args = QCoreApplication::arguments().mid(1);
#if defined(Q_OS_MAC)
    // Relaunch the .app bundle so macOS treats it as a normal app start.
    QDir bundle(QCoreApplication::applicationDirPath());
    if (bundle.cdUp() && bundle.cdUp() && bundle.path().endsWith(QStringLiteral(".app"))) {
        args = QStringList{ QStringLiteral("-n"), bundle.path(), QStringLiteral("--args") } + args;
        program = QStringLiteral("/usr/bin/open");
    }
#else
    // Inside an AppImage the binary's mount goes away when we exit. Trust
    // APPIMAGE only when we run from its mount (APPDIR): another AppImage
    // can leak the variable into the environment we were started from.
    const QString appImage = qEnvironmentVariable("APPIMAGE");
    const QString appDir = qEnvironmentVariable("APPDIR");
    if (!appImage.isEmpty() && QFile::exists(appImage) && !appDir.isEmpty()
        && QCoreApplication::applicationFilePath().startsWith(appDir + QLatin1Char('/')))
        program = appImage;
#endif
    QSettings().sync();   // the layout choice must be on disk before a forced stop
    QProcess p;
    // A forced layout would undo the choice the user just made.
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.remove(QStringLiteral("BASECAMP_LAYOUT"));
    env.remove(QStringLiteral("BASECAMP_SHELL"));
    p.setProcessEnvironment(env);
    // Never hand our stdio down: stdout/stderr are LogSink's pipe, whose only
    // reader is this process. The new copy would mirror its log into a pipe
    // nobody drains and freeze once 64 KB had piled up.
    p.setStandardInputFile(QProcess::nullDevice());
    p.setStandardOutputFile(QProcess::nullDevice());
    p.setStandardErrorFile(QProcess::nullDevice());
    p.setProgram(QStringLiteral("/bin/sh"));
    p.setArguments(QStringList{
        QStringLiteral("-c"),
        QStringLiteral("i=0; while kill -0 \"$0\" 2>/dev/null; do"
                       " i=$((i+1)); if [ $i -eq 50 ]; then pkill -9 -P \"$0\"; kill -9 \"$0\"; fi;"
                       " sleep 0.2; done; exec \"$@\""),
        QString::number(QCoreApplication::applicationPid()), program } + args);
    if (!p.startDetached()) {
        qWarning("Could not start the restart helper; quitting without restart.");
    }
#else
    // Read by main() after teardown.
    qApp->setProperty("basecampRestart", true);
#endif
    QCoreApplication::quit();
}

void ShellPrefs::setLayout(const QString& layout)
{
    if (layout != QLatin1String("desktop") && layout != QLatin1String("sidebar"))
        return;
    if (layout == this->layout())
        return;
    QSettings().setValue(QLatin1String(kLayoutKey), layout);
    emit layoutChanged();
}

QString ShellPrefs::wallpaper() const
{
    const QString v = QSettings().value(QLatin1String(kWallpaperKey), QStringLiteral("basecamp")).toString();
    if (v == QLatin1String("custom") && customPath().isEmpty())
        return QStringLiteral("basecamp");
    return (v == QLatin1String("plain") || v == QLatin1String("custom")) ? v : QStringLiteral("basecamp");
}

QString ShellPrefs::wallpaperPath() const
{
    const QString id = wallpaper();
    if (id == QLatin1String("plain"))
        return QString();
    if (id == QLatin1String("custom"))
        return customPath();
    return QLatin1String(kBuiltin);
}

QUrl ShellPrefs::wallpaperUrl() const
{
    const QString p = wallpaperPath();
    if (p.isEmpty())
        return QUrl();
    return p.startsWith(QLatin1Char(':')) ? QUrl(QStringLiteral("qrc") + p) : QUrl::fromLocalFile(p);
}

QUrl ShellPrefs::builtinWallpaperUrl() const
{
    return QUrl(QStringLiteral("qrc") + QLatin1String(kBuiltin));
}

QUrl ShellPrefs::customWallpaperUrl() const
{
    const QString p = customPath();
    return p.isEmpty() ? QUrl() : QUrl::fromLocalFile(p);
}

void ShellPrefs::setWallpaper(const QString& id)
{
    if (id != QLatin1String("basecamp") && id != QLatin1String("plain") && id != QLatin1String("custom"))
        return;
    QSettings().setValue(QLatin1String(kWallpaperKey), id);
    emit wallpaperChanged();
}

QString ShellPrefs::setCustomWallpaper(const QUrl& file)
{
    const QString src = file.toLocalFile();
    QImageReader reader(src);
    const QByteArray fmt = reader.format();
    if (fmt != "png" && fmt != "jpeg")
        return tr("Choose a PNG or JPEG image.");
    if (QFileInfo(src).size() > 32 * 1024 * 1024)
        return tr("That image is larger than 32 MB.");
    const QSize size = reader.size();
    if (!size.isValid() || size.width() > 8192 || size.height() > 8192)
        return tr("That image is too large (8192 px at most).");

    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                        + QStringLiteral("/wallpapers");
    QDir().mkpath(dir);
    for (const char* ext : { "jpg", "png" })
        QFile::remove(dir + QStringLiteral("/custom.") + QLatin1String(ext));
    const QString dst = dir + QStringLiteral("/custom.") + (fmt == "png" ? QStringLiteral("png")
                                                                          : QStringLiteral("jpg"));
    if (!QFile::copy(src, dst))
        return tr("Could not copy the image.");
    QFile(dst).setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    setWallpaper(QStringLiteral("custom"));
    return QString();
}

QString ShellPrefs::wallpaperFit() const
{
    const QString v = QSettings().value(QLatin1String(kFitKey), QStringLiteral("fit")).toString();
    return (v == QLatin1String("fill") || v == QLatin1String("stretch")) ? v : QStringLiteral("fit");
}

void ShellPrefs::setWallpaperFit(const QString& fit)
{
    if (fit != QLatin1String("fit") && fit != QLatin1String("fill") && fit != QLatin1String("stretch"))
        return;
    QSettings().setValue(QLatin1String(kFitKey), fit);
    emit wallpaperChanged();
}

namespace {
// Basecamp's user plugins folder, where the package manager installs shells.
QString userPluginsDir()
{
    const QString userDir = qEnvironmentVariable("LOGOS_USER_DIR");
    const QString base = userDir.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        : userDir;
    return base + QStringLiteral("/plugins");
}

QJsonObject readJson(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}
} // namespace

QVariantList ShellPrefs::shellPackages() const
{
    QVariantList out;
    const QDir plugins(userPluginsDir());
    for (const QFileInfo& fi : plugins.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        const QJsonObject m = readJson(fi.absoluteFilePath() + QStringLiteral("/manifest.json"));
        if (m.value(QStringLiteral("type")).toString() != QLatin1String("ui_shell"))
            continue;
        QFile vf(fi.absoluteFilePath() + QStringLiteral("/variant"));
        QString variant;
        if (vf.open(QIODevice::ReadOnly))
            variant = QString::fromUtf8(vf.readAll()).trimmed();
        const QString main = m.value(QStringLiteral("main")).toObject().value(variant).toString();
        const QJsonObject sig = readJson(fi.absoluteFilePath() + QStringLiteral("/manifest.sig"));
        const QJsonObject signer = sig.value(QStringLiteral("signer")).toObject();
        QVariantMap e;
        e[QStringLiteral("id")] = m.value(QStringLiteral("name")).toString();
        e[QStringLiteral("name")] = m.value(QStringLiteral("display_name")).toString(m.value(QStringLiteral("name")).toString());
        e[QStringLiteral("version")] = m.value(QStringLiteral("version")).toString();
        e[QStringLiteral("author")] = m.value(QStringLiteral("author")).toString();
        e[QStringLiteral("description")] = m.value(QStringLiteral("description")).toString();
        e[QStringLiteral("signed")] = !sig.isEmpty();
        e[QStringLiteral("signer")] = signer.value(QStringLiteral("name")).toString();
        e[QStringLiteral("signerUrl")] = signer.value(QStringLiteral("url")).toString();
        e[QStringLiteral("did")] = sig.value(QStringLiteral("did")).toString();
        e[QStringLiteral("file")] = main.isEmpty() ? QString() : fi.absoluteFilePath() + QLatin1Char('/') + main;
        out << e;
    }
    // Shells shipped next to the app (development builds only).
    const QDir bundled(QCoreApplication::applicationDirPath() + QStringLiteral("/../plugins/shells"));
    for (const QFileInfo& fi : bundled.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        bool dup = false;
        for (const QVariant& v : out)
            dup = dup || v.toMap().value(QStringLiteral("id")).toString() == fi.fileName();
        if (dup)
            continue;
        QVariantMap e;
        e[QStringLiteral("id")] = fi.fileName();
        QString n = fi.fileName();
        if (!n.isEmpty()) n[0] = n[0].toUpper();
        e[QStringLiteral("name")] = n;
        e[QStringLiteral("author")] = tr("bundled for development");
        e[QStringLiteral("signed")] = false;
        const QDir d(fi.absoluteFilePath());
        const QStringList libs = d.entryList({ QStringLiteral("main_ui.*") }, QDir::Files);
        e[QStringLiteral("file")] = libs.isEmpty() ? QString() : d.absoluteFilePath(libs.first());
        out << e;
    }
    return out;
}

QStringList ShellPrefs::shells() const
{
    QStringList out;
    for (const QVariant& v : shellPackages())
        out << v.toMap().value(QStringLiteral("id")).toString();
    return out;
}

QVariantList ShellPrefs::checkShell(const QString& id) const
{
    QVariantMap pkg;
    for (const QVariant& v : shellPackages())
        if (v.toMap().value(QStringLiteral("id")).toString() == id)
            pkg = v.toMap();
    auto row = [](const QString& title, const QString& status, const QString& detail) {
        return QVariantMap{ { QStringLiteral("title"), title },
                            { QStringLiteral("status"), status },
                            { QStringLiteral("detail"), detail } };
    };
    QVariantList out;
    if (pkg.value(QStringLiteral("signed")).toBool()) {
        const QString who = pkg.value(QStringLiteral("signer")).toString();
        out << row(tr("Signature"), QStringLiteral("warn"),
                   tr("Signed by %1 (name given by the signer). Basecamp doesn't verify signatures yet.")
                       .arg(who.isEmpty() ? tr("an unnamed key") : who));
    } else {
        out << row(tr("Signature"), QStringLiteral("fail"), tr("Not signed: nobody vouches for this package."));
    }
    QFile f(pkg.value(QStringLiteral("file")).toString());
    if (!f.open(QIODevice::ReadOnly)) {
        out << row(tr("Binary"), QStringLiteral("fail"), tr("The shell's library is missing."));
        return out;
    }
    const QByteArray bin = f.readAll();
    auto has = [&bin](std::initializer_list<const char*> needles) {
        QStringList found;
        for (const char* n : needles)
            if (bin.contains(n))
                found << QString::fromLatin1(n);
        return found;
    };
    const QStringList runtime = has({ "TokenManager", "LogosAPIClient", "LogosAPI" });
    out << (runtime.isEmpty()
            ? row(tr("Logos runtime"), QStringLiteral("ok"), tr("No access to core modules or keys found."))
            : row(tr("Logos runtime"), QStringLiteral("fail"), tr("Links the Logos runtime (%1).").arg(runtime.join(QStringLiteral(", ")))));
    const QStringList net = has({ "libQt6Network", "libcurl", "QNetworkAccessManager", "QTcpSocket", "QUdpSocket", "QWebSocket" });
    out << (net.isEmpty()
            ? row(tr("Network"), QStringLiteral("ok"), tr("No network library found."))
            : row(tr("Network"), QStringLiteral("warn"), tr("Uses networking (%1).").arg(net.join(QStringLiteral(", ")))));
    const QStringList qmlNet = has({ "XMLHttpRequest", "WebSocket" });
    out << (qmlNet.isEmpty()
            ? row(tr("QML network access"), QStringLiteral("ok"), tr("No XMLHttpRequest or WebSocket in its QML."))
            : row(tr("QML network access"), QStringLiteral("warn"), tr("Its QML mentions %1.").arg(qmlNet.join(QStringLiteral(", ")))));
    out << row(tr("Files"), QStringLiteral("info"),
               tr("Not checkable: a shell runs inside Basecamp and could write files."));
    return out;
}

void ShellPrefs::switchShell(const QString& id)
{
    QSettings().setValue(QLatin1String(kShellKey), id == QLatin1String("default") ? QString() : id);
    QSettings().sync();
    emit shellChanged();
    restart();
}

QString ShellPrefs::shell() const
{
    QSettings s;
    if (!s.contains(QLatin1String(kShellKey)))
        return activeShell();
    const QString v = s.value(QLatin1String(kShellKey)).toString();
    return v == QLatin1String("default") ? QString() : v;
}

void ShellPrefs::setShell(const QString& name)
{
    const QString v = name == QLatin1String("default") ? QString() : name;
    if (v == shell() && QSettings().contains(QLatin1String(kShellKey)))
        return;
    QSettings().setValue(QLatin1String(kShellKey), v);
    emit shellChanged();
}

QString ShellPrefs::activeShell() const
{
#ifdef BASECAMPIUM
    return QStringLiteral("basecampium");
#else
    return QString();
#endif
}

bool ShellPrefs::tabsShell() const
{
#ifdef BASECAMPIUM
    return true;
#else
    return false;
#endif
}
