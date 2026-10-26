#include "ObjectRanker.h"

#include <QRegularExpression>

#include <algorithm>
#include <cmath>

namespace {

// Whitespace and the ASCII punctuation ranges !-/ :-@ [-` {-~. The same class
// is written out in BasecampSearchRanker.js.
const QRegularExpression& separators()
{
    static const QRegularExpression re(QStringLiteral("[\\s!-/:-@\\[-`{-~]+"),
                                       QRegularExpression::UseUnicodePropertiesOption);
    return re;
}

bool hasWordStartingWith(const QString& text, const QString& token)
{
    if (text.isEmpty()) return false;
    // A word of it starts with the token only if the text holds the token:
    // that check is cheap, the split is not, and most objects miss.
    if (!text.toLower().contains(token)) return false;
    const QStringList words = ObjectRanker::tokens(text);
    return std::any_of(words.cbegin(), words.cend(),
                       [&token](const QString& w) { return w.startsWith(token); });
}

// The strings in a `fields` value, one level of nesting deep (a balance is
// { amount, unit }). Numbers and booleans are not text to match.
void collectStrings(const QVariant& v, QStringList& out, int depth = 0)
{
    const int t = v.metaType().id();
    if (t == QMetaType::QString) {
        out << v.toString();
    } else if (depth < 2 && t == QMetaType::QVariantMap) {
        const QVariantMap m = v.toMap();
        for (auto it = m.cbegin(); it != m.cend(); ++it)
            collectStrings(it.value(), out, depth + 1);
    } else if (depth < 2 && (t == QMetaType::QVariantList || t == QMetaType::QStringList)) {
        for (const QVariant& item : v.toList())
            collectStrings(item, out, depth + 1);
    }
}

qint64 timeOf(const QVariantMap& o)
{
    const QVariant t = o.value(QStringLiteral("time"));
    return t.isValid() ? t.toLongLong() : o.value(QStringLiteral("updated")).toLongLong();
}

} // namespace

namespace ObjectRanker {

QStringList tokens(const QString& text)
{
    return text.toLower().split(separators(), Qt::SkipEmptyParts);
}

int strength(const QVariantMap& o, const QString& token)
{
    if (hasWordStartingWith(o.value(QStringLiteral("title")).toString(), token))
        return kTitleStrength;
    for (const QVariant& k : o.value(QStringLiteral("keywords")).toList())
        if (hasWordStartingWith(k.toString(), token))
            return kKeywordStrength;
    if (hasWordStartingWith(o.value(QStringLiteral("subtitle")).toString(), token))
        return kSubtitleStrength;
    if (kMatchContentFields) {
        QStringList strings;
        collectStrings(o.value(QStringLiteral("fields")), strings);
        for (const QString& s : std::as_const(strings))
            if (hasWordStartingWith(s, token))
                return kFieldStrength;
    }
    return 0;
}

double score(const QVariantMap& o, const QStringList& queryTokens, qint64 nowMs)
{
    int match = 0;
    for (const QString& token : queryTokens) {
        const int s = strength(o, token);
        if (s == 0) return -1;
        match += s;
    }
    const double age = double(std::max<qint64>(0, nowMs - timeOf(o)));
    const double recency = kRecencyWeight * std::pow(2.0, -age / kRecencyHalfLifeMs);
    const double unseen = o.value(QStringLiteral("seen")).toBool() ? 0.0 : kUnseenBoost;
    return kMatchWeight * match + recency + unseen;
}

namespace {

struct Hit { double score; qint64 time; bool unseen; QString id; };

QStringList ordered(QList<Hit>& hits, bool emptyQuery)
{
    if (emptyQuery) {
        std::sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) {
            if (a.unseen != b.unseen) return a.unseen;
            if (a.time != b.time) return a.time > b.time;
            return a.id < b.id;
        });
    } else {
        std::sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) {
            if (a.score != b.score) return a.score > b.score;
            return a.id < b.id;
        });
    }
    QStringList out;
    for (int i = 0; i < hits.size() && i < kMaxResults; ++i)
        out << hits.at(i).id;
    return out;
}

// " w1 w2 …" for `text`'s words, appended to `out`.
void appendWords(QString& out, const QString& text)
{
    if (text.isEmpty()) return;
    for (const QString& w : tokens(text)) {
        out += QLatin1Char(' ');
        out += w;
    }
}

bool startsAWord(const QString& words, const QString& spacedToken)
{
    return !words.isEmpty() && words.contains(spacedToken);
}

int strengthSpaced(const Prepared& o, const QString& spaced)
{
    if (startsAWord(o.title, spaced)) return kTitleStrength;
    if (startsAWord(o.keywords, spaced)) return kKeywordStrength;
    if (startsAWord(o.subtitle, spaced)) return kSubtitleStrength;
    if (kMatchContentFields && startsAWord(o.fields, spaced)) return kFieldStrength;
    return 0;
}

double scoreSpaced(const Prepared& o, const QStringList& spacedTokens, qint64 nowMs)
{
    int match = 0;
    for (const QString& t : spacedTokens) {
        const int s = strengthSpaced(o, t);
        if (s == 0) return -1;
        match += s;
    }
    const double age = double(std::max<qint64>(0, nowMs - o.time));
    const double recency = kRecencyWeight * std::pow(2.0, -age / kRecencyHalfLifeMs);
    return kMatchWeight * match + recency + (o.seen ? 0.0 : kUnseenBoost);
}

QStringList spaced(const QStringList& queryTokens)
{
    QStringList out;
    for (const QString& t : queryTokens) out << QLatin1Char(' ') + t;
    return out;
}

} // namespace

QStringList rank(const QVariantList& objects, const QString& query, qint64 nowMs)
{
    const QStringList queryTokens = tokens(query);
    QList<Hit> hits;
    hits.reserve(objects.size());
    for (const QVariant& v : objects) {
        const QVariantMap o = v.toMap();
        const QString id = o.value(QStringLiteral("id")).toString();
        if (id.isEmpty()) continue;
        if (queryTokens.isEmpty()) {
            hits.append({ 0, timeOf(o), !o.value(QStringLiteral("seen")).toBool(), id });
            continue;
        }
        const double s = score(o, queryTokens, nowMs);
        if (s >= 0)
            hits.append({ s, 0, false, id });
    }
    return ordered(hits, queryTokens.isEmpty());
}

Prepared prepare(const QVariantMap& o)
{
    Prepared p = prepareOrder(o);
    prepareWords(p, o);
    return p;
}

Prepared prepareOrder(const QVariantMap& o)
{
    Prepared p;
    p.id = o.value(QStringLiteral("id")).toString();
    p.time = timeOf(o);
    p.seen = o.value(QStringLiteral("seen")).toBool();
    return p;
}

void prepareWords(Prepared& p, const QVariantMap& o)
{
    p.title.clear(); p.keywords.clear(); p.subtitle.clear(); p.fields.clear();
    appendWords(p.title, o.value(QStringLiteral("title")).toString());
    for (const QVariant& k : o.value(QStringLiteral("keywords")).toList())
        appendWords(p.keywords, k.toString());
    appendWords(p.subtitle, o.value(QStringLiteral("subtitle")).toString());
    if (kMatchContentFields) {
        QStringList strings;
        collectStrings(o.value(QStringLiteral("fields")), strings);
        for (const QString& str : std::as_const(strings))
            appendWords(p.fields, str);
    }
}

int strength(const Prepared& o, const QString& token)
{
    return strengthSpaced(o, QLatin1Char(' ') + token);
}

double score(const Prepared& o, const QStringList& queryTokens, qint64 nowMs)
{
    return scoreSpaced(o, spaced(queryTokens), nowMs);
}

QStringList rank(const QList<const Prepared*>& objects, const QString& query, qint64 nowMs)
{
    const QStringList queryTokens = tokens(query);
    const QStringList spacedTokens = spaced(queryTokens);
    QList<Hit> hits;
    hits.reserve(objects.size());
    for (const Prepared* o : objects) {
        if (!o || o->id.isEmpty()) continue;
        if (queryTokens.isEmpty()) {
            hits.append({ 0, o->time, !o->seen, o->id });
            continue;
        }
        const double s = scoreSpaced(*o, spacedTokens, nowMs);
        if (s >= 0)
            hits.append({ s, 0, false, o->id });
    }
    return ordered(hits, queryTokens.isEmpty());
}

} // namespace ObjectRanker
