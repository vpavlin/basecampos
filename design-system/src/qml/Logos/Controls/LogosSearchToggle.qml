import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Logos.Theme
import Logos.Icons

// Search for compact layouts (docs/RESPONSIVE.md §4): folded, a 44 px search
// icon at the right end of the navigation bar; tapped, it unfolds across the
// bar into a search field with ✕ (clears and folds). It stays unfolded while
// it holds text; Esc folds it. Wider windows keep a normal LogosSearchBar.
//
// Put it last in a bar's RowLayout and hide the bar's title while unfolded:
//
//   RowLayout {
//       LogosText { visible: !search.expanded; Layout.fillWidth: true; text: "Apps" }
//       LogosSearchToggle { id: search; placeholderText: "Search apps…" }
//   }
//   // filter with search.text
//
// Properties:
//   text, placeholderText   the query and its placeholder
//   expanded                read/write; true while the field is shown
// Signals: submitted(string text)
Item {
    id: root

    property alias text: bar.text
    property alias placeholderText: bar.placeholderText
    property bool expanded: false
    readonly property alias searchBar: bar
    signal submitted(string text)

    Layout.fillWidth: expanded
    Layout.preferredWidth: expanded ? -1 : Theme.minTouchTarget
    Layout.preferredHeight: Theme.minTouchTarget
    implicitWidth: expanded ? 240 : Theme.minTouchTarget
    implicitHeight: Theme.minTouchTarget

    function open() {
        expanded = true
        bar.textInput.forceActiveFocus()
    }
    function close() {
        bar.text = ""
        expanded = false
    }

    LogosIconButton {
        objectName: "searchToggle.open"
        visible: !root.expanded
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        size: Theme.minTouchTarget
        flat: true
        iconSource: LogosIcons.search
        Accessible.name: qsTr("Search")
        onClicked: root.open()
    }

    RowLayout {
        visible: root.expanded
        anchors.fill: parent
        spacing: Theme.spacing.tiny

        LogosSearchBar {
            id: bar
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.minTouchTarget
            // ✕ below folds it; the bar's own clear would leave it open and empty.
            clearable: false
            onSubmitted: text => root.submitted(text)
            Keys.onEscapePressed: root.close()
        }
        LogosIconButton {
            objectName: "searchToggle.close"
            Layout.preferredWidth: Theme.minTouchTarget
            Layout.preferredHeight: Theme.minTouchTarget
            size: Theme.minTouchTarget
            flat: true
            iconSource: LogosIcons.close
            Accessible.name: qsTr("Close search")
            onClicked: root.close()
        }
    }
}
