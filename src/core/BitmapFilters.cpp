#include "BitmapFilters.h"

#include <QColor>
#include <QPainter>
#include <QRandomGenerator>
#include <QPolygonF>
#include <QTransform>
#include <QtMath>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace vek::filters {

namespace {

// Bild als Gleitkomma-Puffer, Farben vormultipliziert (Weichzeichnen ohne Farbsäume)
struct Buf {
    int w = 0, h = 0;
    std::vector<std::array<float, 4>> px;

    explicit Buf(const QImage& in)
    {
        const QImage src = in.convertToFormat(QImage::Format_RGBA64_Premultiplied);
        w = src.width();
        h = src.height();
        px.resize(std::size_t(w) * h);
        for (int y = 0; y < h; ++y) {
            const auto* line = reinterpret_cast<const QRgba64*>(src.constScanLine(y));
            for (int x = 0; x < w; ++x)
                px[std::size_t(y) * w + x] = {line[x].red() / 65535.f, line[x].green() / 65535.f, line[x].blue() / 65535.f,
                                              line[x].alpha() / 65535.f};
        }
    }
    Buf(int width, int height) : w(width), h(height), px(std::size_t(width) * height, {0.f, 0.f, 0.f, 0.f}) {}

    std::array<float, 4>& at(int x, int y) { return px[std::size_t(y) * w + x]; }
    const std::array<float, 4>& at(int x, int y) const { return px[std::size_t(y) * w + x]; }
    // Am Rand fortgesetzt
    const std::array<float, 4>& clamped(int x, int y) const { return at(std::clamp(x, 0, w - 1), std::clamp(y, 0, h - 1)); }
    // Bilinear abgetastet
    std::array<float, 4> sample(double x, double y) const
    {
        x = std::clamp(x, 0.0, w - 1.0);
        y = std::clamp(y, 0.0, h - 1.0);
        const int x0 = int(x), y0 = int(y);
        const int x1 = std::min(x0 + 1, w - 1), y1 = std::min(y0 + 1, h - 1);
        const float fx = float(x - x0), fy = float(y - y0);
        std::array<float, 4> r{};
        for (int c = 0; c < 4; ++c)
            r[c] = (at(x0, y0)[c] * (1 - fx) + at(x1, y0)[c] * fx) * (1 - fy) + (at(x0, y1)[c] * (1 - fx) + at(x1, y1)[c] * fx) * fy;
        return r;
    }

    QImage image() const
    {
        QImage out(w, h, QImage::Format_RGBA64_Premultiplied);
        for (int y = 0; y < h; ++y) {
            auto* line = reinterpret_cast<QRgba64*>(out.scanLine(y));
            for (int x = 0; x < w; ++x) {
                const auto& p = at(x, y);
                const float a = std::clamp(p[3], 0.f, 1.f);
                auto ch = [a](float v) { return quint16(std::lround(std::clamp(v, 0.f, a) * 65535.f)); };
                line[x] = qRgba64(ch(p[0]), ch(p[1]), ch(p[2]), quint16(std::lround(a * 65535.f)));
            }
        }
        return out.convertToFormat(QImage::Format_RGBA64);
    }
};

float luminance(const std::array<float, 4>& p)
{
    return 0.299f * p[0] + 0.587f * p[1] + 0.114f * p[2];
}

// Für Farbfilter: unvormultipliziert bearbeiten
template <class F>
QImage perPixel(const QImage& in, F fn)
{
    Buf b(in);
    for (auto& p : b.px) {
        const float a = p[3];
        if (a <= 0)
            continue;
        std::array<float, 3> c{p[0] / a, p[1] / a, p[2] / a};
        fn(c);
        for (int i = 0; i < 3; ++i)
            p[i] = std::clamp(c[i], 0.f, 1.f) * a;
    }
    return b.image();
}

// Abbildung: für jedes Zielpixel die Quellstelle
template <class F>
QImage remap(const QImage& in, F source)
{
    const Buf b(in);
    Buf out(b.w, b.h);
    for (int y = 0; y < b.h; ++y)
        for (int x = 0; x < b.w; ++x) {
            const QPointF s = source(double(x), double(y));
            out.at(x, y) = b.sample(s.x(), s.y());
        }
    return out.image();
}

// Gaussscher Weichzeichner, getrennt in x und y
Buf blurred(const Buf& b, double sigma)
{
    if (sigma <= 0.05)
        return b;
    const int r = std::max(1, int(std::ceil(sigma * 3)));
    std::vector<float> k(2 * r + 1);
    float sum = 0;
    for (int i = -r; i <= r; ++i)
        sum += k[i + r] = float(std::exp(-i * i / (2 * sigma * sigma)));
    for (float& v : k)
        v /= sum;
    Buf tmp(b.w, b.h), out(b.w, b.h);
    for (int y = 0; y < b.h; ++y)
        for (int x = 0; x < b.w; ++x) {
            std::array<float, 4> acc{};
            for (int i = -r; i <= r; ++i) {
                const auto& p = b.clamped(x + i, y);
                for (int c = 0; c < 4; ++c)
                    acc[c] += p[c] * k[i + r];
            }
            tmp.at(x, y) = acc;
        }
    for (int y = 0; y < b.h; ++y)
        for (int x = 0; x < b.w; ++x) {
            std::array<float, 4> acc{};
            for (int i = -r; i <= r; ++i) {
                const auto& p = tmp.clamped(x, y + i);
                for (int c = 0; c < 4; ++c)
                    acc[c] += p[c] * k[i + r];
            }
            out.at(x, y) = acc;
        }
    return out;
}

// Sobel-Betrag der Helligkeit, 0..1
std::vector<float> edges(const Buf& b)
{
    std::vector<float> e(std::size_t(b.w) * b.h);
    for (int y = 0; y < b.h; ++y)
        for (int x = 0; x < b.w; ++x) {
            auto l = [&](int dx, int dy) { return luminance(b.clamped(x + dx, y + dy)); };
            const float gx = -l(-1, -1) - 2 * l(-1, 0) - l(-1, 1) + l(1, -1) + 2 * l(1, 0) + l(1, 1);
            const float gy = -l(-1, -1) - 2 * l(0, -1) - l(1, -1) + l(-1, 1) + 2 * l(0, 1) + l(1, 1);
            e[std::size_t(y) * b.w + x] = std::min(1.f, std::sqrt(gx * gx + gy * gy) / 4.f);
        }
    return e;
}

} // namespace

// --- Unschärfe -------------------------------------------------------------------------------

QImage gaussian(const QImage& in, double radius)
{
    return blurred(Buf(in), radius / 2.0).image();
}

QImage motion(const QImage& in, int distance, double angleDeg)
{
    const Buf b(in);
    Buf out(b.w, b.h);
    const double a = qDegreesToRadians(angleDeg);
    const double dx = std::cos(a), dy = -std::sin(a);
    const int n = std::max(1, distance);
    for (int y = 0; y < b.h; ++y)
        for (int x = 0; x < b.w; ++x) {
            std::array<float, 4> acc{};
            for (int i = -n / 2; i <= n / 2; ++i) {
                const auto p = b.sample(x + dx * i, y + dy * i);
                for (int c = 0; c < 4; ++c)
                    acc[c] += p[c];
            }
            const float cnt = float(2 * (n / 2) + 1);
            for (float& v : acc)
                v /= cnt;
            out.at(x, y) = acc;
        }
    return out.image();
}

QImage radialBlur(const QImage& in, double amount)
{
    const Buf b(in);
    Buf out(b.w, b.h);
    const double cx = b.w / 2.0, cy = b.h / 2.0;
    const double maxAngle = qDegreesToRadians(std::clamp(amount, 0.0, 100.0) * 0.3);
    const int steps = 12;
    for (int y = 0; y < b.h; ++y)
        for (int x = 0; x < b.w; ++x) {
            std::array<float, 4> acc{};
            for (int i = 0; i < steps; ++i) {
                const double t = maxAngle * (i / double(steps - 1) - 0.5);
                const double sx = cx + (x - cx) * std::cos(t) - (y - cy) * std::sin(t);
                const double sy = cy + (x - cx) * std::sin(t) + (y - cy) * std::cos(t);
                const auto p = b.sample(sx, sy);
                for (int c = 0; c < 4; ++c)
                    acc[c] += p[c] / steps;
            }
            out.at(x, y) = acc;
        }
    return out.image();
}

QImage zoomBlur(const QImage& in, double amount)
{
    const Buf b(in);
    Buf out(b.w, b.h);
    const double cx = b.w / 2.0, cy = b.h / 2.0;
    const double k = std::clamp(amount, 0.0, 100.0) / 400.0;
    const int steps = 12;
    for (int y = 0; y < b.h; ++y)
        for (int x = 0; x < b.w; ++x) {
            std::array<float, 4> acc{};
            for (int i = 0; i < steps; ++i) {
                const double s = 1.0 - k * i / double(steps - 1);
                const auto p = b.sample(cx + (x - cx) * s, cy + (y - cy) * s);
                for (int c = 0; c < 4; ++c)
                    acc[c] += p[c] / steps;
            }
            out.at(x, y) = acc;
        }
    return out.image();
}

QImage smooth(const QImage& in, int amount)
{
    Buf b(in);
    const int passes = std::clamp(amount / 20, 1, 5);
    for (int pass = 0; pass < passes; ++pass) {
        Buf out(b.w, b.h);
        for (int y = 0; y < b.h; ++y)
            for (int x = 0; x < b.w; ++x) {
                std::array<float, 4> acc{};
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx)
                        for (int c = 0; c < 4; ++c)
                            acc[c] += b.clamped(x + dx, y + dy)[c] / 9.f;
                out.at(x, y) = acc;
            }
        b = out;
    }
    return b.image();
}

// --- Schärfe ---------------------------------------------------------------------------------

QImage unsharpMask(const QImage& in, double percent, double radius, int threshold)
{
    const Buf b(in);
    const Buf soft = blurred(b, std::max(0.1, radius / 2.0));
    Buf out = b;
    const float k = float(percent / 100.0), t = threshold / 255.f;
    for (std::size_t i = 0; i < b.px.size(); ++i)
        for (int c = 0; c < 3; ++c) {
            const float d = b.px[i][c] - soft.px[i][c];
            if (std::abs(d) >= t)
                out.px[i][c] = b.px[i][c] + d * k;
        }
    return out.image();
}

QImage sharpen(const QImage& in, double amount)
{
    return unsharpMask(in, amount * 2.0, 1.5, 0);
}

// --- Rauschen --------------------------------------------------------------------------------

QImage addNoise(const QImage& in, double amount, bool monochrome, quint32 seed)
{
    QRandomGenerator rng(seed);
    Buf b(in);
    const float k = float(std::clamp(amount, 0.0, 100.0) / 100.0);
    for (auto& p : b.px) {
        if (p[3] <= 0)
            continue;
        const float n = float(rng.generateDouble() * 2 - 1) * k;
        for (int c = 0; c < 3; ++c)
            p[c] += (monochrome ? n : float(rng.generateDouble() * 2 - 1) * k) * p[3];
    }
    return b.image();
}

QImage median(const QImage& in, int radius)
{
    const Buf b(in);
    Buf out(b.w, b.h);
    const int r = std::clamp(radius, 1, 10);
    std::vector<float> vals;
    for (int y = 0; y < b.h; ++y)
        for (int x = 0; x < b.w; ++x) {
            std::array<float, 4> res{};
            for (int c = 0; c < 4; ++c) {
                vals.clear();
                for (int dy = -r; dy <= r; ++dy)
                    for (int dx = -r; dx <= r; ++dx)
                        vals.push_back(b.clamped(x + dx, y + dy)[c]);
                std::nth_element(vals.begin(), vals.begin() + long(vals.size() / 2), vals.end());
                res[c] = vals[vals.size() / 2];
            }
            out.at(x, y) = res;
        }
    return out.image();
}

// --- Kontur ----------------------------------------------------------------------------------

QImage findEdges(const QImage& in, double strength)
{
    const Buf b(in);
    const std::vector<float> e = edges(b);
    Buf out(b.w, b.h);
    const float k = float(strength / 50.0);
    for (std::size_t i = 0; i < e.size(); ++i) {
        const float v = std::clamp(1.f - e[i] * k, 0.f, 1.f);
        out.px[i] = {v, v, v, 1.f};
    }
    return out.image();
}

QImage detectEdges(const QImage& in, double strength)
{
    const Buf b(in);
    const std::vector<float> e = edges(b);
    Buf out(b.w, b.h);
    const float k = float(strength / 50.0);
    for (std::size_t i = 0; i < e.size(); ++i) {
        const float v = std::clamp(e[i] * k, 0.f, 1.f);
        out.px[i] = {v, v, v, 1.f};
    }
    return out.image();
}

QImage traceContour(const QImage& in, int threshold)
{
    const Buf b(in);
    Buf out(b.w, b.h);
    const float t = threshold / 255.f;
    for (int y = 0; y < b.h; ++y)
        for (int x = 0; x < b.w; ++x) {
            const bool on = luminance(b.at(x, y)) >= t;
            bool border = false;
            for (int dy = -1; dy <= 1 && !border; ++dy)
                for (int dx = -1; dx <= 1 && !border; ++dx)
                    border = (luminance(b.clamped(x + dx, y + dy)) >= t) != on;
            const float v = border ? 0.f : 1.f;
            out.at(x, y) = {v, v, v, 1.f};
        }
    return out.image();
}

// --- Farbänderung ----------------------------------------------------------------------------

QImage solarize(const QImage& in, int level)
{
    const float t = std::clamp(level, 0, 255) / 255.f;
    return perPixel(in, [t](std::array<float, 3>& c) {
        for (float& v : c)
            if (v > t)
                v = 1.f - v;
    });
}

QImage psychedelic(const QImage& in, int shift)
{
    const float s = shift / 255.f;
    return perPixel(in, [s](std::array<float, 3>& c) {
        for (float& v : c)
            v = std::fmod(v * 3.f + s, 1.f);
    });
}

QImage halftone(const QImage& in, double cell)
{
    // Graustufen-Raster aus Punkten, deren Fläche der Dunkelheit entspricht
    const Buf b(in);
    const int w = b.w, h = b.h;
    const double c = std::max(2.0, cell);
    QImage out(w, h, QImage::Format_RGBA64);
    out.fill(Qt::white);
    QPainter p(&out);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::black);
    for (double y = 0; y < h; y += c)
        for (double x = 0; x < w; x += c) {
            const auto px = b.sample(x + c / 2, y + c / 2);
            const float a = std::max(px[3], 1e-6f);
            const double dark = 1.0 - luminance({px[0] / a, px[1] / a, px[2] / a, 1}) * (px[3] > 0 ? 1 : 0);
            const double r = c / 2.0 * std::sqrt(std::clamp(dark, 0.0, 1.0)) * 1.4142;
            p.drawEllipse(QPointF(x + c / 2, y + c / 2), r, r);
        }
    p.end();
    return out;
}

// --- Verzerren -------------------------------------------------------------------------------

QImage pixelate(const QImage& in, int cell)
{
    const Buf b(in);
    Buf out(b.w, b.h);
    const int c = std::max(1, cell);
    for (int y0 = 0; y0 < b.h; y0 += c)
        for (int x0 = 0; x0 < b.w; x0 += c) {
            std::array<float, 4> acc{};
            int n = 0;
            for (int y = y0; y < std::min(b.h, y0 + c); ++y)
                for (int x = x0; x < std::min(b.w, x0 + c); ++x, ++n)
                    for (int k = 0; k < 4; ++k)
                        acc[k] += b.at(x, y)[k];
            for (float& v : acc)
                v /= std::max(1, n);
            for (int y = y0; y < std::min(b.h, y0 + c); ++y)
                for (int x = x0; x < std::min(b.w, x0 + c); ++x)
                    out.at(x, y) = acc;
        }
    return out.image();
}

QImage ripple(const QImage& in, double amplitude, double period)
{
    const double per = std::max(2.0, period);
    return remap(in, [=](double x, double y) {
        return QPointF(x + amplitude * std::sin(2 * M_PI * y / per), y + amplitude * std::sin(2 * M_PI * x / per));
    });
}

QImage swirl(const QImage& in, double angleDeg)
{
    const double cx = in.width() / 2.0, cy = in.height() / 2.0, rmax = std::hypot(cx, cy);
    const double a = qDegreesToRadians(angleDeg);
    return remap(in, [=](double x, double y) {
        const double dx = x - cx, dy = y - cy;
        const double r = std::hypot(dx, dy);
        const double t = a * std::max(0.0, 1.0 - r / rmax);
        return QPointF(cx + dx * std::cos(t) - dy * std::sin(t), cy + dx * std::sin(t) + dy * std::cos(t));
    });
}

QImage wind(const QImage& in, int strength, quint32 seed)
{
    QRandomGenerator rng(seed);
    const Buf b(in);
    Buf out = b;
    const int n = std::clamp(strength, 1, 200);
    for (int y = 0; y < b.h; ++y) {
        // Je Zeile eine zufällige Schleppe nach rechts, hellere Pixel weiter
        for (int x = 0; x < b.w; ++x) {
            const auto& p = b.at(x, y);
            const int len = int(rng.bounded(n) * luminance(p));
            for (int i = 1; i <= len && x + i < b.w; ++i) {
                auto& q = out.at(x + i, y);
                const float f = 1.f - float(i) / (len + 1);
                for (int c = 0; c < 4; ++c)
                    q[c] = std::max(q[c], p[c] * f);
            }
        }
    }
    return out.image();
}

QImage tiles(const QImage& in, int count, double offset, quint32 seed)
{
    QRandomGenerator rng(seed);
    const int n = std::clamp(count, 1, 100);
    const double tw = in.width() / double(n), th = in.height() / double(n);
    QImage out(in.size(), QImage::Format_RGBA64);
    out.fill(Qt::white);
    QPainter p(&out);
    const QImage src = in.convertToFormat(QImage::Format_RGBA64);
    for (int j = 0; j < n; ++j)
        for (int i = 0; i < n; ++i) {
            const QRectF from(i * tw, j * th, tw, th);
            const QPointF d((rng.generateDouble() * 2 - 1) * offset / 100.0 * tw, (rng.generateDouble() * 2 - 1) * offset / 100.0 * th);
            p.drawImage(from.translated(d), src, from);
        }
    p.end();
    return out;
}

// --- Kreativ ---------------------------------------------------------------------------------

QImage crystallize(const QImage& in, int cell, quint32 seed)
{
    // Voronoi-Zellen um zufällig verschobene Gitterpunkte, Farbe des Zellkerns
    QRandomGenerator rng(seed);
    const Buf b(in);
    const int c = std::max(2, cell);
    const int gw = b.w / c + 2, gh = b.h / c + 2;
    std::vector<QPointF> seeds(std::size_t(gw) * gh);
    for (int j = 0; j < gh; ++j)
        for (int i = 0; i < gw; ++i)
            seeds[std::size_t(j) * gw + i] = QPointF((i + rng.generateDouble()) * c, (j + rng.generateDouble()) * c);
    Buf out(b.w, b.h);
    for (int y = 0; y < b.h; ++y)
        for (int x = 0; x < b.w; ++x) {
            const int gi = x / c, gj = y / c;
            double best = 1e18;
            QPointF bestSeed;
            for (int j = std::max(0, gj - 1); j <= std::min(gh - 1, gj + 1); ++j)
                for (int i = std::max(0, gi - 1); i <= std::min(gw - 1, gi + 1); ++i) {
                    const QPointF s = seeds[std::size_t(j) * gw + i];
                    const double d = (s.x() - x) * (s.x() - x) + (s.y() - y) * (s.y() - y);
                    if (d < best) {
                        best = d;
                        bestSeed = s;
                    }
                }
            out.at(x, y) = b.sample(bestSeed.x(), bestSeed.y());
        }
    return out.image();
}

QImage vignette(const QImage& in, double amount, double size, const Color& color)
{
    Buf b(in);
    const double cx = b.w / 2.0, cy = b.h / 2.0;
    const double inner = std::clamp(size / 100.0, 0.05, 1.0);
    const float k = float(std::clamp(amount, 0.0, 100.0) / 100.0);
    const float vc[3]{color.r / 255.f, color.g / 255.f, color.b / 255.f};
    for (int y = 0; y < b.h; ++y)
        for (int x = 0; x < b.w; ++x) {
            const double r = std::hypot((x - cx) / cx, (y - cy) / cy) / std::sqrt(2.0);
            const float t = float(std::clamp((r - inner * 0.7) / (1.0 - inner * 0.7), 0.0, 1.0)) * k;
            auto& p = b.at(x, y);
            for (int c = 0; c < 3; ++c)
                p[c] = p[c] * (1 - t) + vc[c] * p[3] * t;
        }
    return b.image();
}

QImage mosaic(const QImage& in, int cell)
{
    // Pixelieren mit hellen Fugen wie Glassteine
    QImage out = pixelate(in, cell);
    QPainter p(&out);
    p.setPen(QPen(QColor(255, 255, 255, 140), 1));
    const int c = std::max(2, cell);
    for (int x = 0; x < out.width(); x += c)
        p.drawLine(x, 0, x, out.height());
    for (int y = 0; y < out.height(); y += c)
        p.drawLine(0, y, out.width(), y);
    p.end();
    return out;
}

// --- 3D --------------------------------------------------------------------------------------

QImage emboss(const QImage& in, double depth, double angleDeg)
{
    const Buf b(in);
    Buf out(b.w, b.h);
    const double a = qDegreesToRadians(angleDeg);
    const double dx = std::cos(a), dy = -std::sin(a);
    const float k = float(std::max(0.1, depth / 10.0));
    for (int y = 0; y < b.h; ++y)
        for (int x = 0; x < b.w; ++x) {
            const float d = luminance(b.sample(x + dx, y + dy)) - luminance(b.sample(x - dx, y - dy));
            const float v = std::clamp(0.5f + d * k, 0.f, 1.f);
            out.at(x, y) = {v, v, v, 1.f};
        }
    return out.image();
}

QImage pinch(const QImage& in, double amount)
{
    const double cx = in.width() / 2.0, cy = in.height() / 2.0, rmax = std::min(cx, cy);
    const double k = std::clamp(amount, -100.0, 100.0) / 100.0;
    return remap(in, [=](double x, double y) {
        const double dx = x - cx, dy = y - cy;
        const double r = std::hypot(dx, dy) / rmax;
        if (r >= 1.0 || r == 0.0)
            return QPointF(x, y);
        const double f = std::pow(r, k > 0 ? 1.0 + k : 1.0 / (1.0 - k)) / r;
        return QPointF(cx + dx * f, cy + dy * f);
    });
}

QImage sphere(const QImage& in, double amount)
{
    const double cx = in.width() / 2.0, cy = in.height() / 2.0, rmax = std::min(cx, cy);
    const double k = std::clamp(amount, 0.0, 100.0) / 100.0;
    return remap(in, [=](double x, double y) {
        const double dx = (x - cx) / rmax, dy = (y - cy) / rmax;
        const double r = std::hypot(dx, dy);
        if (r >= 1.0)
            return QPointF(x, y);
        // Kugel: Winkel auf die Ebene zurückprojiziert
        const double z = std::sqrt(1.0 - r * r);
        const double f = (1.0 - k) + k * (std::asin(r) / (M_PI / 2.0)) / std::max(1e-9, r) * (1.0 - z * 0.0);
        return QPointF(cx + dx * f * rmax, cy + dy * f * rmax);
    });
}

// --- Künstlerische Striche -------------------------------------------------------------------

QImage charcoal(const QImage& in, double size, double edge)
{
    const Buf b(in);
    const Buf soft = blurred(b, std::max(0.5, size / 2.0));
    const std::vector<float> e = edges(soft);
    Buf out(b.w, b.h);
    const float k = float(edge / 25.0);
    for (std::size_t i = 0; i < e.size(); ++i) {
        const float lum = luminance(soft.px[i]) / std::max(1e-6f, soft.px[i][3]);
        const float v = std::clamp(lum * 0.6f + 0.4f - e[i] * k, 0.f, 1.f);
        out.px[i] = {v, v, v, 1.f};
    }
    return out.image();
}

QImage pencil(const QImage& in, double radius)
{
    // Graustufen, Farbabwedeln mit dem weichgezeichneten Negativ
    const Buf b(in);
    Buf gray(b.w, b.h), inv(b.w, b.h);
    for (std::size_t i = 0; i < b.px.size(); ++i) {
        const float a = std::max(b.px[i][3], 1e-6f);
        const float l = b.px[i][3] > 0 ? luminance(b.px[i]) / a : 1.f;
        gray.px[i] = {l, l, l, 1.f};
        inv.px[i] = {1 - l, 1 - l, 1 - l, 1.f};
    }
    const Buf soft = blurred(inv, std::max(0.5, radius / 2.0));
    Buf out(b.w, b.h);
    for (std::size_t i = 0; i < b.px.size(); ++i) {
        const float base = gray.px[i][0], blend = soft.px[i][0];
        const float v = blend >= 1.f ? 1.f : std::min(1.f, base / (1.f - blend));
        out.px[i] = {v, v, v, 1.f};
    }
    return out.image();
}

QImage oilPaint(const QImage& in, int radius, int levels)
{
    // Häufigste Helligkeitsstufe in der Umgebung, deren mittlere Farbe
    const Buf b(in);
    Buf out(b.w, b.h);
    const int r = std::clamp(radius, 1, 8), L = std::clamp(levels, 2, 64);
    std::vector<int> count(L);
    std::vector<std::array<float, 4>> sum(L);
    for (int y = 0; y < b.h; ++y)
        for (int x = 0; x < b.w; ++x) {
            std::fill(count.begin(), count.end(), 0);
            std::fill(sum.begin(), sum.end(), std::array<float, 4>{});
            for (int dy = -r; dy <= r; ++dy)
                for (int dx = -r; dx <= r; ++dx) {
                    const auto& p = b.clamped(x + dx, y + dy);
                    const int l = std::clamp(int(luminance(p) * (L - 1) + 0.5f), 0, L - 1);
                    ++count[l];
                    for (int c = 0; c < 4; ++c)
                        sum[l][c] += p[c];
                }
            const int best = int(std::max_element(count.begin(), count.end()) - count.begin());
            for (int c = 0; c < 4; ++c)
                out.at(x, y)[c] = sum[best][c] / std::max(1, count[best]);
        }
    return out.image();
}

// --- Kamera ----------------------------------------------------------------------------------

QImage diffuse(const QImage& in, int distance, quint32 seed)
{
    QRandomGenerator rng(seed);
    const Buf b(in);
    Buf out(b.w, b.h);
    const int d = std::max(1, distance);
    for (int y = 0; y < b.h; ++y)
        for (int x = 0; x < b.w; ++x)
            out.at(x, y) = b.clamped(x + int(rng.bounded(2 * d + 1)) - d, y + int(rng.bounded(2 * d + 1)) - d);
    return out.image();
}

// --- Modus, Farbmaske ------------------------------------------------------------------------

QImage toBlackWhite(const QImage& in, int threshold)
{
    const float t = std::clamp(threshold, 0, 255) / 255.f;
    return perPixel(in, [t](std::array<float, 3>& c) {
        const float v = luminance({c[0], c[1], c[2], 1}) >= t ? 1.f : 0.f;
        c = {v, v, v};
    });
}

QImage toGrayscale(const QImage& in)
{
    return perPixel(in, [](std::array<float, 3>& c) {
        const float v = luminance({c[0], c[1], c[2], 1});
        c = {v, v, v};
    });
}

QImage toPalette(const QImage& in, int colors)
{
    Q_UNUSED(colors);
    // Qt wählt eine Palette (Median Cut gibt es nicht: feste 256 Farben mit Fehlerstreuung)
    const QImage indexed = in.convertToFormat(QImage::Format_RGB32).convertToFormat(QImage::Format_Indexed8, Qt::DiffuseDither);
    QImage out = indexed.convertToFormat(QImage::Format_RGBA64);
    // Transparenz übernehmen
    const QImage alpha = in.convertToFormat(QImage::Format_RGBA64);
    for (int y = 0; y < out.height(); ++y) {
        auto* o = reinterpret_cast<QRgba64*>(out.scanLine(y));
        const auto* a = reinterpret_cast<const QRgba64*>(alpha.constScanLine(y));
        for (int x = 0; x < out.width(); ++x)
            o[x].setAlpha(a[x].alpha());
    }
    return out;
}

QImage colorMask(const QImage& in, const QList<Color>& colors, double tolerance)
{
    QImage out = in.convertToFormat(QImage::Format_RGBA64);
    const double t = std::clamp(tolerance, 0.0, 100.0) / 100.0 * 441.7; // max. Abstand im RGB-Würfel
    for (int y = 0; y < out.height(); ++y) {
        auto* line = reinterpret_cast<QRgba64*>(out.scanLine(y));
        for (int x = 0; x < out.width(); ++x) {
            const double r = line[x].red8(), g = line[x].green8(), b = line[x].blue8();
            for (const Color& c : colors) {
                if (std::hypot(std::hypot(r - c.r, g - c.g), b - c.b) <= t + 0.5) {
                    line[x] = qRgba64(0, 0, 0, 0);
                    break;
                }
            }
        }
    }
    return out;
}

namespace {

// Werte auf 8 Bit runden (Modus "8 Bit"); 16 Bit bleiben wie gerechnet
float depth(float v, int bits)
{
    return bits >= 16 ? v : std::round(std::clamp(v, 0.f, 1.f) * 255.f) / 255.f;
}

// Projektive Abbildung des ganzen Bildes auf ein Viereck (Pixel); ausserhalb durchsichtig
QImage mapToQuad(const QImage& in, const QPolygonF& quad)
{
    const Buf b(in);
    QTransform forward;
    if (!QTransform::quadToQuad(QPolygonF(QRectF(0, 0, b.w, b.h)), quad, forward))
        return in.convertToFormat(QImage::Format_RGBA64);
    bool invertible = false;
    const QTransform back = forward.inverted(&invertible);
    if (!invertible)
        return in.convertToFormat(QImage::Format_RGBA64);
    Buf out(b.w, b.h);
    for (int y = 0; y < b.h; ++y)
        for (int x = 0; x < b.w; ++x) {
            const QPointF s = back.map(QPointF(x + 0.5, y + 0.5)) - QPointF(0.5, 0.5);
            // halbes Pixel Saum, damit die Kante weich ausläuft
            if (s.x() < -0.5 || s.y() < -0.5 || s.x() > b.w - 0.5 || s.y() > b.h - 0.5)
                continue;
            std::array<float, 4> p = b.sample(s.x(), s.y());
            const float edge = float(std::clamp(std::min({s.x() + 0.5, s.y() + 0.5, b.w - 0.5 - s.x(), b.h - 0.5 - s.y()}), 0.0, 1.0));
            for (float& v : p)
                v *= edge;
            out.at(x, y) = p;
        }
    return out.image();
}

// Viereck so verschieben und skalieren, dass es mittig in w × h passt
QPolygonF fitted(QPolygonF quad, double w, double h)
{
    const QRectF box = quad.boundingRect();
    const double scale = std::min(w / box.width(), h / box.height());
    for (QPointF& p : quad)
        p = QPointF(w / 2 + (p.x() - box.center().x()) * scale, h / 2 + (p.y() - box.center().y()) * scale);
    return quad;
}

} // namespace

QImage toGrayscale(const QImage& in, int bits)
{
    return perPixel(in, [bits](std::array<float, 3>& c) {
        const float v = depth(luminance({c[0], c[1], c[2], 1}), bits);
        c = {v, v, v};
    });
}

QImage duotone(const QImage& in, const Color& ink1, const Color& ink2, double balance, int bits)
{
    // Wie im Druck: jede Farbe deckt je nach Tonwert, Papier weiss, Farben multiplizieren sich.
    // Farbe 1 trägt die Tiefen, Farbe 2 die Mitteltöne; balance verschiebt das Gewicht.
    const float w = float(std::clamp(balance, 0.0, 100.0) / 100.0);
    const float gamma1 = 0.7f + 1.6f * w;       // höher = Farbe 1 nur noch in den Tiefen
    const float gamma2 = 1.6f - 1.0f * w;       // tiefer = Farbe 2 reicht weiter ins Helle
    const std::array<float, 3> a1{1 - ink1.r / 255.f, 1 - ink1.g / 255.f, 1 - ink1.b / 255.f};
    const std::array<float, 3> a2{1 - ink2.r / 255.f, 1 - ink2.g / 255.f, 1 - ink2.b / 255.f};
    return perPixel(in, [=](std::array<float, 3>& c) {
        const float t = 1.f - depth(luminance({c[0], c[1], c[2], 1}), bits); // Deckung 0 = Papier
        const float c1 = std::pow(t, gamma1);
        const float c2 = std::pow(t, gamma2) * (0.35f + 0.65f * w);
        for (int i = 0; i < 3; ++i)
            c[i] = depth((1.f - c1 * a1[i]) * (1.f - c2 * a2[i]), bits);
    });
}

QImage straighten(const QImage& in, double angleDeg, bool crop)
{
    const double w = in.width(), h = in.height();
    const double a = qDegreesToRadians(angleDeg);
    // Grösstes Rechteck mit gleichem Seitenverhältnis innerhalb des gedrehten Bildes
    const double c = std::abs(std::cos(a)), sn = std::abs(std::sin(a));
    // + gut ein Pixel, damit auch die Eckpixel ganz im Bild liegen (weicher Rand)
    const double scale = crop ? std::max((w * c + h * sn) / w, (w * sn + h * c) / h) * (1.0 + 1.5 / std::min(w, h)) : 1.0;
    QTransform t;
    t.translate(w / 2, h / 2);
    t.rotateRadians(a);
    t.scale(scale, scale);
    t.translate(-w / 2, -h / 2);
    return mapToQuad(in, t.map(QPolygonF(QRectF(0, 0, w, h))));
}

QImage rotate3d(const QImage& in, double verticalDeg, double horizontalDeg)
{
    const double w = in.width(), h = in.height();
    const double ax = qDegreesToRadians(std::clamp(verticalDeg, -75.0, 75.0));   // um die waagrechte Achse
    const double ay = qDegreesToRadians(std::clamp(horizontalDeg, -75.0, 75.0)); // um die senkrechte Achse
    const double camera = 2.0 * std::max(w, h);
    QPolygonF quad;
    for (const QPointF& corner : {QPointF(-w / 2, -h / 2), QPointF(w / 2, -h / 2), QPointF(w / 2, h / 2), QPointF(-w / 2, h / 2)}) {
        // erst um y, dann um x drehen; z zeigt zum Betrachter
        double x = corner.x(), y = corner.y(), z = 0;
        const double x1 = x * std::cos(ay) + z * std::sin(ay);
        const double z1 = -x * std::sin(ay) + z * std::cos(ay);
        const double y2 = y * std::cos(ax) - z1 * std::sin(ax);
        const double z2 = y * std::sin(ax) + z1 * std::cos(ax);
        const double f = camera / (camera - z2);
        quad << QPointF(x1 * f, y2 * f);
    }
    return mapToQuad(in, fitted(quad, w, h));
}

QImage perspective(const QImage& in, double horizontal, double vertical)
{
    const double w = in.width(), h = in.height();
    const double ph = std::clamp(horizontal, -100.0, 100.0) / 100.0 * 0.45; // Anteil der Kantenverkürzung
    const double pv = std::clamp(vertical, -100.0, 100.0) / 100.0 * 0.45;
    // Ecken: oben links, oben rechts, unten rechts, unten links
    QPolygonF quad{QPointF(0, 0), QPointF(w, 0), QPointF(w, h), QPointF(0, h)};
    // waagrecht: rechte (+) oder linke (−) Kante wird kürzer
    if (ph > 0) {
        quad[1].ry() += h * ph;
        quad[2].ry() -= h * ph;
    } else {
        quad[0].ry() -= h * ph;
        quad[3].ry() += h * ph;
    }
    // senkrecht: obere (+) oder untere (−) Kante wird kürzer
    if (pv > 0) {
        quad[0].rx() += w * pv;
        quad[1].rx() -= w * pv;
    } else {
        quad[3].rx() -= w * pv;
        quad[2].rx() += w * pv;
    }
    return mapToQuad(in, fitted(quad, w, h));
}

} // namespace vek::filters
