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

QStringList rank(const QVariantList& objects, const QString& query, qint64 nowMs)
{
    const QStringList queryTokens = tokens(query);
    struct Hit { double score; qint64 time; bool unseen; QString id; };
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
    if (queryTokens.isEmpty()) {
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

} // namespace ObjectRanker
