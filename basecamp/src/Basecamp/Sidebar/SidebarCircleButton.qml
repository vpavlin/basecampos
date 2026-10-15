import QtQuick
import QtQuick.Controls
import Logos.Theme
import Logos.Controls

AbstractButton {
    id: root

    implicitHeight: 38
    implicitWidth: 38

    signal tooltipRequested(string text, real y)

    onHoveredChanged: {
        if (hovered && text) {
            var pos = root.mapToItem(null, root.width, root.height / 2)
            root.tooltipRequested(text, pos.y)
        }
    }

    // Dark gray pill background extending to left edge when active/highlighted
    background: Item {
        LogosGlow {
            anchors.fill: parent
            radius: circle.radius
            active: root.hovered
        }

        LogosSurface {
            id: circle
            anchors.fill: parent
            slot: root.hovered || root.checked ? "accent" : ""
            radius: width / 2 * Theme.spacing.roundness
            color: root.hovered || root.checked ? Theme.palette.accentOrange: Theme.palette.surface
        }
    }

    contentItem: Item {
        LogosIcon {
            id: appIcon
            anchors.centerIn: parent
            width: 24
            height: 24
            source: root.icon.source
            // On the accent: accentForeground. Idle: the untinted grey asset under
            // built-in dark (as before), textTertiary under an installed theme.
            // The SVGs are grey (#969696); MultiEffect colorization doesn't simply
            // multiply (a lifted icon stays light), so brightness 1.0 is only used
            // where a tint must land, never for dark's idle state.
            readonly property bool onAccent: root.hovered || root.checked
            color: onAccent ? Theme.palette.accentForeground
                            : Theme.tokenOr("textTertiary", "transparent")
            brightness: onAccent || Theme.currentTheme !== "dark" ? 1.0 : 0
            visible: !!root.icon.source &&
                     !(appIcon.status === Image.Null ||
                       appIcon.status === Image.Error)
        }
        LogosText {
            anchors.centerIn: parent
            text: root.text.substring(0, 4)
            font.pixelSize: Theme.typography.secondaryText
            font.weight: Theme.typography.weightBold
            color: Theme.palette.textSecondary
            visible: !appIcon.visible
        }
    }
}
