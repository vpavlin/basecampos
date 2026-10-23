#include "ShellRoles.h"

#include "IntentRegistry.h"

#include <QSettings>

namespace {
QString settingsKey(const QString& role)
{
    return QStringLiteral("ShellRoles/") + role;
}
} // namespace

ShellRoles::ShellRoles(IntentRegistry* registry,
                       InstalledFn isInstalled,
                       QHash<QString, QString> defaults,
                       QObject* parent)
    : QObject(parent)
    , m_registry(registry)
    , m_isInstalled(std::move(isInstalled))
    , m_defaults(std::move(defaults))
{
    m_last = holders();
}

QString ShellRoles::assigned(const QString& role) const
{
    return QSettings().value(settingsKey(role)).toString();
}

QString ShellRoles::holder(const QString& role) const
{
    if (!IntentRegistry::knownRoles().contains(role))
        return {};

    const QString chosen = assigned(role);
    if (!chosen.isEmpty() && candidates(role).contains(chosen))
        return chosen;

    const QString fallback = m_defaults.value(role);
    if (!fallback.isEmpty() && m_isInstalled && m_isInstalled(fallback))
        return fallback;
    return {};
}

QStringList ShellRoles::candidates(const QString& role) const
{
    QStringList out;
    if (!IntentRegistry::knownRoles().contains(role))
        return out;

    const QString fallback = m_defaults.value(role);
    if (!fallback.isEmpty() && m_isInstalled && m_isInstalled(fallback))
        out.append(fallback);

    if (m_registry) {
        for (const QString& app : m_registry->modulesDeclaringRole(role))
            if (!out.contains(app) && m_isInstalled && m_isInstalled(app))
                out.append(app);
    }
    return out;
}

bool ShellRoles::assign(const QString& role, const QString& app)
{
    if (!candidates(role).contains(app))
        return false;
    {
        // Scoped: a QSettings write reaches other QSettings objects only once
        // it is synced, which the destructor does. holder() reads a fresh one.
        QSettings settings;
        if (app == m_defaults.value(role))
            settings.remove(settingsKey(role));   // the default is "no choice made"
        else
            settings.setValue(settingsKey(role), app);
    }
    reevaluate();
    return true;
}

void ShellRoles::reset(const QString& role)
{
    QSettings().remove(settingsKey(role));
    reevaluate();
}

QVariantMap ShellRoles::holders() const
{
    QVariantMap out;
    for (const QString& role : IntentRegistry::knownRoles())
        out.insert(role, holder(role));
    return out;
}

void ShellRoles::reevaluate()
{
    const QVariantMap now = holders();
    if (now == m_last)
        return;
    m_last = now;
    emit changed();
}
