#include "ThemeRegistry.h"
#include "ThemeColors.h"
#include "ThemeInstall.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <QtMath>

namespace {

constexpr qint64 kMaxDocumentBytes = 64 * 1024;
constexpr int kMaxLabelLength = 40;
constexpr int kMaxIdLength = 64;

// Ids of themes compiled into Theme.qml. A package may not shadow them.
const QStringList kBuiltinIds = { QStringLiteral("dark") };

struct IntRange { int min; int max; };

// Semantic tokens a theme may set. Mirrors DarkTheme.qml; tst_ThemeRegistry
// fails if the two drift apart.
const QStringList& paletteTokenList()
{
    static const QStringList v = {
        "background", "backgroundSecondary", "backgroundTertiary", "backgroundElevated",
        "backgroundMuted", "backgroundBlack", "backgroundInset", "backgroundButton",
        "surface", "surfaceRaised", "surfaceInteractiveHover", "surfaceRecessed",
        "surfaceContrast",
        "text", "textSecondary", "textSubtle", "textTertiary", "textPlaceholder", "textMuted",
        "border", "borderSecondary", "borderTertiary", "borderTertiaryMuted", "borderSubtle",
        "borderHairline", "borderGlyph", "borderInteractive", "borderDark", "borderStrong",
        "primary", "primaryHover", "primaryPressed", "primarySoft", "primaryForeground",
        "success", "successHover", "successPressed", "successBorder",
        "error", "errorHover", "errorPressed", "errorBorder",
        "warning", "warningHover", "warningBorder",
        "info", "primaryBorder",
        "notification", "brandMark", "accentForeground", "accentOrange", "accentOrangeMid", "accentOrangeDeep",
        "accentBurntOrange", "accentYellowSoft",
        "hover", "pressed", "disabled", "focus",
        "glassOverlay", "glassStrong", "overlayDark", "overlayLight", "overlayOrange", "scrim",
    };
    return v;
}

// Tokens that DarkTheme.qml derives from another token. If a theme sets the
// source but not the derived token, re-derive it so the theme's colour isn't
// paired with the default's alpha variant.
struct Derived { const char* token; const char* source; double alpha; };
const Derived kDerived[] = {
    { "primaryBorder", "primary", 0.4 },
    { "borderTertiaryMuted", "borderTertiary", 0.2 },
};

const QHash<QString, IntRange>& spacingRanges()
{
    static const QHash<QString, IntRange> v = {
        { "tiny", {0, 64} }, { "small", {0, 64} }, { "medium", {0, 64} },
        { "large", {0, 64} }, { "xlarge", {0, 64} }, { "xxlarge", {0, 128} },
    };
    return v;
}

const QHash<QString, IntRange>& radiusRanges()
{
    static const QHash<QString, IntRange> v = {
        { "tiny", {0, 999} }, { "small", {0, 999} }, { "medium", {0, 999} },
        { "large", {0, 999} }, { "xlarge", {0, 999} }, { "pill", {0, 999} },
    };
    return v;
}

const QHash<QString, IntRange>& sizeRanges()
{
    static const QHash<QString, IntRange> v = {
        { "mainTitleText", {6, 256} }, { "pageTitleText", {6, 256} },
        { "titleText", {6, 256} }, { "panelTitleText", {6, 256} },
        { "subtitleText", {6, 256} }, { "primaryText", {6, 256} },
        { "secondaryText", {6, 256} }, { "badgeText", {6, 256} },
    };
    return v;
}

const QHash<QString, IntRange>& weightRanges()
{
    static const QHash<QString, IntRange> v = {
        { "regular", {100, 900} }, { "medium", {100, 900} }, { "bold", {100, 900} },
    };
    return v;
}

// "#RRGGBB" or "#RRGGBBAA" (CSS order). Qt's own string form is #AARRGGBB,
// so the alpha byte is moved here rather than handed to QColor as-is.
bool parseColor(const QString& s, QColor* out)
{
    static const QRegularExpression re(QStringLiteral("^#([0-9A-Fa-f]{6})([0-9A-Fa-f]{2})?$"));
    const auto m = re.match(s);
    if (!m.hasMatch())
        return false;
    QColor c(QLatin1Char('#') + m.captured(1));
    if (!c.isValid())
        return false;
    if (m.hasCaptured(2))
        c.setAlpha(m.captured(2).toInt(nullptr, 16));
    *out = c;
    return true;
}

double luminance(const QColor& c)
{
    auto lin = [](double v) {
        return v <= 0.03928 ? v / 12.92 : qPow((v + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * lin(c.redF()) + 0.7152 * lin(c.greenF()) + 0.0722 * lin(c.blueF());
}

// WCAG contrast ratio; a translucent foreground is composited over the background first.
double contrast(QColor fg, const QColor& bg)
{
    if (fg.alphaF() < 1.0) {
        const double a = fg.alphaF();
        fg = QColor::fromRgbF(fg.redF() * a + bg.redF() * (1 - a),
                              fg.greenF() * a + bg.greenF() * (1 - a),
                              fg.blueF() * a + bg.blueF() * (1 - a));
    }
    const double l1 = luminance(fg), l2 = luminance(bg);
    return (qMax(l1, l2) + 0.05) / (qMin(l1, l2) + 0.05);
}

QString requireLabel(const QJsonObject& doc, const char* key, QStringList& errors)
{
    const QJsonValue v = doc.value(QLatin1String(key));
    const QString s = v.toString().trimmed();
    if (!v.isString() || s.isEmpty())
        errors << QStringLiteral("'%1' must be a non-empty string").arg(QLatin1String(key));
    else if (s.size() > kMaxLabelLength)
        errors << QStringLiteral("'%1' is longer than %2 characters").arg(QLatin1String(key)).arg(kMaxLabelLength);
    return s;
}

QVariantMap parseIntGroup(const QJsonObject& doc, const QString& group,
                          const QHash<QString, IntRange>& ranges, QStringList& warnings)
{
    QVariantMap out;
    const QJsonValue v = doc.value(group);
    if (v.isUndefined())
        return out;
    if (!v.isObject()) {
        warnings << QStringLiteral("'%1' is not an object; ignored").arg(group);
        return out;
    }
    const QJsonObject obj = v.toObject();
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        const QString path = group + QLatin1Char('.') + it.key();
        if (!ranges.contains(it.key())) {
            warnings << QStringLiteral("unknown token '%1' ignored").arg(path);
            continue;
        }
        if (!it.value().isDouble()) {
            warnings << QStringLiteral("'%1' is not a number; ignored").arg(path);
            continue;
        }
        const IntRange r = ranges.value(it.key());
        const int raw = qRound(it.value().toDouble());
        const int clamped = qBound(r.min, raw, r.max);
        if (clamped != raw)
            warnings << QStringLiteral("'%1' clamped to %2").arg(path).arg(clamped);
        out.insert(it.key(), clamped);
    }
    return out;
}

} // namespace

ThemeRegistry::ThemeRegistry(QObject* parent)
    : QObject(parent)
{
    rescan();
    auto* bus = Logos::ThemeNotifier::instance();
    connect(bus, &Logos::ThemeNotifier::currentChanged, this, &ThemeRegistry::currentChanged);
    connect(bus, &Logos::ThemeNotifier::themesChanged, this, &ThemeRegistry::rescan);
}

QString ThemeRegistry::current() const
{
    return Logos::ThemeNotifier::instance()->current();
}

void ThemeRegistry::setCurrent(const QString& id)
{
    Logos::ThemeNotifier::instance()->setCurrent(id);
}

QStringList ThemeRegistry::paletteTokens()
{
    return paletteTokenList();
}

void ThemeRegistry::reload()
{
    // Rescans every registry in the process, this one included.
    Logos::ThemeNotifier::instance()->announceThemesChanged();
}

void ThemeRegistry::rescan()
{
    QStringList paths;
    const QString env = qEnvironmentVariable("LOGOS_THEME_PATH");
    for (const QString& p : env.split(QDir::listSeparator(), Qt::SkipEmptyParts))
        paths << QDir::cleanPath(p);
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!appData.isEmpty())
        paths << QDir::cleanPath(appData + QStringLiteral("/themes"));

    QVariantList found;
    QSet<QString> seen;
    for (const QString& root : paths) {
        const QDir dir(root);
        if (!dir.exists())
            continue;
        const auto entries = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QFileInfo& sub : entries) {
            const QFileInfo doc(sub.absoluteFilePath() + QStringLiteral("/theme.json"));
            // No symlinks: a theme may only read the file inside its own package.
            if (sub.isSymLink() || !doc.exists() || doc.isSymLink() || !doc.isFile())
                continue;
            QFile f(doc.absoluteFilePath());
            if (doc.size() > kMaxDocumentBytes || !f.open(QIODevice::ReadOnly)) {
                qWarning("ThemeRegistry: skipping %s (unreadable or larger than 64 KiB)",
                         qPrintable(doc.absoluteFilePath()));
                continue;
            }
            const QVariantMap result = parseDocument(f.read(kMaxDocumentBytes + 1));
            if (!result.value("ok").toBool()) {
                qWarning("ThemeRegistry: rejected %s: %s", qPrintable(doc.absoluteFilePath()),
                         qPrintable(result.value("errors").toStringList().join(QStringLiteral("; "))));
                continue;
            }
            for (const QString& w : result.value("warnings").toStringList())
                qWarning("ThemeRegistry: %s: %s", qPrintable(doc.absoluteFilePath()), qPrintable(w));
            QVariantMap theme = result.value("theme").toMap();
            const QString id = theme.value("id").toString();
            if (seen.contains(id)) {
                qWarning("ThemeRegistry: duplicate theme id '%s' at %s ignored",
                         qPrintable(id), qPrintable(doc.absoluteFilePath()));
                continue;
            }
            seen.insert(id);
            theme.insert("source", doc.absoluteFilePath());
            found << theme;
        }
    }

    m_searchPaths = paths;
    m_themes = found;
    emit themesChanged();
}

QVariantMap ThemeRegistry::parse(const QString& json) const
{
    return parseDocument(json.toUtf8());
}

QVariantMap ThemeRegistry::parseDocument(const QByteArray& data)
{
    QStringList errors, warnings;
    QVariantMap theme;
    auto result = [&](bool ok) {
        return QVariantMap{
            { "ok", ok }, { "errors", errors }, { "warnings", warnings },
            { "theme", ok ? theme : QVariantMap{} },
        };
    };

    if (data.size() > kMaxDocumentBytes) {
        errors << QStringLiteral("document is larger than 64 KiB");
        return result(false);
    }
    QJsonParseError perr;
    const QJsonDocument jd = QJsonDocument::fromJson(data, &perr);
    if (perr.error != QJsonParseError::NoError) {
        errors << QStringLiteral("invalid JSON: %1").arg(perr.errorString());
        return result(false);
    }
    if (!jd.isObject()) {
        errors << QStringLiteral("document must be a JSON object");
        return result(false);
    }
    const QJsonObject doc = jd.object();

    // formatVersion: newer documents load with the keys we know.
    const QJsonValue fv = doc.value("formatVersion");
    const int version = fv.isDouble() ? qRound(fv.toDouble()) : 0;
    if (version < 1)
        errors << QStringLiteral("'formatVersion' must be an integer >= 1");
    else if (version > formatVersion())
        warnings << QStringLiteral("made for a newer format (v%1); unknown keys ignored").arg(version);

    static const QRegularExpression idRe(QStringLiteral("^[a-z0-9_-]+(\\.[a-z0-9_-]+)+$"));
    const QString id = doc.value("id").toString();
    if (!idRe.match(id).hasMatch() || id.size() > kMaxIdLength)
        errors << QStringLiteral("'id' must look like <author>.<name> using [a-z0-9_-], max %1 chars").arg(kMaxIdLength);
    else if (kBuiltinIds.contains(id))
        errors << QStringLiteral("'id' %1 is reserved").arg(id);

    const QString name = requireLabel(doc, "name", errors);
    const QString author = requireLabel(doc, "author", errors);

    const QString mode = doc.value("mode").toString();
    if (mode != QLatin1String("dark") && mode != QLatin1String("light"))
        errors << QStringLiteral("'mode' must be \"dark\" or \"light\"");

    static const QSet<QString> topLevel = {
        "$schema", "formatVersion", "id", "name", "author", "mode",
        "palette", "spacing", "radius", "typography",
    };
    for (const QString& key : doc.keys())
        if (!topLevel.contains(key))
            warnings << QStringLiteral("unknown key '%1' ignored").arg(key);

    // Palette.
    QVariantMap palette;
    const QJsonValue pv = doc.value("palette");
    if (!pv.isUndefined() && !pv.isObject())
        warnings << QStringLiteral("'palette' is not an object; ignored");
    const QJsonObject pobj = pv.toObject();
    const QStringList& tokens = paletteTokenList();
    for (auto it = pobj.begin(); it != pobj.end(); ++it) {
        if (!tokens.contains(it.key())) {
            warnings << QStringLiteral("unknown token 'palette.%1' ignored").arg(it.key());
            continue;
        }
        QColor c;
        if (!it.value().isString() || !parseColor(it.value().toString(), &c)) {
            warnings << QStringLiteral("'palette.%1' is not #RRGGBB or #RRGGBBAA; ignored").arg(it.key());
            continue;
        }
        palette.insert(it.key(), c);
    }
    for (const Derived& d : kDerived) {
        const QString token = QLatin1String(d.token), source = QLatin1String(d.source);
        if (!palette.contains(token) && palette.contains(source)) {
            QColor c = palette.value(source).value<QColor>();
            c.setAlphaF(d.alpha);
            palette.insert(token, c);
        }
    }

    const QVariantMap spacing = parseIntGroup(doc, "spacing", spacingRanges(), warnings);
    const QVariantMap radius = parseIntGroup(doc, "radius", radiusRanges(), warnings);
    QVariantMap typography;
    const QJsonValue tv = doc.value("typography");
    if (tv.isObject()) {
        const QJsonObject tobj = tv.toObject();
        static const QSet<QString> known = { "sizes", "weights", "family", "monoFamily" };
        for (const QString& key : tobj.keys())
            if (!known.contains(key))
                warnings << QStringLiteral("unknown key 'typography.%1' ignored").arg(key);
        // Font families are *names* of fonts installed on the system; a theme
        // never ships font files (the shell would parse untrusted binaries).
        for (const char* k : { "family", "monoFamily" }) {
            const QJsonValue fv = tobj.value(QLatin1String(k));
            if (fv.isUndefined())
                continue;
            const QString fam = fv.toString().trimmed();
            static const QRegularExpression ok(QStringLiteral("^[\\p{L}\\p{N} ._'-]{1,64}$"));
            if (!fv.isString() || !ok.match(fam).hasMatch())
                warnings << QStringLiteral("'typography.%1' must be a font family name; ignored").arg(QLatin1String(k));
            else
                typography.insert(QLatin1String(k), fam);
        }
        typography.insert("sizes", parseIntGroup(tobj, "sizes", sizeRanges(), warnings));
        typography.insert("weights", parseIntGroup(tobj, "weights", weightRanges(), warnings));
    } else if (!tv.isUndefined()) {
        warnings << QStringLiteral("'typography' is not an object; ignored");
    }

    if (!errors.isEmpty())
        return result(false);

    // Readability floor, measured on the pairs the shell leans on most. Tokens
    // a theme leaves out are unknown here, so only fully specified pairs count.
    QVariantMap ratios;
    bool low = false;
    const std::pair<const char*, const char*> pairs[] = {
        { "text", "background" }, { "text", "surface" },
        { "textSecondary", "background" }, { "primary", "background" },
    };
    for (const auto& [fg, bg] : pairs) {
        if (!palette.contains(fg) || !palette.contains(bg))
            continue;
        const double r = contrast(palette.value(fg).value<QColor>(), palette.value(bg).value<QColor>());
        ratios.insert(QStringLiteral("%1On%2").arg(QLatin1String(fg),
                      QString(QLatin1String(bg)).replace(0, 1, QChar(bg[0]).toUpper())),
                      qRound(r * 100) / 100.0);
        if (r < 3.0)
            low = true;
    }
    if (low)
        warnings << QStringLiteral("a core text/background pair is below 3:1 contrast");

    theme = {
        { "id", id }, { "name", name }, { "author", author }, { "mode", mode },
        { "formatVersion", version }, { "palette", palette }, { "spacing", spacing },
        { "radius", radius }, { "typography", typography },
        { "contrast", ratios }, { "lowContrast", low },
    };
    return result(true);
}

Logos::ThemeNotifier* Logos::ThemeNotifier::instance()
{
    static ThemeNotifier* n = new ThemeNotifier;   // lives for the process
    return n;
}

Logos::ThemeNotifier::ThemeNotifier()
    : m_current(QSettings().value(QStringLiteral("LogosDesignSystem/theme"),
                                  QStringLiteral("dark")).toString())
{
}

void Logos::ThemeNotifier::setCurrent(const QString& id)
{
    if (id.isEmpty() || id == m_current)
        return;
    m_current = id;
    emit currentChanged();
}

QColor Logos::themeColor(const QString& token, const QColor& fallback)
{
    // Palette of the active theme, rescanned only when the id changes.
    static QString cachedId;
    static QVariantMap cachedPalette;
    const QString id = ThemeNotifier::instance()->current();
    if (id != cachedId) {
        cachedId = id;
        cachedPalette.clear();
        if (id != QLatin1String("dark")) {
            ThemeRegistry registry;
            for (const QVariant& t : registry.themes()) {
                const QVariantMap theme = t.toMap();
                if (theme.value("id").toString() == id) {
                    cachedPalette = theme.value("palette").toMap();
                    break;
                }
            }
        }
    }
    const QVariant v = cachedPalette.value(token);
    return v.isValid() ? v.value<QColor>() : fallback;
}

QVariantMap Logos::installThemeFile(const QString& path)
{
    QVariantMap out{ { "ok", false } };
    auto fail = [&](const QString& why) {
        out.insert("errors", QStringList{ why });
        return out;
    };

    const QFileInfo src(path);
    if (!src.isFile())
        return fail(QStringLiteral("not a file: %1").arg(path));
    if (src.size() > kMaxDocumentBytes)
        return fail(QStringLiteral("larger than 64 KiB"));
    QFile in(src.absoluteFilePath());
    if (!in.open(QIODevice::ReadOnly))
        return fail(QStringLiteral("can't read %1").arg(path));
    const QByteArray data = in.read(kMaxDocumentBytes + 1);

    const QVariantMap parsed = ThemeRegistry::parseDocument(data);
    out.insert("warnings", parsed.value("warnings"));
    if (!parsed.value("ok").toBool()) {
        out.insert("errors", parsed.value("errors"));
        return out;
    }
    const QVariantMap theme = parsed.value("theme").toMap();
    const QString id = theme.value("id").toString();   // validated: [a-z0-9_.-] only

    const QString root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (root.isEmpty())
        return fail(QStringLiteral("no writable app data location"));
    const QString dir = QDir::cleanPath(root + QStringLiteral("/themes/") + id);
    if (!QDir().mkpath(dir))
        return fail(QStringLiteral("can't create %1").arg(dir));
    QFile outFile(dir + QStringLiteral("/theme.json"));
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || outFile.write(data) != data.size())
        return fail(QStringLiteral("can't write %1").arg(outFile.fileName()));

    out.insert("ok", true);
    out.insert("id", id);
    out.insert("name", theme.value("name"));
    out.insert("errors", QStringList{});
    return out;
}

