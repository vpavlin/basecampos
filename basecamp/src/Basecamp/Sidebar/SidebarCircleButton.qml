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
    background: Rectangle {
        radius: width / 2 * Theme.spacing.roundness
        color: root.hovered || root.checked ? Theme.palette.accentOrange: Theme.palette.surface
    }

    contentItem: Item {
        LogosIcon {
            id: appIcon
            anchors.centerIn: parent
            width: 24
            height: 24
            source: root.icon.source
            // Idle: textTertiary (dark's value is the icons' own #969696, so the
            // default look is unchanged); on the accent: accentForeground.
            color: root.hovered || root.checked ? Theme.palette.accentForeground
                                                : Theme.palette.textTertiary
            // The icons are grey (#969696) SVGs, and MultiEffect colorization
            // multiplies by their luminance: white x grey stays grey. Lift them
            // to white first so the tint lands exactly.
            brightness: 1.0
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
