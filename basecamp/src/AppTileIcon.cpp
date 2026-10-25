#include "AppTileIcon.h"

#include <QApplication>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QUrl>

#include <functional>

namespace {
// Drawn once, large; QIcon scales it down for each place.
constexpr int kTileArt = 256;
constexpr qreal kTileRadiusRatio = 16.0 / 80.0;

QPixmap roundedTile(const std::function<void(QPainter&, const QRect&)>& paint)
{
    const QRect r(0, 0, kTileArt, kTileArt);
    QImage content(r.size(), QImage::Format_ARGB32_Premultiplied);
    content.fill(Qt::transparent);
    {
        QPainter p(&content);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        paint(p, r);
    }
    // The rounded square first, then the content kept only where it is
    // (SourceIn). A composition mode acts only under what is drawn, so masking
    // afterwards with DestinationIn would leave the corners untouched.
    QImage out(r.size(), QImage::Format_ARGB32_Premultiplied);
    out.fill(Qt::transparent);
    QPainter p(&out);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::black);
    const qreal radius = kTileArt * kTileRadiusRatio;
    p.drawRoundedRect(QRectF(r), radius, radius);
    p.setCompositionMode(QPainter::CompositionMode_SourceIn);
    p.drawImage(r, content);
    p.end();
    return QPixmap::fromImage(out);
}
} // namespace

QIcon appTileIcon(const QString& path, const QString& title, const QColor& plate, const QColor& text)
{
    if (path.isEmpty()) {
        return QIcon(roundedTile([&](QPainter& p, const QRect& r) {
            p.fillRect(r, plate);
            QFont f = QApplication::font();
            f.setPixelSize(qRound(kTileArt * 0.375));
            f.setBold(true);
            p.setFont(f);
            p.setPen(text);
            p.drawText(r, Qt::AlignCenter, title.left(2).toUpper());
        }));
    }
    QString file = path;
    if (file.startsWith(QStringLiteral("file:")))
        file = QUrl(file).toLocalFile();
    else if (file.startsWith(QStringLiteral("qrc:")))
        file = file.mid(3);
    const QPixmap art = QIcon(file).pixmap(QSize(kTileArt, kTileArt));
    if (art.isNull())
        return QIcon();
    return QIcon(roundedTile([&art](QPainter& p, const QRect& r) { p.drawPixmap(r, art); }));
}
