pragma Singleton
import QtQuick

import Logos.Theme

// Shell icons a theme's icon pack may replace (Theme.iconOr). The λ logo,
// the tents artwork and the workspace hexagon are not replaceable.
QtObject {
    readonly property url logo:       Qt.resolvedUrl("logos-lambda.svg")
    // Basecamp's own tile (the dock's first entry): a full-bleed square with
    // a smaller λ than the home app's icon, so it sits level with app icons.
    readonly property url basecampTile: Qt.resolvedUrl("basecamp-dock.svg")
    readonly property url tents:      Qt.resolvedUrl("tent.png")
    readonly property url dashboard:  Theme.iconOr("dashboard", Qt.resolvedUrl("dashboard.svg"))
    readonly property url filter:     Theme.iconOr("filter", Qt.resolvedUrl("filter.svg"))
    readonly property url modules:    Theme.iconOr("modules", Qt.resolvedUrl("module.svg"))
    readonly property url settings:   Theme.iconOr("settings", Qt.resolvedUrl("settings.svg"))
    readonly property url workspace:  Qt.resolvedUrl("workspace.svg")
}
