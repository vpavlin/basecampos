import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Logos.Controls
import Logos.Icons
import Logos.Theme

import Basecamp.Backend
import Basecamp.Common

// Settings → Module Inspector. Lists every core module liblogos knows about
// with its load state and live CPU / memory stats, a Load/Unload toggle per
// row, and the per-module Interface drill-down. Uninstall is deliberately
// absent — module management lives in the Package Manager; this view is
// read-only apart from the load toggle.
//
// Formerly the "Core Modules" tab of ModulesView. Split out into its own
// Settings section alongside Apps Inspector, restyled onto LogosTable +
// InspectorPanelHeader to match the App Manager and Package Manager panels.
Item {
    id: root
    objectName: "moduleInspectorView"

    // ─── Public API ───
    // View emits, ContentViews calls the backend — see the Settings wiring in
    // Basecamp/Shell/ContentViews.qml. The Interface screen is the exception:
    // PluginInterfaceView still queries the backend for methods/events itself.
    // `searchText` is owned by SettingsView (one shared page-level search bar,
    // styled like AppManagerView).
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

    property string selectedPlugin: ""
    property bool showingInterface: false
    // The host draws its own back row (Settings on a phone): hide the
    // Interface screen's "← Back" so there is one way back, not two.
    property bool compactBack: false

    // Core modules Basecamp itself runs on: the package pipeline
    // (package_manager, package_downloader) and the auth-token registry
    // (capability_module). Their Load/Unload toggle renders disabled — unloading
    // any of them from the inspector would take the app's own plumbing down.
    readonly property var protectedModules: ["package_manager", "package_downloader", "capability_module"]

    // Open a specific module's Interface screen (methods + events) by name.
    // Equivalent to clicking that module's "Interface" button — exposed for UI
    // automation/tests, which can't disambiguate the per-row buttons by their
    // identical label.
    function openInterface(name) {
        selectedPlugin = name;
        showingInterface = true;
    }

    ModulesFilterProxy {
        id: tableModel

        sourceModel:  root.sourceModel
        searchText:   root.searchText
        sortRoleName: modulesTable.sortRole
    }

    StackLayout {
        anchors.fill: parent
        currentIndex: root.showingInterface ? 1 : 0

        // ─── 0: module list ───
        // Vertical insets are Layout margins on the children rather than
        // anchors on this ColumnLayout — StackLayout owns its children's
        // geometry, so anchor margins here would be ignored.
        ColumnLayout {
            spacing: Theme.spacing.medium

            InspectorPanelHeader {
                id: header

                Layout.fillWidth: true
                Layout.leftMargin: Theme.spacing.large
                Layout.rightMargin: Theme.spacing.large
                Layout.topMargin: Theme.spacing.large
                Layout.minimumWidth: 0

                title: qsTr("Module Inspector")
                subtitle: qsTr("Core modules known to the runtime, with live resource usage.")
                reloadObjectName: "moduleInspector.reloadButton"
                loading: root.loading
                touch: root.touch
                visibleCount: tableModel.visibleCount
                totalCount: tableModel.totalCount

                onReloadClicked: root.reloadRequested()
            }

            LogosTable {
                id: modulesTable
                objectName: "moduleInspector.table"

                visible: !root.cards
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.bottomMargin: Theme.spacing.large

                model: tableModel
                rowHeight: 56
                sortRole: "label"
                sortOrder: Qt.AscendingOrder
                emptyText: tableModel.totalCount === 0
                           ? qsTr("No core modules available.")
                           : qsTr("No modules match the current filter.")

                // Medium: a tap on a row opens its detail too (single tap, no hover).
                onRowClicked: (index, row) => { if (!root.fullTable) detailSheet.openFor(row) }

                onSortRequested: function(role, order) {
                    modulesTable.sortRole = role
                    modulesTable.sortOrder = order
                    tableModel.applySortOrder(order)
                }

                // Keep every row instantiated — see AppsInspectorView for why.
                Component.onCompleted: if (view) view.cacheBuffer = 20000

                // Full table when it fits; three key columns when it doesn't
                // (RESPONSIVE.md §3.5, medium). Narrower still: cards (see below).
                columns: root.fullTable
                         ? [cols.module, cols.status, cols.cpu, cols.memory, cols.actions]
                         : [cols.module, cols.status, cols.actions, cols.more]

                QtObject {
                    id: cols
                    property LogosTableColumn module: LogosTableColumn {
                        title: qsTr("Module")
                        role: "label"
                        minWidth: 180
                        preferredWidth: 240
                        sortable: true
                        cellDelegate: moduleCellComponent
                    }
                    property LogosTableColumn status: LogosTableColumn {
                        title: qsTr("Status")
                        role: "statusText"
                        minWidth: 110
                        preferredWidth: 130
                        sortable: true
                        cellDelegate: statusCellComponent
                    }
                    property LogosTableColumn cpu: LogosTableColumn {
                        title: qsTr("CPU")
                        role: "cpu"
                        minWidth: 80
                        preferredWidth: 90
                        sortable: true
                        cellDelegate: cpuCellComponent
                    }
                    property LogosTableColumn memory: LogosTableColumn {
                        title: qsTr("Memory")
                        role: "memory"
                        minWidth: 90
                        preferredWidth: 110
                        sortable: true
                        cellDelegate: memoryCellComponent
                    }
                    property LogosTableColumn actions: LogosTableColumn {
                        title: ""
                        minWidth: 240
                        preferredWidth: 260
                        fillWidth: true
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
                    id: moduleCellComponent

                    ColumnLayout {
                        spacing: 0

                        LogosText {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignVCenter
                            text: rowItem ? rowItem.label : ""
                            font.pixelSize: Theme.typography.primaryText
                            font.weight: Theme.typography.weightMedium
                            color: Theme.palette.text
                            elide: Text.ElideRight
                        }

                        // Raw module name — the identifier other modules call
                        // it by. Hidden when it matches the display name.
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

                Component {
                    id: statusCellComponent

                    Item {
                        ModuleStatusBadge {
                            // Automation-only: per-module handle so UI tests
                            // can assert one row's load state — the badge
                            // wording ("Loaded"/"Not loaded") also appears in
                            // other tables, so text matching is ambiguous.
                            objectName: "moduleInspector.status."
                                        + (rowItem && rowItem.name ? rowItem.name : "")
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            row: rowItem
                        }
                    }
                }

                // Stats only mean something for a running module; unloaded rows
                // render an em dash so the column stays aligned without
                // implying "0% CPU" is a measurement.
                Component {
                    id: cpuCellComponent

                    LogosText {
                        text: (rowItem && rowItem.isLoaded)
                              ? Number(rowItem.cpu).toFixed(1) + "%" : "—"
                        color: (rowItem && rowItem.isLoaded) ? Theme.palette.text
                                                            : Theme.palette.textMuted
                        font.pixelSize: Theme.typography.primaryText
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }
                }

                Component {
                    id: memoryCellComponent

                    LogosText {
                        text: (rowItem && rowItem.isLoaded)
                              ? Number(rowItem.memory).toFixed(1) + " MB" : "—"
                        color: (rowItem && rowItem.isLoaded) ? Theme.palette.text
                                                            : Theme.palette.textMuted
                        font.pixelSize: Theme.typography.primaryText
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }
                }

                // Medium only: ⋯ opens the row's detail sheet with the dropped columns.
                Component {
                    id: moreCellComponent

                    Item {
                        LogosIconButton {
                            objectName: "moduleInspector.more." + (rowItem && rowItem.name ? rowItem.name : "")
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
                            locked: !!rowItem && root.protectedModules.indexOf(rowItem.name) !== -1
                            interfaceEnabled: true

                            onLoadToggleRequested: {
                                if (rowItem.isLoaded) root.unloadRequested(rowItem.name)
                                else                 root.loadRequested(rowItem.name)
                            }
                            onInterfaceRequested: root.openInterface(rowItem.name)
                        }
                    }
                }
            }

            // Narrow pane: the same rows as cards (CPU · memory on one line).
            InspectorCardList {
                objectName: "moduleInspector.cards"
                visible: root.cards
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.leftMargin: Theme.spacing.large
                Layout.rightMargin: Theme.spacing.large
                Layout.bottomMargin: Theme.spacing.large
                model: root.cards ? tableModel : null
                busy: root.loading
                showStats: true
                interfaceEnabled: true
                protectedModules: root.protectedModules
                emptyText: tableModel.totalCount === 0
                           ? qsTr("No core modules available.")
                           : qsTr("No modules match the current filter.")
                onLoadToggleRequested: row => row.isLoaded ? root.unloadRequested(row.name)
                                                           : root.loadRequested(row.name)
                onInterfaceRequested: row => root.openInterface(row.name)
            }
        }

        // ─── 1: Interface (methods + events) ───
        Item {
            PluginInterfaceView {
                anchors.fill: parent
                anchors.margins: Theme.spacing.large
                pluginName: root.selectedPlugin
                showBack: !root.compactBack
                onBackClicked: root.showingInterface = false
            }
        }
    }

    // Medium (three key columns): the row's detail, dropped columns included,
    // as a sheet over the right side of this pane. Hidden in the other layouts.
    InspectorDetailSheet {
        id: detailSheet
        anchors.fill: parent
        allowed: !root.cards && !root.fullTable && root.visible && !root.showingInterface
        busy: root.loading
        showStats: true
        interfaceEnabled: true
        locked: !!row && root.protectedModules.indexOf(row.name) !== -1
        onInterfaceRequested: r => { close(); root.openInterface(r.name) }
        onLoadToggleRequested: r => r.isLoaded ? root.unloadRequested(r.name)
                                               : root.loadRequested(r.name)
    }

    // Same Reload feedback as the App Manager / Package Manager panels.
    LoadingOverlay {
        anchors.fill: parent
        visible: root.loading
    }
}
