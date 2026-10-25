import QtQuick

import Logos.Controls
import Logos.Theme

import "ObjectFormat.js" as F

// message (§4): conversation, outgoing; sender, senderName, text.
ObjectCardBody {
    id: body
    readonly property bool outgoing: fields.outgoing === true
    readonly property string conversation: F.str(fields.conversation)

    heading: {
        const who = body.outgoing ? qsTr("You") : F.str(fields.senderName)
        return who.length > 0 && body.conversation.length > 0 && body.conversation !== who
               ? qsTr("%1 in %2").arg(who).arg(body.conversation)
               : (who.length > 0 ? who : body.conversation)
    }

    ObjectCardMatchLine {
        plain: F.str(body.fields.text).length > 0 ? F.str(body.fields.text) : F.str(body.object.title)
        query: body.query
        maximumLineCount: 3
        color: body.outgoing ? Theme.palette.textSecondary : Theme.palette.text
    }
}
