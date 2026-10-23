import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Logos.Controls
import Logos.Icons
import Logos.Theme

import Basecamp.Backend
import Basecamp.Common

// Settings → Apps Inspector. Lists the UI plugins discovered in the plugins
// directory with their load state and a Load/Unload toggle per row. Uninstall
// is deliberately absent — module management lives in the Package Manager;
// this view is read-only apart from the load toggle.
//
// Formerly the "UI Modules" tab of ModulesView. Split out into its own Settings
// section so it stands alongside Module Inspector instead of hiding behind a
// tab, and restyled onto LogosTable + InspectorPanelHeader so it reads the same
// as the App Manager and Package Manager panels.
Item {
    id: root
    objectName: "appsInspectorView"

    // ─── Public API ───
    // View emits, ContentViews calls the backend — see the Settings wiring in
    // Basecamp/Shell/ContentViews.qml. `searchText` is owned by SettingsView
    // (one shared page-level search bar, styled like AppManagerView).
    property var sourceModel: null
    property bool loading: false
    property string searchText: ""

    signal reloadRequested()
    signal loadRequested(string name)
    signal unloadRequested(string name)

    // ─── Responsive (RESPONSIVE.md §3.5), by this pane's own width ───
    // ≥ 760 the full table; 600–759 three key columns; < 600 cards.
    readonly property bool cards: width < Theme.breakpoints.medium
    readonly property bool fullTable: width >= 760
    readonly property bool touch: cards || Theme.density === "touch"
    onVisibleChanged: if (!visible) detailSheet.close()

    ModulesFilterProxy {
        id: tableModel

        sourceModel:  root.sourceModel
        searchText:   root.searchText
        sortRoleName: appsTable.sortRole
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: Theme.spacing.large
        anchors.bottomMargin: Theme.spacing.large
        spacing: Theme.spacing.medium

        InspectorPanelHeader {
            id: header

            Layout.fillWidth: true
            Layout.leftMargin: Theme.spacing.large
            Layout.rightMargin: Theme.spacing.large
            Layout.minimumWidth: 0

            title: qsTr("Apps Inspector")
            subtitle: qsTr("UI plugins available in this installation.")
            reloadObjectName: "appsInspector.reloadButton"
            loading: root.loading
            touch: root.touch
            visibleCount: tableModel.visibleCount
            totalCount: tableModel.totalCount

            onReloadClicked: root.reloadRequested()
        }

        LogosTable {
            id: appsTable
            objectName: "appsInspector.table"

            visible: !root.cards
            Layout.fillWidth: true
            Layout.fillHeight: true

            model: tableModel
            rowHeight: 56
            sortRole: "label"
            sortOrder: Qt.AscendingOrder
            emptyText: tableModel.totalCount === 0
                       ? qsTr("No UI plugins found in the plugins directory.")
                       : qsTr("No apps match the current filter.")

            // Medium: a tap on a row opens its detail too (single tap, no hover).
            onRowClicked: (index, row) => { if (!root.fullTable) detailSheet.openFor(row) }

            onSortRequested: function(role, order) {
                appsTable.sortRole = role
                appsTable.sortOrder = order
                tableModel.applySortOrder(order)
            }

            // Keep every row instantiated. The list is short (tens of rows at
            // most) and UI automation looks items up by rendered text, which
            // only finds delegates the ListView has actually created.
            Component.onCompleted: if (view) view.cacheBuffer = 20000

            // Full table when it fits; three key columns when it doesn't
            // (RESPONSIVE.md §3.5, medium). Narrower still: cards (see below).
            columns: root.fullTable
                     ? [cols.app, cols.version, cols.status, cols.description, cols.actions]
                     : [cols.app, cols.status, cols.actions, cols.more]

            QtObject {
                id: cols
                property LogosTableColumn app: LogosTableColumn {
                    title: qsTr("App")
                    role: "label"
                    minWidth: 170
                    preferredWidth: 230
                    sortable: true
                    cellDelegate: appCellComponent
                }
                property LogosTableColumn version: LogosTableColumn {
                    title: qsTr("Version")
                    role: "version"
                    minWidth: 80
                    preferredWidth: 100
                    sortable: true
                    cellDelegate: versionCellComponent
                }
                property LogosTableColumn status: LogosTableColumn {
                    title: qsTr("Status")
                    role: "statusText"
                    minWidth: 110
                    preferredWidth: 130
                    sortable: true
                    cellDelegate: statusCellComponent
                }
                property LogosTableColumn description: LogosTableColumn {
                    title: qsTr("Description")
                    role: "description"
                    minWidth: 160
                    preferredWidth: 200
                    fillWidth: true
                }
                property LogosTableColumn actions: LogosTableColumn {
                    title: ""
                    minWidth: 200
                    preferredWidth: 220
                    fillWidth: !root.fullTable
                    alignment: Qt.AlignRight | Qt.AlignVCenter
                    cellDelegate: actionsCellComponent
                }
                property LogosTableColumn more: LogosTableColumn {
                    title: ""
                    minWidth: 56
                    preferredWidth: 56
                    maxWidth: 56
                    cellPadding: 0
                    alignment: Qt.AlignHCenter | Qt.AlignVCenter
                    cellDelegate: moreCellComponent
                }
            }

            Component {
                id: appCellComponent

                RowLayout {
                    spacing: Theme.spacing.small

                    // Icon tile — falls back to the first two letters of the
                    // module name when the plugin ships no icon.
                    Rectangle {
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                        Layout.alignment: Qt.AlignVCenter
                        radius: Theme.spacing.radiusMedium
                        color: Theme.palette.backgroundButton

                        readonly property bool hasIcon:
                            rowItem && String(rowItem.iconPath || "").length > 0

                        Image {
                            anchors.centerIn: parent
                            visible: parent.hasIcon
                            // Other apps' icons are outside this app's sandbox: the host serves them.
                            source: parent.hasIcon ? "image://basecamp/appicon/" + rowItem.name : ""
                            sourceSize.width: 22
                            sourceSize.height: 22
                        }

                        LogosText {
                            anchors.centerIn: parent
                            visible: !parent.hasIcon
                            text: rowItem ? rowItem.name.substring(0, 2).toUpperCase() : ""
                            font.pixelSize: Theme.typography.secondaryText
                            font.weight: Theme.typography.weightBold
                            color: Theme.palette.textTertiary
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignVCenter
                        spacing: 0

                        LogosText {
                            Layout.fillWidth: true
                            text: rowItem ? rowItem.label : ""
                            font.pixelSize: Theme.typography.primaryText
                            font.weight: Theme.typography.weightMedium
                            color: Theme.palette.text
                            elide: Text.ElideRight
                        }

                        // Raw module name, only when it differs from the
                        // display name — otherwise the cell says it twice.
                        LogosText {
                            Layout.fillWidth: true
                            visible: rowItem && rowItem.label !== rowItem.name
                            text: rowItem ? rowItem.name : ""
                            font.pixelSize: Theme.typography.secondaryText
                            color: Theme.palette.textTertiary
                            elide: Text.ElideRight
                        }
                    }
                }
            }

            Component {
                id: versionCellComponent

                LogosText {
                    readonly property string value:
                        rowItem ? String(rowItem.version || "") : ""

                    text: value.length > 0 ? value : "—"
                    color: value.length > 0 ? Theme.palette.text : Theme.palette.textMuted
                    font.pixelSize: Theme.typography.primaryText
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
            }

            Component {
                id: statusCellComponent

                Item {
                    ModuleStatusBadge {
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        row: rowItem
                    }
                }
            }

            // Medium only: ⋯ opens the row's detail sheet with the dropped columns.
            Component {
                id: moreCellComponent

                Item {
                    LogosIconButton {
                        objectName: "appsInspector.more." + (rowItem && rowItem.name ? rowItem.name : "")
                        anchors.centerIn: parent
                        size: Theme.minTouchTarget
                        flat: true
                        iconSource: LogosIcons.more
                        Accessible.name: qsTr("Details")
                        onClicked: detailSheet.openFor(rowItem)
                    }
                }
            }

            Component {
                id: actionsCellComponent

                Item {
                    ModuleRowActions {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        row: rowItem
                        busy: root.loading

                        onLoadToggleRequested: {
                            if (rowItem.isLoaded) root.unloadRequested(rowItem.name)
                            else                 root.loadRequested(rowItem.name)
                        }
                    }
                }
            }
        }

        // Narrow pane: the same rows as cards. Sorted by name as the table is
        // by default; no column headers to sort with.
        InspectorCardList {
            objectName: "appsInspector.cards"
            visible: root.cards
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: Theme.spacing.large
            Layout.rightMargin: Theme.spacing.large
            model: root.cards ? tableModel : null
            busy: root.loading
            emptyText: tableModel.totalCount === 0
                       ? qsTr("No UI plugins found in the plugins directory.")
                       : qsTr("No apps match the current filter.")
            onLoadToggleRequested: row => row.isLoaded ? root.unloadRequested(row.name)
                                                       : root.loadRequested(row.name)
        }
    }

    // Medium (three key columns): the row's detail, dropped columns included,
    // as a sheet over the right side of this pane. Hidden in the other layouts.
    InspectorDetailSheet {
        id: detailSheet
        anchors.fill: parent
        allowed: !root.cards && !root.fullTable && root.visible
        busy: root.loading
        onLoadToggleRequested: r => r.isLoaded ? root.unloadRequested(r.name)
                                               : root.loadRequested(r.name)
    }

    // Same Reload feedback as the App Manager / Package Manager panels.
    LoadingOverlay {
        anchors.fill: parent
        visible: root.loading
    }
}
