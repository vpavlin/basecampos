#include "Keychain.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace {

QVariantMap ok() { return { { QStringLiteral("ok"), true } }; }

QVariantMap fail(const char* code)
{
    return { { QStringLiteral("ok"), false }, { QStringLiteral("error"), QLatin1String(code) } };
}

// Printable, one line: what a name or a label may hold.
bool oneLine(const QString& s)
{
    for (const QChar c : s)
        if (c.category() == QChar::Other_Control || c == QChar::LineSeparator
            || c == QChar::ParagraphSeparator)
            return false;
    return true;
}

// A note may break lines, nothing else.
bool noteText(const QString& s)
{
    for (const QChar c : s)
        if (c != QLatin1Char('\n') && c.category() == QChar::Other_Control)
            return false;
    return true;
}

bool validKeyId(const QString& id)
{
    if (id.isEmpty() || id.size() > Keychain::kMaxKeyId) return false;
    for (const QChar c : id) {
        const bool fine = (c >= QLatin1Char('a') && c <= QLatin1Char('z'))
                       || (c >= QLatin1Char('A') && c <= QLatin1Char('Z'))
                       || (c >= QLatin1Char('0') && c <= QLatin1Char('9'))
                       || c == QLatin1Char('-') || c == QLatin1Char('_')
                       || c == QLatin1Char('.') || c == QLatin1Char(':');
        if (!fine) return false;
    }
    return true;
}

// `name` trimmed, or "" when it is not a usable name.
QString cleanName(const QVariant& v)
{
    if (v.typeId() != QMetaType::QString) return {};
    const QString name = v.toString().trimmed();
    return name.size() <= Keychain::kMaxName && oneLine(name) ? name : QString();
}

// Absent is "none"; anything but a list of known, distinct names is refused.
bool cleanPurposes(const QVariant& v, QStringList* out)
{
    out->clear();
    if (!v.isValid() || v.isNull()) return true;
    if (v.typeId() != QMetaType::QVariantList && v.typeId() != QMetaType::QStringList)
        return false;
    const QVariantList list = v.toList();
    if (list.size() > Keychain::purposeNames().size()) return false;
    for (const QVariant& p : list) {
        if (p.typeId() != QMetaType::QString) return false;
        const QString s = p.toString();
        if (!Keychain::purposeNames().contains(s)) return false;
        if (!out->contains(s)) out->append(s);
    }
    return true;
}

QByteArray decodeBase64(const QVariant& v, bool* okOut)
{
    *okOut = false;
    if (v.typeId() != QMetaType::QString) return {};
    const QByteArray text = v.toString().toLatin1();
    if (text.isEmpty() || text.size() > 256) return {};
    const auto r = QByteArray::fromBase64Encoding(text, QByteArray::AbortOnBase64DecodingErrors);
    if (!r) return {};
    *okOut = true;
    return *r;
}

QVariantList toList(const QStringList& l) { return QVariantList(l.cbegin(), l.cend()); }

} // namespace

const QStringList& Keychain::algorithms()
{
    static const QStringList a = { QStringLiteral("ed25519"), QStringLiteral("secp256k1") };
    return a;
}

const QStringList& Keychain::purposeNames()
{
    static const QStringList p = { QStringLiteral("message"), QStringLiteral("transaction"),
                                   QStringLiteral("login") };
    return p;
}

bool Keychain::validPublicKey(const QString& algorithm, const QByteArray& key)
{
    if (algorithm == QStringLiteral("ed25519"))
        return key.size() == 32;
    if (algorithm == QStringLiteral("secp256k1")) {
        // SEC1: compressed (02/03 + x) or uncompressed (04 + x + y).
        if (key.size() == 33) return key.at(0) == 0x02 || key.at(0) == 0x03;
        if (key.size() == 65) return key.at(0) == 0x04;
    }
    return false;
}

QString Keychain::fingerprint(const QByteArray& publicKey)
{
    const QString b64 = QString::fromLatin1(publicKey.toBase64());
    return b64.size() <= 12 ? b64 : b64.left(6) + QStringLiteral("…") + b64.right(6);
}

Keychain::Keychain(QString filePath, QObject* parent)
    : QObject(parent)
    , m_path(std::move(filePath))
{
    load();
}

qint64 Keychain::now() const
{
    return m_now ? m_now() : QDateTime::currentMSecsSinceEpoch();
}

QString Keychain::sourceFor(const QString& app) const
{
    return m_sourceFor ? m_sourceFor(app) : QString();
}

int Keychain::indexOf(const QString& app, const QString& keyId) const
{
    for (int i = 0; i < m_keys.size(); ++i)
        if (m_keys.at(i).ownerApp == app && m_keys.at(i).keyId == keyId)
            return i;
    return -1;
}

int Keychain::count(const QString& app) const
{
    if (app.isEmpty()) return m_keys.size();
    int n = 0;
    for (const Key& k : m_keys)
        if (k.ownerApp == app) ++n;
    return n;
}

void Keychain::forgetOtherSource(const QString& app)
{
    // Only when both sides are known: an install the host cannot attribute
    // ("") is not evidence of a different package.
    const QString source = sourceFor(app);
    if (source.isEmpty()) return;
    const qsizetype before = m_keys.size();
    m_keys.removeIf([&](const Key& k) {
        return k.ownerApp == app && !k.ownerSource.isEmpty() && k.ownerSource != source;
    });
    if (m_keys.size() != before) {
        qInfo().noquote() << "Keychain: dropped" << (before - m_keys.size())
                          << "key(s) registered by another source of" << app;
        changed();
    }
}

QVariantMap Keychain::registerKey(const QString& app, const QVariantMap& in)
{
    if (app.isEmpty()) return fail("bad_request");
    forgetOtherSource(app);

    const QString keyId = in.value(QStringLiteral("keyId")).toString();
    const QString name = cleanName(in.value(QStringLiteral("name")));
    const QString algorithm = in.value(QStringLiteral("algorithm")).toString();
    QStringList purposes;
    if (in.value(QStringLiteral("keyId")).typeId() != QMetaType::QString || !validKeyId(keyId)
        || name.isEmpty() || !algorithms().contains(algorithm)
        || !cleanPurposes(in.value(QStringLiteral("purposes")), &purposes))
        return fail("bad_request");

    bool decoded = false;
    const QByteArray publicKey = decodeBase64(in.value(QStringLiteral("publicKey")), &decoded);
    if (!decoded || !validPublicKey(algorithm, publicKey))
        return fail("bad_key");

    if (indexOf(app, keyId) >= 0) return fail("duplicate");
    if (count(app) >= kMaxKeysPerApp) return fail("too_many");

    Key k;
    k.ownerApp = app;
    k.ownerSource = sourceFor(app);
    k.ownerName = m_nameFor ? m_nameFor(app) : QString();
    if (k.ownerName.isEmpty()) k.ownerName = app;
    k.keyId = keyId;
    k.name = name;
    k.algorithm = algorithm;
    k.publicKey = publicKey;
    k.purposes = purposes;
    k.added = now();
    m_keys.append(k);
    qInfo().noquote() << "Keychain:" << app << "registered key" << keyId << algorithm;
    changed();
    return ok();
}

QVariantMap Keychain::updateKey(const QString& app, const QString& keyId, const QVariantMap& changes)
{
    forgetOtherSource(app);
    const int i = indexOf(app, keyId);
    if (app.isEmpty() || i < 0) return fail("not_found");
    if (changes.contains(QStringLiteral("publicKey")) || changes.contains(QStringLiteral("algorithm"))
        || changes.contains(QStringLiteral("label")) || changes.contains(QStringLiteral("note")))
        return fail("bad_request");

    Key k = m_keys.at(i);
    if (changes.contains(QStringLiteral("name"))) {
        k.name = cleanName(changes.value(QStringLiteral("name")));
        if (k.name.isEmpty()) return fail("bad_request");
    }
    if (changes.contains(QStringLiteral("purposes"))
        && !cleanPurposes(changes.value(QStringLiteral("purposes")), &k.purposes))
        return fail("bad_request");
    if (m_nameFor && !m_nameFor(app).isEmpty()) k.ownerName = m_nameFor(app);

    m_keys[i] = k;
    changed();
    return ok();
}

QVariantMap Keychain::removeKey(const QString& app, const QString& keyId)
{
    forgetOtherSource(app);
    const int i = indexOf(app, keyId);
    if (app.isEmpty() || i < 0) return fail("not_found");
    m_keys.removeAt(i);
    qInfo().noquote() << "Keychain:" << app << "removed key" << keyId;
    changed();
    return ok();
}

QVariantList Keychain::myKeys(const QString& app) const
{
    const QString source = sourceFor(app);
    QVariantList out;
    for (const Key& k : m_keys) {
        if (k.ownerApp != app) continue;
        if (!source.isEmpty() && !k.ownerSource.isEmpty() && k.ownerSource != source) continue;
        out.append(QVariantMap{
            { QStringLiteral("keyId"), k.keyId },
            { QStringLiteral("name"), k.name },
            { QStringLiteral("publicKey"), QString::fromLatin1(k.publicKey.toBase64()) },
            { QStringLiteral("algorithm"), k.algorithm },
            { QStringLiteral("purposes"), toList(k.purposes) },
            { QStringLiteral("added"), k.added },
        });
    }
    return out;
}

const QStringList& Keychain::reservedNames()
{
    static const QStringList names = { QStringLiteral("Keychain"), QStringLiteral("Basecamp") };
    return names;
}

namespace {

// Letters and digits only, case folded: what two names are compared on.
QString nameKey(const QString& name)
{
    QString out;
    for (const QChar c : name)
        if (c.isLetterOrNumber()) out.append(c);
    return out.toCaseFolded();
}

} // namespace

QString Keychain::shownName(const QString& app, const QString& displayName, const QStringList& taken)
{
    const QString name = displayName.trimmed();
    const QString key = nameKey(name);
    if (key.isEmpty()) return app;
    for (const QString& t : reservedNames())
        if (nameKey(t) == key) return app;
    for (const QString& t : taken)
        if (nameKey(t) == key) return app;
    return name;
}

QString Keychain::shownName(const QString& app, const QString& displayName) const
{
    return shownName(app, displayName, m_takenFor ? m_takenFor(app) : QStringList());
}

QVariantList Keychain::allKeys() const
{
    // Grouped by owner, owners in the order their first key arrived.
    QStringList owners;
    for (const Key& k : m_keys)
        if (!owners.contains(k.ownerApp)) owners.append(k.ownerApp);
    QVariantList out;
    for (const QString& owner : owners) {
        for (const Key& k : m_keys) {
            if (k.ownerApp != owner) continue;
            out.append(QVariantMap{
                { QStringLiteral("ownerApp"), k.ownerApp },
                { QStringLiteral("ownerName"), shownName(k.ownerApp, k.ownerName) },
                { QStringLiteral("keyId"), k.keyId },
                { QStringLiteral("name"), k.name },
                { QStringLiteral("label"), k.label },
                { QStringLiteral("note"), k.note },
                { QStringLiteral("publicKey"), QString::fromLatin1(k.publicKey.toBase64()) },
                { QStringLiteral("fingerprint"), fingerprint(k.publicKey) },
                { QStringLiteral("algorithm"), k.algorithm },
                { QStringLiteral("purposes"), toList(k.purposes) },
                { QStringLiteral("added"), k.added },
            });
        }
    }
    return out;
}

bool Keychain::setKeyLabel(const QString& ownerApp, const QString& keyId, const QString& label)
{
    const int i = indexOf(ownerApp, keyId);
    const QString clean = label.trimmed();
    if (i < 0 || clean.size() > kMaxLabel || !oneLine(clean)) return false;
    if (m_keys.at(i).label == clean) return true;
    m_keys[i].label = clean;
    changed();
    return true;
}

bool Keychain::setKeyNote(const QString& ownerApp, const QString& keyId, const QString& note)
{
    const int i = indexOf(ownerApp, keyId);
    const QString clean = note.trimmed();
    if (i < 0 || clean.size() > kMaxNote || !noteText(clean)) return false;
    if (m_keys.at(i).note == clean) return true;
    m_keys[i].note = clean;
    changed();
    return true;
}

bool Keychain::removeEntry(const QString& ownerApp, const QString& keyId)
{
    const int i = indexOf(ownerApp, keyId);
    if (i < 0) return false;
    m_keys.removeAt(i);
    qInfo().noquote() << "Keychain: the user removed" << ownerApp << "key" << keyId;
    changed();
    return true;
}

const Keychain::Key* Keychain::find(const QString& ownerApp, const QString& keyId) const
{
    const int i = indexOf(ownerApp, keyId);
    return i < 0 ? nullptr : &m_keys.at(i);
}

void Keychain::removeApp(const QString& app)
{
    const qsizetype before = m_keys.size();
    m_keys.removeIf([&](const Key& k) { return k.ownerApp == app; });
    if (m_keys.size() == before) return;
    qInfo().noquote() << "Keychain:" << app << "uninstalled, removed" << (before - m_keys.size()) << "key(s)";
    changed();
}

void Keychain::changed()
{
    save();
    emit keysChanged();
}

void Keychain::load()
{
    if (m_path.isEmpty()) return;
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly)) return;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    const QJsonArray keys = doc.object().value(QStringLiteral("keys")).toArray();
    for (const QJsonValue& v : keys) {
        const QJsonObject o = v.toObject();
        Key k;
        k.ownerApp = o.value(QStringLiteral("ownerApp")).toString();
        k.ownerSource = o.value(QStringLiteral("ownerSource")).toString();
        k.ownerName = o.value(QStringLiteral("ownerName")).toString();
        k.keyId = o.value(QStringLiteral("keyId")).toString();
        k.name = o.value(QStringLiteral("name")).toString();
        k.algorithm = o.value(QStringLiteral("algorithm")).toString();
        k.publicKey = QByteArray::fromBase64(o.value(QStringLiteral("publicKey")).toString().toLatin1());
        for (const QJsonValue& p : o.value(QStringLiteral("purposes")).toArray())
            if (purposeNames().contains(p.toString())) k.purposes.append(p.toString());
        k.added = static_cast<qint64>(o.value(QStringLiteral("added")).toDouble());
        k.label = o.value(QStringLiteral("label")).toString().left(kMaxLabel);
        k.note = o.value(QStringLiteral("note")).toString().left(kMaxNote);
        // The same checks a registration passes: a hand-edited or damaged
        // file never puts a key the broker could not verify against in front
        // of the user.
        if (k.ownerApp.isEmpty() || !validKeyId(k.keyId) || k.name.isEmpty()
            || !algorithms().contains(k.algorithm) || !validPublicKey(k.algorithm, k.publicKey)
            || indexOf(k.ownerApp, k.keyId) >= 0 || count(k.ownerApp) >= kMaxKeysPerApp) {
            qWarning().noquote() << "Keychain: skipped a malformed entry in" << m_path;
            continue;
        }
        if (k.ownerName.isEmpty()) k.ownerName = k.ownerApp;
        m_keys.append(k);
    }
}

bool Keychain::save() const
{
    if (m_path.isEmpty()) return true;
    QJsonArray keys;
    for (const Key& k : m_keys) {
        keys.append(QJsonObject{
            { QStringLiteral("ownerApp"), k.ownerApp },
            { QStringLiteral("ownerSource"), k.ownerSource },
            { QStringLiteral("ownerName"), k.ownerName },
            { QStringLiteral("keyId"), k.keyId },
            { QStringLiteral("name"), k.name },
            { QStringLiteral("algorithm"), k.algorithm },
            { QStringLiteral("publicKey"), QString::fromLatin1(k.publicKey.toBase64()) },
            { QStringLiteral("purposes"), QJsonArray::fromStringList(k.purposes) },
            { QStringLiteral("added"), static_cast<double>(k.added) },
            { QStringLiteral("label"), k.label },
            { QStringLiteral("note"), k.note },
        });
    }
    const QJsonObject root{ { QStringLiteral("version"), 1 }, { QStringLiteral("keys"), keys } };

    // QSaveFile: a crash mid-write leaves the old file, not half a new one.
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly)
        || file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) < 0
        || !file.commit()) {
        qWarning() << "Keychain: failed to write" << m_path << file.errorString();
        return false;
    }
    return true;
}
