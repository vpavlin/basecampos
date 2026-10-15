pragma Singleton
import QtQuick

import Logos.Theme

// Every icon can be replaced by the active theme's icon pack (Theme.iconOr).
QtObject {
    // Arrows
    readonly property url arrowLeft:        Theme.iconOr("arrowLeft", Qt.resolvedUrl("icons/arrow-left-s-line.svg"))
    readonly property url arrowLeftDouble:  Theme.iconOr("arrowLeftDouble", Qt.resolvedUrl("icons/arrow-left-double-line.svg"))
    readonly property url arrowRight:       Theme.iconOr("arrowRight", Qt.resolvedUrl("icons/arrow-right-s-line.svg"))
    readonly property url arrowRightDouble: Theme.iconOr("arrowRightDouble", Qt.resolvedUrl("icons/arrow-right-double-line.svg"))
    readonly property url triangleUp:       Theme.iconOr("triangleUp", Qt.resolvedUrl("icons/triangle_up.svg"))
    readonly property url triangleDown:     Theme.iconOr("triangleDown", Qt.resolvedUrl("icons/triangle_down.svg"))

    // Actions
    readonly property url refresh:  Theme.iconOr("refresh", Qt.resolvedUrl("icons/refresh.svg"))
    readonly property url install:  Theme.iconOr("install", Qt.resolvedUrl("icons/install.svg"))
    readonly property url trash:    Theme.iconOr("trash", Qt.resolvedUrl("icons/delete.svg"))
    readonly property url more:     Theme.iconOr("more", Qt.resolvedUrl("icons/more.svg"))
    readonly property url search:   Theme.iconOr("search", Qt.resolvedUrl("icons/search.svg"))
    readonly property url close:    Theme.iconOr("close", Qt.resolvedUrl("icons/close.svg"))
    readonly property url copy:     Theme.iconOr("copy", Qt.resolvedUrl("icons/copy.svg"))
    readonly property url check:    Theme.iconOr("check", Qt.resolvedUrl("icons/check.svg"))

    // View modes
    readonly property url grid:     Theme.iconOr("grid", Qt.resolvedUrl("icons/grid.svg"))
    readonly property url list:     Theme.iconOr("list", Qt.resolvedUrl("icons/list.svg"))
    readonly property url pages:    Theme.iconOr("pages", Qt.resolvedUrl("icons/pages.svg"))

    // Status
    readonly property url warning:  Theme.iconOr("warning", Qt.resolvedUrl("icons/warning.svg"))
    readonly property url info:     Theme.iconOr("info", Qt.resolvedUrl("icons/info.svg"))

    // Artwork
    //
    // Shipped PLAIN — the photograph as delivered, no grading, no tint. How
    // dark it wants to be depends on what is laid over it, which is the
    // consuming app's decision: blockchain-ui tints it for its setup screen,
    // and a future consumer may want it untouched. Baking a tint in here would
    // have made one app's choice everybody's.
    readonly property url onboardingBackdrop: Qt.resolvedUrl("icons/onboarding-tents.jpg")
}
