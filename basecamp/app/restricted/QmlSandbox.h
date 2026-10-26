#pragma once

#include <QString>
#include <QStringList>

class QQmlEngine;

// The ui_qml sandbox policy, factored out of PluginLoader so it can be unit
// tested against a bare QQmlEngine (it depends only on Qt, never on the SDK,
// liblogos or the rest of basecamp).
namespace QmlSandbox {

// Applies the sandbox to a freshly-created QML engine:
//   * import paths  — module dir(s) first, then the vetted appLibDir, then Qt's
//                     defaults, so the module's own QML/JS resolves;
//   * a deny-all QQmlNetworkAccessManagerFactory (no network);
//   * a RestrictedUrlInterceptor confining file/qmldir resolution to the
//     module's dir + the vetted/Qt roots, forbidding a qmldir under the
//     module's (untrusted) dir from declaring a native C++ plugin, and forcing
//     `import Logos.<Reserved>` to always resolve from the vetted appLibDir.
//
// Deliberately does NOT add the untrusted installDir to the engine's native
// plugin search path, and keeps Qt's default pluginPathList intact.
//
// `appLibDir` is the vetted application library dir (in production
// <appDir>/../lib); tests pass an explicit value (often empty). `pluginLabel`
// is an optional identifier (e.g. the module name) prefixed onto every sandbox
// diagnostic — omit in tests. Returns the set of import roots that were treated
// as untrusted — useful for assertions.
//
// `storageDir` is the app's own data folder: the engine's offlineStoragePath,
// so QtQuick.LocalStorage database files land there. Qt's default is one
// folder for every engine, outside the profile. Not a wall inside one
// process: Qt hands an already open connection to any engine asking for the
// same database name (tst_qml_sandbox localStorageInTheAppsFolder), so
// nothing secret belongs in LocalStorage. An app's secrets go through
// `basecamp.appdata.*` (AppDataStore), which the host keys by the attested
// caller; the Stub Signer keeps its test keys there. Empty leaves the engine
// as it is.
QStringList configure(QQmlEngine* engine,
                      const QString& installDir,
                      const QString& qmlViewPath,
                      const QString& appLibDir,
                      const QString& pluginLabel = QString(),
                      const QString& storageDir = QString());

} // namespace QmlSandbox
