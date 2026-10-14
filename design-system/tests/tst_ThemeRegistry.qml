import QtQuick
import QtTest

import Logos.Theme

// Themes come from tests/fixtures/themes and the repo's themes/ directory;
// tests/main.cpp points LOGOS_THEME_PATH at both.
TestCase {
    name: "ThemeRegistry"

    function cleanupTestCase() {
        Theme.setTheme("dark")
    }

    function findTheme(id) {
        var list = ThemeRegistry.themes
        for (var i = 0; i < list.length; ++i)
            if (list[i].id === id)
                return list[i]
        return null
    }

    function minimalDoc(extra) {
        var doc = { formatVersion: 1, id: "test.doc", name: "Doc", author: "tests", mode: "dark" }
        for (var k in extra)
            doc[k] = extra[k]
        return JSON.stringify(doc)
    }

    // ----- token contract -----

    function test_palette_tokens_match_dark_theme() {
        var tokens = ThemeRegistry.paletteTokens
        compare(tokens.length, 62)
        for (var i = 0; i < tokens.length; ++i)
            verify(Theme.darkTheme[tokens[i]] !== undefined, "DarkTheme lacks " + tokens[i])
    }

    // ----- parsing -----

    function test_minimal_document_is_valid() {
        var r = ThemeRegistry.parse(minimalDoc({}))
        verify(r.ok, r.errors.join("; "))
        compare(r.theme.id, "test.doc")
        compare(Object.keys(r.theme.palette).length, 0)
    }

    function test_rgba_is_css_order() {
        var r = ThemeRegistry.parse(minimalDoc({ palette: { scrim: "#FF000080" } }))
        verify(r.ok)
        var c = r.theme.palette.scrim
        compare(c.r, 1)
        compare(c.g, 0)
        fuzzyCompare(c.a, 128 / 255, 0.001)
    }

    function test_bad_values_are_dropped_with_warnings() {
        var r = ThemeRegistry.parse(minimalDoc({
            palette: { text: "red", background: "#12345", notAToken: "#000000", surface: 7 },
            spacing: { small: -40 },
            extra: 1
        }))
        verify(r.ok)
        compare(Object.keys(r.theme.palette).length, 0)
        compare(r.theme.spacing.small, 0)
        verify(r.warnings.length >= 6, r.warnings.join("; "))
    }

    function test_derived_tokens_follow_their_source() {
        var r = ThemeRegistry.parse(minimalDoc({ palette: { primary: "#336699" } }))
        var border = r.theme.palette.primaryBorder
        compare(border.r, r.theme.palette.primary.r)
        fuzzyCompare(border.a, 0.4, 0.01)
    }

    function test_rejects_invalid_documents() {
        var cases = [
            "",
            "{ not json",
            "[1, 2, 3]",
            JSON.stringify({ id: "test.x", name: "X", author: "a", mode: "dark" }),
            minimalDoc({ id: "../../etc" }),
            minimalDoc({ id: "Upper.Case" }),
            minimalDoc({ id: "nodot" }),
            minimalDoc({ id: "dark" }),
            minimalDoc({ mode: "sepia" }),
            minimalDoc({ name: "" }),
            minimalDoc({ author: "x".repeat(41) }),
        ]
        for (var i = 0; i < cases.length; ++i) {
            var r = ThemeRegistry.parse(cases[i])
            verify(!r.ok, "case " + i + " should be rejected: " + cases[i])
            verify(r.errors.length > 0)
        }
    }

    function test_rejects_oversized_document() {
        var r = ThemeRegistry.parse(minimalDoc({ padding: "x".repeat(70 * 1024) }))
        verify(!r.ok)
    }

    function test_newer_format_still_loads() {
        var r = ThemeRegistry.parse(minimalDoc({ formatVersion: 2, palette: { text: "#000000" } }))
        verify(r.ok)
        verify(r.warnings.join(" ").indexOf("newer") >= 0)
        compare(r.theme.palette.text.r, 0)
    }

    function test_low_contrast_is_flagged() {
        var r = ThemeRegistry.parse(minimalDoc({ palette: { text: "#777777", background: "#808080" } }))
        verify(r.ok)
        verify(r.theme.lowContrast)
    }

    // ----- discovery -----

    function test_discovers_valid_themes_only() {
        verify(findTheme("logos.light"), "logos.light not found in " + ThemeRegistry.searchPaths)
        verify(findTheme("test.partial"))
        verify(!findTheme("test.broken"))
        verify(!findTheme("dark"))
        // Symlinked package directories are skipped.
        verify(!findTheme("test.outside"))
    }

    function test_logos_light_is_complete_and_readable() {
        var light = findTheme("logos.light")
        compare(Object.keys(light.palette).length, 62)
        verify(!light.lowContrast)
        verify(light.contrast.textOnBackground >= 4.5)
        verify(light.contrast.primaryOnBackground >= 3.0)
    }

    // ----- Theme.qml integration -----

    function test_set_installed_theme() {
        Theme.setTheme("logos.light")
        compare(Theme.currentTheme, "logos.light")
        verify(!Theme.isDark)
        compare(Theme.palette.background.toString(), "#fafafa")
        compare(Theme.themeInfo["logos.light"].author, "Logos")
        verify(Theme.themeNames.indexOf("logos.light") >= 0)
    }

    function test_partial_theme_falls_back_per_token() {
        Theme.setTheme("test.partial")
        compare(Theme.palette.text.toString(), "#112233")
        // "surface": "red" was rejected, so it comes from the default.
        compare(Theme.palette.surface, Theme.darkTheme.surface)
        compare(Theme.palette.info, Theme.darkTheme.info)
    }

    function test_unknown_theme_is_refused() {
        Theme.setTheme("dark")
        Theme.setTheme("nobody.nothing")
        compare(Theme.currentTheme, "dark")
        verify(Theme.isDark)
        compare(Theme.palette, Theme.darkTheme)
    }

    function test_switch_theme_cycles_through_installed() {
        Theme.setTheme("dark")
        var seen = {}
        for (var i = 0; i < Theme.themeNames.length; ++i) {
            seen[Theme.currentTheme] = true
            Theme.switchTheme()
        }
        compare(Object.keys(seen).length, Theme.themeNames.length)
        compare(Theme.currentTheme, "dark")
    }
}
