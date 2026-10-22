import QtQuick
import QtQuick.Controls

import Logos.Theme
import Logos.Controls
import Logos.Icons

// The lab: every common app layout inside one LogosAppScaffold, so resizing
// the window shows sidebar → rail → ☰ drawer and each screen's own rules.
ApplicationWindow {
    id: window
    visible: true
    width: LabArgs.width
    height: LabArgs.height
    title: "Logos Responsive Lab"
    color: Theme.palette.background

    readonly property var pages: [
        { id: "mail",      label: "Mail",       icon: LogosIcons.list,  badge: 3, file: "MailPage.qml" },
        { id: "settings",  label: "Settings",   icon: LogosIcons.more,  file: "SettingsPage.qml" },
        { id: "form",      label: "Form",       icon: LogosIcons.pages, file: "FormPage.qml" },
        { id: "dashboard", label: "Dashboard",  icon: LogosIcons.grid,  file: "DashboardPage.qml" },
        { id: "table",     label: "Table",      icon: LogosIcons.list,  file: "TablePage.qml" },
        { id: "gallery",   label: "Components", icon: LogosIcons.check, file: "GalleryPage.qml" },
    ]
    property string current: LabArgs.page
    readonly property var currentPage: pages.find(p => p.id === current) || pages[0]

    Component.onCompleted: {
        // Always set: the density is saved in settings, so a run without
        // --density would otherwise inherit the last run's.
        Theme.setDensity(LabArgs.density || "comfortable")
        // --state drawer=1: shoot the compact navigation drawer open.
        if (LabArgs.state.drawer) Qt.callLater(scaffold.drawer.open)
    }

    LogosAppScaffold {
        id: scaffold
        anchors.fill: parent
        title: "Responsive Lab"
        pageTitle: loader.item && loader.item.title ? loader.item.title : window.currentPage.label
        navModel: window.pages
        currentNav: window.currentPage.id
        canGoBack: loader.item ? !!loader.item.canGoBack : false
        onNavActivated: id => window.current = id
        onBackRequested: if (loader.item && loader.item.goBack) loader.item.goBack()

        Loader {
            id: loader
            anchors.fill: parent
            source: "pages/" + window.currentPage.file
            // Pages read the window class and lab state from here.
            property string windowClass: scaffold.windowClass
            property var labState: LabArgs.state
        }
    }
}
