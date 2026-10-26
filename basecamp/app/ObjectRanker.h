#pragma once

#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

// The host's own ranking of shell objects (SHELL-OBJECTS-SPEC §2, §5): what
// answers a search when nobody holds the `search` role, or when the holder is
// too slow. basecamp_search (apps/basecamp_search/qml/BasecampSearchRanker.js)
// ranks by the same rules, so switching between the two does not make results
// jump; change both together.
//
// The rules:
//   * The query is split into tokens on whitespace and ASCII punctuation, and
//     lowercased. Every token must match; an object a token misses is dropped.
//   * A token matches a word that starts with it (prefix, case-insensitive),
//     words split the same way. Its strength is the best place it matched:
//     title 4, keywords 3, subtitle 2, the string values of `fields` 1.
//   * score = 10 × (sum of the tokens' strengths)
//           + 5 × 2^(−age / 1 day)    recency, from `time` (else `updated`)
//           + 3 when unseen
//     The match term moves in steps of 10 and the other two stay under 8, so a
//     better match always wins and recency and unseen only order equal ones.
//   * An empty query keeps everything: unseen first, then newest first.
//   * Ties by id, so the order never depends on hash order.
//   * At most kMaxResults ids.
//
// Pure functions of their input and the clock passed in: unit-tested in
// object_search_test.cpp.
namespace ObjectRanker {

// Whether a query matches the string values of `fields`, which in v0 carry
// content: amounts, message text, counterparties (spec §4 marks each field
// label or content). The privacy decision (spec §9) is open; when it lands per
// field, this is the one switch to change (and kMatchContentFields in
// BasecampSearchRanker.js with it).
inline constexpr bool kMatchContentFields = true;

inline constexpr int kMaxResults = 200;

inline constexpr int kTitleStrength = 4;
inline constexpr int kKeywordStrength = 3;
inline constexpr int kSubtitleStrength = 2;
inline constexpr int kFieldStrength = 1;
inline constexpr double kMatchWeight = 10.0;
inline constexpr double kRecencyWeight = 5.0;
inline constexpr double kRecencyHalfLifeMs = 24.0 * 60 * 60 * 1000;
inline constexpr double kUnseenBoost = 3.0;

// Lowercased tokens of `text`, split on whitespace and ASCII punctuation.
QStringList tokens(const QString& text);

// How well one token matches `object`: 0 (not at all) to kTitleStrength.
int strength(const QVariantMap& object, const QString& token);

// The score of `object` for `queryTokens` at `nowMs`; negative when a token
// does not match. Exposed for tests.
double score(const QVariantMap& object, const QStringList& queryTokens, qint64 nowMs);

// `objects` (envelopes) ranked for `query`, best first: their ids.
QStringList rank(const QVariantList& objects, const QString& query, qint64 nowMs);

// ── Prepared objects (basecamp-experiments#27) ──────────────────────────
// The same rules over an object whose words were split once, when it was
// stored, instead of on every query: each place a token can match (title,
// keywords, subtitle, fields) becomes one string of its lowercased words, each
// after a space, so "a word starts with the token" is one search for
// " " + token. A token has no spaces, so that search can only start at a
// word's first letter and cannot run into the next word: the same answer as
// strength(), unit-tested against it.
struct Prepared {
    QString id;
    QString title, keywords, subtitle, fields;   // " word word …", or ""
    qint64 time = 0;
    bool seen = false;
};
Prepared prepare(const QVariantMap& object);
// prepare() in two steps: what an empty query needs (id, time, seen) at once,
// the words later (ObjectSearch splits them when idle, or before a query
// that needs them). A Prepared without its words matches no token.
Prepared prepareOrder(const QVariantMap& object);
void prepareWords(Prepared& prepared, const QVariantMap& object);
int strength(const Prepared& object, const QString& token);
double score(const Prepared& object, const QStringList& queryTokens, qint64 nowMs);
// rank() over prepared objects (pointers, so a caller ranks a subset of its
// index without copying it).
QStringList rank(const QList<const Prepared*>& objects, const QString& query, qint64 nowMs);

} // namespace ObjectRanker
