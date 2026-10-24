pragma Singleton
import QtQml

import "ObjectFormat.js" as F

// ObjectFormat.js for views that are not cards: a home or launcher that names
// its pills and modules the way the cards name their types.
QtObject {
    function typeLabel(type, plural) { return F.typeLabel(type, plural) }
    function relativeTime(t, now) { return F.relativeTime(t, now) }
    function clock(t) { return F.clock(t) }
    function timeOf(envelope) { return F.timeOf(envelope) }
}
