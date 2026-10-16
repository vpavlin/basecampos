#include "ShellPrefs.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QSettings>
#include <QStandardPaths>

namespace {
const char* kLayoutKey = "BasecampShell/layout";
const char* kWallpaperKey = "BasecampShell/wallpaper";
const char* kBuiltin = ":/qt/qml/Basecamp/Icons/Basecamp/Icons/basecamp-wallpaper.jpg";

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
