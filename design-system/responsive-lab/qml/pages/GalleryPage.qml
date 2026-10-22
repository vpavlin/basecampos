import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Logos.Theme
import Logos.Controls
import Logos.Icons

// Components: one labelled instance of every DS control a user taps or types
// into, grouped, so each density can be measured on one screen. Rule: groups
// flow into as many columns as fit at a minimum width of 280 (1 column on a
// phone, 2 on a medium window, 3 on desktop). Every caption prints the
// control's measured size and turns red under Theme.minTouchTarget, so the
// screenshot is the measurement. Controls are NOT padded to 44 here on
// purpose: this page shows them as they ship.
//
// Lab state: scroll=<px> sets contentY; open=menu|combo|info|tooltip opens
// that popup so its rows can be measured too.
Flickable {
    id: page
    readonly property string title: "Components"
    readonly property string windowClass: Theme.windowClass(width)
    readonly property var labState: parent && parent.labState ? parent.labState : ({})
    readonly property int margin: Theme.pageMargin(width)
    readonly property int minGroupWidth: 280
    readonly property int gap: Theme.spacing.large
    // Bumped once laid out, so parts created lazily (repeaters, list rows)
    // are re-read by the captions.
    property int tick: 0
    readonly property int columns: Math.max(1, Math.floor((width - 2 * margin + gap) / (minGroupWidth + gap)))

    contentHeight: grid.implicitHeight + 2 * margin
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    Component.onCompleted: if (labState.scroll !== undefined)
        contentY = Math.max(0, Math.min(Number(labState.scroll), contentHeight - height))

    function sz(item) {
        return item ? Math.round(item.width) + "×" + Math.round(item.height) : "?"
    }
    function small(item) {
        return item && (item.width < Theme.minTouchTarget - 0.5 || item.height < Theme.minTouchTarget - 0.5)
    }

    // A titled card holding a column of Specs.
    component Group: ColumnLayout {
        property string heading
        default property alias specs: box.data
        spacing: Theme.spacing.small
        Layout.fillWidth: true
        Layout.alignment: Qt.AlignTop
        Layout.preferredWidth: 1   // equal shares of the row
        LogosText {
            text: heading
            font.pixelSize: Theme.typography.secondaryText
            font.weight: Font.DemiBold
            color: Theme.palette.textSecondary
            Layout.leftMargin: Theme.spacing.large
        }
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: box.implicitHeight + 2 * Theme.spacing.large
            radius: Theme.spacing.radiusXlarge
            color: Theme.palette.backgroundSecondary
            ColumnLayout {
                id: box
                x: Theme.spacing.large; y: Theme.spacing.large
                width: parent.width - 2 * Theme.spacing.large
                spacing: Theme.spacing.large
            }
        }
    }

    // One control with its caption: "Name  W×H" (+ named parts), red when
    // any measured part is under the 44 px touch minimum.
    component Spec: ColumnLayout {
        id: spec
        property string name
        property Item target: content.children.length ? content.children[0] : null
        property var parts: []          // [[label, item], …] tappable sub-parts
        property bool fill: false       // stretch the control to the card width
        property bool tappable: true    // false: shown for its hover behaviour only
        default property alias body: content.data
        spacing: Theme.spacing.tiny
        Layout.fillWidth: true

        readonly property string measured: {
            page.tick
            var s = page.sz(target)
            for (var i = 0; i < parts.length; ++i)
                s += "  " + parts[i][0] + " " + page.sz(parts[i][1])
            return s
        }
        readonly property bool fails: {
            page.tick
            if (!tappable) return false
            if (page.small(target)) return true
            for (var i = 0; i < parts.length; ++i)
                if (page.small(parts[i][1])) return true
            return false
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing.small
            LogosText {
                text: spec.name
                font.pixelSize: Theme.typography.secondaryText
                font.weight: Font.DemiBold
                color: Theme.palette.textSecondary
            }
            LogosText {
                Layout.fillWidth: true
                text: spec.measured
                font.pixelSize: Theme.typography.secondaryText
                color: !spec.tappable ? Theme.palette.textTertiary
                     : spec.fails ? Theme.palette.error : Theme.palette.success
                wrapMode: Text.Wrap
            }
        }
        Item {
            id: content
            Layout.fillWidth: true
            implicitHeight: spec.target ? spec.target.height : 0
            Component.onCompleted: if (spec.fill && spec.target) spec.target.width = Qt.binding(() => content.width)
        }
    }

    // Log every caption once laid out, so a run can be read without scrolling.
    Timer { interval: 500; running: true; onTriggered: page.tick++ }
    Timer {
        interval: 900; running: true
        onTriggered: {
            var out = []
            function walk(it) {
                for (var i = 0; i < it.children.length; ++i) {
                    var c = it.children[i]
                    if (c.measured !== undefined && c.name !== undefined)
                        out.push(c.name + " = " + c.measured + (!c.tappable ? "  (not tappable)" : c.fails ? "  FAIL" : "  ok"))
                    walk(c)
                }
            }
            walk(grid)
            console.warn("GALLERY density=" + Theme.density + " width=" + page.width + " columns=" + page.columns + " contentHeight=" + Math.round(page.contentHeight) + " viewport=" + page.height + "\n  " + out.join("\n  "))
        }
    }

    GridLayout {
        id: grid
        x: page.margin
        y: page.margin
        width: page.width - 2 * page.margin
        columns: page.columns
        columnSpacing: page.gap
        rowSpacing: Theme.spacing.xlarge

        Group {
            heading: "Buttons"
            Spec { name: "LogosButton primary"; LogosButton { text: "Install"; variant: LogosButton.Variant.Primary } }
            Spec { name: "LogosButton secondary"; LogosButton { text: "Cancel" } }
            Spec { name: "LogosButton compact"; LogosButton { text: "Edit"; compact: true } }
            Spec { name: "LogosButton icon"; LogosButton { text: "Refresh"; leadingIcon.source: LogosIcons.refresh } }
            Spec { name: "LogosIconButton"; LogosIconButton { iconSource: LogosIcons.more } }
            Spec { name: "LogosIconButton flat"; LogosIconButton { iconSource: LogosIcons.trash; flat: true } }
            Spec { name: "LogosCopyButton"; LogosCopyButton { value: "0x6882b350" } }
            Spec { name: "LogosToolButton"; LogosToolButton { text: "B" } }
            Spec {
                name: "LogosInfoButton"
                LogosInfoButton { id: infoBtn; title: "Slot"; text: "Consensus slot of the current chain tip." }
            }
            Spec { name: "LogosLink"; LogosLink { text: "Read the docs"; href: "https://docs.logos.co" } }
            Spec { name: "LogosTile"; LogosTile { label: "Wallet" } }
        }

        Group {
            heading: "Toggles"
            Spec {
                id: cbSpec; name: "LogosCheckbox"
                parts: [["box", cb.indicatorItem]]
                LogosCheckbox { id: cb; text: "Remember me"; checked: true }
            }
            Spec {
                name: "LogosRadioButton"
                parts: [["dot", rb.indicatorItem]]
                LogosRadioButton { id: rb; text: "Testnet"; checked: true }
            }
            Spec { name: "LogosRadioButton (off)"; LogosRadioButton { text: "Mainnet" } }
            Spec {
                name: "LogosSwitch"
                parts: [["track", sw.indicatorItem]]
                LogosSwitch { id: sw; text: "Run a node"; checked: true }
            }
            Spec {
                name: "LogosListRow + switch"; fill: true
                LogosListRow { title: "Use Tor"; showSeparator: false; accessory: LogosSwitch {} }
            }
        }

        Group {
            heading: "Inputs"
            Spec { name: "LogosTextField"; fill: true; LogosTextField { placeholderText: "Display name" } }
            Spec {
                name: "LogosSearchBar"; fill: true
                parts: [["clear", search.clearItem]]
                LogosSearchBar { id: search; text: "node"; placeholderText: "Search apps" }
            }
            Spec { name: "LogosTextArea"; fill: true; LogosTextArea { placeholderText: "Message"; implicitHeight: 80 } }
            Spec {
                name: "LogosSpinBox"
                parts: [["−", spin.down.indicator], ["+", spin.up.indicator]]
                LogosSpinBox { id: spin; value: 3; from: 0; to: 10 }
            }
            Spec {
                name: "LogosSlider"; fill: true
                parts: [["handle", slider.handle]]
                LogosSlider { id: slider; value: 0.4 }
            }
            Spec {
                name: "LogosCopyableText"
                parts: [["copy", copyable.copyButtonItem]]
                LogosCopyableText { id: copyable; text: "6882b350…a91f" ; copyText: "6882b350a91f" }
            }
        }

        Group {
            heading: "Selection"
            Spec {
                name: "LogosComboBox"; fill: true
                LogosComboBox { id: combo; model: ["Testnet", "Devnet", "Mainnet"]; currentIndex: 0 }
            }
            Spec {
                name: "LogosItemDelegate"; fill: true
                LogosItemDelegate { text: "Blockchain node"; highlighted: true }
            }
            Spec {
                name: "LogosListRow"; fill: true
                LogosListRow { title: "Network"; value: "Testnet"; showChevron: true; showSeparator: false }
            }
            Spec {
                id: menuSpec; name: "LogosMenu / MenuItem"
                parts: page.tick >= 0 ? [["item", menu.itemAt(0)]] : []
                LogosButton {
                    id: menuBtn; text: "Actions"; trailingIcon.source: LogosIcons.triangleDown
                    onClicked: menu.popupUnder(menuBtn)
                    LogosMenu {
                        id: menu
                        LogosMenuItem { text: "Rename" }
                        LogosMenuItem { text: "Duplicate" }
                        LogosMenuSeparator {}
                        LogosMenuItem { text: "Delete" }
                    }
                }
            }
        }

        Group {
            heading: "Navigation"
            Spec {
                name: "LogosTabBar / TabButton"; fill: true
                parts: [["tab", tab1]]
                LogosTabBar {
                    LogosTabButton { id: tab1; text: "Overview" }
                    LogosTabButton { text: "Logs" }
                    LogosTabButton { text: "Peers" }
                }
            }
            Spec {
                name: "LogosPaginator"; fill: true
                parts: page.tick >= 0 ? [["‹", pager.prevButton], ["cell", pager.pageCells.itemAt(0)],
                                         ["needs", { width: pager.implicitWidth, height: pager.implicitHeight }]] : []
                LogosPaginator { id: pager; totalCount: 200; pageSize: 20; currentPage: 4; showFirstLast: page.windowClass !== "compact" }
            }
            Spec {
                name: "LogosStageLane (interactive)"; fill: true
                parts: page.tick >= 0 ? [["stage", lane.hitAreaAt(0)]] : []
                LogosStageLane {
                    id: lane; interactive: true; currentIndex: 1
                    stages: [ LogosStage { label: "Started" }, LogosStage { label: "Online" }, LogosStage { label: "Earning" } ]
                }
            }
            Spec {
                name: "LogosToolBar + ToolButton"; fill: true
                parts: [["tool", toolA]]
                LogosToolBar {
                    RowLayout {
                        spacing: Theme.spacing.tiny
                        LogosToolButton { id: toolA; text: "↺" }
                        LogosToolButton { text: "✎" }
                        LogosToolSeparator {}
                        LogosToolButton { text: "⋯" }
                    }
                }
            }
        }

        Group {
            heading: "Feedback"
            Spec {
                name: "LogosNotice closable"; fill: true
                parts: [["close", notice.closeButtonItem]]
                LogosNotice {
                    id: notice; severity: LogosNotice.Warning; closable: true
                    title: "Node behind"; message: "12 slots behind the tip."
                    actions: [ LogosButton { text: "Retry"; compact: true } ]
                }
            }
            Spec {
                name: "LogosToast"; fill: true
                parts: [["close", toast.closeButtonItem]]
                LogosToast { id: toast; duration: 0; closable: true; severity: LogosNotice.Success; title: "Published"; message: "logos-chat-ui 0.2.2 is live."
                             Component.onCompleted: show() }
            }
            Spec {
                name: "LogosToolTip (hover-only)"; tappable: false
                LogosText {
                    text: "Hover for a hint"
                    color: Theme.palette.textSecondary
                    HoverHandler { id: tipHover }
                    LogosToolTip {
                        text: "Shown only on hover"
                        placement: LogosToolTip.Top
                        visible: tipHover.hovered || page.labState.open === "tooltip"
                    }
                }
            }
        }

        Group {
            heading: "Data"
            Spec {
                name: "LogosStatCard interactive"; fill: true
                LogosStatCard { interactive: true; label: "Height"; value: "182 334"; caption: "12 slots behind"; minimumHeight: 0 }
            }
            Spec {
                name: "LogosTable (multi-select)"; fill: true
                parts: page.tick >= 0 ? [["header", { width: table.width, height: table.headerHeight }],
                                         ["row", table.view.itemAtIndex(0)]] : []
                LogosTable {
                    id: table
                    implicitHeight: headerHeight + 3 * rowHeight + 2
                    selectionMode: LogosTable.Multi
                    model: [ { name: "wallet", ver: "0.4.1" }, { name: "chat", ver: "0.2.2" }, { name: "storage", ver: "3.0.3" } ]
                    columns: [
                        LogosTableColumn { title: "Module"; role: "name"; minWidth: 100; fillWidth: true; sortable: true },
                        LogosTableColumn { title: "Version"; role: "ver"; minWidth: 70 }
                    ]
                }
            }
        }
    }

    Timer {
        interval: 300; running: !!page.labState.open
        onTriggered: {
            var o = page.labState.open
            if (o === "menu") menu.popupUnder(menuBtn)
            else if (o === "combo") combo.popup.open()
            else if (o === "info") infoBtn.open()
        }
    }
}
