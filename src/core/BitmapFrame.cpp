#include "BitmapFrame.h"

#include <QtGlobal>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace vek::filters {

namespace {

constexpr std::array<const char*, 8> kShapes{
    QT_TRANSLATE_NOOP("vek::fx", "Gerissenes Papier"), QT_TRANSLATE_NOOP("vek::fx", "Pinselstrich"),
    QT_TRANSLATE_NOOP("vek::fx", "Weicher Rand"),      QT_TRANSLATE_NOOP("vek::fx", "Briefmarke"),
    QT_TRANSLATE_NOOP("vek::fx", "Spritzer"),          QT_TRANSLATE_NOOP("vek::fx", "Welle"),
    QT_TRANSLATE_NOOP("vek::fx", "Oval"),              QT_TRANSLATE_NOOP("vek::fx", "Zackenschere"),
};

double hash(int x, int y, quint32 seed)
{
    quint32 h = quint32(x) * 374761393u + quint32(y) * 668265263u + seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return h / 4294967295.0;
}

// Wertrauschen, glatt interpoliert, 0..1
double noise(double x, double y, quint32 seed)
{
    const double fx = std::floor(x), fy = std::floor(y);
    const int ix = int(fx), iy = int(fy);
    double u = x - fx, v = y - fy;
    u = u * u * (3 - 2 * u);
    v = v * v * (3 - 2 * v);
    const double a = hash(ix, iy, seed), b = hash(ix + 1, iy, seed);
    const double c = hash(ix, iy + 1, seed), d = hash(ix + 1, iy + 1, seed);
    return (a + (b - a) * u) * (1 - v) + (c + (d - c) * u) * v;
}

// Mehrere Oktaven, 0..1
double fbm(double x, double y, quint32 seed, int octaves = 4)
{
    double sum = 0, amp = 0.5, norm = 0;
    for (int i = 0; i < octaves; ++i) {
        sum += noise(x, y, seed + quint32(i) * 101u) * amp;
        norm += amp;
        amp *= 0.5;
        x *= 2.03;
        y *= 2.03;
    }
    return sum / norm;
}

double smoothstep(double e0, double e1, double x)
{
    const double t = std::clamp((x - e0) / (e1 - e0), 0.0, 1.0);
    return t * t * (3 - 2 * t);
}

} // namespace

int frameShapeCount()
{
    return int(kShapes.size());
}

const char* frameShapeName(int shape)
{
    return kShapes[std::size_t(std::clamp(shape, 0, frameShapeCount() - 1))];
}

double frameCoverage(const FrameSettings& s, double x, double y, int w, int h)
{
    if (w <= 0 || h <= 0)
        return 0.0;
    // In Rahmenkoordinaten: um die Mitte zurückdrehen und auf die Rahmengrösse skalieren
    const double cx = w / 2.0, cy = h / 2.0;
    const double rad = -s.rotation * std::numbers::pi / 180.0;
    const double k = 100.0 / std::max(1.0, s.scale);
    const double dx = x - cx, dy = y - cy;
    const double qx = (dx * std::cos(rad) - dy * std::sin(rad)) * k + cx;
    const double qy = (dx * std::sin(rad) + dy * std::cos(rad)) * k + cy;
    if (qx < 0 || qy < 0 || qx > w || qy > h)
        return 0.0;
    const double unit = std::min(w, h);
    const double band = std::clamp(s.width, 1.0, 50.0) / 100.0;
    // Abstand zur nächsten Kante und Lage entlang dieser Kante (in Einheiten der kürzeren Seite)
    const double dl = qx, dr = w - qx, dt = qy, db = h - qy;
    const double d = std::min({dl, dr, dt, db}) / unit;
    const bool vertical = std::min(dl, dr) < std::min(dt, db);
    const double along = (vertical ? qy : qx) / unit;
    const double nx = qx / unit, ny = qy / unit;
    double e = d / band; // 0 an der Kante, 1 am inneren Ende des Randes
    double field = 0.0;  // > 0 = Bild, < 0 = Rand
    double soft = 0.01 + std::clamp(s.softness, 0.0, 100.0) / 100.0 * 0.6;
    const quint32 seed = s.seed;
    switch (std::clamp(s.shape, 0, frameShapeCount() - 1)) {
    case 0: // gerissenes Papier: grobe Lappen und feine Zacken
        field = e - 0.7 + (fbm(nx * 5, ny * 5, seed) - 0.5) * 0.8 + (noise(along * 70, d * 6, seed + 7) - 0.5) * 0.4;
        break;
    case 1: // Pinselstrich: Striche parallel zur Kante
        field = e - 0.55 + (fbm(along * 2.5, d * 45, seed + 3) - 0.5) * 1.4;
        break;
    case 2: // weicher Rand
        field = e - 0.5;
        soft += 0.45;
        break;
    case 3: { // Briefmarke: halbrunde Zähne entlang der Kante
        const double spacing = band * 0.9;
        const double tc = std::round(along / spacing) * spacing;
        const double dist = std::hypot(along - tc, d) / band;
        field = std::min(dist - 0.32, e - 0.02);
        break;
    }
    case 4: // Spritzer: körnig ausfransend
        field = e - 0.5 + (fbm(nx * 8, ny * 8, seed + 11) - 0.5) * 0.7 + (hash(int(nx * 300), int(ny * 300), seed) - 0.5) * 0.9;
        break;
    case 5: // Welle
        field = e - 0.5 + 0.3 * std::sin(along / band * std::numbers::pi);
        break;
    case 6: { // Oval
        const double rx = (qx - cx) / cx, ry = (qy - cy) / cy;
        e = (1.0 - std::hypot(rx, ry)) * unit / 2.0 / (band * unit);
        field = e - 0.5;
        break;
    }
    default: { // Zackenschere
        const double tri = std::abs(along / (band * 0.7) - std::floor(along / (band * 0.7)) - 0.5) * 2.0;
        field = e - 0.25 - tri * 0.5;
        break;
    }
    }
    // Mindestens ein Pixel Kantenglättung
    soft = std::max(soft, 1.0 / (band * unit));
    return smoothstep(-soft, soft, field);
}

QImage frame(const QImage& in, const FrameSettings& s)
{
    QImage out = in.convertToFormat(QImage::Format_RGBA64_Premultiplied);
    const int w = out.width(), h = out.height();
    const double opacity = std::clamp(s.opacity, 0.0, 100.0) / 100.0;
    const double cr = s.color.r / 255.0, cg = s.color.g / 255.0, cb = s.color.b / 255.0;
    for (int y = 0; y < h; ++y) {
        auto* line = reinterpret_cast<QRgba64*>(out.scanLine(y));
        for (int x = 0; x < w; ++x) {
            const double edge = (1.0 - frameCoverage(s, x + 0.5, y + 0.5, w, h)) * opacity;
            if (edge <= 0.0)
                continue;
            QRgba64& px = line[x];
            if (s.border) {
                // Farbrand: Farbe über das Bild, Deckkraft des Bildes bleibt (vormultipliziert)
                const double a = px.alpha() / 65535.0;
                auto mix = [&](quint16 v, double c) { return quint16(std::lround(v + (c * a * 65535.0 - v) * edge)); };
                px = qRgba64(mix(px.red(), cr), mix(px.green(), cg), mix(px.blue(), cb), px.alpha());
            } else {
                const double k = 1.0 - edge;
                px = qRgba64(quint16(std::lround(px.red() * k)), quint16(std::lround(px.green() * k)),
                             quint16(std::lround(px.blue() * k)), quint16(std::lround(px.alpha() * k)));
            }
        }
    }
    return out.convertToFormat(QImage::Format_RGBA64);
}

QImage frameThumbnail(int shape, const QSize& size)
{
    QImage white(size, QImage::Format_RGBA64);
    white.fill(Qt::white);
    FrameSettings s;
    s.shape = shape;
    s.border = true;
    s.color = Color(0, 0, 0);
    s.width = 16;
    return frame(white, s);
}

} // namespace vek::filters
