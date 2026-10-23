// srcdeps: restricted/RestrictedUrlInterceptor.cpp restricted/SandboxLogging.cpp
//
// The ui_qml sandbox lets an image:// URL through only for an image provider the
// host registered on that engine (ADR-067: the settings role's app is served
// other apps' icons that way). Everything else with the image: scheme, and any
// other use of an allowed id, stays blocked.

#include "restricted/RestrictedUrlInterceptor.h"

#include <QTest>
#include <QUrl>

class TestSandboxImageProvider : public QObject {
    Q_OBJECT
private slots:
    void blockedByDefault()
    {
        RestrictedUrlInterceptor sandbox({});
        QVERIFY(sandbox.intercept(QUrl(QStringLiteral("image://basecamp/appicon/chat_ui")),
                                  QQmlAbstractUrlInterceptor::UrlString).isEmpty());
    }

    void allowedOnlyForRegisteredIdAndOnlyAsImage()
    {
        RestrictedUrlInterceptor sandbox({});
        sandbox.allowImageProvider(QStringLiteral("basecamp"));
        const QUrl icon(QStringLiteral("image://basecamp/appicon/chat_ui"));
        QCOMPARE(sandbox.intercept(icon, QQmlAbstractUrlInterceptor::UrlString), icon);
        // Another provider id: blocked.
        QVERIFY(sandbox.intercept(QUrl(QStringLiteral("image://other/x")),
                                  QQmlAbstractUrlInterceptor::UrlString).isEmpty());
        // The allowed id used to load code: blocked.
        QVERIFY(sandbox.intercept(icon, QQmlAbstractUrlInterceptor::QmlFile).isEmpty());
        QVERIFY(sandbox.intercept(icon, QQmlAbstractUrlInterceptor::JavaScriptFile).isEmpty());
        // Network stays blocked.
        QVERIFY(sandbox.intercept(QUrl(QStringLiteral("https://basecamp/x.png")),
                                  QQmlAbstractUrlInterceptor::UrlString).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestSandboxImageProvider)
#include "sandbox_image_provider_test.moc"
