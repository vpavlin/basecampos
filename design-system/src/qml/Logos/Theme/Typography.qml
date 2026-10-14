import QtQuick

QtObject {
    //  font family - using resolved URL for better compatibility
    readonly property FontLoader publicSansRegular: FontLoader {
        source: Qt.resolvedUrl("fonts/PublicSans-Regular.ttf")
        onStatusChanged: if (status == FontLoader.Error) console.warn("Failed to load PublicSans-Regular")
    }
    readonly property FontLoader publicSansMedium: FontLoader {
        source: Qt.resolvedUrl("fonts/PublicSans-Medium.ttf")
        onStatusChanged: if (status == FontLoader.Error) console.warn("Failed to load PublicSans-Medium")
    }
    readonly property FontLoader publicSansBold: FontLoader {
        source: Qt.resolvedUrl("fonts/PublicSans-Bold.ttf")
        onStatusChanged: if (status == FontLoader.Error) console.warn("Failed to load PublicSans-Bold")
    }
    // Families the active theme asks for (names of fonts installed on the
    // system; set by Theme.qml). Empty = use the defaults below.
    // Each may be a name or a fallback list of names; the first installed wins.
    property var familyOverride: ""
    property var monoOverride: ""

    // Installed families, read once; a theme naming a missing font falls back.
    readonly property var _installed: Qt.fontFamilies()
    // A list arriving from C++ is a sequence wrapper, not a JS Array, so test
    // for "string" rather than Array.isArray().
    function _names(wanted) {
        if (!wanted) return []
        if (typeof wanted === "string") return [wanted]
        return Array.prototype.slice.call(wanted)
    }
    function _pick(wanted) {
        const list = _names(wanted)
        for (let i = 0; i < list.length; ++i)
            if (_installed.indexOf(list[i]) >= 0)
                return list[i]
        return ""
    }
    readonly property string _familyPick: _pick(familyOverride)
    readonly property string _monoPick: _pick(monoOverride)
    readonly property bool familyAvailable: _names(familyOverride).length === 0 || _familyPick.length > 0
    readonly property bool monoAvailable: _names(monoOverride).length === 0 || _monoPick.length > 0
    // What the theme asked for, for display ("JetBrains Mono, …").
    readonly property string familyRequested: _names(familyOverride).join(", ")

    readonly property string _bundledSans: publicSansRegular.status == FontLoader.Ready ? publicSansRegular.name : "sans-serif"

    // Body family. Kept under its historical name: every control reads it, so
    // a theme's family restyles all text. Fallback: bundled Public Sans.
    readonly property string publicSans: _familyPick.length > 0 ? _familyPick : _bundledSans
    // Same value under a role name, for new code.
    readonly property string family: publicSans

    // Family for hex identifiers, addresses and code, where fixed-width glyphs
    // aid scanning and copying. Defaults to the platform's monospace.
    readonly property string mono: _monoPick.length > 0 ? _monoPick : "monospace"

    // weights
    readonly property int weightRegular: 400
    readonly property int weightMedium: 500
    readonly property int weightBold: 700

    // Text font sizes (descending hierarchy)
    readonly property int mainTitleText: 256   // splash / marketing
    readonly property int pageTitleText: 36    // page-level headline
    readonly property int titleText: 30        // section / dialog title
    readonly property int panelTitleText: 24   // panel / table title
    readonly property int subtitleText: 16     // page subtitle / lead body
    readonly property int primaryText: 14      // default body text
    readonly property int secondaryText: 12    // caption / small text
    readonly property int badgeText: 8         // badge / tiny text
}
