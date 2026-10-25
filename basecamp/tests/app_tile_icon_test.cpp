// srcdeps: AppTileIcon.cpp
//
// The shell's widget chrome draws an app's tile the way the QML tile does
// (basecamp-experiments#40): icons are full-bleed squares and the rounding is
// done when drawing. Covered here: an opaque square icon comes out with
// transparent corners and opaque edges, an app with no icon gets its letters on
// the plate in the same shape, and a missing file gives a null icon.

#include "AppTileIcon.h"

#include <QImage>
#include <QTemporaryDir>
#include <QUrl>
#include <QtTest>

class TestAppTileIcon : public QObject {
    Q_OBJECT

private slots:
    void testSquareIconIsRoundedWhenDrawn();
    void testNoIconShowsLettersInTheSameShape();
    void testMissingFileIsNull();
    void testFileUrlIsRead();

private:
    static QImage drawn(const QIcon& icon, int size)
    {
        return icon.pixmap(QSize(size, size)).toImage().convertToFormat(QImage::Format_ARGB32);
    }
    QString squarePng()
    {
        const QString file = m_dir.filePath(QStringLiteral("square.png"));
        QImage square(256, 256, QImage::Format_ARGB32);
        square.fill(QColor(61, 139, 110));
        square.save(file);
        return file;
    }

    QTemporaryDir m_dir;
};

void TestAppTileIcon::testSquareIconIsRoundedWhenDrawn()
{
    const QImage img = drawn(appTileIcon(squarePng(), QStringLiteral("Stub Wallet"), Qt::black, Qt::white), 80);
    QCOMPARE(img.size(), QSize(80, 80));
    // The corners are cut away: the picture's own corners are opaque.
    QCOMPARE(qAlpha(img.pixel(0, 0)), 0);
    QCOMPARE(qAlpha(img.pixel(79, 0)), 0);
    QCOMPARE(qAlpha(img.pixel(0, 79)), 0);
    QCOMPARE(qAlpha(img.pixel(79, 79)), 0);
    // The middle of each edge and the centre stay the icon, opaque.
    QCOMPARE(qAlpha(img.pixel(40, 0)), 255);
    QCOMPARE(qAlpha(img.pixel(0, 40)), 255);
    QCOMPARE(qAlpha(img.pixel(40, 40)), 255);
    QCOMPARE(QColor(img.pixel(40, 40)), QColor(61, 139, 110));
}

void TestAppTileIcon::testNoIconShowsLettersInTheSameShape()
{
    const QImage img = drawn(appTileIcon(QString(), QStringLiteral("Stub Wallet"), QColor(35, 35, 35), Qt::white), 80);
    QCOMPARE(qAlpha(img.pixel(0, 0)), 0);
    QCOMPARE(qAlpha(img.pixel(79, 79)), 0);
    QCOMPARE(qAlpha(img.pixel(40, 0)), 255);
    QCOMPARE(QColor(img.pixel(4, 40)), QColor(35, 35, 35));
    // Letters: some pixel near the centre is the text colour's.
    bool letters = false;
    for (int x = 20; x < 60 && !letters; ++x)
        for (int y = 20; y < 60 && !letters; ++y)
            letters = qRed(img.pixel(x, y)) > 200;
    QVERIFY(letters);
}

void TestAppTileIcon::testMissingFileIsNull()
{
    QVERIFY(appTileIcon(m_dir.filePath(QStringLiteral("none.png")), QStringLiteral("X"), Qt::black, Qt::white).isNull());
}

void TestAppTileIcon::testFileUrlIsRead()
{
    // UIPluginManager hands out file: URLs with a cache-busting query.
    const QString url = QUrl::fromLocalFile(squarePng()).toString() + QStringLiteral("?v=1");
    const QImage img = drawn(appTileIcon(url, QStringLiteral("X"), Qt::black, Qt::white), 40);
    QCOMPARE(qAlpha(img.pixel(0, 0)), 0);
    QCOMPARE(qAlpha(img.pixel(20, 20)), 255);
}

QTEST_MAIN(TestAppTileIcon)
#include "app_tile_icon_test.moc"
