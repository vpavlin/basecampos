#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <functional>

class IntentRegistry;

// ── ShellRoles ───────────────────────────────────────────────────────────────
//
// Which app fills each of the shell's roles: home, launcher, packages,
// settings (docs SHELL-ROLES-SPEC.md, ADR-067).
//
// An app CLAIMS a role in its metadata.json (IntentRegistry reads that); the
// USER assigns it here. The bundled default for a role needs no claim: it is
// named in code. Holding a role is what the registry's role restrictions check,
// so the assignment is the grant.
//
// holder() never names an app that isn't installed: an assignment whose app
// is gone falls back to the default, and a missing default gives "" — the shell
// then shows its own fallback for that role rather than nothing.
//
// Same constraint as IntentRegistry: no logos headers, no widgets, so the rules
// are unit-testable.
class ShellRoles : public QObject {
    Q_OBJECT
public:
    using InstalledFn = std::function<bool(const QString& moduleName)>;

    ShellRoles(IntentRegistry* registry,
               InstalledFn isInstalled,
               QHash<QString, QString> defaults,
               QObject* parent = nullptr);

    QString holder(const QString& role) const;
    QString defaultFor(const QString& role) const { return m_defaults.value(role); }

    // The default (when installed) first, then every installed app claiming
    // the role, sorted. These are the only apps assign() accepts.
    QStringList candidates(const QString& role) const;

    // false (and nothing stored) when `app` is not a candidate.
    bool assign(const QString& role, const QString& app);
    void reset(const QString& role);

    // role -> current holder, for every known role.
    QVariantMap holders() const;

    // The registry or the installed set changed: holders may have too.
    void reevaluate();

signals:
    void changed();

private:
    QString assigned(const QString& role) const;

    IntentRegistry* m_registry;
    InstalledFn m_isInstalled;
    QHash<QString, QString> m_defaults;
    QVariantMap m_last;
};
