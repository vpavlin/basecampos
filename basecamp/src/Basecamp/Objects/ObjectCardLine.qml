import QtQuick

import Logos.Controls
import Logos.Theme

// One line (or a few) of plain text in a card body: wraps at the card's width,
// elides past `maximumLineCount`, and takes no room when empty.
LogosText {
    property bool secondary: false

    width: parent ? parent.width : implicitWidth
    visible: text.length > 0
    textFormat: Text.PlainText
    wrapMode: Text.Wrap
    maximumLineCount: 1
    elide: Text.ElideRight
    color: secondary ? Theme.palette.textSecondary : Theme.palette.text
    font.pixelSize: secondary ? Theme.typography.secondaryText : Theme.typography.primaryText
}
