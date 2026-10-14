#include "ThemeInstaller.h"

#include <QDir>
#include <QStandardPaths>

#include <ThemeInstall.h>

QVariantMap ThemeInstaller::install(const QUrl& file) const
{
    return Logos::installThemeFile(file.isLocalFile() ? file.toLocalFile() : file.toString());
}

QString ThemeInstaller::themesDir() const
{
    return QDir::cleanPath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                           + QStringLiteral("/themes"));
}
