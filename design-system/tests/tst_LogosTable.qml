import QtQuick
import QtTest

import Logos.Theme
import Logos.Controls

TestCase {
    id: root
    name: "LogosTable"
    width: 800
    height: 400
    when: windowShown

    ListModel {
        id: peopleModel
        ListElement { name: "Alice";   role_: "Engineer" }
        ListElement { name: "Bob";     role_: "Designer" }
        ListElement { name: "Carol";   role_: "Manager"  }
    }

    Component {
        id: tableComp

        LogosTable {
            anchors.fill: parent
            model: peopleModel
            columns: [
                LogosTableColumn { title: "Name"; role: "name";   minWidth: 100; preferredWidth: 150; sortable: true },
                LogosTableColumn { title: "Role"; role: "role_";  minWidth: 100; preferredWidth: 200; sortable: true; fillWidth: true }
            ]
        }
    }

    function _create(parent) {
        return createTemporaryObject(tableComp, parent || root)
    }

    // ─── Defaults ───

    function test_defaults() {
        var t = _create()
        compare(t.selectionMode, LogosTable.None)
        compare(t.selectedIndices.length, 0)
        compare(t.allSelected, false)
        compare(t.sortRole, "")
        compare(t.sortOrder, Qt.AscendingOrder)
        compare(t.loading, false)
        compare(t.rowHeight, 64)
        compare(t.headerHeight, 36)
        verify(t.view !== null, "view alias must resolve to inner ListView")
    }

    function test_columns_assigned() {
        var t = _create()
        compare(t.columns.length, 2)
        compare(t.columns[0].title, "Name")
        compare(t.columns[1].fillWidth, true)
    }

    // ─── Sorting (consumer-driven) ───

    function test_header_click_emits_sortRequested_when_sortable() {
        var t = _create()
        var spy = sortSpy.createObject(root, { target: t })
        // Simulate header click via the internal helper (bypasses geometry).
        t.d._onHeaderClick(t.columns[0])
        compare(spy.count, 1)
        compare(spy.signalArguments[0][0], "name")
        compare(spy.signalArguments[0][1], Qt.AscendingOrder)
    }

    function test_sort_cycles_asc_then_desc() {
        var t = _create()
        var spy = sortSpy.createObject(root, { target: t })

        t.d._onHeaderClick(t.columns[0])
        compare(spy.signalArguments[0][1], Qt.AscendingOrder)

        // Simulate the consumer applying sort state, then click again.
        t.sortRole = "name"
        t.sortOrder = Qt.AscendingOrder
        t.d._onHeaderClick(t.columns[0])
        compare(spy.signalArguments[1][1], Qt.DescendingOrder)
    }

    function test_non_sortable_header_does_not_emit_sort() {
        var t = _create()
        t.columns[0].sortable = false
        var spy = sortSpy.createObject(root, { target: t })
        t.d._onHeaderClick(t.columns[0])
        compare(spy.count, 0)
    }

    function test_headerClicked_always_fires() {
        var t = _create()
        t.columns[0].sortable = false
        var spy = headerSpy.createObject(root, { target: t })
        t.d._onHeaderClick(t.columns[0])
        compare(spy.count, 1)
        compare(spy.signalArguments[0][0], "name")
    }

    // ─── Selection ───

    function test_selection_none_does_not_prepend_column() {
        var t = _create()
        compare(t.selectionMode, LogosTable.None)
        compare(t.d.effectiveColumns.length, 2)
    }

    function test_selection_multi_prepends_column() {
        var t = _create()
        t.selectionMode = LogosTable.Multi
        compare(t.d.effectiveColumns.length, 3)
        compare(t.d.effectiveColumns[0].minWidth, 40)
    }

    function test_toggle_row_in_multi_appends() {
        var t = _create()
        t.selectionMode = LogosTable.Multi
        var spy = selectionSpy.createObject(root, { target: t })

        t.d._toggleRow(0)
        compare(t.selectedIndices, [0])
        t.d._toggleRow(2)
        compare(t.selectedIndices, [0, 2])
        compare(spy.count, 2)
    }

    function test_toggle_row_in_multi_removes_when_present() {
        var t = _create()
        t.selectionMode = LogosTable.Multi
        t.d._toggleRow(0)
        t.d._toggleRow(1)
        t.d._toggleRow(0)   // remove
        compare(t.selectedIndices, [1])
    }

    function test_toggle_row_in_single_replaces() {
        var t = _create()
        t.selectionMode = LogosTable.Single
        t.d._toggleRow(0)
        compare(t.selectedIndices, [0])
        t.d._toggleRow(2)
        compare(t.selectedIndices, [2])
    }

    function test_toggle_row_in_single_clears_when_same() {
        var t = _create()
        t.selectionMode = LogosTable.Single
        t.d._toggleRow(1)
        t.d._toggleRow(1)
        compare(t.selectedIndices.length, 0)
    }

    function test_select_all_picks_every_row() {
        var t = _create()
        t.selectionMode = LogosTable.Multi
        t.d._toggleAll()
        compare(t.selectedIndices, [0, 1, 2])
        compare(t.allSelected, true)
    }

    function test_select_all_clears_when_already_all() {
        var t = _create()
        t.selectionMode = LogosTable.Multi
        t.d._toggleAll()
        t.d._toggleAll()   // unselect all
        compare(t.selectedIndices.length, 0)
        compare(t.allSelected, false)
    }

    // ─── Empty + loading ───

    function test_empty_when_model_count_zero() {
        var t = _create()
        var emptyModel = Qt.createQmlObject('import QtQuick; ListModel {}', t)
        t.model = emptyModel
        compare(t.empty, true)
    }

    function test_not_empty_with_data() {
        var t = _create()
        compare(t.empty, false)
    }

    // ─── Sizing inputs ───

    function test_totalMinWidth_sums_columns() {
        var t = _create()
        // Two columns at minWidth 100 each + selection col when active
        compare(t.d.totalMinWidth, 200)
        t.selectionMode = LogosTable.Multi
        compare(t.d.totalMinWidth, 240)
    }

    function test_column_sizing_defaults() {
        var col = Qt.createQmlObject('import Logos.Controls; LogosTableColumn {}', root)
        compare(col.minWidth, 80)
        compare(col.preferredWidth, 80)
        compare(col.maxWidth, -1)
        compare(col.fillWidth, false)
    }

    function test_preferredWidth_defaults_to_minWidth() {
        var col = Qt.createQmlObject('import Logos.Controls; LogosTableColumn { minWidth: 200 }', root)
        compare(col.preferredWidth, 200)
    }

    // ─── Row click signal ───

    function test_rowClicked_signal_can_be_emitted() {
        var t = _create()
        var spy = rowClickSpy.createObject(root, { target: t })
        t.rowClicked(1, peopleModel.get(1))
        compare(spy.count, 1)
        compare(spy.signalArguments[0][0], 1)
    }

    // ─── Override hooks compile + take effect ───

    function test_emptyText_default() {
        var t = _create()
        compare(t.emptyText.length > 0, true)
    }

    function test_view_alias_is_listview() {
        var t = _create()
        // The aliased ListView must accept ListView-only methods.
        verify(typeof t.view.positionViewAtIndex === "function",
               "view alias must expose ListView API")
    }

    // ─── Header tap areas ───

    Component {
        id: actionsTableComp

        LogosTable {
            anchors.fill: parent
            model: peopleModel
            columns: [
                LogosTableColumn { title: "Name"; role: "name"; minWidth: 100; fillWidth: true; sortable: true },
                LogosTableColumn { title: "Role"; role: "role_"; minWidth: 100 },
                LogosTableColumn { minWidth: 44; maxWidth: 44 }   // untitled actions column
            ]
        }
    }

    function _headerTapAreas(item, out) {
        out = out || []
        if (item.objectName === "headerTapArea")
            out.push(item)
        for (var i = 0; i < item.children.length; ++i)
            _headerTapAreas(item.children[i], out)
        return out
    }

    function test_untitled_header_cell_is_not_a_tap_target() {
        var t = createTemporaryObject(actionsTableComp, root)
        waitForRendering(t)
        var areas = _headerTapAreas(t)
        compare(areas.length, 3)
        areas.sort(function(a, b) { return a.mapToItem(t, 0, 0).x - b.mapToItem(t, 0, 0).x })
        compare(areas[0].enabled, true)    // sortable
        compare(areas[1].enabled, true)    // titled, not sortable: still emits headerClicked
        compare(areas[2].enabled, false)   // untitled, not sortable
    }

    function test_header_tap_area_spans_the_whole_cell() {
        var t = createTemporaryObject(actionsTableComp, root)
        waitForRendering(t)
        var areas = _headerTapAreas(t)
        areas.sort(function(a, b) { return a.mapToItem(t, 0, 0).x - b.mapToItem(t, 0, 0).x })
        // Cell widths: the Loader is inset by the padding; the tap area is not.
        compare(areas[0].mapToItem(t, 0, 0).x, 0)
        compare(areas[0].width, areas[1].mapToItem(t, 0, 0).x)
        compare(areas[0].height, t.headerHeight)
    }

    function test_sortable_header_reaches_44_under_touch() {
        Theme.compactWindow = true
        var t = createTemporaryObject(actionsTableComp, root)
        waitForRendering(t)
        compare(t.headerHeight, Theme.minTouchTarget)
        var areas = _headerTapAreas(t)
        areas.sort(function(a, b) { return a.mapToItem(t, 0, 0).x - b.mapToItem(t, 0, 0).x })
        verify(areas[0].height >= Theme.minTouchTarget)
        verify(areas[0].width >= Theme.minTouchTarget)
        Theme.compactWindow = false
    }

    // ─── SignalSpy templates (re-used per test) ───

    Component { id: sortSpy;       SignalSpy { signalName: "sortRequested"  } }
    Component { id: headerSpy;     SignalSpy { signalName: "headerClicked"  } }
    Component { id: selectionSpy;  SignalSpy { signalName: "selectionChanged" } }
    Component { id: rowClickSpy;   SignalSpy { signalName: "rowClicked"     } }
}
