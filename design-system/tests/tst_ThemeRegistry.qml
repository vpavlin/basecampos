import QtQuick
import QtTest

import Logos.Theme
import Logos.Controls

// Themes come from tests/fixtures/themes and the repo's themes/ directory;
// tests/main.cpp points LOGOS_THEME_PATH at both.
TestCase {
    name: "ThemeRegistry"

    LogosSwitch { id: sw; checked: true }
    LogosCheckbox { id: cb; checked: true }
    LogosButton { id: primaryBtn; text: "Go"; variant: LogosButton.Variant.Primary }
    LogosButton { id: plainBtn; text: "Go" }
    LogosTabButton { id: tabBtn; text: "Tab" }
    LogosItemDelegate { id: itemDel; text: "Item" }
    LogosCheckbox { id: cbLabel; text: "Check" }
    LogosTextField { id: field; text: "typed" }

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
        compare(tokens.length, 65)
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
        compare(Object.keys(light.palette).length, 65)
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

    // ----- content on primary -----

    function test_content_on_primary_uses_primary_foreground() {
        var themes = ["dark", "logos.light"]
        for (var i = 0; i < themes.length; ++i) {
            Theme.setTheme(themes[i])
            compare(sw.handleColor, Theme.palette.primaryForeground, themes[i] + " switch handle")
            compare(cb.checkmarkColor, Theme.palette.primaryForeground, themes[i] + " checkmark")
            compare(primaryBtn.labelItem.color, Theme.palette.primaryForeground, themes[i] + " primary label")
            compare(plainBtn.labelItem.color, Theme.palette.text, themes[i] + " plain label")
        }
        sw.checked = false
        compare(sw.handleColor, Theme.palette.text)
        sw.checked = true
        Theme.setTheme("dark")
    }

    function test_new_tokens_keep_dark_unchanged() {
        // primaryForeground and brandMark equal the old `text` value in the default theme.
        compare(Theme.darkTheme.primaryForeground, Theme.darkTheme.text)
        compare(Theme.darkTheme.brandMark, Theme.darkTheme.text)
        // accentForeground keeps the black glyph dark already draws on accentOrange.
        compare(Theme.darkTheme.accentForeground, Theme.darkTheme.backgroundBlack)
    }

    // ----- process-wide current theme -----

    function test_current_is_shared_state() {
        Theme.setTheme("logos.light")
        compare(ThemeRegistry.current, "logos.light")
        // Writing the shared state directly (what another engine's setTheme
        // does) moves this engine's Theme too.
        ThemeRegistry.current = "dark"
        compare(Theme.currentTheme, "dark")
        compare(Theme.palette, Theme.darkTheme)
        ThemeRegistry.current = "logos.light"
        compare(Theme.palette.background.toString(), "#fafafa")
        Theme.setTheme("dark")
    }

    // ----- font families -----

    function test_family_names_validate() {
        var r = ThemeRegistry.parse(minimalDoc({ typography: { family: "Noto Serif", monoFamily: "JetBrains Mono" } }))
        verify(r.ok)
        compare(r.theme.typography.family, "Noto Serif")
        compare(r.theme.typography.monoFamily, "JetBrains Mono")
        var bad = ThemeRegistry.parse(minimalDoc({ typography: { family: "<img src=x>", monoFamily: 12 } }))
        verify(bad.ok)
        compare(bad.theme.typography.family, undefined)
        compare(bad.theme.typography.monoFamily, undefined)
        verify(bad.warnings.join(" ").indexOf("font family") >= 0)
    }

    function test_missing_family_falls_back() {
        var bundled = Theme.typography.publicSans
        Theme.setTheme("test.fonts")
        compare(Theme.typography.familyOverride, "No Such Font Family")
        verify(!Theme.typography.familyAvailable)
        compare(Theme.typography.publicSans, bundled)
        compare(Theme.typography.mono, "monospace")
        Theme.setTheme("dark")
    }

    function test_installed_family_applies() {
        // The bundled face is always installed, so a theme naming it must
        // take effect (and the override must follow the active theme).
        Theme.setTheme("test.fonts-ok")
        compare(Theme.typography.familyOverride, "Public Sans")
        verify(Theme.typography.familyAvailable)
        compare(Theme.typography.family, "Public Sans")
        Theme.setTheme("dark")
        compare(Theme.typography.familyOverride, "")
    }

    function test_controls_render_in_theme_family() {
        // Controls that pass `font: root.font` into LogosText used to replace
        // the themed family with the Control's default font.
        const fam = Theme.typography.family
        compare(tabBtn.labelItem.font.family, fam, "LogosTabButton")
        compare(itemDel.labelItem.font.family, fam, "LogosItemDelegate")
        compare(cbLabel.labelItem.font.family, fam, "LogosCheckbox")
        compare(field.textInput.font.family, fam, "LogosTextField")
        compare(primaryBtn.labelItem.font.family, fam, "LogosButton")
    }

    // ----- corners and font fallback lists -----

    function test_radius_scale_parses_and_clamps() {
        var r = ThemeRegistry.parse(minimalDoc({ radius: { scale: 3, small: 2 } }))
        verify(r.ok)
        compare(r.theme.radius.scale, 1)
        compare(r.theme.radius.small, 2)
        verify(r.warnings.join(" ").indexOf("scale") >= 0)
    }

    function test_radius_scale_squares_everything() {
        compare(Theme.spacing.radiusLarge, 8)
        compare(Theme.spacing.roundness, 1)
        Theme.setTheme("test.square")
        compare(Theme.spacing.radiusLarge, 0)     // override 20, scaled by 0
        compare(Theme.spacing.radiusPill, 0)
        compare(Theme.spacing.roundness, 0)
        compare(sw.handleColor !== undefined, true)
        Theme.setTheme("dark")
        compare(Theme.spacing.radiusLarge, 8)
    }

    function test_family_fallback_list_picks_first_installed() {
        Theme.setTheme("test.square")
        compare(Theme.typography.family, "Public Sans")
        verify(Theme.typography.familyAvailable)
        compare(Theme.typography.familyRequested, "No Such Font, Public Sans")
        Theme.setTheme("dark")
    }

    function test_token_or_keeps_dark_literal() {
        Theme.setTheme("dark")
        compare(Theme.tokenOr("textSecondary", "#a0a0a0"), "#a0a0a0")
        Theme.setTheme("logos.light")
        compare(Theme.tokenOr("textSecondary", "#a0a0a0"), Theme.palette.textSecondary)
        Theme.setTheme("dark")
    }

    // ----- gradients (v2) -----

    LogosSurface { id: surface; width: 100; height: 40; slot: "sidebar"; color: "#123456" }

    function test_gradients_parse_linear_and_radial() {
        var r = ThemeRegistry.parse(minimalDoc({ gradients: {
            sidebar: { type: "linear", angle: 450, stops: [["1", "#000000"], [0, "#FF0000"]] },
            workspace: { type: "radial", center: [0.5, 0], radius: 9,
                         stops: [[0, "#111111"], [1, "#222222"]] }
        } }))
        verify(r.ok, r.errors.join("; "))
        var g = r.theme.gradients
        compare(g.sidebar.type, "linear")
        compare(g.sidebar.angle, 90, "angle taken modulo 360")
        compare(g.sidebar.stops[0].position, 0, "stops sorted by position")
        compare(String(g.sidebar.stops[0].color), "#ff0000")
        compare(g.workspace.type, "radial")
        compare(g.workspace.radius, 3, "radius clamped to 3")
        compare(g.workspace.center[1], 0)
    }

    function test_gradients_reject_bad_slots_quietly() {
        var r = ThemeRegistry.parse(minimalDoc({ gradients: {
            nope: { type: "linear", stops: [[0, "#000000"], [1, "#FFFFFF"]] },
            header: { type: "conic", stops: [[0, "#000000"], [1, "#FFFFFF"]] },
            accent: { type: "linear", stops: [[0, "#000000"], [1, "red"]] }
        } }))
        verify(r.ok, "bad gradients never reject the theme")
        compare(Object.keys(r.theme.gradients).length, 0)
        verify(r.warnings.length >= 3, r.warnings.join("; "))
    }

    function test_gradient_contrast_checked_per_stop() {
        var r = ThemeRegistry.parse(minimalDoc({
            palette: { text: "#FFFFFF" },
            gradients: { workspace: { type: "linear", stops: [[0, "#000000"], [1, "#FAFAFA"]] } }
        }))
        verify(r.ok)
        verify(r.warnings.join(" ").indexOf("gradients.workspace") >= 0, r.warnings.join("; "))
    }

    function test_surface_builds_stops_when_a_theme_with_gradients_loads() {
        // Regression: rebuild() ran before hasGradient updated, so a theme
        // switch left the gradient with no stops (drawn transparent).
        var spec = { type: "linear", angle: 0, stops: [{ position: 0, color: "#ff0000" },
                                                       { position: 1, color: "#0000ff" }] }
        var s = createTemporaryObject(surfaceComponent, surface)
        compare(s.stopCount, 0)
        s.gradient = spec
        compare(s.stopCount, 2)
        verify(s.hasGradient)
        s.gradient = null
        compare(s.stopCount, 0)
    }

    Component {
        id: surfaceComponent
        LogosSurface { width: 50; height: 20; gradient: null }
    }

    function test_surface_is_solid_without_a_gradient() {
        Theme.setTheme("dark")
        compare(Object.keys(Theme.gradients).length, 0)
        verify(!surface.hasGradient, "dark sets no gradients")
    }

    // ----- icon packs (v2) -----

    readonly property string lucideSearch:
        '<!-- @license lucide-static - ISC --><svg class="lucide" xmlns="http://www.w3.org/2000/svg" ' +
        'width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">' +
        '<circle cx="11" cy="11" r="8" /><path d="m21 21-4.3-4.3" /></svg>'

    function test_sanitizer_keeps_shapes_and_whitens_paints() {
        var out = ThemeRegistry.sanitizedSvg(lucideSearch, true)
        verify(out.length > 0, "a plain Lucide icon is accepted")
        verify(out.indexOf("<circle") >= 0 && out.indexOf("<path") >= 0, out)
        verify(out.indexOf("currentColor") < 0, "currentColor becomes white")
        verify(out.indexOf('stroke="#FFFFFF"') >= 0, out)
        verify(out.indexOf('fill="none"') >= 0, "fill none is kept")
        verify(out.indexOf("class=") < 0 && out.indexOf("license") < 0, "class and comments dropped")
    }

    function test_sanitizer_strips_scripts_handlers_and_external_refs() {
        var evil = '<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" viewBox="0 0 24 24" onload="x()">' +
            '<script>alert(1)</script><style>*{}</style>' +
            '<foreignObject><div xmlns="http://www.w3.org/1999/xhtml">hi</div></foreignObject>' +
            '<image href="file:///etc/passwd" width="24" height="24"/>' +
            '<use xlink:href="https://evil.example/x.svg#a"/>' +
            '<path d="M0 0h24v24H0z" fill="url(https://evil.example/p)" onclick="y()"/></svg>'
        var out = ThemeRegistry.sanitizedSvg(evil, true)
        verify(out.length > 0, "the safe part survives")
        var bad = ["script", "style", "foreignObject", "<image", "onload", "onclick", "evil.example", "passwd"]
        for (var i = 0; i < bad.length; ++i)
            verify(out.indexOf(bad[i]) < 0, bad[i] + " must be stripped: " + out)
        verify(out.indexOf("<path") >= 0)
    }

    function test_sanitizer_rejects_entities_non_svg_and_non_square() {
        compare(ThemeRegistry.sanitizedSvg('<!DOCTYPE svg [<!ENTITY a "aaaa">]><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">&a;</svg>'), "")
        compare(ThemeRegistry.sanitizedSvg('<html xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24"/>'), "")
        compare(ThemeRegistry.sanitizedSvg('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 12"><path d="M0 0"/></svg>'), "")
        compare(ThemeRegistry.sanitizedSvg('not xml'), "")
    }

    function test_icons_block_accepts_only_assets_paths() {
        var r = ThemeRegistry.parse(minimalDoc({ icons: { set: {
            search: "assets/icons/search.svg",
            close: "../../etc/x.svg",
            copy: "/abs/copy.svg",
            check: "assets/icons/check.png",
            "Bad Name": "assets/icons/x.svg"
        } } }))
        verify(r.ok)
        compare(Object.keys(r.theme.icons.set).length, 1)
        compare(r.theme.icons.set.search, "assets/icons/search.svg")
    }

    function test_icon_or_falls_back_without_a_pack() {
        Theme.setTheme("dark")
        compare(Theme.iconOr("search", "qrc:/fallback.svg"), "qrc:/fallback.svg")
    }

    // ----- backgrounds (v2) -----

    function test_backgrounds_parse_and_validate() {
        var r = ThemeRegistry.parse(minimalDoc({ backgrounds: {
            workspace: { image: "assets/bg/night.jpg", fit: "contain", opacity: 3,
                         tint: "#000000", tintStrength: 0.2 },
            sidebar: { image: "assets/bg/x.jpg" },
            onboarding: { image: "../../etc/x.jpg" }
        } }))
        verify(r.ok)
        compare(Object.keys(r.theme.backgrounds).length, 1, "only the valid workspace slot")
        var w = r.theme.backgrounds.workspace
        compare(w.image, "assets/bg/night.jpg")
        compare(w.fit, "contain")
        compare(w.opacity, 1, "opacity clamped")
        compare(w.tintStrength, 0.2)
        verify(r.warnings.join(" ").indexOf("backgrounds.sidebar") >= 0, r.warnings.join("; "))
    }

    function test_motion_is_bounded() {
        var r = ThemeRegistry.parse(minimalDoc({
            backgrounds: { workspace: { image: "assets/bg/a.jpg", motion: { strength: 5, period: 1 } } },
            gradients: { sidebar: { type: "linear", stops: [[0, "#000000"], [1, "#FFFFFF"]],
                                    motion: { strength: 0.4, period: 40 } } }
        }))
        verify(r.ok)
        compare(r.theme.backgrounds.workspace.motion.strength, 1)
        compare(r.theme.backgrounds.workspace.motion.period, 10)
        compare(r.theme.gradients.sidebar.motion.period, 40)
        verify(r.warnings.join(" ").indexOf("motion") >= 0)
    }

    // ----- component tokens & density -----

    LogosTextField { id: densityField }
    LogosComboBox { id: densityCombo; model: ["a"] }

    function cleanup_density() { Theme.setDensity("") }

    function test_component_defaults_equal_todays_literals() {
        Theme.setTheme("dark")
        Theme.setDensity("")
        compare(Theme.density, "comfortable")
        compare(plainBtn.implicitHeight, 44)
        compare(densityField.implicitHeight, 40)
        compare(densityCombo.implicitHeight, 32)
        compare(Theme.components.button.radius, Theme.spacing.radiusXlarge)
    }

    function test_touch_density_reaches_44px_targets() {
        Theme.setDensity("touch")
        verify(densityCombo.implicitHeight >= 44, "combo " + densityCombo.implicitHeight)
        compare(densityField.implicitHeight, 48)
        compare(plainBtn.implicitHeight, 53)
        Theme.setDensity("")
    }

    function test_compact_density_shrinks() {
        Theme.setDensity("compact")
        compare(densityField.implicitHeight, 34)
        verify(plainBtn.implicitHeight < 44)
        Theme.setDensity("")
    }

    function test_components_parse_and_clamp() {
        var r = ThemeRegistry.parse(minimalDoc({
            density: "touch",
            components: {
                button: { height: 200, radius: 0, borderWidth: 2 },
                card: { shadowColor: "#FF000080", shadowOpacity: 0.4, shadowBlur: 12 },
                widget: { height: 10 }
            }
        }))
        verify(r.ok)
        compare(r.theme.density, "touch")
        compare(r.theme.components.button.height, 72, "clamped")
        compare(r.theme.components.button.radius, 0)
        compare(r.theme.components.card.shadowOpacity, 0.4)
        verify(r.theme.components.widget === undefined)
        var bad = ThemeRegistry.parse(minimalDoc({ density: "huge" }))
        compare(bad.theme.density, "")
    }

    // ----- per-control colours -----

    function test_control_colours_default_to_todays_palette() {
        Theme.setTheme("dark")
        compare(plainBtn.backgroundItem.color, Theme.palette.backgroundSecondary)
        compare(primaryBtn.backgroundItem.border.color, Theme.palette.primary)
        compare(field.backgroundItem.color, Theme.palette.backgroundSecondary)
        compare(tabBtn.activeColor, Theme.palette.primary)
        compare(itemDel.highlightColor, Theme.colors.getColor(Theme.palette.primary, 0.18))
        compare(Theme.components.button.secondary.gradient, "")
        compare(Theme.components.button.primary.gradient, "primary")
    }

    function test_control_colours_from_theme() {
        Theme.setTheme("test.styled")
        compare(plainBtn.backgroundItem.color.toString(), "#204060")
        compare(plainBtn.labelItem.color.toString(), "#ffffff")
        // A palette token's name resolves through the palette.
        compare(Theme.components.button.secondary.backgroundHover.toString(), "#2a2a2a")
        compare(Theme.components.field.borderFocus.toString(), "#3366cc")
        compare(field.backgroundItem.color.toString(), "#101820")
        compare(tabBtn.activeColor.toString(), "#ff8800")
        compare(Theme.components.button.secondary.gradient, "accent")
        // Keys the theme leaves out keep the default.
        compare(Theme.components.button.primary.background, Theme.palette.primary)
        Theme.setTheme("dark")
        compare(plainBtn.backgroundItem.color, Theme.palette.backgroundSecondary)
    }

    function test_control_colours_rejected() {
        var r = ThemeRegistry.parse(minimalDoc({
            components: {
                button: { primary: { background: "red", text: "#000000", border: "notAToken",
                                     gradient: "workspace", shimmer: "#FFFFFF" },
                          secondary: "#FFFFFF" },
                search: { background: "#FFFFFF", text: "#EEEEEE" },
                tab: { background: "#FFFFFF" }
            }
        }))
        verify(r.ok)
        var w = r.warnings.join(" | ")
        compare(r.theme.components.button.primary.background, undefined)
        compare(r.theme.components.button.primary.border, undefined)
        compare(r.theme.components.button.primary.gradient, undefined, "not defined by this theme")
        compare(r.theme.components.button.primary.text.toString(), "#000000")
        verify(w.indexOf("components.button.primary.shimmer") >= 0, w)
        verify(w.indexOf("'components.button.secondary' is not an object") >= 0, w)
        verify(w.indexOf("'components.search.text' is below 4.5:1") >= 0, w)
        verify(w.indexOf("components.tab.background") >= 0, w)
        compare(r.theme.components.tab.background, undefined)
    }
}
