#include "ObjectStore.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaType>
#include <QRegularExpression>
#include <QSettings>
#include <QTimer>

#include <algorithm>
#include <cmath>

namespace {

// The user's per-app choice outlives the session, like Notifications/muted.
// The objects themselves never touch disk.
constexpr const char* kDisabledKey = "Search/disabled";

const QString kOpen = QStringLiteral("open");

bool isString(const QVariant& v) { return v.metaType().id() == QMetaType::QString; }
bool isMap(const QVariant& v)    { return v.metaType().id() == QMetaType::QVariantMap; }
bool isList(const QVariant& v)
{
    const int t = v.metaType().id();
    return t == QMetaType::QVariantList || t == QMetaType::QStringList;
}
bool isBool(const QVariant& v)   { return v.metaType().id() == QMetaType::Bool; }
// Lenient about width, as the broker is: QML hands over Int or Double
// depending on whether the literal had a fraction.
//
// FINITE, too. NaN and Infinity arrive from QML as ordinary Doubles (a
// `Date.parse` of a bad string, a division by zero), and NaN passes every
// `< 0` / `> 100` range check below because every comparison with it is false.
// It would then be stored, sorted on and serialized: JSON has no NaN.
bool isNumber(const QVariant& v)
{
    switch (v.metaType().id()) {
    case QMetaType::Int: case QMetaType::UInt: case QMetaType::LongLong:
    case QMetaType::ULongLong:
        return true;
    case QMetaType::Double: case QMetaType::Float:
        return std::isfinite(v.toDouble());
    default:
        return false;
    }
}

// The largest instant a JS Date can hold (ECMA-262 §21.4.1.1), in ms. A bound
// on `time` and `expires` before they become qint64: converting a double past
// qint64's range is undefined behaviour, and on x86 1e20 comes out as
// INT64_MIN — an `expires` meant as "never" would read as long past and the
// object would be purged straight after the publish was answered ok.
constexpr double kMaxEpochMs = 8.64e15;

// ── The v0 type table (spec §4) ──────────────────────────────────────────────
//
// What each field may hold. Deliberately coarse: enough that a view can draw
// the card without defending against every shape, not a schema language.
//   string   plain text, at most ObjectStore::kMaxText
//   text     string or number (amounts: "0.3319" and 0.3319 both mean it)
//   number / bool
//   strings  a list of plain strings
//   ref      another object's key, or { type?, key, app? }
//   balance  { amount, unit, fiat? }
//   a|b|c    one of those strings
using FieldSpec = QHash<QString, QString>;

const QHash<QString, FieldSpec>& typeTable()
{
    static const QHash<QString, FieldSpec> table = {
        { QStringLiteral("account"), {
            { QStringLiteral("name"), QStringLiteral("string") },
            { QStringLiteral("network"), QStringLiteral("string") },
            { QStringLiteral("balance"), QStringLiteral("balance") } } },
        { QStringLiteral("asset"), {
            { QStringLiteral("name"), QStringLiteral("string") },
            { QStringLiteral("symbol"), QStringLiteral("string") },
            { QStringLiteral("account"), QStringLiteral("ref") },
            { QStringLiteral("amount"), QStringLiteral("text") },
            { QStringLiteral("fiat"), QStringLiteral("text") },
            { QStringLiteral("change"), QStringLiteral("number") } } },
        { QStringLiteral("transaction"), {
            { QStringLiteral("direction"), QStringLiteral("in|out") },
            { QStringLiteral("status"),
              QStringLiteral("pending|sending|receiving|done|failed|scheduled") },
            { QStringLiteral("network"), QStringLiteral("string") },
            { QStringLiteral("amount"), QStringLiteral("text") },
            { QStringLiteral("unit"), QStringLiteral("string") },
            { QStringLiteral("counterparty"), QStringLiteral("string") },
            { QStringLiteral("address"), QStringLiteral("string") } } },
        { QStringLiteral("message"), {
            { QStringLiteral("conversation"), QStringLiteral("ref") },
            { QStringLiteral("outgoing"), QStringLiteral("bool") },
            { QStringLiteral("sender"), QStringLiteral("string") },
            { QStringLiteral("senderName"), QStringLiteral("string") },
            { QStringLiteral("text"), QStringLiteral("string") } } },
        { QStringLiteral("post"), {
            { QStringLiteral("community"), QStringLiteral("ref") },
            { QStringLiteral("author"), QStringLiteral("string") },
            { QStringLiteral("authorName"), QStringLiteral("string") },
            { QStringLiteral("text"), QStringLiteral("string") },
            { QStringLiteral("score"), QStringLiteral("number") },
            { QStringLiteral("likes"), QStringLiteral("number") } } },
        { QStringLiteral("community"), {
            { QStringLiteral("name"), QStringLiteral("string") },
            { QStringLiteral("members"), QStringLiteral("number") },
            { QStringLiteral("joined"), QStringLiteral("bool") } } },
        { QStringLiteral("contact"), {
            { QStringLiteral("name"), QStringLiteral("string") },
            { QStringLiteral("groups"), QStringLiteral("strings") },
            { QStringLiteral("subject"), QStringLiteral("string") } } },
        { QStringLiteral("news"), {
            { QStringLiteral("source"), QStringLiteral("string") },
            // Carried for the app's own use; the shell never opens it.
            { QStringLiteral("url"), QStringLiteral("string") },
            { QStringLiteral("text"), QStringLiteral("string") } } },
        { QStringLiteral("notification"), {
            { QStringLiteral("level"), QStringLiteral("info|success|warning|error") },
            { QStringLiteral("body"), QStringLiteral("string") } } },
    };
    return table;
}

bool plainString(const QVariant& v, int max)
{
    return isString(v) && v.toString().size() <= max;
}

bool fieldMatches(const QVariant& v, const QString& kind)
{
    if (kind == QLatin1String("string"))
        return plainString(v, ObjectStore::kMaxText);
    if (kind == QLatin1String("text"))
        return plainString(v, ObjectStore::kMaxText) || isNumber(v);
    if (kind == QLatin1String("number"))
        return isNumber(v);
    if (kind == QLatin1String("bool"))
        return isBool(v);
    if (kind == QLatin1String("strings")) {
        if (!isList(v)) return false;
        const QVariantList l = v.toList();
        if (l.size() > ObjectStore::kMaxKeywords) return false;
        return std::all_of(l.cbegin(), l.cend(),
                           [](const QVariant& s) { return plainString(s, ObjectStore::kMaxKeyword); });
    }
    if (kind == QLatin1String("ref")) {
        if (plainString(v, ObjectStore::kMaxKey)) return true;
        if (!isMap(v)) return false;
        const QVariantMap m = v.toMap();
        return plainString(m.value(QStringLiteral("key")), ObjectStore::kMaxKey)
            && !m.value(QStringLiteral("key")).toString().isEmpty()
            && (!m.contains(QStringLiteral("type")) || plainString(m.value(QStringLiteral("type")), 64))
            && (!m.contains(QStringLiteral("app")) || plainString(m.value(QStringLiteral("app")), 64));
    }
    if (kind == QLatin1String("balance")) {
        if (!isMap(v)) return false;
        const QVariantMap m = v.toMap();
        return fieldMatches(m.value(QStringLiteral("amount")), QStringLiteral("text"))
            && plainString(m.value(QStringLiteral("unit")), 32)
            && (!m.contains(QStringLiteral("fiat"))
                || fieldMatches(m.value(QStringLiteral("fiat")), QStringLiteral("text")));
    }
    // An enum: "a|b|c".
    return isString(v) && kind.split(QLatin1Char('|')).contains(v.toString());
}

const QStringList kKinds = { QStringLiteral("item"), QStringLiteral("summary"),
                             QStringLiteral("action"), QStringLiteral("setting") };

// Supplied by the publisher. Everything else is host-set or unknown.
const QStringList kEnvelopeFields = {
    QStringLiteral("key"), QStringLiteral("type"), QStringLiteral("kind"),
    QStringLiteral("title"), QStringLiteral("subtitle"), QStringLiteral("fields"),
    QStringLiteral("keywords"), QStringLiteral("subjects"), QStringLiteral("refs"),
    QStringLiteral("actions"), QStringLiteral("progress"), QStringLiteral("time"),
    QStringLiteral("expires"), QStringLiteral("state"),
};
const QStringList kHostFields = {
    QStringLiteral("id"), QStringLiteral("app"), QStringLiteral("updated"),
    QStringLiteral("seen"), QStringLiteral("icon"), QStringLiteral("iconFullBleed"),
};

bool stringList(const QVariant& v, int maxItems, int maxEach)
{
    if (!isList(v)) return false;
    const QVariantList l = v.toList();
    if (l.size() > maxItems) return false;
    return std::all_of(l.cbegin(), l.cend(), [maxEach](const QVariant& s) {
        return plainString(s, maxEach) && !s.toString().isEmpty();
    });
}

bool anyOf(const QVariantMap& filter, const char* key, const QString& value)
{
    const QVariant f = filter.value(QLatin1String(key));
    if (!f.isValid()) return true;
    const QStringList wanted = f.toStringList();
    return wanted.isEmpty() || wanted.contains(value);
}

} // namespace

const QStringList& ObjectStore::knownTypes()
{
    static const QStringList types = [] {
        QStringList t = typeTable().keys();
        t.sort();
        return t;
    }();
    return types;
}

ObjectStore::ObjectStore(QObject* parent)
    : QObject(parent)
{
    const QStringList disabled = QSettings().value(QLatin1String(kDisabledKey)).toStringList();
    m_disabled = QSet<QString>(disabled.begin(), disabled.end());

    m_expiry = new QTimer(this);
    m_expiry->setSingleShot(true);
    connect(m_expiry, &QTimer::timeout, this, &ObjectStore::purgeExpired);
}

qint64 ObjectStore::now() const
{
    return m_now ? m_now() : QDateTime::currentMSecsSinceEpoch();
}

QString ObjectStore::idFor(const QString& app, const QString& key)
{
    // A hash rather than "app/key": the id ends up in views and the search
    // app, which have no business parsing the publisher's own key out of it.
    // The newline cannot appear in an app name, so no two pairs collide by
    // concatenation.
    const QByteArray digest = QCryptographicHash::hash(
        (app + QLatin1Char('\n') + key).toUtf8(), QCryptographicHash::Sha1);
    return QStringLiteral("o") + QString::fromLatin1(digest.toHex().left(20));
}

bool ObjectStore::allow(const QString& app, bool record)
{
    const qint64 t = now();
    auto& times = m_recent[app];
    while (!times.empty() && t - times.front() >= kRateWindowMs)
        times.pop_front();
    if (int(times.size()) >= kRateRequests)
        return false;
    if (record)
        times.push_back(t);
    return true;
}

QVariantMap ObjectStore::validate(const QString& app, int index, const QVariantMap& in,
                                  QString* error) const
{
    // Bad shape is the developer's to fix: say which object and why in the
    // log, since the result envelope carries a code and nothing else.
    auto bad = [&](const QString& why) {
        qWarning().noquote() << "ObjectStore:" << app << "object" << index
                             << "refused —" << why;
        *error = QStringLiteral("bad_request");
        return QVariantMap();
    };
    auto note = [&](const QString& what) {
        qWarning().noquote() << "ObjectStore:" << app << "object" << index << what;
    };

    // Everything kept is a canonical intent payload type (LogosIntent.h): no
    // QStringList, no uint. An envelope may yet cross an engine boundary (the
    // search role, a view's results), and the broker refuses anything else.
    QVariantMap out;
    for (auto it = in.cbegin(); it != in.cend(); ++it) {
        if (kHostFields.contains(it.key()))
            note(QStringLiteral("— '%1' is set by the host; ignored").arg(it.key()));
        else if (!kEnvelopeFields.contains(it.key()))
            note(QStringLiteral("— unknown field '%1' dropped").arg(it.key()));
    }

    const QVariant key = in.value(QStringLiteral("key"));
    if (!isString(key) || key.toString().isEmpty() || key.toString().size() > kMaxKey)
        return bad(QStringLiteral("`key` must be a string of 1 to %1 characters").arg(kMaxKey));
    out[QStringLiteral("key")] = key;

    const QVariant typeV = in.value(QStringLiteral("type"));
    if (!isString(typeV))
        return bad(QStringLiteral("`type` is required"));
    const QString type = typeV.toString();
    // An app's own type names that app, so two apps cannot share one by
    // accident, and graduating it to the shared list is a visible change.
    static const QRegularExpression customName(QStringLiteral("^[a-z0-9_]+(\\.[a-z0-9_]+)*$"));
    const QString ownPrefix = QStringLiteral("x.") + app + QLatin1Char('.');
    const bool custom = type.startsWith(ownPrefix) && type.size() <= 96
                     && customName.match(type.mid(ownPrefix.size())).hasMatch();
    if (!custom && !typeTable().contains(type))
        return bad(QStringLiteral("unknown type '%1' (v0 types, or x.%2.<name>)").arg(type, app));
    out[QStringLiteral("type")] = type;

    const QVariant kind = in.value(QStringLiteral("kind"), QStringLiteral("item"));
    if (!isString(kind) || !kKinds.contains(kind.toString()))
        return bad(QStringLiteral("`kind` must be item, summary, action or setting"));
    out[QStringLiteral("kind")] = kind;

    const QVariant title = in.value(QStringLiteral("title"));
    if (!isString(title) || title.toString().trimmed().isEmpty()
        || title.toString().trimmed().size() > kMaxTitle)
        return bad(QStringLiteral("`title` must be plain text of 1 to %1 characters").arg(kMaxTitle));
    out[QStringLiteral("title")] = title.toString().trimmed();

    if (in.contains(QStringLiteral("subtitle"))) {
        const QVariant s = in.value(QStringLiteral("subtitle"));
        if (!plainString(s, kMaxSubtitle))
            return bad(QStringLiteral("`subtitle` must be plain text of at most %1").arg(kMaxSubtitle));
        out[QStringLiteral("subtitle")] = s.toString().trimmed();
    }

    // Fields: the type's own, checked; an app type's kept whole, since the
    // shell has no list to check them against and draws them generically.
    QVariantMap fields;
    if (in.contains(QStringLiteral("fields"))) {
        const QVariant f = in.value(QStringLiteral("fields"));
        if (!isMap(f))
            return bad(QStringLiteral("`fields` must be an object"));
        const QVariantMap given = f.toMap();
        if (custom) {
            fields = given;
        } else {
            const FieldSpec& spec = typeTable().value(type);
            for (auto it = given.cbegin(); it != given.cend(); ++it) {
                const QString kindOf = spec.value(it.key());
                if (kindOf.isEmpty()) {
                    note(QStringLiteral("— field '%1' is not a %2 field; dropped").arg(it.key(), type));
                    continue;
                }
                if (!fieldMatches(it.value(), kindOf))
                    return bad(QStringLiteral("field '%1' should be %2").arg(it.key(), kindOf));
                fields.insert(it.key(), it.value());
            }
        }
    }
    out[QStringLiteral("fields")] = fields;

    if (in.contains(QStringLiteral("keywords"))) {
        if (!stringList(in.value(QStringLiteral("keywords")), kMaxKeywords, kMaxKeyword))
            return bad(QStringLiteral("`keywords`: at most %1 strings of %2").arg(kMaxKeywords).arg(kMaxKeyword));
        out[QStringLiteral("keywords")] = in.value(QStringLiteral("keywords")).toList();
    }
    if (in.contains(QStringLiteral("subjects"))) {
        if (!stringList(in.value(QStringLiteral("subjects")), kMaxSubjects, kMaxSubject))
            return bad(QStringLiteral("`subjects`: at most %1 strings").arg(kMaxSubjects));
        out[QStringLiteral("subjects")] = in.value(QStringLiteral("subjects")).toList();
    }

    if (in.contains(QStringLiteral("refs"))) {
        const QVariant r = in.value(QStringLiteral("refs"));
        if (!isList(r) || r.toList().size() > kMaxRefs)
            return bad(QStringLiteral("`refs`: at most %1 of { type, key, app? }").arg(kMaxRefs));
        QVariantList refs;
        for (const QVariant& v : r.toList()) {
            const QVariantMap m = v.toMap();
            if (!isMap(v) || !plainString(m.value(QStringLiteral("type")), 96)
                || m.value(QStringLiteral("type")).toString().isEmpty()
                || !plainString(m.value(QStringLiteral("key")), kMaxKey)
                || m.value(QStringLiteral("key")).toString().isEmpty()
                || (m.contains(QStringLiteral("app")) && !plainString(m.value(QStringLiteral("app")), 64)))
                return bad(QStringLiteral("each ref is { type, key, app? }"));
            QVariantMap ref{ { QStringLiteral("type"), m.value(QStringLiteral("type")) },
                             { QStringLiteral("key"), m.value(QStringLiteral("key")) } };
            if (m.contains(QStringLiteral("app")))
                ref[QStringLiteral("app")] = m.value(QStringLiteral("app"));
            refs << ref;
        }
        out[QStringLiteral("refs")] = refs;
    }

    if (in.contains(QStringLiteral("actions"))) {
        const QVariant a = in.value(QStringLiteral("actions"));
        if (!isList(a) || a.toList().size() > kMaxActions)
            return bad(QStringLiteral("`actions`: at most %1 of { id, label, input? }").arg(kMaxActions));
        QVariantList actions;
        QStringList seenIds;
        for (const QVariant& v : a.toList()) {
            const QVariantMap m = v.toMap();
            const QString id = m.value(QStringLiteral("id")).toString();
            // `open` is the object itself (spec §3), never a button: a second
            // way to raise it would only be a second label for the same thing.
            if (!isMap(v) || !plainString(m.value(QStringLiteral("id")), kMaxActionId) || id.isEmpty()
                || id == kOpen || seenIds.contains(id)
                || !plainString(m.value(QStringLiteral("label")), kMaxActionLabel)
                || m.value(QStringLiteral("label")).toString().trimmed().isEmpty()
                || (m.contains(QStringLiteral("input"))
                    && m.value(QStringLiteral("input")) != QVariant(QStringLiteral("text"))))
                return bad(QStringLiteral("each action is { id, label, input?: \"text\" }, ids unique and not \"open\""));
            seenIds << id;
            QVariantMap action{ { QStringLiteral("id"), id },
                                { QStringLiteral("label"), m.value(QStringLiteral("label")).toString().trimmed() } };
            if (m.contains(QStringLiteral("input")))
                action[QStringLiteral("input")] = QStringLiteral("text");
            actions << action;
        }
        out[QStringLiteral("actions")] = actions;
    }

    if (in.contains(QStringLiteral("progress"))) {
        const QVariant p = in.value(QStringLiteral("progress"));
        if (!isNumber(p) || p.toDouble() < 0 || p.toDouble() > 100)
            return bad(QStringLiteral("`progress` must be 0..100"));
        out[QStringLiteral("progress")] = p.toDouble();
    }
    // Current state (an account, a membership) rather than an event: kept
    // when the app is over its cap (evict()). Only `true` is stored, so a
    // flagless publisher's envelopes are exactly what they were.
    if (in.contains(QStringLiteral("state"))) {
        const QVariant st = in.value(QStringLiteral("state"));
        if (!isBool(st))
            return bad(QStringLiteral("`state` must be true or false"));
        if (st.toBool())
            out[QStringLiteral("state")] = true;
    }
    for (const char* name : { "time", "expires" }) {
        const QString n = QLatin1String(name);
        if (!in.contains(n)) continue;
        const QVariant t = in.value(n);
        if (!isNumber(t) || t.toDouble() < 0 || t.toDouble() > kMaxEpochMs)
            return bad(QStringLiteral("`%1` must be ms since the epoch").arg(n));
        out[n] = qint64(t.toDouble());
    }
    return out;
}

QString ObjectStore::publish(const QString& app, const QVariantMap& params,
                             int* badIndex, QStringList* ids)
{
    return publishBatch(app, params, badIndex, ids, /*fromHost=*/false);
}

QString ObjectStore::publishFromHost(const QString& app, const QVariantMap& params)
{
    return publishBatch(app, params, nullptr, nullptr, /*fromHost=*/true);
}

QString ObjectStore::publishBatch(const QString& app, const QVariantMap& params,
                                  int* badIndex, QStringList* ids, bool fromHost)
{
    if (badIndex) *badIndex = -1;
    if (app.isEmpty())
        return QStringLiteral("bad_request");

    const QVariant objectsV = params.value(QStringLiteral("objects"));
    if (!isList(objectsV)) {
        qWarning().noquote() << "ObjectStore:" << app << "publish refused — `objects` must be a list";
        return QStringLiteral("bad_request");
    }
    // Over the rate, refused before any object is looked at: an app that
    // floods sends many requests at once, and validating each batch only to
    // refuse it held the UI thread (basecamp-experiments#63). The request is
    // counted below, once it is valid, so a bad batch still costs nothing.
    if (!fromHost && !allow(app, /*record=*/false)) {
        qWarning().noquote() << "ObjectStore:" << app << "publish refused — over"
                             << kRateRequests << "requests per" << kRateWindowMs / 1000 << "s";
        return QStringLiteral("failed");
    }
    const QVariantList objects = objectsV.toList();
    if (objects.size() > kMaxObjectsPerApp) {
        qWarning().noquote() << "ObjectStore:" << app << "publish refused —" << objects.size()
                             << "objects in one batch, more than the" << kMaxObjectsPerApp << "an app may keep";
        return QStringLiteral("failed");
    }

    // Validate the whole batch before touching anything: all or nothing.
    QList<QVariantMap> clean;
    QSet<QString> batchKeys;
    for (int i = 0; i < objects.size(); ++i) {
        QString error;
        if (!isMap(objects.at(i))) {
            qWarning().noquote() << "ObjectStore:" << app << "object" << i << "refused — not an object";
            if (badIndex) *badIndex = i;
            return QStringLiteral("bad_request");
        }
        QVariantMap o = validate(app, i, objects.at(i).toMap(), &error);
        if (!error.isEmpty()) {
            if (badIndex) *badIndex = i;
            return error;
        }
        // A limit, not a shape: `failed`, as for notifications over theirs.
        const QByteArray json = QJsonDocument(
            QJsonObject::fromVariantMap(o.value(QStringLiteral("fields")).toMap())).toJson(QJsonDocument::Compact);
        if (json.size() > kMaxFieldsBytes) {
            qWarning().noquote() << "ObjectStore:" << app << "object" << i << "refused — `fields` is"
                                 << json.size() << "bytes, over" << kMaxFieldsBytes;
            if (badIndex) *badIndex = i;
            return QStringLiteral("failed");
        }
        batchKeys.insert(o.value(QStringLiteral("key")).toString());
        clean << o;
    }
    const auto stateInBatch = std::count_if(clean.cbegin(), clean.cend(), [](const QVariantMap& o) {
        return o.value(QStringLiteral("state")).toBool();
    });
    if (stateInBatch > kMaxStatePerApp) {
        qWarning().noquote() << "ObjectStore:" << app << "publish refused —" << stateInBatch
                             << "current-state objects in one batch, more than the" << kMaxStatePerApp
                             << "an app may keep";
        return QStringLiteral("failed");
    }

    if (!fromHost && !allow(app)) {
        qWarning().noquote() << "ObjectStore:" << app << "publish refused — over"
                             << kRateRequests << "requests per" << kRateWindowMs / 1000 << "s";
        return QStringLiteral("failed");
    }

    if (ids) {
        ids->clear();
        for (const QVariantMap& o : std::as_const(clean))
            ids->append(idFor(app, o.value(QStringLiteral("key")).toString()));
    }

    // Answered like anyone else, kept by no one (spec §8).
    if (m_disabled.contains(app))
        return QString();

    const qint64 t = now();
    const QString icon = m_iconFor ? m_iconFor(app) : QString();
    const bool iconFullBleed = m_iconFullBleedFor && m_iconFullBleedFor(app);
    QSet<QString>& mine = m_byApp[app];
    for (QVariantMap o : std::as_const(clean)) {
        const QString id = idFor(app, o.value(QStringLiteral("key")).toString());
        o[QStringLiteral("id")] = id;
        o[QStringLiteral("app")] = app;
        o[QStringLiteral("icon")] = icon;
        o[QStringLiteral("iconFullBleed")] = iconFullBleed;
        o[QStringLiteral("updated")] = t;
        if (!o.contains(QStringLiteral("time")))
            o[QStringLiteral("time")] = t;
        // An update in place (a transfer's progress) is not news: replacing
        // keeps whether a view has shown it. Only a new key starts unseen,
        // and a notification, whose replacement is news by definition.
        const auto existing = m_objects.constFind(id);
        o[QStringLiteral("seen")] = !fromHost && existing != m_objects.cend()
            && existing->value(QStringLiteral("seen")).toBool();
        m_objects.insert(id, o);
        touched(id);
        mine.insert(id);
    }
    evict(app);
    purgeExpired();   // drops anything published already expired, and re-arms
    emit objectsChanged(app);
    emit changed();
    return QString();
}

QString ObjectStore::retract(const QString& app, const QVariantMap& params)
{
    if (app.isEmpty())
        return QStringLiteral("bad_request");
    const bool all = params.value(QStringLiteral("all")) == QVariant(true);
    const QVariant keys = params.value(QStringLiteral("keys"));
    if (!all && !stringList(keys, kMaxObjectsPerApp, kMaxKey)) {
        qWarning().noquote() << "ObjectStore:" << app
                             << "retract refused — give { keys: [ … ] } or { all: true }";
        return QStringLiteral("bad_request");
    }
    if (all) {
        removeApp(app);
        return QString();
    }
    QStringList ids;
    for (const QString& key : keys.toStringList())
        ids << idFor(app, key);
    removeIds(app, ids);
    return QString();
}

void ObjectStore::removeIds(const QString& app, const QStringList& ids)
{
    auto mine = m_byApp.find(app);
    if (mine == m_byApp.end())
        return;
    bool removed = false;
    for (const QString& id : ids) {
        if (mine->remove(id)) {
            m_objects.remove(id);
            dropped(id);
            removed = true;
        }
    }
    if (mine->isEmpty())
        m_byApp.erase(mine);
    if (removed) {
        emit objectsChanged(app);
        emit changed();
    }
}

void ObjectStore::evict(const QString& app)
{
    auto mine = m_byApp.find(app);
    if (mine == m_byApp.end() || mine->size() <= kMaxStatePerApp)
        return;
    // Oldest `time` first (spec §8); ties broken by id so the result does not
    // depend on hash order. Current state and events are two queues: state
    // over its own cap loses its oldest, then the app over its cap loses its
    // oldest EVENTS. State is at most kMaxStatePerApp of kMaxObjectsPerApp,
    // so there are always enough events to evict and state never goes to
    // make room for them (basecamp-experiments#63).
    QList<QPair<qint64, QString>> states, events;
    for (const QString& id : std::as_const(*mine)) {
        const QVariantMap o = m_objects.value(id);
        (o.value(QStringLiteral("state")).toBool() ? states : events)
            << qMakePair(o.value(QStringLiteral("time")).toLongLong(), id);
    }
    const int stateExcess = int(states.size()) - kMaxStatePerApp;
    const int eventExcess = int(mine->size()) - std::max(0, stateExcess) - kMaxObjectsPerApp;
    auto drop = [&](QList<QPair<qint64, QString>>& queue, int n) {
        std::sort(queue.begin(), queue.end());
        for (int i = 0; i < n; ++i) {
            mine->remove(queue.at(i).second);
            m_objects.remove(queue.at(i).second);
            dropped(queue.at(i).second);
        }
    };
    if (stateExcess > 0) {
        drop(states, stateExcess);
        qWarning().noquote() << "ObjectStore:" << app << "over" << kMaxStatePerApp
                             << "current-state objects — evicted the" << stateExcess << "oldest";
    }
    if (eventExcess > 0) {
        drop(events, eventExcess);
        qWarning().noquote() << "ObjectStore:" << app << "over" << kMaxObjectsPerApp
                             << "objects — evicted the" << eventExcess << "oldest events";
    }
}

void ObjectStore::removeApp(const QString& app)
{
    const auto mine = m_byApp.constFind(app);
    if (mine == m_byApp.cend())
        return;
    removeIds(app, QStringList(mine->cbegin(), mine->cend()));
}

void ObjectStore::retainApps(const QStringList& apps)
{
    const QStringList present = m_byApp.keys();
    for (const QString& app : present)
        if (!apps.contains(app))
            removeApp(app);
}

void ObjectStore::purgeExpired()
{
    const qint64 t = now();
    QHash<QString, QStringList> expired;
    for (auto it = m_objects.cbegin(); it != m_objects.cend(); ++it) {
        const QVariant e = it->value(QStringLiteral("expires"));
        if (e.isValid() && e.toLongLong() <= t)
            expired[it->value(QStringLiteral("app")).toString()] << it.key();
    }
    for (auto it = expired.cbegin(); it != expired.cend(); ++it)
        removeIds(it.key(), it.value());
    scheduleExpiry();
}

void ObjectStore::scheduleExpiry()
{
    // One timer, aimed at the next object to expire, rather than polling.
    qint64 next = -1;
    for (const QVariantMap& o : std::as_const(m_objects)) {
        const QVariant e = o.value(QStringLiteral("expires"));
        if (e.isValid() && (next < 0 || e.toLongLong() < next))
            next = e.toLongLong();
    }
    if (next < 0) {
        m_expiry->stop();
        return;
    }
    // Capped so a far-off expiry cannot overflow the timer, and re-aimed when it fires.
    const qint64 wait = std::clamp<qint64>(next - now(), 0, 60 * 60 * 1000);
    m_expiry->start(int(wait));
}

void ObjectStore::touched(const QString& id)
{
    m_versionOf.insert(id, ++m_version);
    m_gone.remove(id);
}

void ObjectStore::dropped(const QString& id)
{
    m_versionOf.remove(id);
    m_gone.insert(id, ++m_version);
    if (m_gone.size() > kMaxTombstones) {
        // Forget every removal: a reader behind this point starts over.
        m_gone.clear();
        m_goneFloor = m_version;
    }
}

ObjectStore::Changes ObjectStore::changesSince(quint64 since) const
{
    const_cast<ObjectStore*>(this)->purgeExpired();
    Changes out;
    out.version = m_version;
    out.reset = since == 0 || since > m_version || since < m_goneFloor;
    if (!out.reset && since == m_version)
        return out;
    for (auto it = m_objects.cbegin(); it != m_objects.cend(); ++it)
        if (out.reset || m_versionOf.value(it.key()) > since)
            out.changed << it.value();
    if (!out.reset)
        for (auto it = m_gone.cbegin(); it != m_gone.cend(); ++it)
            if (it.value() > since)
                out.removed << it.key();
    return out;
}

int ObjectStore::total() const
{
    return m_objects.size();
}

int ObjectStore::count(const QString& app) const
{
    const_cast<ObjectStore*>(this)->purgeExpired();
    if (app.isEmpty())
        return m_objects.size();
    return m_byApp.value(app).size();
}

bool ObjectStore::matches(const QVariantMap& o, const QVariantMap& filter)
{
    if (!anyOf(filter, "types", o.value(QStringLiteral("type")).toString())
        || !anyOf(filter, "apps", o.value(QStringLiteral("app")).toString())
        || !anyOf(filter, "kinds", o.value(QStringLiteral("kind")).toString())
        || !anyOf(filter, "keys", o.value(QStringLiteral("key")).toString())
        || !anyOf(filter, "ids", o.value(QStringLiteral("id")).toString()))
        return false;
    if (filter.value(QStringLiteral("unseenOnly")).toBool() && o.value(QStringLiteral("seen")).toBool())
        return false;
    const QStringList subjects = filter.value(QStringLiteral("subjects")).toStringList();
    if (!subjects.isEmpty()) {
        const QStringList mine = o.value(QStringLiteral("subjects")).toStringList();
        if (std::none_of(subjects.cbegin(), subjects.cend(),
                         [&mine](const QString& s) { return mine.contains(s); }))
            return false;
    }
    return true;
}

QVariantList ObjectStore::list(const QVariantMap& filter) const
{
    const_cast<ObjectStore*>(this)->purgeExpired();
    QList<QVariantMap> hits;
    for (const QVariantMap& o : std::as_const(m_objects))
        if (matches(o, filter))
            hits << o;
    std::sort(hits.begin(), hits.end(), [](const QVariantMap& a, const QVariantMap& b) {
        const qint64 ta = a.value(QStringLiteral("time")).toLongLong();
        const qint64 tb = b.value(QStringLiteral("time")).toLongLong();
        if (ta != tb) return ta > tb;
        return a.value(QStringLiteral("id")).toString() < b.value(QStringLiteral("id")).toString();
    });
    QVariantList out;
    out.reserve(hits.size());
    for (const QVariantMap& o : std::as_const(hits))
        out << o;
    return out;
}

QStringList ObjectStore::types() const
{
    const_cast<ObjectStore*>(this)->purgeExpired();
    QSet<QString> set;
    for (const QVariantMap& o : std::as_const(m_objects))
        set.insert(o.value(QStringLiteral("type")).toString());
    QStringList out(set.begin(), set.end());
    out.sort();
    return out;
}

QHash<QString, int> ObjectStore::countsOfType(const QString& type) const
{
    const_cast<ObjectStore*>(this)->purgeExpired();
    QHash<QString, int> out;
    if (type.isEmpty())
        return out;
    for (const QVariantMap& o : std::as_const(m_objects)) {
        if (o.value(QStringLiteral("type")).toString() != type)
            continue;
        const QString app = o.value(QStringLiteral("app")).toString();
        // Off already deletes an app's objects; checked again so a race with
        // the switch never names an app the user turned off.
        if (!app.isEmpty() && isEnabled(app))
            ++out[app];
    }
    return out;
}

QVariantMap ObjectStore::get(const QString& id) const
{
    const_cast<ObjectStore*>(this)->purgeExpired();
    return m_objects.value(id);
}

QList<QVariantMap> ObjectStore::getMany(const QStringList& ids) const
{
    const_cast<ObjectStore*>(this)->purgeExpired();
    QList<QVariantMap> out;
    out.reserve(ids.size());
    for (const QString& id : ids)
        out << m_objects.value(id);
    return out;
}

void ObjectStore::markSeen(const QStringList& ids)
{
    QStringList marked;
    for (const QString& id : ids) {
        auto it = m_objects.find(id);
        if (it == m_objects.end() || it->value(QStringLiteral("seen")).toBool())
            continue;
        (*it)[QStringLiteral("seen")] = true;
        touched(id);
        marked << id;
    }
    if (marked.isEmpty())
        return;
    emit seenChanged(marked);
    emit changed();
}

QStringList ObjectStore::disabledApps() const
{
    QStringList out(m_disabled.begin(), m_disabled.end());
    out.sort();
    return out;
}

void ObjectStore::setEnabled(const QString& app, bool enabled)
{
    if (app.isEmpty() || enabled == !m_disabled.contains(app))
        return;
    if (enabled) {
        m_disabled.remove(app);
    } else {
        m_disabled.insert(app);
        // Off removes everything that app published, at once (spec §8).
        removeApp(app);
    }
    QSettings().setValue(QLatin1String(kDisabledKey), disabledApps());
    emit disabledAppsChanged();
}
