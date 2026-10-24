import QtQuick

import Logos.Controls
import Logos.Theme

import "ObjectFormat.js" as F

// account (§4): name, network; balance { amount, unit, fiat? }.
ObjectCardBody {
    id: body
    readonly property var balance: fields.balance && typeof fields.balance === "object" ? fields.balance : ({})
    readonly property string balanceText: F.amount(balance.amount, balance.unit)

    heading: F.str(fields.name)
    trailing: balanceText

    ObjectCardLine {
        text: body.balanceText
        font.pixelSize: Theme.typography.subtitleText + 4
        font.weight: Theme.typography.weightBold
    }
    ObjectCardLine {
        secondary: true
        text: [F.str(body.balance.fiat), F.str(body.fields.network)].filter(s => s.length > 0).join(" · ")
    }
}
