#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QVariantMap>

// Data the host keeps for one app, that only that app can read
// (basecamp-experiments#60). Made for what QML LocalStorage cannot keep
// apart: every ui_qml view runs in the host process, and Qt hands an already
// open LocalStorage connection to any engine asking for the same database
// name, so one app can read another's database (tst_qml_sandbox
// localStorageInTheAppsFolder). The Stub Signer keeps its test keys here.
//
// One JSON object per app, app_data/<app>/appdata.json in the profile, owner
// read/write only, rewritten whole through QSaveFile. The app is always the
// caller the intent broker attested (`basecamp.appdata.read` / `.write`,
// MainUIBackend); nothing in the arguments names it, so an app can only ever
// reach its own. An empty base directory keeps everything in memory (tests).
//
// QtCore only, so app_data_store_test.cpp drives it without the host.
class AppDataStore : public QObject {
    Q_OBJECT
public:
    // The whole object, as compact JSON.
    static constexpr int kMaxBytes = 64 * 1024;

    explicit AppDataStore(QString baseDir = QString(), QObject* parent = nullptr);

    // { ok: true, data } (data {} when nothing was written), or { ok: false,
    // error: bad_request } for a name that is not a plain folder name.
    QVariantMap read(const QString& app);
    // `data` must be an object; it replaces what was there. { ok: true } or
    // { ok: false, error }: bad_request, too_big, failed (not written).
    QVariantMap write(const QString& app, const QVariant& data);
    // The app was uninstalled: its data goes.
    void removeApp(const QString& app);

private:
    QString fileFor(const QString& app) const;

    QString m_base;
    QHash<QString, QVariantMap> m_cache;   // the memory store, and a read cache
};
