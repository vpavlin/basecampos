import QtQuick
import QtTest

import Logos.Controls
import Logos.Theme

TestCase {
    name: "LogosMenuItem"
    width: 200
    height: 50
    when: windowShown

    LogosMenuItem {
        id: item
        text: "Action"
    }

    LogosMenuItem {
        id: plain
        width: 200
        text: "Rename"
    }

    SignalSpy {
        id: triggeredSpy
        target: item
        signalName: "triggered"
    }

    function init() {
        triggeredSpy.clear()
        item.text = "Action"
        item.enabled = true
        item.checkable = false
        item.checked = false
    }

    function test_aliases_resolve() {
        verify(item.labelItem)
        verify(item.backgroundItem)
    }

    function test_text_round_trips() {
        item.text = "Updated"
        tryCompare(item.labelItem, "text", "Updated")
    }

    function test_triggered_fires() {
        item.triggered()
        compare(triggeredSpy.count, 1)
    }

    function test_joins_tab_focus_chain() {
        compare(item.activeFocusOnTab, true)
        compare(item.focusPolicy, Qt.StrongFocus)
    }

    // ─── Check mark (checkable && checked) ───

    // The TestCase item itself is not visible, so a visibility check needs a
    // parentless item (effectively visible on its own).
    Component { id: looseItemComp; LogosMenuItem { width: 200; text: "Sort by name" } }

    function test_check_mark_only_when_checkable_and_checked() {
        var m = createTemporaryObject(looseItemComp, null)
        verify(m.visible)
        verify(m.indicatorItem)
        compare(m.indicatorItem.visible, false)       // not checkable
        m.checked = true                               // checked, not checkable
        compare(m.indicatorItem.visible, false)
        m.checkable = true
        m.checked = true
        compare(m.indicatorItem.visible, true)
        m.checked = false
        compare(m.indicatorItem.visible, false)
    }

    function test_check_mark_sits_at_the_trailing_edge() {
        item.width = 200
        item.checkable = true
        item.checked = true
        var ind = item.indicatorItem
        verify(ind.x > item.width / 2)
        verify(ind.x + ind.width <= item.width - item.rightPadding)
        // vertically centred
        fuzzyCompare(ind.y + ind.height / 2, item.height / 2, 1)
    }

    function test_checkable_reserves_the_space_checked_or_not() {
        var plainPad = plain.labelItem.rightPadding
        item.checkable = true
        item.checked = false
        var pad = item.labelItem.rightPadding
        verify(pad > plainPad)
        item.checked = true
        compare(item.labelItem.rightPadding, pad)
        // The text never runs under the mark.
        verify(item.indicatorItem.x >= item.width - item.rightPadding - pad)
    }

    function test_non_checkable_label_unchanged() {
        compare(plain.labelItem.leftPadding, Theme.components.menuItem.paddingX)
        compare(plain.labelItem.rightPadding, Theme.components.menuItem.paddingX)
        compare(plain.indicatorItem.visible, false)
    }
}
