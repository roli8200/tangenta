#include "Icons.h"

#include <QFile>
#include <QHash>
#include <QIconEngine>
#include <QPainter>
#include <QPixmapCache>
#include <QSvgRenderer>

#include <cmath>

namespace vek {

namespace {

// Ausgegraut: Graustufen mit verminderter Deckkraft
QImage disabled(QImage img)
{
    img = img.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < img.height(); ++y) {
        auto* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            const int gray = qGray(line[x]);
            const int light = 110 + gray * 120 / 255;
            line[x] = qRgba(light, light, light, qAlpha(line[x]) * 55 / 100);
        }
    }
    return img;
}

// Rendert das SVG erst, wenn eine Grösse gebraucht wird, und zwar in genau den
// Bildschirmpixeln (Grösse × Skalierung). So bleiben die Symbole bei jeder Skalierung
// scharf, auch bei 125 %, 150 % oder 175 %.
class SvgIconEngine final : public QIconEngine {
public:
    SvgIconEngine(QString name, QByteArray svg) : m_name(std::move(name)), m_svg(std::move(svg)) {}

    QIconEngine* clone() const override { return new SvgIconEngine(m_name, m_svg); }
    QString key() const override { return QStringLiteral("vek-svg"); }
    QSize actualSize(const QSize& size, QIcon::Mode, QIcon::State) override { return size; }
    bool isNull() override { return m_svg.isEmpty(); }

    void paint(QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State state) override
    {
        const qreal scale = painter->device() ? painter->device()->devicePixelRatioF() : 1.0;
        painter->drawPixmap(rect, scaledPixmap(rect.size(), mode, state, scale));
    }

    QPixmap pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state) override
    {
        return scaledPixmap(size, mode, state, 1.0);
    }

    QPixmap scaledPixmap(const QSize& size, QIcon::Mode mode, QIcon::State, qreal scale) override
    {
        const QSize pixels(int(std::ceil(size.width() * scale)), int(std::ceil(size.height() * scale)));
        const QString cacheKey = QStringLiteral("vek-icon/%1/%2x%3/%4")
                                     .arg(m_name)
                                     .arg(pixels.width())
                                     .arg(pixels.height())
                                     .arg(mode == QIcon::Disabled ? 1 : 0);
        QPixmap pm;
        if (!QPixmapCache::find(cacheKey, &pm)) {
            QImage img(pixels, QImage::Format_ARGB32_Premultiplied);
            img.fill(Qt::transparent);
            QPainter p(&img);
            QSvgRenderer(m_svg).render(&p, QRectF(QPointF(), QSizeF(pixels)));
            p.end();
            pm = QPixmap::fromImage(mode == QIcon::Disabled ? disabled(img) : img);
            QPixmapCache::insert(cacheKey, pm);
        }
        pm.setDevicePixelRatio(scale);
        return pm;
    }

private:
    QString m_name;
    QByteArray m_svg;
};

} // namespace

QIcon icon(const QString& name)
{
    static QHash<QString, QIcon> cache;
    if (auto it = cache.constFind(name); it != cache.constEnd())
        return *it;

    QFile file(QStringLiteral(":/icons/%1.svg").arg(name));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QIcon result(new SvgIconEngine(name, file.readAll()));
    cache.insert(name, result);
    return result;
}

} // namespace vek
