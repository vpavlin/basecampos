pragma Singleton
import QtQml

import "ObjectFormat.js" as F
import "ObjectHighlight.js" as H

// ObjectFormat.js for views that are not cards: a home or launcher that names
// its pills and modules the way the cards name their types.
QtObject {
    function typeLabel(type, plural) { return F.typeLabel(type, plural) }
    function relativeTime(t, now) { return F.relativeTime(t, now) }
    function clock(t) { return F.clock(t) }
    function timeOf(envelope) { return F.timeOf(envelope) }
    // `text` escaped as StyledText, what `query` matches in `color`
    // (ObjectHighlight.js, basecamp-experiments#46). For a Text whose
    // textFormat is Text.StyledText, always: the text is safe only escaped.
    function mark(text, query, color) { return H.mark(text, query, color) }
}
