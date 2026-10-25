import QtQuick

import Logos.Controls
import Logos.Theme

import "ObjectFormat.js" as F

// transaction (§4): direction, status, network; amount, unit, counterparty,
// address. A `summary` ("41 more transactions today") shows its subtitle.
ObjectCardBody {
    id: body
    readonly property bool summary: F.str(object.kind) === "summary"
    readonly property string amountText: F.signedAmount(fields)

    trailing: amountText

    Row {
        width: parent.width
        spacing: Theme.spacing.small
        visible: !body.summary && (amountLine.text.length > 0 || status.visible)
        ObjectCardLine {
            id: amountLine
            width: Math.min(implicitWidth, parent.width - status.width - parent.spacing)
            text: body.amountText
            font.pixelSize: Theme.typography.subtitleText
            font.weight: Theme.typography.weightMedium
            color: F.str(body.fields.direction) === "in" ? Theme.palette.success : Theme.palette.text
        }
        ObjectCardStatus {
            id: status
            anchors.verticalCenter: parent.verticalCenter
            text: F.statusLabel(body.fields.status)
            tone: F.statusTone(body.fields.status)
        }
    }
    ObjectCardMatchLine {
        secondary: true
        query: body.query
        plain: body.summary ? F.str(body.object.subtitle)
                           : [F.str(body.object.subtitle), F.str(body.fields.network)]
                               .filter(s => s.length > 0).join(" · ")
        maximumLineCount: 2
    }
}
