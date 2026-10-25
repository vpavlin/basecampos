#pragma once

#include <QColor>
#include <QIcon>
#include <QString>

// An app's tile as the QML views draw it (Basecamp/Icons/BasecampAppTile.qml,
// basecamp-experiments#40), for the shell's widget chrome: tabs, the one-bar
// pill, the switcher's cards, a desktop window's title bar.
//
// Icons are full-bleed squares and the rounding is done here, the same share of
// the edge at every size (radiusXlarge at 80 px), so every place shows the one
// shape. `path` is the app's icon (a file, file: or qrc: URL); without one, the
// app's letters (the first two of `title`) on the plate, as the QML tile shows
// them, in the theme's `plate` and `text` colours. A null icon when `path`
// names nothing loadable.
QIcon appTileIcon(const QString& path, const QString& title, const QColor& plate, const QColor& text);
