#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

// Discovers installable themes on disk and turns each `theme.json` into a
// validated token map that Theme.qml layers over the built-in dark palette.
//
// A theme is data, never code: the document is parsed here with QJsonDocument,
// every key is checked against a whitelist, every value is type-checked and
// clamped, and nothing in it is ever handed to a QML engine to evaluate.
//
// Search paths, in priority order (first id wins):
//   1. LOGOS_THEME_PATH (QDir::listSeparator()-separated) - dev / tests
//   2. <AppDataLocation>/themes                           - installed themes
// Each path holds one directory per theme: <path>/<dir>/theme.json.
class ThemeRegistry : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QVariantList themes READ themes NOTIFY themesChanged)
    Q_PROPERTY(QStringList searchPaths READ searchPaths NOTIFY themesChanged)
    Q_PROPERTY(QStringList paletteTokens READ paletteTokens CONSTANT)
    Q_PROPERTY(int formatVersion READ formatVersion CONSTANT)

public:
    explicit ThemeRegistry(QObject* parent = nullptr);

    QVariantList themes() const { return m_themes; }
    QStringList searchPaths() const { return m_searchPaths; }
    static QStringList paletteTokens();
    static int formatVersion() { return 1; }

    // Re-scan the search paths.
    Q_INVOKABLE void reload();

    // Parse one theme document. Returns
    //   { ok: bool, errors: [string], warnings: [string], theme: {...} }
    // where theme = { id, name, author, mode, formatVersion,
    //                 palette: {token: color}, spacing: {...}, radius: {...},
    //                 typography: { sizes: {...}, weights: {...} },
    //                 contrast: { textOnBackground: real, ... }, lowContrast: bool }
    // Pure function: no file access, safe to expose to any engine.
    Q_INVOKABLE QVariantMap parse(const QString& json) const;

    static QVariantMap parseDocument(const QByteArray& data);

signals:
    void themesChanged();

private:
    QStringList m_searchPaths;
    QVariantList m_themes;
};
