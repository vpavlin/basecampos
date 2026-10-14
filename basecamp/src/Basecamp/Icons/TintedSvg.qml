import QtQuick

// An SVG drawn in a theme colour.
//
// `markup` is the SVG source with %COLOR% wherever it should take `color`. It
// is rebuilt as a data: URL, so it follows the active theme without the
// MultiEffect LogosIcon needs (and so it also renders offscreen).
Image {
    id: root

    property color color
    property string markup

    function _channel(v) { return Math.round(v * 255) }

    source: {
        var c = root.color
        var svg = markup.split("%COLOR%")
            .join("rgb(" + _channel(c.r) + "," + _channel(c.g) + "," + _channel(c.b) + ")")
        return "data:image/svg+xml;utf8," + encodeURIComponent(svg)
    }
    opacity: color.a
}
