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
    property string familyOverride: ""
    property string monoOverride: ""

    // Installed families, read once; a theme naming a missing font falls back.
    readonly property var _installed: Qt.fontFamilies()
    readonly property bool familyAvailable: familyOverride.length === 0 || _installed.indexOf(familyOverride) >= 0
    readonly property bool monoAvailable: monoOverride.length === 0 || _installed.indexOf(monoOverride) >= 0

    readonly property string _bundledSans: publicSansRegular.status == FontLoader.Ready ? publicSansRegular.name : "sans-serif"

    // Body family. Kept under its historical name: every control reads it, so
    // a theme's family restyles all text. Fallback: bundled Public Sans.
    readonly property string publicSans: familyOverride.length > 0 && familyAvailable ? familyOverride : _bundledSans
    // Same value under a role name, for new code.
    readonly property string family: publicSans

    // Family for hex identifiers, addresses and code, where fixed-width glyphs
    // aid scanning and copying. Defaults to the platform's monospace.
    readonly property string mono: monoOverride.length > 0 && monoAvailable ? monoOverride : "monospace"

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
