import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Logos.Controls
import Logos.Icons
import Logos.Theme

import Basecamp.Backend
import Basecamp.Common

Rectangle {
    id: root
    // For UI tests: `loading` says when the catalog is in.
    objectName: "appManager.view"

    // ─── Public API ───
    property var appsProxy: null

    // Applies a query from outside the view — the welcome page's search hands
    // its text over when a result routes here. Goes through d.searchText so the
    // visible field and the proxy stay in step.
    function applySearch(query) { d.searchText = query }
    property var repositories: []
    property bool loading: false
    property bool showUnavailable: false

    // This host's variant, for the "nothing for this platform" empty state.
    property string hostVariant: ""
    signal appClicked(string name, string repositoryUrl)
    signal manageAppRequested(string name, string repositoryUrl)
    signal uninstallAppRequested(string name, string repositoryUrl)
    signal navigateToRepositories()
    signal refreshRequested()

    // ─── Responsive (docs/RESPONSIVE.md in the design system) ───
    // Measured on the view's own width (a shell tab, a torn-off window, a phone):
    //   expanded (≥ 840)  as on desktop: title + search, Categories sidebar,
    //                     the Apps panel with every control in its header.
    //   medium            the sidebar narrows to 168; the panel header wraps.
    //   compact (< 600)   no page title (the shell's bar names the page). A bar
    //                     with the category picker and a 44 px search icon at its
    //                     right end that unfolds across it; Reload, Manage
    //                     Repositories, Show unavailable and the view mode move
    //                     into the panel's ⋯ menu; tiles get narrower cells.
    readonly property string windowClass: Theme.windowClass(width)
    readonly property bool compact: windowClass === "compact"
    // DS controls in this engine floor at 44 px while the view is phone-narrow
    // (Theme.touch). Each QML engine has its own Theme, and this view's engine
    // is the shell's content widget, so the view sets it.
    // Only while shown: Settings' engine keeps a hidden copy of this view,
    // and the visible screen in an engine must be the one that decides.
    Binding {
        target: Theme
        property: "compactWindow"
        when: root.visible && root.width > 0
        value: Theme.windowClass(root.width) === "compact"
        restoreMode: Binding.RestoreValue
    }
    Component.onDestruction: Theme.compactWindow = false
    readonly property bool touch: Theme.touch
    // Carry a query across a class change: unfold the compact search with it,
    // or hand it back to the full search bar.
    onCompactChanged: {
        if (compact && d.searchText.length > 0) { compactSearch.text = d.searchText; compactSearch.expanded = true }
        if (!compact) compactSearch.expanded = false
    }

    QtObject {
        id: d

        readonly property string installStateFilter: {
            switch (panelHeader.installStateIndex) {
            case 1:  return "installed"
            case 2:  return "notInstalled"
            default: return "all"
            }
        }
        onInstallStateFilterChanged:
            if (root.appsProxy) root.appsProxy.installStateFilter = installStateFilter

        property string searchText: ""
        onSearchTextChanged: {
            if (root.appsProxy) root.appsProxy.searchText = searchText
            // A query set from outside (applySearch) must show in compact too,
            // or the grid is filtered with the search folded away and empty.
            if (root.compact && compactSearch.text !== searchText) {
                compactSearch.text = searchText
                if (searchText.length > 0) compactSearch.expanded = true
            }
        }

        property int    selectedCategoryIndex: 0   // 0 = "All"
        property string viewMode: "grid"
        // Compact: three tiles across a phone instead of one stretched tile.
        // Medium: three across rather than two tiles adrift in wide cells.
        readonly property int gridMinCellWidth:
            root.compact ? 104 : root.windowClass === "medium" ? 140 : 180

        readonly property var categories:
            root.appsProxy ? root.appsProxy.categories : ["All"]

        readonly property string selectedCategory:
            d.categories[d.selectedCategoryIndex] || "All"
        onSelectedCategoryChanged:
            if (root.appsProxy) root.appsProxy.categoryFilter = selectedCategory
    }

    // Initial sync — push d's current values onto the proxy as soon as
    // it's assigned. The change handlers inside `d` keep things in sync
    // after that.
    onAppsProxyChanged: {
        if (appsProxy) {
            appsProxy.categoryFilter     = d.selectedCategory
            appsProxy.searchText         = d.searchText
            appsProxy.installStateFilter = d.installStateFilter
        }
    }

    // Filter proxy for the "Local" section — installed packages with no
    // catalog row (empty repositoryUrl). Hoisted out of the grid so the
    // empty-state guard below can gate on its visibleCount. objectName is
    // the test anchor (tests/ui-tests.mjs looks it up via findByProperty).
    AppsFilterProxy {
        id: localAppsProxy
        objectName: "appManager.localAppsProxy"
        sourceModel: root.appsProxy
        matchLocalOnly: true
        excludeMainUi: false
        showUnavailable: true
    }

    // 0 in a desktop window: the window's frosted glass shows through.
    property real backgroundOpacity: 1
    color: Theme.colors.getColor(Theme.palette.background, backgroundOpacity)

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.pageMargin(root.width)
        anchors.rightMargin: Theme.pageMargin(root.width)
        anchors.topMargin: root.compact ? Theme.spacing.small : Theme.pageMargin(root.width)
        anchors.bottomMargin: root.compact ? Theme.spacing.large : Theme.pageMargin(root.width)
        spacing: root.compact ? Theme.spacing.small : Theme.spacing.xlarge

        // ─── Compact bar ───
        // Category picker on the left, search folded into a 44 px icon at
        // the right end; unfolded, the field spans the bar (picker hidden).
        RowLayout {
            id: compactBar
            visible: root.compact
            Layout.fillWidth: true
            spacing: Theme.spacing.small

            AbstractButton {
                id: categoryPicker
                objectName: "appManager.categoryPicker"
                visible: !compactSearch.expanded
                Layout.fillWidth: true
                Layout.leftMargin: -Theme.spacing.small
                // A fixed implicit width: the bar's layout gives the picker
                // its width, and the label is capped by that width below. When
                // the implicit width came from the label, the two fed each other
                // and, in a hidden copy of this view (Settings' engine keeps one
                // in its StackLayout), the layout never settled: the main
                // thread spun at 90% when Settings opened at phone width.
                implicitWidth: Theme.minTouchTarget
                implicitHeight: Theme.minTouchTarget
                Accessible.name: qsTr("Category: %1").arg(d.selectedCategory)
                onClicked: categoryMenu.popupUnder(categoryPicker)

                background: Rectangle {
                    radius: Theme.spacing.radiusLarge
                    color: categoryPicker.pressed ? Theme.components.listItem.backgroundPressed : "transparent"
                }
                contentItem: RowLayout {
                    spacing: Theme.spacing.tiny
                    LogosText {
                        Layout.leftMargin: Theme.spacing.small
                        text: d.selectedCategoryIndex === 0 ? qsTr("All categories") : d.selectedCategory
                        font.pixelSize: Theme.typography.primaryText
                        font.weight: Theme.typography.weightMedium
                        color: Theme.palette.text
                        elide: Text.ElideRight
                        Layout.maximumWidth: Math.max(0, categoryPicker.width - 40)
                    }
                    LogosIcon {
                        source: LogosIcons.triangleDown
                        color: Theme.palette.textSecondary
                        Layout.preferredWidth: 16
                        Layout.preferredHeight: 16
                    }
                    Item { Layout.fillWidth: true }
                }
            }

            LogosMenu {
                id: categoryMenu
                objectName: "appManager.categoryMenu"

                Instantiator {
                    model: d.categories
                    delegate: LogosMenuItem {
                        objectName: "appManager.categoryItem." + modelData
                        required property var modelData
                        required property int index
                        text: modelData
                        checkable: true
                        checked: index === d.selectedCategoryIndex
                        onTriggered: d.selectedCategoryIndex = index
                    }
                    onObjectAdded: (index, object) => categoryMenu.insertItem(index, object)
                    onObjectRemoved: (index, object) => categoryMenu.removeItem(object)
                }
            }

            LogosSearchToggle {
                id: compactSearch
                objectName: "appManager.searchToggle"
                placeholderText: qsTr("Search apps…")
                onTextChanged: if (root.compact && text !== d.searchText) d.searchText = text
            }
        }

        // Header: title + subtitle on the left, search on the right.
        RowLayout {
            visible: !root.compact
            Layout.fillWidth: true
            spacing: Theme.spacing.xlarge

            ColumnLayout {
                id: titleBlock
                Layout.fillWidth: true
                spacing: Theme.spacing.tiny

                LogosText {
                    text: "Applications"
                    font.pixelSize: Theme.typography.pageTitleText
                    font.weight: Theme.typography.weightBold
                    color: Theme.palette.text
                }

                LogosText {
                    text: "Install and manage applications."
                    font.pixelSize: Theme.typography.primaryText
                    color: Theme.palette.textSecondary
                }
            }

            Item { Layout.fillWidth: true }

            LogosSearchBar {
                id: searchBar
                objectName: "appManager.searchField"
                Layout.alignment: Qt.AlignRight
                Layout.preferredWidth: 605
                Layout.minimumWidth: 200
                // Shrinks beside the title rather than running off the right edge.
                Layout.maximumWidth: Math.max(200, root.width - 2 * Theme.pageMargin(root.width)
                                                   - titleBlock.implicitWidth - 2 * Theme.spacing.xlarge)
                text: d.searchText
                placeholderText: qsTr("Search apps…")
                shortcutHint: searchShortcut.nativeText
                onTextChanged: {
                    if (text !== d.searchText)
                        d.searchText = text
                }
            }

            // ⌘K focuses + selects the search bar.
            Shortcut {
                id: searchShortcut
                objectName: "appManager.searchShortcut"
                sequence: "Ctrl+K"
                context: Qt.WindowShortcut
                enabled: root.visible
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
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.spacing.medium

            // ─── Categories sidebar ───
            LogosListView {
                id: categoriesList
                visible: !root.compact

                // Expanded: 200 as on desktop; medium leaves the apps more room.
                readonly property int sidebarWidth: root.windowClass === "expanded" ? 200 : 168
                Layout.preferredWidth: sidebarWidth
                Layout.minimumWidth: sidebarWidth
                Layout.maximumWidth: sidebarWidth
                Layout.fillHeight: true

                model: d.categories
                currentIndex: d.selectedCategoryIndex

                header: LogosText {
                    width: categoriesList.width
                    topPadding: Theme.spacing.tiny
                    bottomPadding: Theme.spacing.tiny
                    text: qsTr("Categories")
                    font.pixelSize: Theme.typography.subtitleText
                    font.weight: Theme.typography.weightRegular
                    color: Theme.palette.text
                }

                delegate: LogosItemDelegate {
                    id: cell
                    objectName: "appManager.category." + modelData
                    width: ListView.view.width
                    text: modelData
                    highlighted: ListView.isCurrentItem
                    radius: Theme.spacing.radiusLarge
                    highlightColor: Theme.palette.backgroundButton
                    hoverColor: "transparent"
                    textColor: (cell.highlighted || cell.hovered)
                                   ? Theme.palette.text
                                   : Theme.palette.textTertiary
                    onClicked: d.selectedCategoryIndex = index
                }
            }

            // ─── Apps panel ───
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: Theme.palette.surfaceRaised
                radius: Theme.spacing.radiusXlarge
                clip: true

                ColumnLayout {
                    anchors.fill: parent
                    anchors.topMargin: Theme.spacing.medium
                    anchors.bottomMargin: Theme.spacing.medium
                    spacing: Theme.spacing.medium

                    AppManagerPanelHeader {
                        id: panelHeader
                        Layout.fillWidth: true
                        Layout.leftMargin: Theme.spacing.medium
                        Layout.rightMargin: Theme.spacing.medium

                        loading: root.loading
                        compact: root.compact
                        touch: root.touch
                        showLabels: root.windowClass === "expanded"
                        viewMode: d.viewMode
                        showUnavailable: root.showUnavailable
                        onReloadClicked: root.refreshRequested()
                        onRepositoriesClicked: root.navigateToRepositories()
                        onViewModeChangeRequested: (mode) => d.viewMode = mode
                        onShowUnavailableRequested: (show) => root.showUnavailable = show
                    }

                    // Empty state — shown when no repositories are configured
                    // AND there are no local-only installed rows to fall back on.
                    // If either exists, we render the grid instead.
                    EmptyView {
                        objectName: "appManager.noReposView"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: root.repositories.length === 0
                                 && localAppsProxy.visibleCount === 0
                                 && !root.loading

                        title: qsTr("No repositories configured")
                        subtitle: qsTr("Add a package repository to browse and install apps.")
                        actionText: qsTr("Manage Repositories")
                        onActionClicked: root.navigateToRepositories()
                    }

                    Flickable {
                        id: gridScroll
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: root.repositories.length > 0
                                 || localAppsProxy.visibleCount > 0
                                 || root.loading
                        clip: true
                        contentWidth: width
                        contentHeight: gridColumn.implicitHeight
                        boundsBehavior: Flickable.StopAtBounds

                        ScrollBar.vertical: LogosScrollBar {
                            policy: ScrollBar.AsNeeded
                            rightPadding: 4
                        }

                        ColumnLayout {
                            id: gridColumn
                            x: Theme.spacing.medium
                            width: gridScroll.width - 2 * Theme.spacing.medium
                            spacing: Theme.spacing.large

                            Repeater {
                                model: root.repositories
                                delegate: AppRepoSection {
                                    required property var modelData

                                    AppsFilterProxy {
                                        id: repoFilter
                                        sourceModel: root.appsProxy
                                        repositoryUrlFilter: modelData.url || ""
                                        excludeMainUi: false
                                        showUnavailable: true
                                    }

                                    title: modelData.displayLabel
                                           || modelData.displayName
                                           || modelData.name
                                           || modelData.url
                                           || qsTr("Repository")
                                    count: repoFilter.visibleCount
                                    modulesSource: repoFilter
                                    viewMode: d.viewMode
                                    touch: root.touch
                                    gridMinCellWidth: d.gridMinCellWidth
                                    visible: modelData.enabled !== false
                                             && repoFilter.visibleCount > 0

                                    onAppClicked: (name, url) => root.appClicked(name, url)
                                    onAppManageRequested: (name, url) => root.manageAppRequested(name, url)
                                    onAppUninstallRequested: (name, url) => root.uninstallAppRequested(name, url)
                                }
                            }

                            // Local — synthetic "repo" section for installed
                            // packages the catalog doesn't claim. Sits after
                            // the real repos so it always renders last.
                            AppRepoSection {
                                AppsFilterProxy {
                                    id: localFilter
                                    sourceModel: root.appsProxy
                                    matchLocalOnly: true
                                    excludeMainUi: false
                                    showUnavailable: true
                                }

                                title: qsTr("local")
                                count: localFilter.visibleCount
                                modulesSource: localFilter
                                viewMode: d.viewMode
                                touch: root.touch
                                gridMinCellWidth: d.gridMinCellWidth
                                visible: localFilter.visibleCount > 0

                                onAppClicked: (name, url) => root.appClicked(name, url)
                                onAppManageRequested: (name, url) => root.manageAppRequested(name, url)
                                onAppUninstallRequested: (name, url) => root.uninstallAppRequested(name, url)
                            }

                            // Empty-search state — every section above hides
                            // itself when the search matches nothing, so fill
                            // the viewport with feedback instead of a blank
                            // panel. Search-only: an empty search keeps the
                            // no-repos EmptyView / category filtering as-is.
                            EmptyView {
                                objectName: "appManager.emptyView"
                                Layout.fillWidth: true
                                Layout.preferredHeight: gridScroll.height
                                visible: d.searchText.length > 0
                                         && root.appsProxy !== null
                                         && root.appsProxy.visibleCount === 0

                                title: qsTr("No apps match your search.")
                                subtitle: qsTr("Try a different search term.")
                            }

                            EmptyView {
                                objectName: "appManager.noneForPlatformView"
                                Layout.fillWidth: true
                                Layout.preferredHeight: gridScroll.height
                                visible: d.searchText.length === 0
                                         && d.selectedCategoryIndex === 0
                                         && !root.showUnavailable
                                         && !root.loading
                                         && root.repositories.length > 0
                                         && root.appsProxy !== null
                                         && root.appsProxy.visibleCount === 0

                                title: qsTr("Nothing available for this platform")
                                subtitle: (root.hostVariant.length > 0
                                           && !root.hostVariant.startsWith("unknown"))
                                          ? qsTr("No configured repository publishes a build for %1.")
                                                .arg(root.hostVariant)
                                          : qsTr("No configured repository publishes a build for this platform.")
                            }
                        }
                    }
                }
            }
        }
    }

    // Global loading overlay — matches Package Manager UI's Reload feedback.
    LoadingOverlay {
        objectName: "appManager.loadingOverlay"
        anchors.fill: parent
        visible: root.loading
    }
}
