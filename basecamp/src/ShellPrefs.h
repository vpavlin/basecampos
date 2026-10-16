#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

// Shell-level preferences that are not part of a theme: which layout the shell
// is built with, and the desktop layout's wallpaper. Persisted in QSettings;
// exposed to the Settings page as the `shellPrefs` context property.
class ShellPrefs : public QObject
{
    Q_OBJECT
    // The saved choice ("sidebar" or "desktop"); applies on the next start.
    Q_PROPERTY(QString layout READ layout WRITE setLayout NOTIFY layoutChanged)
    // What this run was built with (the saved choice, or BASECAMP_LAYOUT).
    Q_PROPERTY(QString activeLayout READ activeLayout CONSTANT)
    // "basecamp" (built-in photo), "plain" (theme background) or "custom".
    Q_PROPERTY(QString wallpaper READ wallpaper NOTIFY wallpaperChanged)
    Q_PROPERTY(QUrl wallpaperUrl READ wallpaperUrl NOTIFY wallpaperChanged)
    Q_PROPERTY(QUrl builtinWallpaperUrl READ builtinWallpaperUrl CONSTANT)
    Q_PROPERTY(QUrl customWallpaperUrl READ customWallpaperUrl NOTIFY wallpaperChanged)
    // "fit" (whole image, blurred spare room), "fill" (cover, crops) or "stretch".
    Q_PROPERTY(QString wallpaperFit READ wallpaperFit WRITE setWallpaperFit NOTIFY wallpaperChanged)

public:
    explicit ShellPrefs(QObject* parent = nullptr);

    QString layout() const;
    void setLayout(const QString& layout);
    QString activeLayout() const { return m_activeLayout; }
    bool desktopLayout() const { return m_activeLayout == QLatin1String("desktop"); }

    QString wallpaper() const;
    // A local path or qrc path for painting; empty = plain.
    QString wallpaperPath() const;
    QUrl wallpaperUrl() const;
    QUrl builtinWallpaperUrl() const;
    QUrl customWallpaperUrl() const;

    Q_INVOKABLE void setWallpaper(const QString& id);
    QString wallpaperFit() const;
    void setWallpaperFit(const QString& fit);
    // Copies the chosen image into Basecamp's data folder and selects it.
    // Returns an error message, or "" on success.
    Q_INVOKABLE QString setCustomWallpaper(const QUrl& file);

signals:
    void layoutChanged();
    void wallpaperChanged();

private:
    QString m_activeLayout;
};
