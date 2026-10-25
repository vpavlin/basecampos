import QtQuick

import Basecamp.Icons

import "ObjectFormat.js" as F

// The publisher's icon on a card: the local file the host serves for that app
// (`image://basecamp/appicon/<app>`), or the tile's letters when there is none
// (ADR-071). Never a picture the object itself names: v0 objects carry none.
// Decoration only; the card around it takes the click.
BasecampAppTile {
    property var object: ({})

    size: 32
    label: F.str(object.app)
    source: F.str(object.icon)
    fullBleed: object.iconFullBleed === true
}
