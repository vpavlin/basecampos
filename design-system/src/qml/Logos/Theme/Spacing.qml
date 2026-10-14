import QtQuick

QtObject {    
    // Spacing scale
    readonly property int tiny: 4
    readonly property int small: 8
    readonly property int medium: 12
    readonly property int large: 16
    readonly property int xlarge: 20
    readonly property int xxlarge: 40
    
    // radius. The active theme can override any step and scale them all
    // (set by Theme.qml from the theme's `radius` block).
    property var radiusOverrides: ({})
    property real radiusScale: 1.0
    function _radius(key, fallback) {
        const v = radiusOverrides[key]
        return Math.round((v !== undefined ? v : fallback) * radiusScale)
    }

    readonly property int radiusTiny: _radius("tiny", 2)
    readonly property int radiusSmall: _radius("small", 4)
    readonly property int radiusMedium: _radius("medium", 6)
    readonly property int radiusLarge: _radius("large", 8)
    readonly property int radiusXlarge: _radius("xlarge", 16)
    readonly property int radiusPill: _radius("pill", 999)

    // Multiplier for shapes drawn as half their size (pills, round buttons):
    // `radius: height / 2 * Theme.spacing.roundness`. 0 = square corners.
    readonly property real roundness: radiusScale
}
