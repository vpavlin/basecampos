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
    readonly property Spacing spacing: Spacing {}
    readonly property Typography typography: Typography {}
    
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
        var info = { "dark": { id: "dark", name: "Logos Dark", author: "Logos", mode: "dark", builtin: true } }
        var installed = ThemeRegistry.themes
        for (var i = 0; i < installed.length; ++i) {
            var t = installed[i]
            info[t.id] = { id: t.id, name: t.name, author: t.author, mode: t.mode, builtin: false }
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
    readonly property string currentTheme:
        settings && availableThemes[settings.theme] ? settings.theme : "dark"

    // ===== CURRENT THEME (Semantic Palette) =====
    readonly property var palette: availableThemes[currentTheme]

    // ===== CONVENIENCE PROPERTIES =====
    readonly property bool isDark: themeInfo[currentTheme].mode === "dark"
    readonly property var themeNames: Object.keys(availableThemes)
    
    // ===== METHODS =====
    function switchTheme() {
        // Cycle through available themes
        var themes = Object.keys(availableThemes)
        var currentIndex = themes.indexOf(currentTheme)
        var nextIndex = (currentIndex + 1) % themes.length
        settings.theme = themes[nextIndex]
        console.log("Theme switched to:", settings.theme)
    }
    
    function setTheme(themeName) {
        if (!availableThemes[themeName]) {
            console.warn("Invalid theme:", themeName, ". Available themes:", Object.keys(availableThemes))
            return
        }
        settings.theme = themeName
        console.log("Theme set to:", settings.theme)
    }
    
    function setDark() {
        setTheme("dark")
    }
    
    // Helper to check if a theme is available
    function isThemeAvailable(themeName) {
        return availableThemes.hasOwnProperty(themeName)
    }
}
