#include "ThemeRegistry.h"
#include "ThemeColors.h"
#include "ThemeInstall.h"

#include <QColor>
#include <QUrl>
#include <QBuffer>
#include <QImage>
#include <QHash>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <cmath>
#include <algorithm>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
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
// An uploaded bundle may embed its assets (icon SVGs) in the same file; the
// installer splits them out, so theme.json on disk stays under 64 KiB.
constexpr qint64 kMaxBundleBytes = 16 * 1024 * 1024;   // room for background images
constexpr int kMaxBundleAssets = 256;
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
// Optional "motion": { strength 0..1, period 10..120 s }. Engine-defined
// movement only; a theme picks how much and how slow, never how.
QVariant parseMotion(const QJsonObject& owner, const QString& at, QStringList& warnings)
{
    if (!owner.contains(QStringLiteral("motion")))
        return QVariant();
    const QJsonObject m = owner.value(QStringLiteral("motion")).toObject();
    const double strength = m.value(QStringLiteral("strength")).toDouble(0.5);
    const double period = m.value(QStringLiteral("period")).toDouble(30);
    if (strength < 0 || strength > 1 || period < 10 || period > 120)
        warnings << QStringLiteral("'%1.motion' clamped (strength 0..1, period 10..120 s)").arg(at);
    return QVariantMap{ { "strength", qBound(0.0, strength, 1.0) },
                        { "period", qBound(10.0, period, 120.0) } };
}

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

// Per-control colours (components.<control>.<key>). A value is #RRGGBB[AA] or
// the name of a palette token; `gradient` names a gradient slot the theme
// defines. Accepted keys are taken out of `group`, so the size parser after
// this never sees them.
QVariantMap parseStyle(QJsonObject& group, const QStringList& keys, const QString& at,
                       const QVariantMap& gradients, QStringList& warnings)
{
    QVariantMap out;
    for (const QString& key : keys) {
        const QJsonValue v = group.take(key);
        if (v.isUndefined())
            continue;
        const QString path = at + QLatin1Char('.') + key;
        const QString s = v.toString();
        if (key == QLatin1String("gradient")) {
            if (gradients.contains(s))
                out.insert(key, s);
            else
                warnings << QStringLiteral("'%1' names no gradient this theme defines; ignored").arg(path);
            continue;
        }
        QColor c;
        if (parseColor(s, &c))
            out.insert(key, c);
        else if (paletteTokenList().contains(s))
            out.insert(key, s);
        else
            warnings << QStringLiteral("'%1' is not a colour or palette token; ignored").arg(path);
    }
    return out;
}

// A style's text on its own background, when both resolve to a colour here.
void checkStyleContrast(const QVariantMap& style, const QString& at,
                        const QVariantMap& palette, QStringList& warnings)
{
    auto resolve = [&](const QString& key, QColor* out) {
        const QVariant v = style.value(key);
        if (v.typeId() == QMetaType::QColor) { *out = v.value<QColor>(); return true; }
        if (v.typeId() == QMetaType::QString && palette.contains(v.toString())) {
            *out = palette.value(v.toString()).value<QColor>();
            return true;
        }
        return false;
    };
    QColor fg, bg;
    if (resolve(QStringLiteral("text"), &fg) && resolve(QStringLiteral("background"), &bg)
        && contrast(fg, bg) < 4.5)
        warnings << QStringLiteral("'%1.text' is below 4.5:1 on '%1.background'").arg(at);
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
    // Built-in themes first: Light ships inside the library (Dark is the QML
    // default), and a user file with the same id cannot replace it.
    QStringList paths{ QStringLiteral(":/qt/qml/Logos/Theme/builtin") };
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

            // Icon pack: read each file from inside this theme's folder only,
            // sanitise it, and hand it on as a data: URL so nothing downstream
            // (including sandboxed apps) ever reads theme files.
            QVariantMap iconUrls;
            const QString base = QDir(sub.absoluteFilePath()).canonicalPath();
            const QVariantMap set = theme.value("icons").toMap().value("set").toMap();
            for (auto it = set.begin(); it != set.end(); ++it) {
                const QFileInfo fi(sub.absoluteFilePath() + QLatin1Char('/') + it.value().toString());
                const QString canonical = fi.canonicalFilePath();
                if (!fi.isFile() || fi.isSymLink() || canonical.isEmpty()
                    || !canonical.startsWith(base + QLatin1Char('/')) || fi.size() > 32 * 1024) {
                    qWarning("ThemeRegistry: %s: icon '%s' missing, outside the theme or over 32 KiB",
                             qPrintable(id), qPrintable(it.key()));
                    continue;
                }
                QFile icon(canonical);
                if (!icon.open(QIODevice::ReadOnly))
                    continue;
                QString why;
                const QByteArray clean = sanitizeSvg(icon.read(32 * 1024 + 1), true, &why);
                if (clean.isEmpty()) {
                    qWarning("ThemeRegistry: %s: icon '%s' rejected: %s",
                             qPrintable(id), qPrintable(it.key()), qPrintable(why));
                    continue;
                }
                iconUrls.insert(it.key(), QStringLiteral("data:image/svg+xml;base64,")
                                              + QString::fromLatin1(clean.toBase64()));
            }
            theme.insert("iconUrls", iconUrls);

            // Backgrounds: the image must sit inside this theme's folder. Its
            // average colour (after the tint) is checked against the text
            // colour; if text would be hard to read, the tint is raised until
            // it isn't, so a theme can't ship an unreadable workspace.
            QVariantMap backgroundSpecs;
            const QVariantMap bgs = theme.value("backgrounds").toMap();
            const QVariantMap pal = theme.value("palette").toMap();
            const QColor textColor = pal.contains("text") ? pal.value("text").value<QColor>()
                : (theme.value("mode").toString() == QLatin1String("light") ? QColor("#000000") : QColor("#FFFFFF"));
            const QColor fallbackTint = pal.contains("background") ? pal.value("background").value<QColor>()
                : (theme.value("mode").toString() == QLatin1String("light") ? QColor("#FFFFFF") : QColor("#000000"));
            for (auto it = bgs.begin(); it != bgs.end(); ++it) {
                QVariantMap spec = it.value().toMap();
                const QFileInfo fi(sub.absoluteFilePath() + QLatin1Char('/') + spec.value("image").toString());
                const QString canonical = fi.canonicalFilePath();
                if (!fi.isFile() || fi.isSymLink() || canonical.isEmpty()
                    || !canonical.startsWith(base + QLatin1Char('/')) || fi.size() > 4 * 1024 * 1024) {
                    qWarning("ThemeRegistry: %s: background '%s' missing, outside the theme or over 4 MiB",
                             qPrintable(id), qPrintable(it.key()));
                    continue;
                }
                const QImage img(canonical);
                if (img.isNull()) {
                    qWarning("ThemeRegistry: %s: background '%s' is not a readable image",
                             qPrintable(id), qPrintable(it.key()));
                    continue;
                }
                const QImage tiny = img.scaled(16, 16, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                                       .convertToFormat(QImage::Format_RGB32);
                double r = 0, g = 0, bl = 0;
                for (int y = 0; y < 16; ++y)
                    for (int x = 0; x < 16; ++x) {
                        const QColor c = tiny.pixelColor(x, y);
                        r += c.redF(); g += c.greenF(); bl += c.blueF();
                    }
                const double opacity = spec.value("opacity").toDouble();
                const QColor tint = spec.value("tint").isValid() ? spec.value("tint").value<QColor>() : fallbackTint;
                auto mixed = [&](double k) {
                    // image over the background colour (opacity), then the tint on top (k)
                    auto ch = [&](double imgc, double bgc, double tc) {
                        const double under = imgc * opacity + bgc * (1 - opacity);
                        return under * (1 - k) + tc * k;
                    };
                    return QColor::fromRgbF(ch(r / 256, fallbackTint.redF(), tint.redF()),
                                            ch(g / 256, fallbackTint.greenF(), tint.greenF()),
                                            ch(bl / 256, fallbackTint.blueF(), tint.blueF()));
                };
                double k = spec.value("tintStrength").toDouble();
                const double asked = k;
                while (k < 0.95 && contrast(textColor, mixed(k)) < 4.5)
                    k = qMin(0.95, k + 0.05);
                if (k > asked + 1e-9)
                    qWarning("ThemeRegistry: %s: background '%s' tint raised to %.2f for readable text",
                             qPrintable(id), qPrintable(it.key()), k);
                spec.insert("tint", tint);
                spec.insert("tintStrength", k);
                spec.insert("url", QUrl::fromLocalFile(canonical).toString());
                spec.remove("image");
                backgroundSpecs.insert(it.key(), spec);
            }
            theme.insert("backgroundSpecs", backgroundSpecs);
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
        "palette", "spacing", "radius", "typography", "gradients", "icons", "backgrounds", "effects", "components", "density", "assets",
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
    QJsonObject radiusDoc = doc.value("radius").toObject();
    const QJsonValue scaleValue = radiusDoc.take(QStringLiteral("scale"));
    QJsonObject radiusOnly = doc;
    radiusOnly.insert(QStringLiteral("radius"), radiusDoc);
    QVariantMap radius = parseIntGroup(radiusOnly, "radius", radiusRanges(), warnings);
    if (!scaleValue.isUndefined()) {
        if (!scaleValue.isDouble()) {
            warnings << QStringLiteral("'radius.scale' is not a number; ignored");
        } else {
            const double raw = scaleValue.toDouble();
            const double clamped = qBound(0.0, raw, 1.0);
            if (clamped != raw)
                warnings << QStringLiteral("'radius.scale' clamped to %1").arg(clamped);
            radius.insert(QStringLiteral("scale"), clamped);
        }
    }
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
            // A name, or a fallback list of names (first installed one wins).
            const QJsonArray names = fv.isArray() ? fv.toArray() : QJsonArray{ fv };
            static const QRegularExpression ok(QStringLiteral("^[\\p{L}\\p{N} ._'-]{1,64}$"));
            QStringList valid;
            for (const QJsonValue& n : names) {
                const QString fam = n.toString().trimmed();
                if (n.isString() && ok.match(fam).hasMatch() && valid.size() < 8)
                    valid << fam;
                else
                    warnings << QStringLiteral("'typography.%1' entry must be a font family name; ignored").arg(QLatin1String(k));
            }
            if (!valid.isEmpty())
                typography.insert(QLatin1String(k), valid.size() == 1 && !fv.isArray()
                                                       ? QVariant(valid.first()) : QVariant(valid));
        }
        typography.insert("sizes", parseIntGroup(tobj, "sizes", sizeRanges(), warnings));
        typography.insert("weights", parseIntGroup(tobj, "weights", weightRanges(), warnings));
    } else if (!tv.isUndefined()) {
        warnings << QStringLiteral("'typography' is not an object; ignored");
    }

    // Gradients (format v2, also accepted in v1 files): named slots only.
    // Each becomes { type, angle, center: [x, y], radius, stops: [{position, color}] }.
    QVariantMap gradients;
    const QJsonValue gv = doc.value("gradients");
    if (!gv.isUndefined() && !gv.isObject())
        warnings << QStringLiteral("'gradients' is not an object; ignored");
    static const QStringList gradientSlots = {
        "workspace", "sidebar", "primary", "accent", "header", "card",
    };
    const QJsonObject gobj = gv.toObject();
    for (auto it = gobj.begin(); it != gobj.end(); ++it) {
        const QString slot = it.key();
        const QString at = QStringLiteral("gradients.%1").arg(slot);
        if (!gradientSlots.contains(slot)) {
            warnings << QStringLiteral("unknown slot '%1' ignored").arg(at);
            continue;
        }
        const QJsonObject g = it.value().toObject();
        const QString type = g.value("type").toString();
        if (type != QLatin1String("linear") && type != QLatin1String("radial")) {
            warnings << QStringLiteral("'%1.type' must be \"linear\" or \"radial\"; slot ignored").arg(at);
            continue;
        }
        QVariantList stops;
        for (const QJsonValue& sv : g.value("stops").toArray()) {
            const QJsonArray pair = sv.toArray();
            QColor c;
            bool posOk = false;
            const double pos = pair.size() == 2
                ? (pair.at(0).isString() ? pair.at(0).toString().toDouble(&posOk)
                                         : (posOk = pair.at(0).isDouble(), pair.at(0).toDouble()))
                : 0.0;
            if (!posOk || !pair.at(1).isString() || !parseColor(pair.at(1).toString(), &c)) {
                warnings << QStringLiteral("'%1' has an invalid stop; dropped").arg(at);
                continue;
            }
            stops << QVariantMap{ { "position", qBound(0.0, pos, 1.0) }, { "color", c } };
            if (stops.size() == 8)
                break;
        }
        if (stops.size() < 2) {
            warnings << QStringLiteral("'%1' needs at least 2 valid stops; slot ignored").arg(at);
            continue;
        }
        std::stable_sort(stops.begin(), stops.end(), [](const QVariant& a, const QVariant& b) {
            return a.toMap().value("position").toDouble() < b.toMap().value("position").toDouble();
        });
        QVariantMap out{ { "type", type }, { "stops", stops } };
        if (const QVariant motion = parseMotion(g, at, warnings); motion.isValid())
            out.insert("motion", motion);
        if (type == QLatin1String("linear")) {
            const double angle = std::fmod(g.value("angle").toDouble(90.0), 360.0);
            out.insert("angle", angle < 0 ? angle + 360.0 : angle);
        } else {
            const QJsonArray center = g.value("center").toArray();
            out.insert("center", QVariantList{
                center.size() == 2 ? qBound(0.0, center.at(0).toDouble(0.5), 1.0) : 0.5,
                center.size() == 2 ? qBound(0.0, center.at(1).toDouble(0.5), 1.0) : 0.5 });
            const double radius = g.value("radius").toDouble(1.0);
            const double clamped = qBound(0.1, radius, 3.0);
            if (clamped != radius)
                warnings << QStringLiteral("'%1.radius' clamped to %2").arg(at).arg(clamped);
            out.insert("radius", clamped);
        }
        gradients.insert(slot, out);
    }

    // Icon pack: names map to SVG files inside the theme's own folder. Only
    // the references are checked here; the files are read and sanitised by
    // rescan(), which knows the folder.
    QVariantMap icons;
    const QJsonValue iv = doc.value("icons");
    if (!iv.isUndefined() && !iv.isObject())
        warnings << QStringLiteral("'icons' is not an object; ignored");
    if (iv.isObject()) {
        const QJsonObject iobj = iv.toObject();
        const QString style = iobj.value("style").toString(QStringLiteral("tinted"));
        if (style != QLatin1String("tinted"))
            warnings << QStringLiteral("'icons.style' \"%1\" not supported yet; using \"tinted\"").arg(style);
        static const QRegularExpression nameRe(QStringLiteral("^[a-z][A-Za-z0-9]{0,31}$"));
        static const QRegularExpression pathRe(QStringLiteral("^assets/[a-z0-9_./-]{1,120}\\.svg$"));
        QVariantMap set;
        const QJsonObject sobj = iobj.value("set").toObject();
        for (auto it = sobj.begin(); it != sobj.end(); ++it) {
            const QString path = it.value().toString();
            if (!nameRe.match(it.key()).hasMatch()) {
                warnings << QStringLiteral("'icons.set.%1' is not a valid icon name; ignored").arg(it.key());
                continue;
            }
            if (!pathRe.match(path).hasMatch() || path.contains(QLatin1String(".."))) {
                warnings << QStringLiteral("'icons.set.%1' must be an assets/….svg path inside the theme; ignored").arg(it.key());
                continue;
            }
            if (set.size() == 128) {
                warnings << QStringLiteral("'icons.set' has more than 128 icons; the rest are ignored");
                break;
            }
            set.insert(it.key(), path);
        }
        icons = { { "style", QStringLiteral("tinted") }, { "set", set } };
    }

    // Backgrounds: raster images (from the theme folder) in named slots.
    QVariantMap backgrounds;
    const QJsonValue bv = doc.value("backgrounds");
    if (!bv.isUndefined() && !bv.isObject())
        warnings << QStringLiteral("'backgrounds' is not an object; ignored");
    static const QStringList backgroundSlots = { "workspace", "onboarding" };
    static const QStringList fits = { "cover", "contain", "tile", "center" };
    static const QRegularExpression imageRe(QStringLiteral("^assets/[a-z0-9_./-]{1,120}\\.(png|jpg|jpeg)$"));
    const QJsonObject bobj = bv.toObject();
    for (auto it = bobj.begin(); it != bobj.end(); ++it) {
        const QString at = QStringLiteral("backgrounds.%1").arg(it.key());
        if (!backgroundSlots.contains(it.key())) {
            warnings << QStringLiteral("unknown slot '%1' ignored").arg(at);
            continue;
        }
        const QJsonObject b = it.value().toObject();
        const QString image = b.value("image").toString();
        if (!imageRe.match(image).hasMatch() || image.contains(QLatin1String(".."))) {
            warnings << QStringLiteral("'%1.image' must be an assets/….png/.jpg path; slot ignored").arg(at);
            continue;
        }
        QString fit = b.value("fit").toString(QStringLiteral("cover"));
        if (!fits.contains(fit)) {
            warnings << QStringLiteral("'%1.fit' unknown; using cover").arg(at);
            fit = QStringLiteral("cover");
        }
        QColor tint;
        if (b.contains("tint") && !parseColor(b.value("tint").toString(), &tint))
            warnings << QStringLiteral("'%1.tint' is not a colour; ignored").arg(at);
        QVariantMap bgSpec{
            { "image", image }, { "fit", fit },
            { "opacity", qBound(0.0, b.value("opacity").toDouble(1.0), 1.0) },
            { "tint", tint.isValid() ? QVariant(tint) : QVariant() },
            { "tintStrength", qBound(0.0, b.value("tintStrength").toDouble(0.0), 1.0) },
        };
        if (const QVariant motion = parseMotion(b, at, warnings); motion.isValid())
            bgSpec.insert("motion", motion);
        backgrounds.insert(it.key(), bgSpec);
    }

    // Effects: decorative, bounded values only.
    QVariantMap effects;
    const QJsonValue ev = doc.value("effects");
    if (!ev.isUndefined() && !ev.isObject())
        warnings << QStringLiteral("'effects' is not an object; ignored");
    const QJsonObject eobj = ev.toObject();
    for (auto it = eobj.begin(); it != eobj.end(); ++it) {
        if (it.key() != QLatin1String("hoverGlow")) {
            warnings << QStringLiteral("unknown effect 'effects.%1' ignored").arg(it.key());
            continue;
        }
        const QJsonObject g = it.value().toObject();
        QColor c;
        if (!parseColor(g.value("color").toString(), &c)) {
            warnings << QStringLiteral("'effects.hoverGlow.color' is not a colour; ignored");
            continue;
        }
        effects.insert("hoverGlow", QVariantMap{
            { "color", c },
            { "blur", qBound(0.0, g.value("blur").toDouble(16), 48.0) },
            { "spread", qBound(0.0, g.value("spread").toDouble(0), 16.0) },
            { "opacity", qBound(0.0, g.value("opacity").toDouble(0.5), 1.0) },
        });
    }

    // Component tokens: per-control sizes and styles (comfortable density,
    // logical px). Defaults live in Theme.qml and equal today's literals.
    QVariantMap components;
    const QJsonValue cv = doc.value("components");
    if (!cv.isUndefined() && !cv.isObject())
        warnings << QStringLiteral("'components' is not an object; ignored");
    if (cv.isObject()) {
        const QJsonObject cobj = cv.toObject();
        static const QHash<QString, QHash<QString, IntRange>> groups = {
            { "button",     { { "height", { 24, 72 } }, { "compactHeight", { 20, 56 } },
                              { "paddingX", { 0, 48 } }, { "radius", { 0, 999 } },
                              { "borderWidth", { 0, 4 } } } },
            { "field",      { { "height", { 24, 72 } }, { "paddingX", { 0, 48 } },
                              { "radius", { 0, 999 } }, { "borderWidth", { 0, 4 } } } },
            { "search",     { { "height", { 28, 80 } }, { "radius", { 0, 999 } },
                              { "borderWidth", { 0, 4 } } } },
            { "combo",      { { "height", { 24, 72 } }, { "radius", { 0, 999 } },
                              { "borderWidth", { 0, 4 } } } },
            { "listItem",   { { "height", { 24, 72 } }, { "paddingX", { 0, 48 } } } },
            { "menuItem",   { { "height", { 24, 72 } }, { "paddingX", { 0, 48 } } } },
            { "tab",        { { "height", { 24, 72 } }, { "paddingX", { 0, 48 } } } },
            { "toolButton", { { "size", { 20, 72 } } } },
            { "iconButton", { { "size", { 20, 72 } } } },
            { "card",       { { "radius", { 0, 64 } }, { "borderWidth", { 0, 4 } },
                              { "shadowBlur", { 0, 48 } }, { "shadowOffsetY", { -16, 16 } } } },
        };
        for (auto it = cobj.begin(); it != cobj.end(); ++it) {
            if (!groups.contains(it.key())) {
                warnings << QStringLiteral("unknown component 'components.%1' ignored").arg(it.key());
                continue;
            }
            QJsonObject group = it.value().toObject();
            QVariantMap values;
            const QString at = QStringLiteral("components.") + it.key();
            static const QStringList variantKeys = {
                "background", "backgroundHover", "backgroundPressed", "text", "border", "gradient",
            };
            static const QHash<QString, QStringList> colourKeys = {
                { "field",      { "background", "text", "placeholder", "border", "borderFocus", "gradient" } },
                { "search",     { "background", "text", "border", "borderFocus", "gradient" } },
                { "combo",      { "background", "backgroundPressed", "text", "border", "borderFocus" } },
                { "listItem",   { "backgroundHover", "backgroundPressed", "backgroundSelected", "text" } },
                { "menuItem",   { "backgroundHover", "text" } },
                { "tab",        { "text", "textSelected" } },
                { "toolButton", { "backgroundHover", "backgroundPressed", "text" } },
                { "iconButton", { "background", "backgroundHover", "backgroundPressed", "text", "border", "gradient" } },
                { "card",       { "background", "backgroundHover", "border", "gradient" } },
            };
            if (it.key() == QLatin1String("button")) {
                for (const QString variant : { QStringLiteral("primary"), QStringLiteral("secondary") }) {
                    const QJsonValue vv = group.take(variant);
                    if (vv.isUndefined())
                        continue;
                    if (!vv.isObject()) {
                        warnings << QStringLiteral("'%1.%2' is not an object; ignored").arg(at, variant);
                        continue;
                    }
                    QJsonObject vobj = vv.toObject();
                    const QString vat = at + QLatin1Char('.') + variant;
                    const QVariantMap style = parseStyle(vobj, variantKeys, vat, gradients, warnings);
                    for (auto k = vobj.begin(); k != vobj.end(); ++k)
                        warnings << QStringLiteral("unknown token '%1.%2' ignored").arg(vat, k.key());
                    checkStyleContrast(style, vat, palette, warnings);
                    values.insert(variant, style);
                }
            } else {
                const QVariantMap style = parseStyle(group, colourKeys.value(it.key()), at, gradients, warnings);
                checkStyleContrast(style, at, palette, warnings);
                for (auto k = style.begin(); k != style.end(); ++k)
                    values.insert(k.key(), k.value());
            }
            if (it.key() == QLatin1String("card")) {
                // Colour and opacity are not integers; take them out first.
                QColor sc;
                const QJsonValue scv = group.take(QStringLiteral("shadowColor"));
                if (!scv.isUndefined()) {
                    if (parseColor(scv.toString(), &sc))
                        values.insert("shadowColor", sc);
                    else
                        warnings << QStringLiteral("'components.card.shadowColor' is not a colour; ignored");
                }
                const QJsonValue sov = group.take(QStringLiteral("shadowOpacity"));
                if (!sov.isUndefined())
                    values.insert("shadowOpacity", qBound(0.0, sov.toDouble(0.0), 1.0));
            }
            QJsonObject wrapped;
            wrapped.insert(QStringLiteral("components.") + it.key(), group);
            const QVariantMap ints = parseIntGroup(wrapped, QStringLiteral("components.") + it.key(),
                                                   groups.value(it.key()), warnings);
            for (auto v = ints.begin(); v != ints.end(); ++v)
                values.insert(v.key(), v.value());
            components.insert(it.key(), values);
        }
    }
    QString density = doc.value("density").toString();
    if (!density.isEmpty() && density != QLatin1String("compact")
        && density != QLatin1String("comfortable") && density != QLatin1String("touch")) {
        warnings << QStringLiteral("'density' must be compact, comfortable or touch; ignored");
        density.clear();
    }

    // Text that sits on a gradient slot must stay readable on every stop.
    const std::pair<const char*, const char*> onSlot[] = {
        { "workspace", "text" }, { "sidebar", "text" },
        { "primary", "primaryForeground" }, { "accent", "accentForeground" },
    };
    for (const auto& [slot, fg] : onSlot) {
        if (!gradients.contains(slot) || !palette.contains(fg))
            continue;
        for (const QVariant& sv : gradients.value(slot).toMap().value("stops").toList()) {
            if (contrast(palette.value(fg).value<QColor>(), sv.toMap().value("color").value<QColor>()) < 3.0) {
                warnings << QStringLiteral("'%1' is below 3:1 on a stop of 'gradients.%2'")
                                .arg(QLatin1String(fg), QLatin1String(slot));
                break;
            }
        }
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
        { "radius", radius }, { "typography", typography }, { "gradients", gradients },
        { "icons", icons }, { "backgrounds", backgrounds },
        { "effects", effects }, { "components", components }, { "density", density },
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

namespace {
// The active theme's resolved document, rescanned only when the id changes.
const QVariantMap& activeThemeDocument()
{
    static QString cachedId;
    static QVariantMap cached;
    const QString id = Logos::ThemeNotifier::instance()->current();
    if (id != cachedId) {
        cachedId = id;
        cached.clear();
        if (id != QLatin1String("dark")) {
            ThemeRegistry registry;
            for (const QVariant& t : registry.themes()) {
                const QVariantMap theme = t.toMap();
                if (theme.value("id").toString() == id) {
                    cached = theme;
                    break;
                }
            }
        }
    }
    return cached;
}
} // namespace

QColor Logos::themeColor(const QString& token, const QColor& fallback)
{
    const QVariant v = activeThemeDocument().value("palette").toMap().value(token);
    return v.isValid() ? v.value<QColor>() : fallback;
}

double Logos::themeRadiusScale()
{
    const QVariant v = activeThemeDocument().value("radius").toMap().value("scale");
    return v.isValid() ? v.toDouble() : 1.0;
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
    if (src.size() > kMaxBundleBytes)
        return fail(QStringLiteral("larger than 16 MiB"));
    QFile in(src.absoluteFilePath());
    if (!in.open(QIODevice::ReadOnly))
        return fail(QStringLiteral("can't read %1").arg(path));
    QByteArray data = in.read(kMaxBundleBytes + 1);

    // A bundle carries its assets inline: { "assets": { "assets/icons/x.svg": "<svg…>" } }.
    // Split them out; what is left must be a normal theme document.
    QJsonObject assets;
    {
        QJsonParseError perr;
        QJsonDocument whole = QJsonDocument::fromJson(data, &perr);
        if (whole.isObject() && whole.object().contains(QStringLiteral("assets"))) {
            QJsonObject obj = whole.object();
            assets = obj.take(QStringLiteral("assets")).toObject();
            data = QJsonDocument(obj).toJson(QJsonDocument::Indented);
        }
    }
    if (data.size() > kMaxDocumentBytes)
        return fail(QStringLiteral("theme document (without assets) larger than 64 KiB"));

    const QVariantMap parsed = ThemeRegistry::parseDocument(data);
    QStringList warnings = parsed.value("warnings").toStringList();
    if (!parsed.value("ok").toBool()) {
        out.insert("warnings", warnings);
        out.insert("errors", parsed.value("errors"));
        return out;
    }
    const QVariantMap theme = parsed.value("theme").toMap();
    const QString id = theme.value("id").toString();   // validated: [a-z0-9_.-] only

    // Validate and sanitise every asset before anything is written.
    static const QRegularExpression pathRe(QStringLiteral("^assets/[a-z0-9_./-]{1,120}\\.(svg|png|jpg|jpeg)$"));
    QHash<QString, QByteArray> files;
    if (assets.size() > kMaxBundleAssets)
        return fail(QStringLiteral("more than %1 assets").arg(kMaxBundleAssets));
    for (auto it = assets.begin(); it != assets.end(); ++it) {
        if (!pathRe.match(it.key()).hasMatch() || it.key().contains(QLatin1String(".."))
            || !it.value().isString()) {
            warnings << QStringLiteral("asset '%1' ignored (must be an assets/….svg/.png/.jpg path)").arg(it.key());
            continue;
        }
        if (!it.key().endsWith(QLatin1String(".svg"))) {
            // Raster: "data:image/png|jpeg;base64,…", checked by content, size
            // capped, and re-encoded so metadata and trailing bytes are dropped.
            const QString v = it.value().toString();
            const int comma = v.indexOf(QLatin1Char(','));
            const QString head = v.left(comma);
            if (comma < 0 || !(head == QLatin1String("data:image/png;base64")
                               || head == QLatin1String("data:image/jpeg;base64"))) {
                warnings << QStringLiteral("asset '%1' ignored (expected a base64 PNG or JPEG data URL)").arg(it.key());
                continue;
            }
            const QByteArray raw = QByteArray::fromBase64(v.mid(comma + 1).toLatin1());
            QImage img;
            if (raw.size() > 4 * 1024 * 1024 || !img.loadFromData(raw) || img.isNull()
                || img.width() > 3840 || img.height() > 3840) {
                warnings << QStringLiteral("asset '%1' rejected (not a PNG/JPEG up to 4 MiB and 3840 px)").arg(it.key());
                continue;
            }
            QByteArray encoded;
            QBuffer buf(&encoded);
            buf.open(QIODevice::WriteOnly);
            const bool png = it.key().endsWith(QLatin1String(".png"));
            img.save(&buf, png ? "PNG" : "JPEG", png ? -1 : 90);
            files.insert(it.key(), encoded);
            continue;
        }
        QString why;
        const QByteArray clean = ThemeRegistry::sanitizeSvg(it.value().toString().toUtf8(), true, &why);
        if (clean.isEmpty()) {
            warnings << QStringLiteral("asset '%1' rejected: %2").arg(it.key(), why);
            continue;
        }
        files.insert(it.key(), clean);
    }
    out.insert("warnings", warnings);

    const QString root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (root.isEmpty())
        return fail(QStringLiteral("no writable app data location"));
    const QString dir = QDir::cleanPath(root + QStringLiteral("/themes/") + id);
    if (!QDir().mkpath(dir))
        return fail(QStringLiteral("can't create %1").arg(dir));
    // Replacing a theme replaces its assets too.
    QDir(dir + QStringLiteral("/assets")).removeRecursively();
    for (auto it = files.begin(); it != files.end(); ++it) {
        const QString target = QDir::cleanPath(dir + QLatin1Char('/') + it.key());
        if (!target.startsWith(dir + QLatin1Char('/')))
            continue;   // belt and braces; the path pattern already forbids this
        QDir().mkpath(QFileInfo(target).absolutePath());
        QFile f(target);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate) || f.write(it.value()) != it.value().size())
            return fail(QStringLiteral("can't write %1").arg(target));
    }
    QFile outFile(dir + QStringLiteral("/theme.json"));
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || outFile.write(data) != data.size())
        return fail(QStringLiteral("can't write %1").arg(outFile.fileName()));

    out.insert("assets", files.size());
    out.insert("ok", true);
    out.insert("id", id);
    out.insert("name", theme.value("name"));
    out.insert("errors", QStringList{});
    return out;
}

QByteArray ThemeRegistry::sanitizeSvg(const QByteArray& svg, bool tinted, QString* error)
{
    auto fail = [error](const QString& why) {
        if (error)
            *error = why;
        return QByteArray();
    };
    if (svg.size() > 32 * 1024)
        return fail(QStringLiteral("larger than 32 KiB"));

    static const QSet<QString> elements = {
        "svg", "g", "path", "rect", "circle", "ellipse", "line", "polyline", "polygon",
        "defs", "linearGradient", "radialGradient", "stop", "use", "title",
    };
    static const QSet<QString> attributes = {
        "id", "viewBox", "width", "height", "d", "x", "y", "x1", "y1", "x2", "y2",
        "cx", "cy", "r", "rx", "ry", "points", "transform", "fill", "stroke",
        "stroke-width", "stroke-linecap", "stroke-linejoin", "stroke-miterlimit",
        "fill-rule", "clip-rule", "opacity", "fill-opacity", "stroke-opacity",
        "offset", "stop-color", "stop-opacity", "gradientUnits", "gradientTransform", "href",
    };
    static const QSet<QString> paints = { "fill", "stroke", "stop-color" };

    QXmlStreamReader in(svg);
    QByteArray out;
    QXmlStreamWriter w(&out);
    int depth = 0, skipDepth = -1, count = 0;
    qsizetype pathChars = 0;
    bool sawRoot = false;
    while (!in.atEnd()) {
        in.readNext();
        if (in.tokenType() == QXmlStreamReader::DTD)
            return fail(QStringLiteral("DOCTYPE / entities are not allowed"));
        if (in.isStartElement()) {
            ++depth;
            const QString name = in.name().toString();
            if (skipDepth >= 0)
                continue;
            if (!sawRoot) {
                if (name != QLatin1String("svg"))
                    return fail(QStringLiteral("root element is not <svg>"));
                const QStringList vb = in.attributes().value("viewBox").toString()
                                           .split(QRegularExpression(QStringLiteral("[\\s,]+")),
                                                  Qt::SkipEmptyParts);
                if (vb.size() != 4 || qAbs(vb.at(2).toDouble() - vb.at(3).toDouble()) > 1e-6
                    || vb.at(2).toDouble() <= 0)
                    return fail(QStringLiteral("needs a square viewBox"));
            }
            if (!elements.contains(name) || in.namespaceUri() != QLatin1String("http://www.w3.org/2000/svg")) {
                skipDepth = depth;   // drop the element and everything inside it
                continue;
            }
            if (++count > 500)
                return fail(QStringLiteral("more than 500 elements"));
            if (!sawRoot)
                w.writeStartElement(QStringLiteral("svg"));
            else
                w.writeStartElement(name);
            if (!sawRoot) {
                w.writeDefaultNamespace(QStringLiteral("http://www.w3.org/2000/svg"));
                if (tinted && !in.attributes().hasAttribute("fill"))
                    w.writeAttribute(QStringLiteral("fill"), QStringLiteral("#FFFFFF"));
            }
            sawRoot = true;
            for (const QXmlStreamAttribute& a : in.attributes()) {
                const QString an = a.name().toString();
                QString value = a.value().toString();
                if (!attributes.contains(an))
                    continue;   // incl. on* handlers, style, class, xlink:* except href
                if (an == QLatin1String("href") && !value.startsWith(QLatin1Char('#')))
                    continue;   // local references only
                if (value.contains(QLatin1String("url(")) && !value.contains(QLatin1String("url(#")))
                    continue;
                if (tinted && paints.contains(an) && value != QLatin1String("none")
                    && !value.startsWith(QLatin1String("url(#")))
                    value = QStringLiteral("#FFFFFF");
                if (an == QLatin1String("d") && (pathChars += value.size()) > 20000)
                    return fail(QStringLiteral("path data too long"));
                w.writeAttribute(an, value);
            }
        } else if (in.isEndElement()) {
            if (skipDepth < 0)
                w.writeEndElement();
            else if (depth == skipDepth)
                skipDepth = -1;
            --depth;
        }
        // Text, comments and processing instructions are dropped.
    }
    if (in.hasError())
        return fail(QStringLiteral("not well-formed XML: %1").arg(in.errorString()));
    if (!sawRoot)
        return fail(QStringLiteral("no <svg> element"));
    return out;
}

QString ThemeRegistry::sanitizedSvg(const QString& svg, bool tinted) const
{
    return QString::fromUtf8(sanitizeSvg(svg.toUtf8(), tinted));
}
