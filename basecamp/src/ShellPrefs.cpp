#include "ShellPrefs.h"

#include <QCoreApplication>
#include <QProcess>
#include <QProcessEnvironment>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QSettings>
#include <QStandardPaths>

namespace {
const char* kLayoutKey = "BasecampShell/layout";
const char* kWallpaperKey = "BasecampShell/wallpaper";
const char* kFitKey = "BasecampShell/wallpaperFit";
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
    p.setProcessEnvironment(env);
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
