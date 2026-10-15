import QtQuick
import QtQuick.Controls
import Logos.Theme
import Logos.Controls
import Basecamp.Icons

AbstractButton {
    id: root

    implicitHeight: 46
    checkable: true
    autoExclusive: true

    signal tooltipRequested(string text, real y)

    onHoveredChanged: {
        if (hovered && text) {
            var pos = root.mapToItem(null, root.width, root.height / 2)
            root.tooltipRequested(text, pos.y)
        }
    }

    // workspace.svg's hexagon, filled with backgroundInset (its #141414 in dark).
    background: Item {
      implicitWidth: hexagon.width
      implicitHeight: hexagon.height

      // The hexagon isn't a rounded rect, so the glow is a soft round one.
      LogosGlow {
          anchors.centerIn: hexagon
          width: hexagon.height * 0.9
          height: width
          radius: width / 2
          active: root.hovered
      }

      TintedSvg {
        id: hexagon
        width: 56
        height: 46
        anchors.centerIn: parent
        color: Theme.palette.backgroundInset
        fillMode: Image.PreserveAspectFit
        markup: '<svg width="58" height="50" viewBox="0 0 58 50" fill="none" xmlns="http://www.w3.org/2000/svg"><path d="M21.4484 0C21.3155 0.0181203 0 36.9232 0 36.9232C0 36.9232 7.36289 49.6436 7.56825 49.9336L50.2235 50L57.7615 36.9775L36.3916 0.048322L21.4545 0H21.4484Z" fill="%COLOR%"/></svg>'
      }
    }

    contentItem: Item {
        Image {
            id: glyph
            anchors.centerIn: parent
            width: 24
            height: 24
            source: root.icon.source
            fillMode: Image.PreserveAspectFit
        }

        Rectangle {
            objectName: "sidebar.workspaceActiveDot"
            visible: root.checked
            width: 4
            height: 4
            radius: width / 2 * Theme.spacing.roundness
            color: Theme.palette.accentOrange
            anchors.horizontalCenter: glyph.horizontalCenter
            anchors.top: glyph.bottom
            anchors.topMargin: Theme.spacing.tiny
        }
    }
}
