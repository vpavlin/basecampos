#pragma once

#include <QColor>
#include <QObject>
#include <QString>

namespace Logos {

// Process-wide active theme. Every QML engine's ThemeRegistry and any C++ that
// paints outside QML follow this one object, so a theme change made in one
// engine (Settings) restyles every engine and the widget chrome live.
// Separate processes (sandboxed apps in ui-host) pick the change up on restart.
class ThemeNotifier : public QObject
{
    Q_OBJECT

public:
    static ThemeNotifier* instance();

    // Active theme id ("dark" = built-in). Seeded from the persisted
    // `LogosDesignSystem/theme` setting.
    QString current() const { return m_current; }
    void setCurrent(const QString& id);

    // Tell every ThemeRegistry to rescan (a theme was installed or removed).
    void announceThemesChanged() { emit themesChanged(); }

signals:
    void currentChanged();
    void themesChanged();

private:
    ThemeNotifier();
    QString m_current;
};

// Colour of a semantic `token` (a Theme.palette name) in the active installed
// theme, for C++ code that paints outside QML (widget palettes, stylesheets,
// clear colours). Returns `fallback` under the built-in dark theme or when the
// active theme leaves `token` out; callers pass today's literal, so the default
// look is unchanged. Re-read it on ThemeNotifier::currentChanged.
QColor themeColor(const QString& token, const QColor& fallback);

} // namespace Logos
