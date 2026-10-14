pragma Singleton
import QtQuick
import QtCore

QtObject {
    id: theme
    
    // ===== SETTINGS PERSISTENCE =====
    property Settings settings: Settings {
        category: "LogosDesignSystem"
        property string theme: "dark"
    }
    
    // ===== RAW COLOR PALETTE (Design Tokens) =====
    // Single source of truth; passed to all themes
    readonly property ColorPalette colors: ColorPalette {}
    
    // ===== DESIGN TOKENS =====
    readonly property Spacing spacing: Spacing {
        radiusOverrides: theme._activeRadius.overrides
        radiusScale: theme._activeRadius.scale
    }

    // radius block of the active installed theme, split into per-step
    // overrides and the overall scale (defaults for built-in dark).
    readonly property var _activeRadius: {
        var installed = ThemeRegistry.themes
        for (var i = 0; i < installed.length; ++i) {
            if (installed[i].id !== currentTheme)
                continue
            var r = installed[i].radius || {}
            var overrides = {}
            for (var k in r)
                if (k !== "scale")
                    overrides[k] = r[k]
            return { overrides: overrides, scale: r.scale !== undefined ? r.scale : 1.0 }
        }
        return { overrides: {}, scale: 1.0 }
    }
    readonly property Typography typography: Typography {
        familyOverride: theme._activeTypography.family || ""
        monoOverride: theme._activeTypography.monoFamily || ""
    }

    // typography block of the active installed theme ({} for built-in dark).
    readonly property var _activeTypography: {
        var installed = ThemeRegistry.themes
        for (var i = 0; i < installed.length; ++i)
            if (installed[i].id === currentTheme)
                return installed[i].typography || {}
        return {}
    }
    
    // ===== THEME INSTANCES =====
    // Currently available themes - created first
    property var darkTheme: DarkTheme { colors: theme.colors }
    // Add new theme instances here as needed
    
    // ===== INSTALLED THEMES =====
    // Data-only theme packages discovered on disk (see ThemeRegistry.h).
    // Each one is layered over darkTheme token by token, so a theme that
    // leaves a token out gets the default rather than an unset colour.
    function _resolvePalette(entry) {
        var resolved = {}
        var tokens = ThemeRegistry.paletteTokens
        for (var i = 0; i < tokens.length; ++i) {
            var token = tokens[i]
            resolved[token] = entry.palette[token] !== undefined
                ? entry.palette[token] : darkTheme[token]
        }
        return resolved
    }

    // id -> { id, name, author, mode, builtin }
    readonly property var themeInfo: {
        var info = { "dark": { id: "dark", name: "Dark", author: "Logos", mode: "dark", builtin: true } }
        var installed = ThemeRegistry.themes
        for (var i = 0; i < installed.length; ++i) {
            var t = installed[i]
            info[t.id] = { id: t.id, name: t.name, author: t.author, mode: t.mode, builtin: false,
                           family: (t.typography && t.typography.family) || "",
                           monoFamily: (t.typography && t.typography.monoFamily) || "" }
        }
        return info
    }

    // ===== AVAILABLE THEMES =====
    property var availableThemes: {
        var themes = { "dark": darkTheme }
        var installed = ThemeRegistry.themes
        for (var i = 0; i < installed.length; ++i)
            themes[installed[i].id] = _resolvePalette(installed[i])
        return themes
    }

    // The stored choice if it is still installed, else the default. The
    // setting itself is left alone so a temporarily missing theme comes back.
    // ThemeRegistry.current is shared by every engine in the process, so a
    // change made anywhere restyles every view live; `settings` persists it.
    readonly property string currentTheme:
        availableThemes[ThemeRegistry.current] ? ThemeRegistry.current : "dark"

    // ===== CURRENT THEME (Semantic Palette) =====
    readonly property var palette: availableThemes[currentTheme]

    // ===== CONVENIENCE PROPERTIES =====
    readonly property bool isDark: themeInfo[currentTheme].mode === "dark"
    readonly property var themeNames: Object.keys(availableThemes)
    
    // ===== METHODS =====

    // Migration helper for hardcoded colours: returns `literal` under the
    // built-in dark theme (the default look stays pixel-identical) and the
    // palette `token` under an installed theme. QML twin of the C++
    // Logos::themeColor(token, fallback). Prefer plain Theme.palette.<token>
    // in new code.
    function tokenOr(token, literal) {
        return currentTheme === "dark" ? literal : palette[token]
    }

    function switchTheme() {
        // Cycle through available themes
        var themes = Object.keys(availableThemes)
        var currentIndex = themes.indexOf(currentTheme)
        var nextIndex = (currentIndex + 1) % themes.length
        setTheme(themes[nextIndex])
    }
    
    function setTheme(themeName) {
        if (!availableThemes[themeName]) {
            console.warn("Invalid theme:", themeName, ". Available themes:", Object.keys(availableThemes))
            return
        }
        settings.theme = themeName
        ThemeRegistry.current = themeName
        console.log("Theme set to:", themeName)
    }
    
    function setDark() {
        setTheme("dark")
    }
    
    // Helper to check if a theme is available
    function isThemeAvailable(themeName) {
        return availableThemes.hasOwnProperty(themeName)
    }
}
