import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Logos.Theme
import Logos.Controls
import Logos.Icons

// Packages: a data table that adapts by window class (of the page's own width).
//   expanded (≥ 840): toolbar on one line, LogosTable with all 6 columns + a
//     ⋯ column, a pager sized to the rows that fit; a tap opens a 320 px detail
//     panel beside the table (✕ closes it), and Type/Updated move into it.
//   medium (600–839): same toolbar line; the table keeps Name, Version,
//     Status + ⋯ and scrolls (no pager); a tap opens the detail as a sheet over
//     the right of the table, where the dropped columns live.
//   compact (< 600): no table. Search on its own line, filter + Install below,
//     then one card-row per package (name + badge / version · size · updated /
//     ⋯ / ›). A tap opens a full-page detail; the top bar's ‹ goes back.
// Rows never depend on hover or double-click: one tap opens, ⋯ opens a menu.
Item {
    id: page

    readonly property string windowClass: Theme.windowClass(width)
    readonly property bool compact: windowClass === "compact"
    readonly property bool expanded: windowClass === "expanded"
    readonly property int margin: Theme.pageMargin(width)
    // Search, filter and Install share one height so the toolbar reads as a row.
    readonly property int toolbarHeight: Math.max(Theme.components.button.height, Theme.components.combo.height)

    // The package whose detail is shown ("" = none).
    property string openName: (typeof labState !== "undefined" && labState && labState.open === "1")
                              ? d.all[0].name : ""
    readonly property var openPkg: d.find(openName)

    readonly property string title: compact && openPkg ? openPkg.name : "Packages"
    readonly property bool canGoBack: compact && openPkg !== null
    function goBack() { openName = "" }

    // ─── Data ───
    QtObject {
        id: d

        // name, version, type, sizeKb, status, updated (ISO), signer, cid, about
        readonly property var all: [
            row("blockchain_module",  "0.3.0",  "Module",  18400, "Installed", "2026-10-13", "Logos", "Runs the Cryptarchia node and its wallet."),
            row("blockchain_ui",      "0.3.1",  "App",      4200, "Update",    "2026-10-11", "Logos", "Node dashboard: sync, peers, rewards."),
            row("wallet_module",      "0.2.6",  "Module",   6100, "Installed", "2026-10-02", "Logos", "Keys, notes and transfers."),
            row("keycard_module",     "0.2.1",  "Module",   2300, "Installed", "2026-09-18", "Logos", "Keycard smart-card signer."),
            row("chat_module",        "0.4.2",  "Module",   5400, "Installed", "2026-10-09", "Logos", "Private messaging over Delivery."),
            row("chat_ui",            "0.4.0",  "App",      3900, "Update",    "2026-09-30", "Logos", "Chat client."),
            row("delivery_module",    "0.3.0",  "Module",   9700, "Installed", "2026-10-05", "Logos", "Waku-based message delivery."),
            row("storage_module",     "3.0.3",  "Module",  12800, "Installed", "2026-09-27", "Logos", "Content-addressed storage node."),
            row("storage_ui",         "1.1.0",  "App",      2100, "Disabled",  "2026-08-14", "Logos", "Browse and pin storage content."),
            row("package_manager",    "0.5.0",  "Module",   1700, "Installed", "2026-10-15", "Logos", "Installs and updates packages."),
            row("capability_module",  "0.2.0",  "Module",    900, "Installed", "2026-09-01", "Logos", "Grants modules their permissions."),
            row("lez_core",           "0.1.4",  "Module",  31500, "Failed",    "2026-10-17", "Logos", "Execution-zone runtime (RISC0)."),
            row("tx_sender_module",   "0.1.2",  "Module",   1100, "Installed", "2026-09-22", "Logos", "Builds and sends transactions."),
            row("keystore_module",    "0.2.3",  "Module",   1400, "Installed", "2026-09-22", "Logos", "Encrypted key storage."),
            row("docs_assistant",     "0.2.0",  "App",     48200, "Installed", "2026-10-06", "xAlisher", "Offline docs.logos.co search."),
            row("radio_receiver",     "0.2.6",  "App",      3300, "Update",    "2026-10-08", "xAlisher", "Listen to Logos Radio."),
            row("radio_booth",        "0.2.3",  "App",      3600, "Installed", "2026-10-16", "xAlisher", "Broadcast to Logos Radio."),
            row("samizdat",           "0.1.0",  "App",      2800, "Disabled",  "2026-10-01", "xAlisher", "Zine library on Logos storage."),
            row("logos_node_1click",  "0.2.6",  "App",      1900, "Installed", "2026-09-29", "xAlisher", "One-click node setup."),
            row("accounts_ui",        "0.3.0",  "App",      2500, "Installed", "2026-10-13", "Logos", "Accounts and identities."),
            row("notifications_ui",   "0.1.1",  "App",      1200, "Update",    "2026-10-19", "Logos", "Notification centre."),
            row("theme_daybreak",     "1.0.0",  "Theme",     640, "Installed", "2026-10-14", "Logos", "A light theme."),
            row("theme_doomslayer",   "0.9.2",  "Theme",    1800, "Disabled",  "2026-09-12", "beachbum", "A loud theme."),
            row("logos_cpp_sdk",      "0.2.0",  "Library",  7600, "Installed", "2026-08-30", "Logos", "C++ SDK for modules."),
            row("qt_crypto_helpers",  "0.1.3",  "Library",   520, "Failed",    "2026-10-18", "community", "Hashing and encoding helpers.")
        ]

        function row(name, version, type, sizeKb, status, updated, signer, about) {
            var h = 0
            for (var i = 0; i < name.length; i++) h = (h * 31 + name.charCodeAt(i)) >>> 0
            return { name: name, version: version, type: type, sizeKb: sizeKb,
                     size: sizeKb >= 1024 ? (sizeKb / 1024).toFixed(1) + " MB" : sizeKb + " KB",
                     status: status, updated: updated, updatedShort: shortDate(updated),
                     signer: signer, about: about,
                     cid: "zDvZRwzm" + h.toString(16) + "q8Hk3" + (h * 7 >>> 0).toString(36) }
        }
        function shortDate(iso) {
            var m = ["Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"]
            var p = iso.split("-")
            return parseInt(p[2]) + " " + m[parseInt(p[1]) - 1]
        }
        function find(name) {
            for (var i = 0; i < all.length; i++) if (all[i].name === name) return all[i]
            return null
        }

        readonly property var types: ["All types", "Module", "App", "Library", "Theme"]
        property string query: ""
        property int typeIndex: 0
        property string sortRole: "name"
        property int sortOrder: Qt.AscendingOrder
        property int pageNo: 1
        // Expanded pages hold as many rows as the table shows, so nothing is cut.
        readonly property int pageSize: Math.max(5, Math.floor((table.height - table.headerHeight) / table.rowHeight))
        onPageSizeChanged: refilter()

        property var filtered: []
        function refilter() {
            var q = query.toLowerCase(), t = types[typeIndex]
            var out = all.filter(p => (typeIndex === 0 || p.type === t)
                                      && (q === "" || p.name.indexOf(q) >= 0))
            var key = sortRole === "size" ? "sizeKb" : sortRole
            var dir = sortOrder === Qt.AscendingOrder ? 1 : -1
            out.sort((a, b) => a[key] < b[key] ? -dir : a[key] > b[key] ? dir : 0)
            filtered = out
            fill(listModel, out)
            pageNo = Math.min(pageNo, Math.max(1, Math.ceil(out.length / pageSize)))
            fillPage()
        }
        function fillPage() {
            fill(pageModel, filtered.slice((pageNo - 1) * pageSize, pageNo * pageSize))
        }
        function fill(model, rows) {
            model.clear()
            for (var i = 0; i < rows.length; i++) model.append(rows[i])
        }
        onQueryChanged: refilter()
        onTypeIndexChanged: refilter()
        onPageNoChanged: fillPage()

        readonly property int updates: all.filter(p => p.status === "Update").length
    }
    ListModel { id: listModel }
    ListModel { id: pageModel }
    Component.onCompleted: d.refilter()

    function statusColor(s) {
        return s === "Installed" ? Theme.palette.success
             : s === "Update" ? Theme.palette.warning
             : s === "Failed" ? Theme.palette.error : Theme.palette.textSecondary
    }
    function statusLabel(s) { return s === "Update" ? "Update" : s }

    // ─── Row actions (⋯) ───
    property var menuPkg: null
    function openMenu(name, anchor) {
        menuPkg = d.find(name)
        rowMenu.popupUnder(anchor)
    }
    LogosMenu {
        id: rowMenu
        LogosMenuItem { text: "Open details"; onTriggered: page.openName = page.menuPkg.name }
        LogosMenuItem {
            text: "Update"
            enabled: page.menuPkg !== null && page.menuPkg.status === "Update"
        }
        LogosMenuItem { text: page.menuPkg && page.menuPkg.status === "Disabled" ? "Enable" : "Disable" }
        LogosMenuSeparator {}
        LogosMenuItem { text: "Uninstall" }
    }

    // ─── Reusable pieces ───
    component StatusBadge: LogosBadge {
        property string status
        text: page.statusLabel(status)
        color: page.statusColor(status)
    }

    component MoreButton: LogosIconButton {
        id: moreBtn
        property string pkgName
        flat: true
        iconSource: LogosIcons.more
        Accessible.name: "Actions for " + pkgName
        onClicked: page.openMenu(pkgName, moreBtn)
    }

    // Detail: every field of one package. Full page (compact), sheet (medium),
    // side panel (expanded).
    component Detail: Rectangle {
        id: detail
        property var pkg
        property bool closable: true
        color: Theme.palette.background

        Flickable {
            anchors.fill: parent
            contentHeight: detailCol.implicitHeight + 2 * pad
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            readonly property int pad: page.compact ? page.margin : Theme.spacing.xlarge

            ColumnLayout {
                id: detailCol
                x: parent.parent.pad
                y: parent.parent.pad
                width: detail.width - 2 * parent.parent.pad
                spacing: Theme.spacing.large

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacing.small
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacing.tiny
                        LogosText {
                            Layout.fillWidth: true
                            text: detail.pkg ? detail.pkg.name : ""
                            font.pixelSize: Theme.typography.panelTitleText
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }
                        RowLayout {
                            spacing: Theme.spacing.small
                            StatusBadge { status: detail.pkg ? detail.pkg.status : "" }
                            LogosText {
                                text: detail.pkg ? "v" + detail.pkg.version + " · " + detail.pkg.type : ""
                                color: Theme.palette.textSecondary
                            }
                        }
                    }
                    LogosIconButton {
                        visible: detail.closable
                        Layout.alignment: Qt.AlignTop
                        flat: true
                        iconSource: LogosIcons.close
                        Accessible.name: "Close details"
                        onClicked: page.openName = ""
                    }
                }

                LogosText {
                    Layout.fillWidth: true
                    text: detail.pkg ? detail.pkg.about : ""
                    wrapMode: Text.Wrap
                    color: Theme.palette.textSecondary
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: Theme.spacing.small
                    LogosButton {
                        visible: detail.pkg && detail.pkg.status === "Update"
                        text: "Update"
                        variant: LogosButton.Variant.Primary
                    }
                    LogosButton { text: detail.pkg && detail.pkg.status === "Disabled" ? "Enable" : "Disable" }
                    LogosButton { text: "Uninstall" }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: fields.implicitHeight
                    radius: Theme.spacing.radiusXlarge
                    color: Theme.palette.backgroundSecondary
                    clip: true
                    ColumnLayout {
                        id: fields
                        width: parent.width
                        spacing: 0
                        Repeater {
                            model: detail.pkg ? [
                                ["Version", detail.pkg.version], ["Type", detail.pkg.type],
                                ["Size", detail.pkg.size], ["Status", detail.pkg.status],
                                ["Updated", detail.pkg.updated], ["Signed by", detail.pkg.signer]
                            ] : []
                            LogosListRow {
                                required property var modelData
                                required property int index
                                Layout.fillWidth: true
                                title: modelData[0]
                                value: modelData[1]
                                showSeparator: index < 5
                            }
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacing.tiny
                    LogosText {
                        text: "Content ID"
                        font.pixelSize: Theme.typography.secondaryText
                        font.weight: Font.DemiBold
                        color: Theme.palette.textSecondary
                    }
                    LogosCopyableText {
                        Layout.fillWidth: true
                        text: detail.pkg ? detail.pkg.cid : ""
                        buttonSize: Theme.minTouchTarget
                    }
                }
            }
        }
    }

    // ═══ Layout: one toolbar grid, then cards (compact) or the table ═══
    ColumnLayout {
        anchors.fill: parent
        visible: !(page.compact && page.openPkg)
        spacing: 0

        // compact: search on its own line, filter + Install below it.
        // medium/expanded: search · filter · (space) · Install on one line.
        GridLayout {
            Layout.fillWidth: true
            Layout.margins: page.margin
            Layout.bottomMargin: page.compact ? Theme.spacing.small : Theme.spacing.large
            columns: page.compact ? 2 : 4
            columnSpacing: page.compact ? Theme.spacing.small : Theme.spacing.medium
            rowSpacing: Theme.spacing.small

            LogosSearchBar {
                id: search
                Layout.columnSpan: page.compact ? 2 : 1
                Layout.fillWidth: true
                Layout.maximumWidth: page.compact ? -1 : 420
                Layout.preferredWidth: page.compact ? -1 : 420
                Layout.preferredHeight: page.toolbarHeight
                placeholderText: "Search packages"
                onTextChanged: d.query = text
            }
            LogosComboBox {
                id: typeFilter
                Layout.fillWidth: page.compact
                Layout.preferredWidth: page.compact ? -1 : (page.expanded ? 160 : 132)
                Layout.preferredHeight: page.toolbarHeight
                model: d.types
                currentIndex: d.typeIndex
                onActivated: index => d.typeIndex = index
            }
            Item { visible: !page.compact; Layout.fillWidth: true }
            LogosButton {
                id: installButton
                Layout.preferredHeight: page.toolbarHeight
                text: "Install"
                variant: LogosButton.Variant.Primary
                leadingIcon.source: LogosIcons.install
                leadingIcon.color: Theme.palette.primaryForeground
            }
            LogosText {
                visible: page.compact
                Layout.columnSpan: 2
                text: listModel.count + " packages · " + d.updates + " updates"
                font.pixelSize: Theme.typography.secondaryText
                color: Theme.palette.textSecondary
            }
        }

        // ── compact: card rows ──
        ListView {
            id: cards
            visible: page.compact
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            model: page.compact ? listModel : null
            ScrollBar.vertical: LogosScrollBar {}

            delegate: LogosListRow {
                id: card
                required property var model
                required property int index
                width: ListView.view.width
                implicitHeight: 72
                leftPadding: page.margin
                rightPadding: Theme.spacing.small
                showSeparator: index < cards.count - 1
                Accessible.name: model.name + ", " + model.status
                onClicked: page.openName = model.name

                contentItem: RowLayout {
                    spacing: Theme.spacing.small
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacing.tiny
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacing.small
                            LogosText {
                                Layout.fillWidth: true
                                text: card.model.name
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }
                            StatusBadge { status: card.model.status }
                        }
                        LogosText {
                            Layout.fillWidth: true
                            text: card.model.version + " · " + card.model.size + " · " + card.model.updatedShort
                            font.pixelSize: Theme.typography.secondaryText
                            color: Theme.palette.textSecondary
                            elide: Text.ElideRight
                        }
                    }
                    MoreButton { pkgName: card.model.name }
                    LogosIcon {
                        source: LogosIcons.arrowRight
                        color: Theme.palette.textSecondary
                        Layout.preferredWidth: 20
                        Layout.preferredHeight: 20
                    }
                }
            }
        }

        // ── medium + expanded: table, detail beside (expanded) or over (medium) ──
        RowLayout {
            visible: !page.compact
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: page.margin
            Layout.rightMargin: page.margin
            Layout.bottomMargin: page.margin
            spacing: Theme.spacing.large

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Theme.spacing.medium

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    LogosTable {
                        id: table
                        anchors.fill: parent
                        model: page.compact ? null : page.expanded ? pageModel : listModel
                        rowHeight: Math.max(56, Theme.minTouchTarget + Theme.spacing.small)
                        // DS header is 36 px; on touch it is a row of sort targets.
                        headerHeight: Theme.density === "touch" ? Theme.minTouchTarget : 36
                        sortRole: d.sortRole
                        sortOrder: d.sortOrder
                        emptyText: "No packages match"
                        columns: !page.expanded ? page.mediumColumns
                               : page.openPkg ? page.panelColumns : page.wideColumns
                        onRowClicked: (index, row) => page.openName = row.name
                        onSortRequested: (role, order) => {
                            d.sortRole = role; d.sortOrder = order; d.refilter()
                        }
                    }

                    // Medium: the detail is a sheet over the right of the table.
                    Rectangle {
                        visible: !page.expanded && page.openPkg !== null
                        anchors { top: parent.top; bottom: parent.bottom; right: parent.right }
                        width: Math.min(340, parent.width * 0.75)
                        radius: Theme.spacing.radiusXlarge
                        color: Theme.palette.background
                        border.color: Theme.palette.border
                        clip: true
                        Detail {
                            anchors.fill: parent
                            anchors.margins: 1
                            color: "transparent"
                            pkg: page.openPkg
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    LogosText {
                        Layout.fillWidth: true
                        text: listModel.count + " packages · " + d.updates + " updates"
                        font.pixelSize: Theme.typography.secondaryText
                        color: Theme.palette.textSecondary
                        elide: Text.ElideRight
                    }
                    LogosPaginator {
                        visible: page.expanded
                        totalCount: listModel.count
                        pageSize: d.pageSize
                        currentPage: d.pageNo
                        showFirstLast: false
                        onPageRequested: p => d.pageNo = p
                    }
                }
            }

            // Expanded: the detail is a panel beside the table.
            Rectangle {
                visible: page.expanded && page.openPkg !== null
                Layout.preferredWidth: 320
                Layout.fillHeight: true
                radius: Theme.spacing.radiusXlarge
                color: Theme.palette.backgroundSecondary
                clip: true
                Detail {
                    anchors.fill: parent
                    color: "transparent"
                    pkg: page.openPkg
                }
            }
        }
    }

    // compact: full-page detail; the top bar's ‹ goes back.
    Detail {
        anchors.fill: parent
        visible: page.compact && page.openPkg !== null
        pkg: page.openPkg
        closable: false
    }
    // ─── Columns ───
    Component {
        id: statusCell
        Item {
            StatusBadge { anchors.verticalCenter: parent.verticalCenter; status: rowItem ? rowItem.status : "" }
        }
    }
    Component {
        id: moreCell
        Item {
            MoreButton { anchors.centerIn: parent; pkgName: rowItem ? rowItem.name : "" }
        }
    }
    Component {
        id: nameCell
        Item {
            LogosText {
                anchors.fill: parent
                verticalAlignment: Text.AlignVCenter
                text: rowItem ? rowItem.name : ""
                font.weight: Font.DemiBold
                color: rowItem && rowItem.name === page.openName ? Theme.palette.accentOrange : Theme.palette.text
                elide: Text.ElideRight
            }
        }
    }

    property list<QtObject> wideColumns: [
        LogosTableColumn { title: "Name"; role: "name"; sortable: true; minWidth: 160; preferredWidth: 200; fillWidth: true; cellDelegate: nameCell },
        LogosTableColumn { title: "Version"; role: "version"; minWidth: 80 },
        LogosTableColumn { title: "Type"; role: "type"; minWidth: 80 },
        LogosTableColumn { title: "Size"; role: "size"; sortable: true; minWidth: 84; alignment: Qt.AlignRight | Qt.AlignVCenter },
        LogosTableColumn { title: "Status"; role: "status"; minWidth: 110; cellDelegate: statusCell },
        LogosTableColumn { title: "Updated"; role: "updated"; sortable: true; minWidth: 110 },
        LogosTableColumn { minWidth: 56; maxWidth: 56; cellPadding: 0; cellDelegate: moreCell }
    ]
    // Expanded with the detail panel open: Type and Updated move to the panel.
    property list<QtObject> panelColumns: [
        LogosTableColumn { title: "Name"; role: "name"; sortable: true; minWidth: 160; fillWidth: true; cellDelegate: nameCell },
        LogosTableColumn { title: "Version"; role: "version"; minWidth: 80 },
        LogosTableColumn { title: "Size"; role: "size"; sortable: true; minWidth: 84; alignment: Qt.AlignRight | Qt.AlignVCenter },
        LogosTableColumn { title: "Status"; role: "status"; minWidth: 110; cellDelegate: statusCell },
        LogosTableColumn { minWidth: 56; maxWidth: 56; cellPadding: 0; cellDelegate: moreCell }
    ]
    property list<QtObject> mediumColumns: [
        LogosTableColumn { title: "Name"; role: "name"; sortable: true; minWidth: 150; fillWidth: true; cellDelegate: nameCell },
        LogosTableColumn { title: "Version"; role: "version"; minWidth: 80 },
        LogosTableColumn { title: "Status"; role: "status"; minWidth: 110; cellDelegate: statusCell },
        LogosTableColumn { minWidth: 56; maxWidth: 56; cellPadding: 0; cellDelegate: moreCell }
    ]
}
