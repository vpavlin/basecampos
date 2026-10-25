import QtQuick

import Logos.Theme

// The base of a type's body inside ObjectCard: the envelope and its fields in,
// a heading for the card (replaces the title when set) and a short value for
// the compact row out. Lays its lines out top to bottom at the card's width.
Column {
    property var object: ({})
    property var fields: ({})
    // The card's heading; "" keeps the object's title.
    property string heading: ""
    // A short value for the one-line row ("+40.00 TST", "1,240 members").
    property string trailing: ""
    // The bar's text: lines of the object's own words mark what it matches
    // (ObjectCardMatchLine, basecamp-experiments#46).
    property string query: ""

    spacing: Theme.spacing.tiny
}
