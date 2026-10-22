import QtQuick
import QtTest

import Logos.Theme
import Logos.Controls

// Theme.compactWindow / Theme.touch: a compact window floors tappables at
// Theme.minTouchTarget at any density, without density's scale.
TestCase {
    id: root
    name: "ThemeCompactWindow"
    width: 400
    height: 300
    when: windowShown

    Component {
        id: scaffoldComp
        Item {
            property alias scaffold: sc
            width: 390; height: 300
            LogosAppScaffold { id: sc; title: "App"; anchors.fill: parent }
        }
    }

    LogosMenuItem { id: menuItem; text: "Rename" }
    LogosComboBox { id: combo; model: ["a"] }
    LogosPaginator { id: pager; totalCount: 200; pageSize: 20; currentPage: 4; width: 500 }

    function init() {
        Theme.setDensity("comfortable")
        Theme.compactWindow = false
        pager.forceCompact = false
    }
    function cleanup() {
        Theme.compactWindow = false
        Theme.setDensity("")
    }

    function test_touch_follows_compactWindow() {
        compare(Theme.touch, false)
        Theme.compactWindow = true
        compare(Theme.touch, true)
        compare(Theme.density, "comfortable")
        Theme.compactWindow = false
        compare(Theme.touch, false)
    }

    function test_touch_density_is_touch_without_compactWindow() {
        Theme.setDensity("touch")
        compare(Theme.compactWindow, false)
        compare(Theme.touch, true)
    }

    function test_size_floors_at_44_without_scaling() {
        Theme.compactWindow = true
        compare(Theme.densityScale, 1.0)
        compare(Theme._size(32), Theme.minTouchTarget)   // floored
        compare(Theme._size(54), 54)                     // not scaled (touch: 65)
        compare(Theme._pad(12), 12)                      // paddings never scale with compactWindow
        compare(Theme.components.menuItem.height, 44)
        compare(Theme.components.combo.height, 44)
        compare(Theme.components.button.height, 44)      // 53 at touch density
        compare(Theme.components.search.height, 54)
    }

    function test_controls_reach_44_in_a_compact_window() {
        compare(menuItem.implicitHeight, 32)
        compare(combo.implicitHeight, 32)
        Theme.compactWindow = true
        compare(menuItem.implicitHeight, 44)
        compare(combo.implicitHeight, 44)
    }

    function test_compactWindow_is_not_persisted() {
        verify(Theme.settings.compactWindow === undefined)
    }

    function test_scaffold_sets_compactWindow_from_its_width() {
        // Parentless host: the TestCase item is not visible, and the
        // scaffold only drives Theme.compactWindow while it is.
        var host = createTemporaryObject(scaffoldComp, null)
        var s = host.scaffold
        verify(s.visible)
        tryCompare(Theme, "compactWindow", true)
        host.width = 1000
        tryCompare(Theme, "compactWindow", false)
        host.width = 390
        tryCompare(Theme, "compactWindow", true)
        host.visible = false                 // a hidden scaffold does not count
        tryCompare(Theme, "compactWindow", false)
        // (A parentless item hidden once does not become visible again in
        // this window-less harness, so re-showing is not checked here.)
        host.width = 1000
        tryCompare(Theme, "compactWindow", false)
        Theme.compactWindow = true           // destruction resets it
        s.destroy()
        tryCompare(Theme, "compactWindow", false)
    }

    function test_paginator_goes_compact_by_itself() {
        // 500 px fits the 36 px cells (full form min 444) on desktop...
        compare(pager.compactForm, false)
        // ...but not the 44 px cells of a compact window (min 580).
        Theme.compactWindow = true
        compare(pager._d.cell, 44)
        compare(pager.compactForm, true)
        compare(pager.prevButton.width >= 44, true)
    }

    function test_paginator_forceCompact() {
        compare(pager.compactForm, false)
        pager.forceCompact = true
        compare(pager.compactForm, true)
        pager.forceCompact = false
        compare(pager.compactForm, false)
    }
}
