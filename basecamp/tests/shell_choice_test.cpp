// Unit tests for which shell Basecamp starts with (app/interfaces/
// ShellChoice.h): Basecampium ("Basecamp") is the default for a fresh
// profile, the host's built-in shell stays as "Basecamp Legacy".
//
// The load-bearing case is the difference between "no choice saved" (a fresh
// profile: the default shell) and "Legacy chosen" (stays Legacy), including
// the "" and "default" values older profiles wrote for the built-in shell.
//
// Run: nix build .#unit-tests -L

#include "ShellChoice.h"

#include <QtTest/QtTest>

class ShellChoiceTest : public QObject {
    Q_OBJECT

private slots:
    void freshProfileStartsTheDefaultShell()
    {
        QCOMPARE(ShellChoice::startShell(QString(), false, QString(), true), QStringLiteral("basecampium"));
    }

    void freshProfileWithoutTheDefaultShellStartsLegacy()
    {
        QCOMPARE(ShellChoice::startShell(QString(), false, QString(), false), QString());
    }

    void legacyChoiceStaysLegacy()
    {
        for (const QString& saved : { QStringLiteral("legacy"), QString(), QStringLiteral("default") })
            QCOMPARE(ShellChoice::startShell(QString(), true, saved, true), QString());
    }

    void savedPackageWins()
    {
        QCOMPARE(ShellChoice::startShell(QString(), true, QStringLiteral("other"), true), QStringLiteral("other"));
        QCOMPARE(ShellChoice::startShell(QString(), true, QStringLiteral("basecampium"), false),
                 QStringLiteral("basecampium"));
    }

    void environmentWins()
    {
        QCOMPARE(ShellChoice::startShell(QStringLiteral("other"), true, QStringLiteral("legacy"), true),
                 QStringLiteral("other"));
        QCOMPARE(ShellChoice::startShell(QStringLiteral("legacy"), false, QString(), true), QString());
        QCOMPARE(ShellChoice::startShell(QStringLiteral("default"), true, QStringLiteral("basecampium"), true),
                 QString());
    }

    void defaultShellGetsASecondStart()
    {
        // Nobody chose the default: one leftover marker is not enough.
        QCOMPARE(ShellChoice::strikesToFallBack(false), 2);
        // A shell someone picked falls back on the first one, as before.
        QCOMPARE(ShellChoice::strikesToFallBack(true), 1);
    }

    void displayNames()
    {
        QCOMPARE(ShellChoice::displayName(QStringLiteral("basecampium"), QStringLiteral("Basecampium")),
                 QStringLiteral("Basecamp"));
        QCOMPARE(ShellChoice::displayName(QString()), QStringLiteral("Basecamp Legacy"));
        QCOMPARE(ShellChoice::displayName(QStringLiteral("legacy")), QStringLiteral("Basecamp Legacy"));
        QCOMPARE(ShellChoice::displayName(QStringLiteral("other"), QStringLiteral("Other shell")),
                 QStringLiteral("Other shell"));
        QCOMPARE(ShellChoice::displayName(QStringLiteral("other")), QStringLiteral("other"));
    }
};

QTEST_GUILESS_MAIN(ShellChoiceTest)
#include "shell_choice_test.moc"
