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

    spacing: Theme.spacing.tiny
}
