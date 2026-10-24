import QtQuick

import Logos.Controls
import Logos.Theme

// A small coloured label for a status or a state ("Done", "Failed", "Joined").
// tone: "success", "error", "warning", "info" or "" (neutral).
LogosBadge {
    property string tone: ""

    visible: text.length > 0
    // The publisher's words, never markup (ADR-071).
    Component.onCompleted: labelItem.textFormat = Text.PlainText
    color: tone === "success" ? Theme.palette.success
         : tone === "error" ? Theme.palette.error
         : tone === "warning" ? Theme.palette.warning
         : tone === "info" ? Theme.palette.info
         : Theme.palette.textSecondary
}
