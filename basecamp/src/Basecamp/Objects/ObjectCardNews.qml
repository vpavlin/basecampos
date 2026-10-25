import QtQuick

import Logos.Controls
import Logos.Theme

import "ObjectFormat.js" as F

// news (§4): source, url (shown as text, never opened by the shell); text.
ObjectCardBody {
    id: body
    trailing: F.str(fields.source)

    ObjectCardMatchLine { plain: F.str(body.fields.text); query: body.query; maximumLineCount: 3; secondary: true }
    ObjectCardLine {
        secondary: true
        text: F.str(body.fields.source)
        color: Theme.palette.textTertiary
    }
}
