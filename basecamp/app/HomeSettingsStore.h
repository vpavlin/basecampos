#pragma once

#include <QString>
#include <QVariantMap>

// What the app filling the `home` role keeps of its own across restarts: its
// widgets and their layout on the grid (basecamp-experiments#51). The shell
// stores it for the app (ShellApi::saveHomeSettings), since a sandboxed role
// app has no storage of its own. Opaque to the host: one JSON object per
// app, so another app given the role later starts from nothing and never
// reads this one's.
//
// One file (home-settings.json under LogosBasecampPaths::baseDirectory(), so
// --user-dir and the portable/dev split apply), every app's settings in it,
// read again on each load: two windows of the same profile see each other's
// last save.
class HomeSettingsStore {
public:
    // Larger settings are refused (a few widgets and four layouts are ~5 KB).
    static constexpr int kMaxBytes = 64 * 1024;

    explicit HomeSettingsStore(QString filePath);

    // `app`'s settings, {} when none were saved or the file is unreadable.
    QVariantMap load(const QString& app) const;
    enum class Result { Refused, Unchanged, Saved };
    // Replaces `app`'s settings (as JSON: what does not convert is dropped).
    // Refused with no app name, over kMaxBytes, or when the file could not
    // be written; Unchanged when the same settings were on disk already.
    Result save(const QString& app, const QVariantMap& settings);

private:
    QString m_path;
};
