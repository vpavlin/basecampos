import QtQuick

import Logos.Controls
import Logos.Theme

import "ObjectFormat.js" as F

// notification (§4, ADR-042): level; body.
ObjectCardBody {
    id: body
    readonly property string level: F.str(fields.level)

    trailing: level

    ObjectCardMatchLine { plain: F.str(body.fields.body); query: body.query; maximumLineCount: 3 }
    ObjectCardStatus {
        text: body.level
        tone: body.level === "error" ? "error" : body.level === "warning" ? "warning" : "info"
    }
}
