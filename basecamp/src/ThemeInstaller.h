#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

// Settings → Appearance's bridge to Logos::installThemeFile(). It lives in the
// shell's own Basecamp.Backend module, so only the shell UI can install a
// theme; sandboxed apps only ever see Logos.Theme, which can read but not write.
class ThemeInstaller : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString themesDir READ themesDir CONSTANT)

public:
    using QObject::QObject;

    // { ok, id, name, errors: [string], warnings: [string] }
    Q_INVOKABLE QVariantMap install(const QUrl& file) const;

    QString themesDir() const;
};
