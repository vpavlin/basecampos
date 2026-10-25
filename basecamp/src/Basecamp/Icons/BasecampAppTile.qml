import QtQuick

import Logos.Controls
import Logos.Theme

// An app's tile, the one look every view of an app shares (home's Recent apps
// and Apps rows, Applications, the sidebar, the dock, cards, Settings):
// basecamp-experiments#40.
//
// Icons are full-bleed squares; nothing we ship bakes in a rounded corner or
// a rounded background. The rounding is done here, by the tile's mask, so one
// radius applies everywhere: the same share of the edge at every size
// (radiusXlarge at 80 px, scaled), following the theme's roundness. An app
// with no icon shows its letters on the plate: one shape, never a square
// inside a square.
//
// Decoration only: no hover chrome and no touch padding. The row or cell
// around it takes the tap and sizes the target.
LogosTile {
    // Edge length in px.
    property int size: 40
    // The icon fills the tile (a 0.4.0+ package's validated square). Older
    // packages ship a small glyph, drawn inset on the plate.
    property bool fullBleed: false

    tileSize: size
    radius: Math.round(size * Theme.spacing.radiusXlarge / 80)
    fallbackColor: Theme.palette.surfaceRaised
    insetArtwork: !fullBleed
    interactive: false
    implicitWidth: size
    implicitHeight: size
    padding: 0
    topInset: 0
    bottomInset: 0
    leftInset: 0
    rightInset: 0
    Accessible.ignored: true
}
