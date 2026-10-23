// srcdeps: IntentRegistry.cpp ShellRoles.cpp
//
// Shell roles (SHELL-ROLES-SPEC.md, ADR-067): an app CLAIMS a role in its
// metadata.json, the USER assigns it, and only the current holder passes a
// role-restricted intent. The security-relevant rules are tested here: a claim
// alone grants nothing, a vanished holder falls back, and nothing fails open.

#include "IntentRegistry.h"
#include "ShellRoles.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

namespace {

QString makeApp(QTemporaryDir& root, const QString& name, const QByteArray& json)
{
    const QString dir = root.filePath(name);
    QDir().mkpath(dir);
    QFile file(QDir(dir).filePath(QStringLiteral("metadata.json")));
    file.open(QIODevice::WriteOnly);
    file.write(json);
    file.close();
    return dir;
}

QVariantMap plugin(const QString& installDir)
{
    return { { QStringLiteral("installDir"), installDir },
             { QStringLiteral("type"), QStringLiteral("ui_qml") } };
}

const QString kUninstall = QStringLiteral("basecamp.packages.confirm_uninstall");

} // namespace

class TestShellRoles : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        // The build sandbox has no writable home: keep settings in a temp dir.
        QVERIFY(m_settingsDir.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settingsDir.path());
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("ShellRolesTest"));
        QCoreApplication::setApplicationName(QStringLiteral("ShellRolesTest"));
    }
    void init() { QSettings().clear(); }

    void testRolesAreReadFromMetadata();
    void testDefaultHoldsRoleWithoutClaim();
    void testClaimAloneGrantsNothing();
    void testAssignMovesTheGrant();
    void testCannotAssignNonCandidate();
    void testUninstalledHolderFallsBack();
    void testMissingDefaultGivesNoHolder();
    void testRoleRestrictionFailsClosed();

private:
    QTemporaryDir m_settingsDir;
};

void TestShellRoles::testRolesAreReadFromMetadata()
{
    QTemporaryDir root;
    const QString dir = makeApp(root, QStringLiteral("alt_pm"), R"({
        "name": "alt_pm", "type": "ui_qml",
        "roles": ["packages", "launcher", "wizard", 7]
    })");
    IntentRegistry registry;
    registry.rebuild({ { QStringLiteral("alt_pm"), plugin(dir) } }, nullptr, nullptr);

    QVERIFY(registry.declaresRole(QStringLiteral("alt_pm"), QStringLiteral("packages")));
    QVERIFY(registry.declaresRole(QStringLiteral("alt_pm"), QStringLiteral("launcher")));
    QVERIFY(!registry.declaresRole(QStringLiteral("alt_pm"), QStringLiteral("wizard")));
    QCOMPARE(registry.modulesDeclaringRole(QStringLiteral("packages")),
             QStringList{ QStringLiteral("alt_pm") });
    // Unknown roles are reported, not silently dropped.
    QVERIFY(registry.diagnostics().join('\n').contains(QStringLiteral("wizard")));
}

void TestShellRoles::testDefaultHoldsRoleWithoutClaim()
{
    IntentRegistry registry;
    ShellRoles roles(&registry, [](const QString&) { return true; },
                     { { QStringLiteral("packages"), QStringLiteral("package_manager_ui") } });
    QCOMPARE(roles.holder(QStringLiteral("packages")), QStringLiteral("package_manager_ui"));
    QCOMPARE(roles.holder(QStringLiteral("launcher")), QString());
    QCOMPARE(roles.holder(QStringLiteral("nonsense")), QString());
}

void TestShellRoles::testClaimAloneGrantsNothing()
{
    QTemporaryDir root;
    const QString dir = makeApp(root, QStringLiteral("alt_pm"),
                                R"({"name":"alt_pm","type":"ui_qml","roles":["packages"]})");
    IntentRegistry registry;
    registry.rebuild({ { QStringLiteral("alt_pm"), plugin(dir) } }, nullptr, nullptr);
    ShellRoles roles(&registry, [](const QString&) { return true; },
                     { { QStringLiteral("packages"), QStringLiteral("package_manager_ui") } });
    registry.setRoleHolderResolver([&roles](const QString& r) { return roles.holder(r); });
    registry.restrictIntentToRole(kUninstall, QStringLiteral("packages"));

    // Installing an app that claims the role does not take it.
    QCOMPARE(roles.holder(QStringLiteral("packages")), QStringLiteral("package_manager_ui"));
    QVERIFY(!registry.requesterAllowed(kUninstall, QStringLiteral("alt_pm")));
    QVERIFY(registry.requesterAllowed(kUninstall, QStringLiteral("package_manager_ui")));
    QCOMPARE(roles.candidates(QStringLiteral("packages")),
             (QStringList{ QStringLiteral("package_manager_ui"), QStringLiteral("alt_pm") }));
}

void TestShellRoles::testAssignMovesTheGrant()
{
    QTemporaryDir root;
    const QString dir = makeApp(root, QStringLiteral("alt_pm"),
                                R"({"name":"alt_pm","type":"ui_qml","roles":["packages"]})");
    IntentRegistry registry;
    registry.rebuild({ { QStringLiteral("alt_pm"), plugin(dir) } }, nullptr, nullptr);
    ShellRoles roles(&registry, [](const QString&) { return true; },
                     { { QStringLiteral("packages"), QStringLiteral("package_manager_ui") } });
    registry.setRoleHolderResolver([&roles](const QString& r) { return roles.holder(r); });
    registry.restrictIntentToRole(kUninstall, QStringLiteral("packages"));

    int changes = 0;
    connect(&roles, &ShellRoles::changed, this, [&changes]() { ++changes; });
    QVERIFY(roles.assign(QStringLiteral("packages"), QStringLiteral("alt_pm")));
    QCOMPARE(changes, 1);
    QVERIFY(registry.requesterAllowed(kUninstall, QStringLiteral("alt_pm")));
    QVERIFY(!registry.requesterAllowed(kUninstall, QStringLiteral("package_manager_ui")));

    // Persisted: a fresh store reads the same choice.
    ShellRoles again(&registry, [](const QString&) { return true; },
                     { { QStringLiteral("packages"), QStringLiteral("package_manager_ui") } });
    QCOMPARE(again.holder(QStringLiteral("packages")), QStringLiteral("alt_pm"));

    roles.reset(QStringLiteral("packages"));
    QCOMPARE(roles.holder(QStringLiteral("packages")), QStringLiteral("package_manager_ui"));
}

void TestShellRoles::testCannotAssignNonCandidate()
{
    IntentRegistry registry;
    ShellRoles roles(&registry, [](const QString&) { return true; },
                     { { QStringLiteral("packages"), QStringLiteral("package_manager_ui") } });
    QVERIFY(!roles.assign(QStringLiteral("packages"), QStringLiteral("evil_ui")));
    QVERIFY(!roles.assign(QStringLiteral("nonsense"), QStringLiteral("package_manager_ui")));
    QCOMPARE(roles.holder(QStringLiteral("packages")), QStringLiteral("package_manager_ui"));
}

void TestShellRoles::testUninstalledHolderFallsBack()
{
    QTemporaryDir root;
    const QString dir = makeApp(root, QStringLiteral("alt_pm"),
                                R"({"name":"alt_pm","type":"ui_qml","roles":["packages"]})");
    IntentRegistry registry;
    registry.rebuild({ { QStringLiteral("alt_pm"), plugin(dir) } }, nullptr, nullptr);
    QSet<QString> installed{ QStringLiteral("alt_pm"), QStringLiteral("package_manager_ui") };
    ShellRoles roles(&registry, [&installed](const QString& n) { return installed.contains(n); },
                     { { QStringLiteral("packages"), QStringLiteral("package_manager_ui") } });
    QVERIFY(roles.assign(QStringLiteral("packages"), QStringLiteral("alt_pm")));

    installed.remove(QStringLiteral("alt_pm"));
    QCOMPARE(roles.holder(QStringLiteral("packages")), QStringLiteral("package_manager_ui"));
}

void TestShellRoles::testMissingDefaultGivesNoHolder()
{
    IntentRegistry registry;
    ShellRoles roles(&registry, [](const QString&) { return false; },
                     { { QStringLiteral("packages"), QStringLiteral("package_manager_ui") } });
    QCOMPARE(roles.holder(QStringLiteral("packages")), QString());
    QVERIFY(roles.candidates(QStringLiteral("packages")).isEmpty());
}

void TestShellRoles::testRoleRestrictionFailsClosed()
{
    IntentRegistry registry;
    registry.restrictIntentToRole(kUninstall, QStringLiteral("packages"));

    // No resolver: nobody.
    QVERIFY(!registry.requesterAllowed(kUninstall, QStringLiteral("package_manager_ui")));

    // A resolver with no holder: nobody, and an empty requester never matches.
    registry.setRoleHolderResolver([](const QString&) { return QString(); });
    QVERIFY(!registry.requesterAllowed(kUninstall, QString()));
    QVERIFY(!registry.requesterAllowed(kUninstall, QStringLiteral("package_manager_ui")));

    // Restricting to an unknown role is refused and reported.
    registry.restrictIntentToRole(QStringLiteral("basecamp.x.y"), QStringLiteral("wizard"));
    QVERIFY(registry.requesterAllowed(QStringLiteral("basecamp.x.y"), QStringLiteral("anyone")));
    QVERIFY(registry.diagnostics().join('\n').contains(QStringLiteral("wizard")));

    // Siblings untouched.
    QVERIFY(registry.requesterAllowed(QStringLiteral("basecamp.packages.confirm_install"),
                                      QStringLiteral("evil_ui")));
}

QTEST_GUILESS_MAIN(TestShellRoles)
#include "shell_roles_test.moc"
