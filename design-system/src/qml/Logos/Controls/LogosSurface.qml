import QtQuick
import QtQuick.Shapes

import Logos.Theme

// A filled, optionally rounded surface. If the active theme defines a
// gradient for `slot` (theme file `gradients.<slot>`), it draws that;
// otherwise it draws `color`, so themes without gradients (incl. Dark) look
// exactly as a plain Rectangle would.
//
//     background: LogosSurface {
//         slot: "sidebar"
//         color: Theme.palette.backgroundSecondary
//         radius: Theme.spacing.radiusXlarge
//     }
//
// Gradient slots: workspace, sidebar, primary, accent, header, card. `gradient`
// sets an explicit spec instead. Background-image slots: workspace,
// onboarding (drawn over the fill, with the theme's tint).
Item {
    id: root

    property string slot: ""
    property color color: "transparent"
    property real radius: 0
    property color borderColor: "transparent"
    property real borderWidth: 0

    // An explicit spec (same shape as a parsed theme gradient) wins over the
    // theme's slot; leave undefined to follow the theme.
    property var gradient: undefined

    readonly property var gradientSpec:
        root.gradient !== undefined ? root.gradient
        : root.slot.length > 0 && Theme.gradients[root.slot] ? Theme.gradients[root.slot] : null
    readonly property int stopCount: d.made.length
    readonly property bool hasGradient: gradientSpec !== null
    readonly property bool isRadial: hasGradient && gradientSpec.type === "radial"

    Rectangle {
        anchors.fill: parent
        visible: !root.hasGradient
        radius: root.radius
        color: root.color
        border.color: root.borderColor
        border.width: root.borderWidth
    }

    Shape {
        anchors.fill: parent
        visible: root.hasGradient
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeWidth: root.borderWidth > 0 ? root.borderWidth : -1
            strokeColor: root.borderColor
            fillGradient: root.isRadial ? d.radial : d.linear

            PathRectangle {
                x: 0
                y: 0
                width: root.width
                height: root.height
                radius: Math.min(root.radius, root.width / 2, root.height / 2)
            }
        }
    }

    // A theme background image for this slot, over the fill, with its tint.
    readonly property var backgroundSpec:
        root.slot.length > 0 && Theme.backgrounds[root.slot] ? Theme.backgrounds[root.slot] : null

    // Engine-defined motion (theme `motion`: strength 0..1, period seconds):
    // one slow phase drives everything, and nothing runs while hidden or when
    // the theme asks for no motion.
    readonly property var motionSpec:
        root.backgroundSpec && root.backgroundSpec.motion ? root.backgroundSpec.motion
        : root.gradientSpec && root.gradientSpec.motion ? root.gradientSpec.motion : null
    readonly property real motionStrength: motionSpec ? motionSpec.strength : 0
    property real phase: 0

    NumberAnimation on phase {
        from: 0
        to: 2 * Math.PI
        duration: (root.motionSpec ? root.motionSpec.period : 30) * 1000
        loops: Animation.Infinite
        running: root.motionStrength > 0 && root.visible && root.width > 0
    }

    Item {
        id: bgLayer
        anchors.fill: parent
        visible: root.backgroundSpec !== null
        clip: true

        readonly property real zoom: 1 + 0.08 * root.motionStrength
        readonly property real dx: width * 0.025 * root.motionStrength
        readonly property real dy: height * 0.015 * root.motionStrength
        readonly property int fillMode: {
            var fit = root.backgroundSpec ? root.backgroundSpec.fit : "cover"
            return fit === "contain" ? Image.PreserveAspectFit
                 : fit === "tile"    ? Image.Tile
                 : fit === "center"  ? Image.Pad
                                     : Image.PreserveAspectCrop
        }

        // Slow drift and zoom.
        Image {
            width: parent.width * bgLayer.zoom
            height: parent.height * bgLayer.zoom
            x: (parent.width - width) / 2 + Math.cos(root.phase) * bgLayer.dx
            y: (parent.height - height) / 2 + Math.sin(2 * root.phase) * bgLayer.dy
            source: root.backgroundSpec ? root.backgroundSpec.url : ""
            asynchronous: true
            opacity: root.backgroundSpec ? root.backgroundSpec.opacity : 1
            fillMode: bgLayer.fillMode
        }

        // A second, slightly larger copy drifting the other way and breathing
        // in and out: the light in the image appears to shift.
        Image {
            visible: root.motionStrength > 0
            width: parent.width * (bgLayer.zoom + 0.04)
            height: parent.height * (bgLayer.zoom + 0.04)
            x: (parent.width - width) / 2 - Math.cos(root.phase) * bgLayer.dx * 1.6
            y: (parent.height - height) / 2 - Math.sin(root.phase) * bgLayer.dy
            source: root.backgroundSpec ? root.backgroundSpec.url : ""
            asynchronous: true
            opacity: (0.5 - 0.5 * Math.cos(3 * root.phase)) * 0.35 * root.motionStrength
            fillMode: bgLayer.fillMode
        }

        Rectangle {
            anchors.fill: parent
            color: root.backgroundSpec ? root.backgroundSpec.tint : "transparent"
            opacity: root.backgroundSpec ? root.backgroundSpec.tintStrength : 0
        }
    }

    QtObject {
        id: d

        // CSS-like angle: 0 = left to right, 90 = top to bottom. The line runs
        // through the centre and is long enough to reach the far corners.
        readonly property real rad: root.hasGradient && !root.isRadial
                                    ? (root.gradientSpec.angle
                                       + Math.sin(root.phase) * 12 * root.motionStrength) * Math.PI / 180
                                    : 0
        readonly property real halfLength: (Math.abs(root.width * Math.cos(rad))
                                            + Math.abs(root.height * Math.sin(rad))) / 2

        property LinearGradient linear: LinearGradient {
            x1: root.width / 2 - Math.cos(d.rad) * d.halfLength
            y1: root.height / 2 - Math.sin(d.rad) * d.halfLength
            x2: root.width / 2 + Math.cos(d.rad) * d.halfLength
            y2: root.height / 2 + Math.sin(d.rad) * d.halfLength
        }

        property RadialGradient radial: RadialGradient {
            centerX: root.isRadial ? (root.gradientSpec.center[0]
                                      + Math.cos(root.phase) * 0.05 * root.motionStrength) * root.width : 0
            centerY: root.isRadial ? (root.gradientSpec.center[1]
                                      + Math.sin(2 * root.phase) * 0.03 * root.motionStrength) * root.height : 0
            centerRadius: root.isRadial ? root.gradientSpec.radius * Math.max(root.width, root.height) : 0
            focalX: centerX
            focalY: centerY
        }

        property Component stop: Component { GradientStop {} }

        property var made: []

        function rebuild() {
            for (var j = 0; j < d.made.length; ++j)
                d.made[j].destroy()
            d.made = []
            // Read the spec itself: this runs from onGradientSpecChanged, before
            // the bindings derived from it (hasGradient, isRadial) update.
            var spec = root.gradientSpec
            if (!spec)
                return
            var src = spec.stops
            var made = []
            for (var i = 0; i < src.length; ++i)
                made.push(d.stop.createObject(root, { position: src[i].position, color: src[i].color }))
            d.made = made
            ;(spec.type === "radial" ? d.radial : d.linear).stops = made
        }
    }

    onGradientSpecChanged: d.rebuild()
    Component.onCompleted: d.rebuild()
}
