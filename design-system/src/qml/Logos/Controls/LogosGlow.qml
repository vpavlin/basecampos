import QtQuick
import QtQuick.Effects

import Logos.Theme

// The theme's hover glow (theme file `effects.hoverGlow`) around a rounded
// item. Put it behind the item, same geometry, and drive `active`:
//
//     LogosGlow { anchors.fill: tile; radius: tile.radius; active: root.hovered }
//
// Draws nothing when the theme sets no glow (built-in dark), so the default
// look is unchanged.
RectangularShadow {
    id: root

    property bool active: false
    readonly property var spec: Theme.hoverGlow

    visible: spec !== null && opacity > 0
    color: spec ? spec.color : "transparent"
    blur: spec ? spec.blur : 0
    spread: spec ? spec.spread : 0
    opacity: spec && active ? spec.opacity : 0
    offset: Qt.vector2d(0, 0)

    Behavior on opacity { NumberAnimation { duration: 140; easing.type: Easing.OutQuad } }
}
