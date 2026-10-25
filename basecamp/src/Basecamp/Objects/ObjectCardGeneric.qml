import QtQuick

import Logos.Controls
import Logos.Theme

import "ObjectFormat.js" as F

// Any type without a body of its own, an app's `x.<app>.<name>` included:
// the title (in the card's heading) and the subtitle.
ObjectCardBody {
    id: body
    trailing: ""

    ObjectCardMatchLine { secondary: true; plain: F.str(body.object.subtitle); query: body.query; maximumLineCount: 2 }
}
