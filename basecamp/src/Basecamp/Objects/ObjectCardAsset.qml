import QtQuick

import Logos.Controls
import Logos.Theme

import "ObjectFormat.js" as F

// asset (§4): name, symbol; amount, fiat, change (%).
ObjectCardBody {
    id: body
    readonly property string amountText: F.amount(fields.amount, fields.symbol)
    readonly property string changeText: F.change(fields.change)

    heading: F.str(fields.name)
    trailing: F.str(fields.fiat).length > 0 ? F.str(fields.fiat) : amountText

    ObjectCardLine {
        text: body.amountText
        font.pixelSize: Theme.typography.subtitleText
        font.weight: Theme.typography.weightMedium
    }
    Row {
        spacing: Theme.spacing.small
        visible: fiat.visible || change.visible
        ObjectCardLine { id: fiat; width: implicitWidth; secondary: true; text: F.str(body.fields.fiat) }
        ObjectCardLine {
            id: change
            width: implicitWidth
            secondary: true
            text: body.changeText
            color: body.fields.change > 0 ? Theme.palette.success
                 : body.fields.change < 0 ? Theme.palette.error : Theme.palette.textSecondary
        }
    }
}
