import QtQuick

import Logos.Controls
import Logos.Theme

import "ObjectFormat.js" as F

// community (§4): name, members, joined.
ObjectCardBody {
    id: body
    readonly property string membersText: F.members(fields.members)

    heading: F.str(fields.name)
    trailing: membersText

    Row {
        width: parent.width
        spacing: Theme.spacing.small
        ObjectCardLine {
            width: Math.min(implicitWidth, parent.width - joined.width - parent.spacing)
            anchors.verticalCenter: parent.verticalCenter
            secondary: true
            text: body.membersText
        }
        ObjectCardStatus {
            id: joined
            anchors.verticalCenter: parent.verticalCenter
            text: body.fields.joined === true ? qsTr("Joined") : ""
            tone: "success"
        }
    }
}
