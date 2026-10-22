import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Logos.Controls
import Logos.Icons
import Logos.Theme

Rectangle {
    id: root

    property var    repositories:        []
    property bool   repositoriesLoading: false
    property string downloadSource:      ""
    // Real Qt models (ModuleInstanceModel), owned by MainUIBackend and
    // populated on every uiModulesChanged/coreModulesChanged tick.
    property var    uiModulesModel:      null
    property var    coreModulesModel:    null
    property bool   modulesLoading:      false
    // False where the host manages no package repositories.
    property bool   repositoriesAvailable: true

    signal repositoryRefreshRequested()
    signal repositoryAddRequested(string url)
    signal repositoryRemoveRequested(string url)
    signal repositoryEnabledRequested(string url, bool enabled)
    signal downloadSourceRequested(string source)
    signal repositoriesBecameVisible()

    signal appsRefreshRequested()
    signal appLoadRequested(string name)
    signal appUnloadRequested(string name)
    signal appsInspectorBecameVisible()

    signal modulesRefreshRequested()
    signal moduleLoadRequested(string name)
    signal moduleUnloadRequested(string name)
    signal moduleInspectorBecameVisible()

    function reportRepositoryResult(operation, url, success, error) {
        repositoriesView.reportOperationResult(operation, url, success, error)
    }

    function showRepositories() { if (root.repositoriesAvailable) d.openSection(d.sectionRepositories) }

    // ─── Responsive (docs/RESPONSIVE.md in the design system) ───
    // Settings is a list → detail screen. Measured on Settings' own width (it
    // sits in a shell tab, a torn-off window or a phone):
    //   medium / expanded  Sections list on the left, the section on the right.
    //   compact (< 600)    the Sections list is the page; a tap pushes the
    //                      section full width under a "‹ Settings" back row.
    readonly property string windowClass: Theme.windowClass(width)
    readonly property bool compact: windowClass === "compact"
    readonly property bool touch: compact || Theme.density === "touch"
    // Narrowed to compact while a section was open (a window dragged narrow):
    // stay on that section rather than jumping back to the list.
    onCompactChanged: {
        if (compact && width > 0) d.detailOpen = true
        // Carry a query across: unfold the compact search with it, or hand
        // the compact query back to the full search bar.
        if (compact && d.searchText.length > 0) { compactSearch.text = d.searchText; compactSearch.expanded = true }
        if (!compact) compactSearch.expanded = false
    }

    QtObject {
        id: d

        // Sub-views in the right pane. Order maps 1:1 to the StackLayout below.
        readonly property int sectionDashboard:       0
        readonly property int sectionAppsInspector:   1
        readonly property int sectionModuleInspector: 2
        readonly property int sectionAppearance:      3
        readonly property int sectionNotifications:   4
        readonly property int sectionRepositories:    5

        readonly property var sections: [
            { label: qsTr("Dashboard"),        hint: qsTr("Version and build commits") },
            { label: qsTr("Apps Inspector"),   hint: qsTr("Load and unload apps") },
            { label: qsTr("Module Inspector"), hint: qsTr("Core modules, CPU and memory") },
            { label: qsTr("Customisation"),    hint: qsTr("Theme, density and shell") },
            { label: qsTr("Notifications"),    hint: qsTr("Which apps may notify you") },
            { label: qsTr("Package Repositories"), hint: qsTr("Where apps come from") }
        ].slice(0, root.repositoriesAvailable ? 6 : 5)   // Repositories must stay last

        property int selectedIndex: 0

        // Search text is shared by the two inspectors. Reset whenever the
        // user switches away so stale queries don't leak between panels.
        property string searchText: ""

        readonly property bool searchable:
            selectedIndex === sectionAppsInspector ||
            selectedIndex === sectionModuleInspector

        onSelectedIndexChanged: { searchText = ""; compactSearch.close() }

        readonly property int sidebarWidth: root.windowClass === "expanded" ? 200 : 168

        // Compact only: a section is pushed over the Sections list.
        property bool detailOpen: false
        readonly property bool showList: !root.compact || !detailOpen
        readonly property bool showDetail: !root.compact || detailOpen

        // A drill-down inside a section (Module Inspector → Interface) is
        // the first thing back closes.
        readonly property bool nestedOpen:
            selectedIndex === sectionModuleInspector && moduleInspector.showingInterface

        function openSection(i) { selectedIndex = i; detailOpen = true }
        function goBack() {
            if (compactSearch.expanded) compactSearch.close()
            else if (nestedOpen) moduleInspector.showingInterface = false
            else detailOpen = false
        }
    }

    // Phone back key / Esc / Alt+Left close the open section in compact.
    Shortcut {
        sequences: [StandardKey.Back, "Esc"]
        context: Qt.WindowShortcut
        enabled: root.visible && root.compact && d.detailOpen
        onActivated: d.goBack()
    }

    // 0 in a desktop window: the window's frosted glass shows through.
    property real backgroundOpacity: 1
    color: Theme.colors.getColor(Theme.palette.background, backgroundOpacity)

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.pageMargin(root.width)
        anchors.rightMargin: Theme.pageMargin(root.width)
        anchors.topMargin: root.compact ? Theme.spacing.medium : Theme.pageMargin(root.width)
        anchors.bottomMargin: root.compact ? Theme.spacing.large : Theme.pageMargin(root.width)
        spacing: root.compact ? Theme.spacing.medium : Theme.spacing.xlarge

        // ─── Compact navigation bar (a section is open) ───
        // "‹ Settings": the whole label is the 44 px back target and names
        // where back goes; the section keeps its own title below it. A
        // searchable section adds a 44 px search icon at the right end that
        // unfolds across the bar (the back label hides while it's open).
        RowLayout {
            id: compactBar
            visible: root.compact && d.detailOpen
            Layout.fillWidth: true
            Layout.leftMargin: -Theme.spacing.small
            spacing: Theme.spacing.tiny

            AbstractButton {
                id: backRow
                objectName: "settings.back"
                visible: !compactSearch.expanded
                Layout.fillWidth: true
                implicitHeight: Theme.minTouchTarget
                Accessible.name: qsTr("Back")
                onClicked: d.goBack()

                background: Rectangle {
                    radius: Theme.spacing.radiusLarge
                    color: backRow.pressed ? Theme.components.listItem.backgroundPressed : "transparent"
                }
                contentItem: RowLayout {
                    spacing: Theme.spacing.tiny
                    LogosIcon {
                        source: LogosIcons.arrowLeft
                        color: Theme.palette.text
                        Layout.preferredWidth: 20
                        Layout.preferredHeight: 20
                        Layout.leftMargin: Theme.spacing.small
                    }
                    LogosText {
                        Layout.fillWidth: true
                        text: d.nestedOpen ? qsTr("Module Inspector") : qsTr("Settings")
                        font.pixelSize: Theme.typography.primaryText
                        font.weight: Theme.typography.weightMedium
                        color: Theme.palette.text
                        elide: Text.ElideRight
                    }
                }
            }

            LogosSearchToggle {
                id: compactSearch
                objectName: "settings.searchToggle"
                visible: d.searchable && !d.nestedOpen
                placeholderText: d.selectedIndex === d.sectionAppsInspector
                                 ? qsTr("Search apps…") : qsTr("Search modules…")
                onTextChanged: if (root.compact && text !== d.searchText) d.searchText = text
            }
        }

        // ─── Header ───
        // Title + subtitle on the left; when an inspector is selected, a
        // page-level search bar joins on the right — same layout as
        // AppManagerView so the two feel familiar side by side.
        // Compact: no header at all — the shell's bar names the page and
        // search lives in the compact bar above.
        GridLayout {
            Layout.fillWidth: true
            visible: !root.compact
            columns: 3
            columnSpacing: Theme.spacing.xlarge
            rowSpacing: Theme.spacing.medium

            // Compact: the shell's title bar already says "Settings"; the
            // Sections list starts at the top instead of under a second title.
            ColumnLayout {
                id: titleBlock
                visible: !root.compact
                Layout.fillWidth: true
                spacing: Theme.spacing.tiny

                LogosText {
                    text: qsTr("Settings")
                    font.pixelSize: Theme.typography.pageTitleText
                    font.weight: Theme.typography.weightBold
                    color: Theme.palette.text
                }

                LogosText {
                    text: qsTr("Manage modules, apps and dashboards.")
                    font.pixelSize: Theme.typography.primaryText
                    color: Theme.palette.textSecondary
                }
            }

            Item { Layout.fillWidth: true }

            LogosSearchBar {
                id: searchBar
                objectName: "settings.searchField"
                visible: d.searchable
                Layout.alignment: Qt.AlignRight
                Layout.preferredWidth: 605
                Layout.minimumWidth: 200
                // Shrinks beside the title rather than running off the right edge.
                Layout.maximumWidth: Math.max(200, root.width - 2 * Theme.pageMargin(root.width)
                                                   - titleBlock.implicitWidth - 2 * Theme.spacing.xlarge)
                text: d.searchText
                placeholderText: d.selectedIndex === d.sectionAppsInspector
                                 ? qsTr("Search apps…")
                                 : qsTr("Search modules…")
                shortcutHint: searchShortcut.nativeText
                onTextChanged: {
                    if (text !== d.searchText)
                        d.searchText = text
                }
            }

            Shortcut {
                id: searchShortcut
                objectName: "settings.searchShortcut"
                sequence: "Ctrl+K"
                context: Qt.WindowShortcut
                enabled: root.visible && d.searchable && d.showDetail
                onActivated: {
                    if (root.compact) {
                        compactSearch.open()
                        return
                    }
                    searchBar.textInput.forceActiveFocus()
                    searchBar.textInput.selectAll()
                }
            }
        }

        RowLayout {
            id: body
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.spacing.medium

            // ─── Sections sidebar ───
            LogosListView {
                id: sectionsList
                visible: !root.compact

                // Expanded: 200 as on desktop; medium leaves the section more room.
                Layout.preferredWidth: d.sidebarWidth
                Layout.minimumWidth: d.sidebarWidth
                Layout.maximumWidth: d.sidebarWidth
                Layout.fillHeight: true

                model: d.sections
                currentIndex: d.selectedIndex

                header: LogosText {
                    width: sectionsList.width
                    topPadding: Theme.spacing.tiny
                    bottomPadding: Theme.spacing.tiny
                    text: qsTr("Sections")
                    font.pixelSize: Theme.typography.subtitleText
                    font.weight: Theme.typography.weightRegular
                    color: Theme.palette.text
                }

                delegate: LogosItemDelegate {
                    id: cell
                    objectName: "settings.nav." + index
                    width: ListView.view.width
                    text: modelData.label
                    highlighted: ListView.isCurrentItem
                    radius: Theme.spacing.radiusLarge
                    highlightColor: Theme.palette.backgroundButton
                    hoverColor: "transparent"
                    textColor: (cell.highlighted || cell.hovered)
                                   ? Theme.palette.text
                                   : Theme.palette.textTertiary
                    implicitHeight: root.touch ? Math.max(Theme.minTouchTarget, Theme.components.listItem.height)
                                               : Theme.components.listItem.height
                    onClicked: d.selectedIndex = index
                }
            }

            // ─── Compact: the Sections list is the page ───
            // One grouped card of rows; chevron = opens a section.
            LogosScrollView {
                id: compactSections
                objectName: "settings.sectionsCompact"
                visible: root.compact && !d.detailOpen
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                Rectangle {
                    width: compactSections.availableWidth
                    implicitHeight: sectionRows.implicitHeight
                    radius: Theme.spacing.radiusXlarge
                    color: Theme.palette.surfaceRaised
                    clip: true

                    ColumnLayout {
                        id: sectionRows
                        width: parent.width
                        spacing: 0
                        Repeater {
                            model: d.sections
                            delegate: LogosListRow {
                                required property var modelData
                                required property int index
                                objectName: "settings.section." + index
                                Layout.fillWidth: true
                                title: modelData.label
                                subtitle: modelData.hint
                                showChevron: true
                                showSeparator: index < d.sections.length - 1
                                onClicked: d.openSection(index)
                            }
                        }
                    }
                }
            }

            // ─── Right pane ───
            Rectangle {
                objectName: "settings.sectionPane"
                visible: d.showDetail
                Layout.fillWidth: true
                // A section never widens the page: without a cap, the inspector
                // tables' column minimums (~720 px) pushed this pane past the
                // window's right edge at medium widths.
                Layout.minimumWidth: 0
                Layout.maximumWidth: Math.max(0, root.width - 2 * Theme.pageMargin(root.width)
                                              - (root.compact ? 0 : d.sidebarWidth + Theme.spacing.medium))
                Layout.fillHeight: true
                color: Theme.palette.surfaceRaised
                radius: Theme.spacing.radiusXlarge
                clip: true

                StackLayout {
                    anchors.fill: parent
                    currentIndex: d.selectedIndex

                    // 0 — Dashboard.
                    DashboardView {}

                    // 1 — Apps Inspector (UI plugins).
                    AppsInspectorView {
                        sourceModel: root.uiModulesModel
                        loading:     root.modulesLoading
                        searchText:  d.searchText

                        onReloadRequested: root.appsRefreshRequested()
                        onLoadRequested:   name => root.appLoadRequested(name)
                        onUnloadRequested: name => root.appUnloadRequested(name)
                        onVisibleChanged:  if (visible) root.appsInspectorBecameVisible()
                    }

                    // 2 — Module Inspector (core modules).
                    ModuleInspectorView {
                        id: moduleInspector
                        compactBack: root.compact
                        sourceModel: root.coreModulesModel
                        loading:     root.modulesLoading
                        searchText:  d.searchText

                        onReloadRequested: root.modulesRefreshRequested()
                        onLoadRequested:   name => root.moduleLoadRequested(name)
                        onUnloadRequested: name => root.moduleUnloadRequested(name)
                        onVisibleChanged:  if (visible) root.moduleInspectorBecameVisible()
                    }

                    // 3 — Appearance (themes).
                    AppearanceView {}

                    // 4 — Notifications (which apps may notify).
                    NotificationsView {}

                    // 5 — Package Repositories (last: dropped when unavailable).
                    RepositoriesView {
                        id: repositoriesView

                        repositories:   root.repositories
                        loading:        root.repositoriesLoading
                        downloadSource: root.downloadSource

                        onRefreshRequested:    root.repositoryRefreshRequested()
                        onAddRequested:        url => root.repositoryAddRequested(url)
                        onRemoveRequested:     url => root.repositoryRemoveRequested(url)
                        onSetEnabledRequested: (url, enabled) =>
                                                   root.repositoryEnabledRequested(url, enabled)
                        onDownloadSourceRequested: source => root.downloadSourceRequested(source)
                        onVisibleChanged:      if (visible)
                                                   root.repositoriesBecameVisible()
                    }

                }
            }
        }
    }
}
