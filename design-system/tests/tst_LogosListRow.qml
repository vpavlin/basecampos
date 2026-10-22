import QtQuick
import QtQuick.Controls
import QtTest

import Logos.Theme
import Logos.Controls

TestCase {
    name: "LogosListRow"
    width: 400
    height: 300
    when: windowShown

    LogosListRow {
        id: row
        width: 360
        title: "Run a node"
        subtitle: "Uses about 2 GB of disk"
        accessory: LogosSwitch { id: sw }
    }
    SignalSpy { id: toggledSpy; target: sw; signalName: "toggled" }

    LogosListRow { id: plain; y: 100; width: 360; title: "Network"; value: "Testnet"; showChevron: true }

    // The suite's window isn't exposed, so input events can't be delivered;
    // drive the row's click handler directly.
    function test_row_tap_toggles_and_emits() {
        toggledSpy.clear()
        const before = sw.checked
        row.clicked()
        compare(sw.checked, !before, "tapping the row flips the switch")
        compare(toggledSpy.count, 1, "and the app hears it through toggled")
    }

    function test_height_at_least_touch_target() {
        verify(plain.height >= Theme.minTouchTarget)
        verify(row.height >= Theme.minTouchTarget)
    }

    function test_accessory_reparented_into_row() {
        verify(sw.parent !== null)
        verify(sw.parent !== row, "accessory sits in the row's slot, not on the row")
        let p = sw.parent, inside = false
        while (p) { if (p === row) inside = true; p = p.parent }
        verify(inside, "the slot is inside the row")
    }
}
