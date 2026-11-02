#include "ImageAdjust.h"

#include <QColor>

#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

namespace vek::adjust {

namespace {

// Gerechnet wird in 16 Bit je Kanal (0..65535). Die Formeln arbeiten weiter auf der
// gewohnten Skala 0..255, aber stufenlos: v = Wert / 257.
constexpr int kLevels = 65536;
constexpr double kScale = 257.0;
using Table = std::vector<quint16>;

quint16 clamp16(double v255)
{
    return quint16(std::clamp(std::lround(v255 * kScale), 0L, 65535L));
}

// Tabelle über alle 65536 Eingangswerte aus einer Funktion auf der Skala 0..255
Table makeTable(const std::function<double(double)>& fn)
{
    Table t(kLevels);
    for (int i = 0; i < kLevels; ++i)
        t[std::size_t(i)] = clamp16(fn(i / kScale));
    return t;
}

Table identity()
{
    Table t(kLevels);
    for (int i = 0; i < kLevels; ++i)
        t[std::size_t(i)] = quint16(i);
    return t;
}

// Pixel für Pixel in 16 Bit (nicht vormultipliziert, damit halbtransparente Bilder richtig bleiben)
QImage forEachPixel(const QImage& image, const std::function<QRgba64(QRgba64)>& fn)
{
    QImage out = image.convertToFormat(QImage::Format_RGBA64);
    for (int y = 0; y < out.height(); ++y) {
        auto* line = reinterpret_cast<QRgba64*>(out.scanLine(y));
        for (int x = 0; x < out.width(); ++x)
            line[x] = fn(line[x]);
    }
    return out;
}

QImage applyTables(const QImage& image, const Table& r, const Table& g, const Table& b)
{
    return forEachPixel(image, [&](QRgba64 c) {
        return QRgba64::fromRgba64(r[c.red()], g[c.green()], b[c.blue()], c.alpha());
    });
}

double smoothstep(double a, double b, double x)
{
    const double t = std::clamp((x - a) / (b - a), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

// Feines Histogramm (alle 65536 Stufen) je Kanal, für genaue Grenzen beim Strecken
std::array<std::vector<long>, 3> fineHistogram(const QImage& image)
{
    std::array<std::vector<long>, 3> h{std::vector<long>(kLevels), std::vector<long>(kLevels), std::vector<long>(kLevels)};
    const QImage img = image.convertToFormat(QImage::Format_RGBA64);
    for (int y = 0; y < img.height(); ++y) {
        const auto* line = reinterpret_cast<const QRgba64*>(img.constScanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            if (line[x].alpha() == 0)
                continue;
            ++h[0][line[x].red()];
            ++h[1][line[x].green()];
            ++h[2][line[x].blue()];
        }
    }
    return h;
}

// Untere und obere Grenze (0..255, stufenlos), unter bzw. über der je fraction der Werte liegt
std::pair<double, double> range(const std::vector<long>& counts, double fraction)
{
    long total = 0;
    for (long c : counts)
        total += c;
    const long cut = long(total * fraction);
    long sum = 0;
    int low = 0;
    for (; low < kLevels - 1; ++low) {
        sum += counts[std::size_t(low)];
        if (sum > cut)
            break;
    }
    sum = 0;
    int high = kLevels - 1;
    for (; high > 0; --high) {
        sum += counts[std::size_t(high)];
        if (sum > cut)
            break;
    }
    high = std::max(high, low + 1);
    return {low / kScale, high / kScale};
}

Table stretchTable(double low, double high)
{
    return makeTable([=](double v) { return (v - low) * 255.0 / (high - low); });
}

// Glatte, monotone Kurve durch die Stützpunkte (Fritsch–Carlson), x und y auf 0..255
std::function<double(double)> curveFunction(QList<QPointF> points)
{
    if (points.isEmpty())
        return [](double x) { return x; };
    std::sort(points.begin(), points.end(), [](const QPointF& a, const QPointF& b) { return a.x() < b.x(); });
    if (points.first().x() > 0)
        points.prepend(QPointF(0, points.first().y()));
    if (points.last().x() < 255)
        points.append(QPointF(255, points.last().y()));
    const int n = int(points.size());
    std::vector<double> d(static_cast<std::size_t>(n - 1));
    std::vector<double> m(static_cast<std::size_t>(n));
    for (int i = 0; i < n - 1; ++i) {
        const double dx = std::max(1e-6, points[i + 1].x() - points[i].x());
        d[std::size_t(i)] = (points[i + 1].y() - points[i].y()) / dx;
    }
    m[0] = d[0];
    m[std::size_t(n - 1)] = d[std::size_t(n - 2)];
    for (int i = 1; i < n - 1; ++i)
        m[std::size_t(i)] = d[std::size_t(i - 1)] * d[std::size_t(i)] <= 0 ? 0.0 : (d[std::size_t(i - 1)] + d[std::size_t(i)]) / 2.0;
    for (int i = 0; i < n - 1; ++i) {
        if (d[std::size_t(i)] == 0.0) {
            m[std::size_t(i)] = m[std::size_t(i + 1)] = 0.0;
            continue;
        }
        const double a = m[std::size_t(i)] / d[std::size_t(i)], b = m[std::size_t(i + 1)] / d[std::size_t(i)];
        const double s = a * a + b * b;
        if (s > 9.0) {
            const double t = 3.0 / std::sqrt(s);
            m[std::size_t(i)] = t * a * d[std::size_t(i)];
            m[std::size_t(i + 1)] = t * b * d[std::size_t(i)];
        }
    }
    return [points, m](double x) {
        int seg = 0;
        while (seg < int(points.size()) - 2 && x > points[seg + 1].x())
            ++seg;
        const QPointF p0 = points[seg], p1 = points[seg + 1];
        const double h = std::max(1e-6, p1.x() - p0.x());
        const double t = std::clamp((x - p0.x()) / h, 0.0, 1.0);
        const double t2 = t * t, t3 = t2 * t;
        return (2 * t3 - 3 * t2 + 1) * p0.y() + (t3 - 2 * t2 + t) * h * m[std::size_t(seg)] + (-2 * t3 + 3 * t2) * p1.y()
               + (t3 - t2) * h * m[std::size_t(seg + 1)];
    };
}

} // namespace

bool Basic::isIdentity() const
{
    return temperature == 0 && tint == 0 && saturation == 0 && brightness == 0 && contrast == 0 && highlights == 0
           && shadows == 0 && midtones == 0;
}

QImage apply(const QImage& image, const Basic& b)
{
    if (b.isIdentity())
        return image.convertToFormat(QImage::Format_RGBA64);
    // Tonwerte als Tabellen je Kanal, Sättigung danach je Pixel
    const double contrast = b.contrast * 1.28;
    const double factor = (259.0 * (contrast + 255.0)) / (255.0 * (259.0 - contrast));
    auto tone = [&](double v) {
        v += b.brightness * 1.28;
        v = factor * (v - 128.0) + 128.0;
        v += b.highlights / 100.0 * 70.0 * smoothstep(128.0, 255.0, v);
        v += b.shadows / 100.0 * 70.0 * (1.0 - smoothstep(0.0, 128.0, v));
        v += b.midtones / 100.0 * 50.0 * std::max(0.0, 1.0 - std::abs(v - 128.0) / 128.0);
        return v;
    };
    const Table r = makeTable([&](double v) { return tone(v + b.temperature * 0.6 + b.tint * 0.25); });
    const Table g = makeTable([&](double v) { return tone(v - b.tint * 0.5); });
    const Table bl = makeTable([&](double v) { return tone(v - b.temperature * 0.6 + b.tint * 0.25); });
    if (b.saturation == 0)
        return applyTables(image, r, g, bl);
    const double sat = 1.0 + b.saturation / 100.0;
    return forEachPixel(image, [&](QRgba64 c) {
        double R = r[c.red()], G = g[c.green()], B = bl[c.blue()];
        const double lum = 0.299 * R + 0.587 * G + 0.114 * B;
        auto px = [](double v) { return quint16(std::clamp(std::lround(v), 0L, 65535L)); };
        return QRgba64::fromRgba64(px(lum + (R - lum) * sat), px(lum + (G - lum) * sat), px(lum + (B - lum) * sat),
                                   c.alpha());
    });
}

QImage autoAdjust(const QImage& image)
{
    const auto h = fineHistogram(image);
    const auto [rl, rh] = range(h[0], 0.005);
    const auto [gl, gh] = range(h[1], 0.005);
    const auto [bl, bh] = range(h[2], 0.005);
    return applyTables(image, stretchTable(rl, rh), stretchTable(gl, gh), stretchTable(bl, bh));
}

QImage stretchContrast(const QImage& image)
{
    const auto h = fineHistogram(image);
    std::vector<long> sum(kLevels);
    for (int i = 0; i < kLevels; ++i)
        sum[std::size_t(i)] = h[0][std::size_t(i)] + h[1][std::size_t(i)] + h[2][std::size_t(i)];
    const auto [low, high] = range(sum, 0.005);
    const Table t = stretchTable(low, high);
    return applyTables(image, t, t, t);
}

QImage invert(const QImage& image)
{
    return forEachPixel(image, [](QRgba64 c) {
        return QRgba64::fromRgba64(65535 - c.red(), 65535 - c.green(), 65535 - c.blue(), c.alpha());
    });
}

QImage desaturate(const QImage& image)
{
    return forEachPixel(image, [](QRgba64 c) {
        const auto l = quint16(std::clamp(std::lround(0.299 * c.red() + 0.587 * c.green() + 0.114 * c.blue()), 0L, 65535L));
        return QRgba64::fromRgba64(l, l, l, c.alpha());
    });
}

QImage posterize(const QImage& image, int levels)
{
    levels = std::clamp(levels, 2, 255);
    const Table t = makeTable([levels](double v) { return std::round(v / 255.0 * (levels - 1)) * 255.0 / (levels - 1); });
    return applyTables(image, t, t, t);
}

QImage gamma(const QImage& image, double value)
{
    value = std::clamp(value, 0.1, 10.0);
    const Table t = makeTable([value](double v) { return 255.0 * std::pow(v / 255.0, 1.0 / value); });
    return applyTables(image, t, t, t);
}

QImage brightnessContrastIntensity(const QImage& image, int brightness, int contrast, int intensity)
{
    Basic b;
    b.brightness = brightness;
    b.contrast = contrast;
    b.midtones = intensity;
    return apply(image, b);
}

QImage hueSaturationLightness(const QImage& image, int hue, int saturation, int lightness)
{
    return forEachPixel(image, [=](QRgba64 c) {
        // QColor rechnet intern mit 16 Bit je Kanal
        QColor color = QColor::fromRgba64(c);
        float h, s, l, a;
        color.getHslF(&h, &s, &l, &a);
        if (h >= 0.0f)
            h = std::fmod(h + hue / 360.0f + 1.0f, 1.0f);
        s = std::clamp(s * (1.0f + saturation / 100.0f), 0.0f, 1.0f);
        l = lightness >= 0 ? l + (1.0f - l) * lightness / 100.0f : l * (1.0f + lightness / 100.0f);
        color.setHslF(std::max(h, 0.0f), s, std::clamp(l, 0.0f, 1.0f));
        QRgba64 out = color.rgba64();
        out.setAlpha(c.alpha());
        return out;
    });
}

QImage colorBalance(const QImage& image, int cyanRed, int magentaGreen, int yellowBlue)
{
    // Wirkt vor allem auf die Mitteltöne, wie die Farbbalance in Bildprogrammen
    auto table = [](int shift) {
        return makeTable([shift](double v) {
            const double weight = std::max(0.0, 1.0 - std::abs(v - 128.0) / 160.0);
            return v + shift * 0.9 * weight;
        });
    };
    return applyTables(image, table(cyanRed), table(magentaGreen), table(yellowBlue));
}

std::array<int, 256> curveTable(QList<QPointF> points)
{
    const auto curve = curveFunction(std::move(points));
    std::array<int, 256> table;
    for (int x = 0; x < 256; ++x)
        table[std::size_t(x)] = std::clamp(int(std::lround(curve(x))), 0, 255);
    return table;
}

QImage toneCurve(const QImage& image, const QList<QPointF>& points, int channel)
{
    const Table t = makeTable(curveFunction(points));
    const Table id = identity();
    return applyTables(image, channel < 0 || channel == 0 ? t : id, channel < 0 || channel == 1 ? t : id,
                       channel < 0 || channel == 2 ? t : id);
}

std::array<std::array<int, 256>, 3> histogram(const QImage& image)
{
    // Für die Anzeige 256 Säulen, gezählt aus den 16-Bit-Werten
    std::array<std::array<int, 256>, 3> h{};
    const QImage img = image.convertToFormat(QImage::Format_RGBA64);
    for (int y = 0; y < img.height(); ++y) {
        const auto* line = reinterpret_cast<const QRgba64*>(img.constScanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            if (line[x].alpha() == 0)
                continue;
            ++h[0][std::size_t(line[x].red() >> 8)];
            ++h[1][std::size_t(line[x].green() >> 8)];
            ++h[2][std::size_t(line[x].blue() >> 8)];
        }
    }
    return h;
}

} // namespace vek::adjust
