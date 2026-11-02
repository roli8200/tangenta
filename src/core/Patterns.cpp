#include "Patterns.h"

#include "ImagePool.h"

#include <QCache>
#include <QMutex>
#include <QObject>
#include <QPainter>
#include <QPainterPath>
#include <QStringList>

#include <cmath>
#include <numbers>

namespace vek {

namespace {

const QStringList& twoColorNames()
{
    static const QStringList names{QObject::tr("Schachbrett"), QObject::tr("Streifen waagrecht"), QObject::tr("Streifen senkrecht"),
                                   QObject::tr("Schräg /"),     QObject::tr("Schräg \\"),         QObject::tr("Gitter"),
                                   QObject::tr("Kreuzschraffur"), QObject::tr("Punkte"),         QObject::tr("Grosse Punkte"),
                                   QObject::tr("Ziegel"),       QObject::tr("Wellen"),            QObject::tr("Zickzack"),
                                   QObject::tr("Schuppen"),     QObject::tr("Rauten"),            QObject::tr("Dreiecke"),
                                   QObject::tr("Kreise")};
    return names;
}

const QStringList& textureNames()
{
    static const QStringList names{QObject::tr("Wolken"), QObject::tr("Marmor"), QObject::tr("Holz"),
                                   QObject::tr("Granit"), QObject::tr("Wasser"), QObject::tr("Leinwand")};
    return names;
}

// Zweifarbige Kachel 32 x 32: Vordergrund auf Hintergrund
QImage twoColorTile(int index, const QColor& fg, const QColor& bg)
{
    constexpr int n = 32;
    QImage img(n, n, QImage::Format_ARGB32_Premultiplied);
    img.fill(bg);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(fg);
    auto line = [&](QPointF a, QPointF b, double w) {
        p.setPen(QPen(fg, w, Qt::SolidLine, Qt::FlatCap));
        p.drawLine(a, b);
        p.setPen(Qt::NoPen);
    };
    switch (index) {
    case 0: p.drawRect(0, 0, 16, 16); p.drawRect(16, 16, 16, 16); break;
    case 1: p.drawRect(0, 0, n, 10); break;
    case 2: p.drawRect(0, 0, 10, n); break;
    case 3:
        for (int k = -1; k <= 1; ++k)
            line(QPointF(k * n, n), QPointF(k * n + n, 0), 6);
        break;
    case 4:
        for (int k = -1; k <= 1; ++k)
            line(QPointF(k * n, 0), QPointF(k * n + n, n), 6);
        break;
    case 5: p.drawRect(0, 0, n, 3); p.drawRect(0, 0, 3, n); break;
    case 6:
        for (int k = -1; k <= 1; ++k) {
            line(QPointF(k * n, n), QPointF(k * n + n, 0), 2.5);
            line(QPointF(k * n, 0), QPointF(k * n + n, n), 2.5);
        }
        break;
    case 7: p.drawEllipse(QPointF(8, 8), 3.5, 3.5); p.drawEllipse(QPointF(24, 24), 3.5, 3.5); break;
    case 8: p.drawEllipse(QPointF(16, 16), 11, 11); break;
    case 9:
        p.drawRect(0, 0, n, 2);
        p.drawRect(0, 16, n, 2);
        p.drawRect(0, 0, 2, 16);
        p.drawRect(16, 16, 2, 16);
        break;
    case 10: {
        QPainterPath w;
        for (int row : {8, 24}) {
            w.moveTo(0, row);
            w.cubicTo(8, row - 8, 8, row - 8, 16, row);
            w.cubicTo(24, row + 8, 24, row + 8, 32, row);
        }
        p.setPen(QPen(fg, 3));
        p.setBrush(Qt::NoBrush);
        p.drawPath(w);
        break;
    }
    case 11:
        p.setPen(QPen(fg, 3, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
        p.setBrush(Qt::NoBrush);
        p.drawPolyline(QPolygonF({QPointF(0, 20), QPointF(8, 12), QPointF(16, 20), QPointF(24, 12), QPointF(32, 20)}));
        break;
    case 12:
        p.setPen(QPen(fg, 2));
        p.setBrush(Qt::NoBrush);
        for (const QPointF& c : {QPointF(0, 0), QPointF(16, 16), QPointF(32, 0), QPointF(0, 32), QPointF(32, 32)})
            p.drawEllipse(c, 16, 16);
        break;
    case 13: p.drawPolygon(QPolygonF({QPointF(16, 2), QPointF(30, 16), QPointF(16, 30), QPointF(2, 16)})); break;
    case 14: p.drawPolygon(QPolygonF({QPointF(0, 32), QPointF(16, 0), QPointF(32, 32)})); break;
    case 15:
        p.setPen(QPen(fg, 3));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPointF(16, 16), 10, 10);
        break;
    default: break;
    }
    return img;
}

// Wertrauschen auf einem kachelbaren Gitter
double hash(int x, int y, quint32 seed)
{
    quint32 h = quint32(x) * 374761393u + quint32(y) * 668265263u + seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return double((h ^ (h >> 16)) & 0xffff) / 65535.0;
}

double noise(double x, double y, int period, quint32 seed)
{
    const int x0 = int(std::floor(x)), y0 = int(std::floor(y));
    const double fx = x - x0, fy = y - y0;
    auto at = [&](int ix, int iy) { return hash(((ix % period) + period) % period, ((iy % period) + period) % period, seed); };
    const double sx = fx * fx * (3 - 2 * fx), sy = fy * fy * (3 - 2 * fy);
    const double a = at(x0, y0) + (at(x0 + 1, y0) - at(x0, y0)) * sx;
    const double b = at(x0, y0 + 1) + (at(x0 + 1, y0 + 1) - at(x0, y0 + 1)) * sx;
    return a + (b - a) * sy;
}

// Fraktales Rauschen 0..1, u/v in 0..1 (eine Kachel)
double fbm(double u, double v, int base, quint32 seed, int octaves = 5)
{
    double sum = 0, amp = 0.5, norm = 0;
    int period = base;
    for (int o = 0; o < octaves; ++o) {
        sum += amp * noise(u * period, v * period, period, seed + quint32(o) * 97u);
        norm += amp;
        amp *= 0.5;
        period *= 2;
    }
    return sum / norm;
}

QImage textureTile(int index, quint32 seed, const QColor& c1, const QColor& c2)
{
    constexpr int n = 256;
    QImage img(n, n, QImage::Format_ARGB32_Premultiplied);
    const double twoPi = 2.0 * std::numbers::pi;
    for (int y = 0; y < n; ++y) {
        auto* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < n; ++x) {
            const double u = double(x) / n, v = double(y) / n;
            double t = 0;
            switch (index) {
            case 0: t = fbm(u, v, 4, seed); break;                                                         // Wolken
            case 1: t = 0.5 + 0.5 * std::sin(twoPi * (2 * u + 3.0 * fbm(u, v, 4, seed))); break;          // Marmor
            case 2: {                                                                                       // Holz
                const double r = std::hypot(u - 0.5, v - 0.5) * 8 + 2.0 * fbm(u, v, 4, seed);
                t = r - std::floor(r);
                break;
            }
            case 3: t = fbm(u, v, 32, seed, 3) > 0.55 ? 1.0 : 0.25 * fbm(u, v, 16, seed + 7); break;   // Granit
            case 4: t = 0.5 + 0.5 * std::sin(twoPi * (6 * v + 1.5 * fbm(u, v, 4, seed))); break;          // Wasser
            case 5: {                                                                                       // Leinwand
                const double weave = (int(x / 4) + int(y / 4)) % 2 ? std::abs(std::sin(twoPi * x / 8)) : std::abs(std::sin(twoPi * y / 8));
                t = 0.6 * weave + 0.4 * fbm(u, v, 16, seed, 2);
                break;
            }
            default: break;
            }
            t = std::clamp(t, 0.0, 1.0);
            auto mix = [t](int a, int b) { return int(std::lround(a + (b - a) * t)); };
            line[x] = qRgba(mix(c1.red(), c2.red()), mix(c1.green(), c2.green()), mix(c1.blue(), c2.blue()), 255);
        }
    }
    return img;
}

QString cacheKey(const PatternFill& p)
{
    return QStringLiteral("%1/%2/%3/%4/%5/%6").arg(int(p.kind)).arg(p.index).arg(p.fg.toHex(), p.bg.toHex(), p.imageKey).arg(p.seed);
}

} // namespace

namespace patterns {

int twoColorCount()
{
    return int(twoColorNames().size());
}

QString twoColorName(int index)
{
    return twoColorNames().value(index);
}

int textureCount()
{
    return int(textureNames().size());
}

QString textureName(int index)
{
    return textureNames().value(index);
}

} // namespace patterns

QImage PatternFill::image() const
{
    static QCache<QString, QImage> cache(64);
    static QMutex mutex;
    const QString key = cacheKey(*this);
    {
        const QMutexLocker lock(&mutex);
        if (const QImage* hit = cache.object(key))
            return *hit;
    }
    QImage img;
    switch (kind) {
    case Kind::TwoColor: img = twoColorTile(index, fg.toQColor(), bg.toQColor()); break;
    case Kind::Texture: img = textureTile(index, seed, fg.toQColor(), bg.toQColor()); break;
    case Kind::Bitmap: img = imagepool::displayImage(imageKey); break;
    }
    if (img.isNull())
        return img;
    const QMutexLocker lock(&mutex);
    cache.insert(key, new QImage(img));
    return img;
}

double PatternFill::lightness(const QPointF& mm) const
{
    // Zweifarbig: Maske mit schwarzem Vordergrund auf weissem Hintergrund. Je Bildpunkt
    // aufgerufen: das zuletzt benutzte Bild bereithalten
    PatternFill mask = *this;
    mask.tile = 1.0;
    if (kind == Kind::TwoColor) {
        mask.fg = Color(0, 0, 0);
        mask.bg = Color(255, 255, 255);
    }
    thread_local PatternFill lastMask;
    thread_local QImage lastImage;
    if (lastImage.isNull() || !(lastMask == mask)) {
        lastMask = mask;
        lastImage = mask.image();
    }
    const QImage& img = lastImage;
    if (img.isNull() || tile <= 0)
        return 0.0;
    const double u = mm.x() / tile - std::floor(mm.x() / tile), v = mm.y() / tile - std::floor(mm.y() / tile);
    const QRgb px = img.pixel(std::clamp(int(u * img.width()), 0, img.width() - 1), std::clamp(int(v * img.height()), 0, img.height() - 1));
    return (0.299 * qRed(px) + 0.587 * qGreen(px) + 0.114 * qBlue(px)) / 255.0;
}

QJsonObject PatternFill::toJson() const
{
    QJsonObject o{{QStringLiteral("kind"), int(kind)}, {QStringLiteral("index"), index}, {QStringLiteral("fg"), fg.toHex()},
                  {QStringLiteral("bg"), bg.toHex()}, {QStringLiteral("tile"), tile}};
    if (kind == Kind::Bitmap)
        o[QStringLiteral("image")] = imageKey;
    if (kind == Kind::Texture)
        o[QStringLiteral("seed")] = qint64(seed);
    return o;
}

PatternFill PatternFill::fromJson(const QJsonObject& json)
{
    PatternFill p;
    p.kind = Kind(std::clamp(json.value(QStringLiteral("kind")).toInt(), 0, 2));
    p.index = std::max(0, json.value(QStringLiteral("index")).toInt());
    p.fg = Color::fromHex(json.value(QStringLiteral("fg")).toString()).value_or(Color(0, 0, 0));
    p.bg = Color::fromHex(json.value(QStringLiteral("bg")).toString()).value_or(Color(255, 255, 255));
    p.imageKey = json.value(QStringLiteral("image")).toString();
    p.seed = quint32(json.value(QStringLiteral("seed")).toInteger(1));
    p.tile = std::max(0.1, json.value(QStringLiteral("tile")).toDouble(10.0));
    return p;
}

} // namespace vek
