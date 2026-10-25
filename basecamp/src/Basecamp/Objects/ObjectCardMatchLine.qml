import QtQuick

import Logos.Theme

import "ObjectHighlight.js" as H

// An ObjectCardLine that marks what the search matched (basecamp-experiments#46):
// set `plain` (the publisher's text) and `query` (the bar's), never `text`.
// The text is always escaped and always StyledText, with or without a query,
// so nothing an app sent is read as markup at any moment (ADR-072).
ObjectCardLine {
    property string plain: ""
    property string query: ""

    textFormat: Text.StyledText
    text: H.mark(plain, query, Theme.palette.accentOrange)
}
