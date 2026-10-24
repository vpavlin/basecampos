import QtQuick

import Logos.Controls
import Logos.Theme

import "ObjectFormat.js" as F

// The publisher's icon on a card: the local file the host serves for that app
// (`image://basecamp/appicon/<app>`), or the tile's letters when there is none
// (ADR-071). Never a picture the object itself names: v0 objects carry none.
// Decoration only; the card around it takes the click.
LogosTile {
    property var object: ({})
    property int size: 32

    label: F.str(object.app)
    source: F.str(object.icon)
    tileSize: size
    radius: Theme.spacing.radiusMedium
    fallbackColor: Theme.palette.surfaceRaised
    interactive: false
    // Not a tap target here, so no touch padding around it.
    implicitWidth: size
    implicitHeight: size
    padding: 0
    topInset: 0
    bottomInset: 0
    leftInset: 0
    rightInset: 0
    Accessible.ignored: true
}
