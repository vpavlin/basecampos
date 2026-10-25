#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>

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
    // Shell packages (prototype): installed shells under plugins/shells/, the
    // saved choice ("" = Basecamp Legacy, the host's built-in shell; applies
    // after a restart), and whether this build is the Basecampium (tabs)
    // shell, the default one, shown as "Basecamp" (ShellChoice.h).
    Q_PROPERTY(QStringList shells READ shells NOTIFY shellsChanged)
    // Installed shell packages (manifest type "ui_shell"), each
    // {id, name, version, author, description, signed, signer, signerUrl, did, file},
    // plus builtIn for the default shell when it ships next to the app.
    Q_PROPERTY(QVariantList shellPackages READ shellPackages NOTIFY shellsChanged)
    Q_PROPERTY(QString shell READ shell WRITE setShell NOTIFY shellChanged)
    Q_PROPERTY(QString activeShell READ activeShell CONSTANT)
    Q_PROPERTY(bool tabsShell READ tabsShell CONSTANT)
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

    // Quits Basecamp; main() starts it again once everything has shut down.
    Q_INVOKABLE void restart();

    // Opens a file picker parented to Basecamp's window, so it stays above it
    // (a QML FileDialog inside a QQuickWidget has no real parent window and can
    // open behind the full-screen desktop). Returns the chosen file, or empty.
    Q_INVOKABLE QUrl pickFile(const QString& title, const QString& filter);

    QStringList shells() const;
    QVariantList shellPackages() const;
    // Install-time style checks over a shell's binary, as hints: each
    // {title, status: "ok"|"warn"|"fail"|"info", detail}.
    Q_INVOKABLE QVariantList checkShell(const QString& id) const;
    // Saves the choice and restarts Basecamp with it.
    Q_INVOKABLE void switchShell(const QString& id);
    Q_INVOKABLE void rescanShells() { emit shellsChanged(); }
    QString shell() const;
    void setShell(const QString& name);
    QString activeShell() const;
    bool tabsShell() const;

    Q_INVOKABLE void setWallpaper(const QString& id);
    QString wallpaperFit() const;
    void setWallpaperFit(const QString& fit);
    // Copies the chosen image into Basecamp's data folder and selects it.
    // Returns an error message, or "" on success.
    Q_INVOKABLE QString setCustomWallpaper(const QUrl& file);

signals:
    void layoutChanged();
    void shellChanged();
    void shellsChanged();
    void wallpaperChanged();

private:
    QString m_activeLayout;
};
