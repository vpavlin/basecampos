#pragma once

#include <QColor>
#include <QString>

namespace Logos {

// Colour of a semantic `token` (a Theme.palette name) in the active installed
// theme, for C++ code that paints outside QML (widget palettes, stylesheets,
// clear colours).
//
// Returns `fallback` when the active theme is the built-in dark one, or when
// the active theme leaves `token` out. Callers pass today's literal as the
// fallback, so the default look is unchanged. Resolved once per process from
// the `LogosDesignSystem/theme` setting: a theme change applies on restart.
QColor themeColor(const QString& token, const QColor& fallback);

} // namespace Logos
