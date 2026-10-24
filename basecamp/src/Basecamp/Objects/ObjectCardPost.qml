import QtQuick

import Logos.Controls
import Logos.Theme

import "ObjectFormat.js" as F

// post (§4): community (a ref); author, authorName, text, score, likes.
ObjectCardBody {
    id: body
    readonly property string scoreText: typeof fields.score === "number" && isFinite(fields.score)
                                        ? qsTr("%1 points").arg(F.grouped(fields.score)) : ""
    readonly property string likesText: typeof fields.likes === "number" && isFinite(fields.likes)
                                        ? qsTr("%1 likes").arg(F.grouped(fields.likes)) : ""

    heading: F.str(fields.authorName)
    trailing: scoreText

    ObjectCardLine {
        text: F.str(body.fields.text).length > 0 ? F.str(body.fields.text) : F.str(body.object.title)
        maximumLineCount: 3
    }
    ObjectCardLine {
        secondary: true
        text: [body.scoreText, body.likesText].filter(s => s.length > 0).join(" · ")
    }
    ObjectCardLine { secondary: true; text: F.str(body.object.subtitle) }
}
