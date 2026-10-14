#pragma once

#include <QString>
#include <QVariantMap>

namespace Logos {

// Validate a theme document at `path` and install it as
// <AppDataLocation>/themes/<id>/theme.json, where ThemeRegistry finds it.
//
// Returns { ok: bool, id: string, name: string, errors: [string], warnings: [string] }.
// The file is parsed as data (ThemeRegistry::parseDocument) before anything is
// written; an invalid document writes nothing. An existing theme with the same
// id is replaced.
//
// Deliberately C++-only: the host exposes it to its own UI. It is not reachable
// from QML, so a sandboxed app can read the active theme but can't install one.
QVariantMap installThemeFile(const QString& path);

} // namespace Logos
