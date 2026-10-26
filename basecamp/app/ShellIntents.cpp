#include "ShellIntents.h"

#include "IntentRegistry.h"

namespace ShellIntents {

void registerWith(IntentRegistry* registry,
                  const QString& shellModuleName,
                  const QString& displayName,
                  const QString& iconSource)
{
    if (!registry) return;

    registry->registerShellUses(shellModuleName,
                                { QStringLiteral("packages.show"),
                                  QStringLiteral("packages.install"),
                                  kObjectsActionIntent,
                                  kKeychainSignerIntent,
                                  kKeychainEndedIntent });

    QStringList provides = kNavigationIntents;
    provides += kPackageConfirmIntents;
    provides << kNotificationsPostIntent;
    provides << kObjectsPublishIntent << kObjectsRetractIntent;
    provides += kKeychainIntents;
    provides += kAppDataIntents;

    registry->registerShellProvider(shellModuleName, provides,
                                    kNavigationIntents, displayName, iconSource);

    for (const QString& destructive : kRestrictedToPackageManagerUi)
        registry->restrictIntentToRole(destructive, kPackagesRole);

    // `objects.action` is raised by the shell alone. The spec's name for it
    // was `basecamp.objects.action`, shell-only by construction because the
    // shell owns that namespace; renamed so that apps may PROVIDE it, it lost
    // that guarantee on the requester side. Without this an app declaring
    // `uses: objects.action` could send a publisher {key, action: "join"} or
    // a forged reply text through an ordinary request, one chooser row away
    // from looking exactly like an action the user raised from a view.
    // submitTo only stops other apps from NAMING the provider; this stops
    // them asking at all, answered `unavailable` like any denial.
    registry->restrictIntentToRequesters(kObjectsActionIntent, { shellModuleName });

    // The same for the signer's two: an app asking a signer `keychain.sign`
    // itself would put its own payload and words in front of the user as if
    // they came through Keychain, past its key choice and its check.
    registry->restrictIntentToRequesters(kKeychainSignerIntent, { shellModuleName });
    registry->restrictIntentToRequesters(kKeychainEndedIntent, { shellModuleName });
}

} // namespace ShellIntents
