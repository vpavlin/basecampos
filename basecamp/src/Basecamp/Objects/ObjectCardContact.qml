import QtQuick

import Logos.Controls
import Logos.Theme

import "ObjectFormat.js" as F

// contact (§4): name, groups; subject (not shown: an opaque key).
ObjectCardBody {
    id: body
    heading: F.str(fields.name)
    trailing: F.list(fields.groups)

    ObjectCardLine { secondary: true; text: F.list(body.fields.groups) }
}
