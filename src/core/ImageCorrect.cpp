#include "ImageCorrect.h"
#include "ImageAdjust.h"

#include <QList>
#include <QPointF>

#include <algorithm>
#include <cmath>
#include <vector>

namespace vek::adjust {

namespace {

// Pixel unvormultipliziert als 0 … 1 bearbeiten; fn(r, g, b) ändert die Werte an Ort und Stelle
template <class F>
QImage eachPixel(const QImage& image, F fn)
{
    QImage out = image.convertToFormat(QImage::Format_RGBA64);
    for (int y = 0; y < out.height(); ++y) {
        auto* line = reinterpret_cast<QRgba64*>(out.scanLine(y));
        for (int x = 0; x < out.width(); ++x) {
            QRgba64& p = line[x];
            double c[3] = {p.red() / 65535.0, p.green() / 65535.0, p.blue() / 65535.0};
            fn(c);
            auto ch = [](double v) { return quint16(std::lround(std::clamp(v, 0.0, 1.0) * 65535.0)); };
            p = QRgba64::fromRgba64(ch(c[0]), ch(c[1]), ch(c[2]), p.alpha());
        }
    }
    return out;
}

double luma(const double* c)
{
    return 0.299 * c[0] + 0.587 * c[1] + 0.114 * c[2];
}

// HSL mit Farbton in Grad (0 … 360), Sättigung und Helligkeit 0 … 1
struct Hsl {
    double h = 0, s = 0, l = 0;
};

Hsl toHsl(double r, double g, double b)
{
    const double mx = std::max({r, g, b}), mn = std::min({r, g, b});
    Hsl o;
    o.l = (mx + mn) / 2.0;
    const double d = mx - mn;
    if (d < 1e-9)
        return o;
    o.s = o.l > 0.5 ? d / (2.0 - mx - mn) : d / (mx + mn);
    if (mx == r)
        o.h = std::fmod((g - b) / d + 6.0, 6.0);
    else if (mx == g)
        o.h = (b - r) / d + 2.0;
    else
        o.h = (r - g) / d + 4.0;
    o.h *= 60.0;
    return o;
}

void fromHsl(Hsl c, double* rgb)
{
    c.h = std::fmod(std::fmod(c.h, 360.0) + 360.0, 360.0);
    const double q = c.l < 0.5 ? c.l * (1 + c.s) : c.l + c.s - c.l * c.s;
    const double p = 2 * c.l - q;
    auto hue = [p, q](double t) {
        t = std::fmod(t + 1.0, 1.0);
        if (t < 1.0 / 6)
            return p + (q - p) * 6 * t;
        if (t < 0.5)
            return q;
        if (t < 2.0 / 3)
            return p + (q - p) * (2.0 / 3 - t) * 6;
        return p;
    };
    const double h = c.h / 360.0;
    rgb[0] = hue(h + 1.0 / 3);
    rgb[1] = hue(h);
    rgb[2] = hue(h - 1.0 / 3);
}

} // namespace

// --- Lokaler Ausgleich --------------------------------------------------------------------

QImage localEqualize(const QImage& image, int windowWidth, int windowHeight)
{
    QImage out = image.convertToFormat(QImage::Format_RGBA64);
    const int w = out.width(), h = out.height();
    if (w == 0 || h == 0)
        return out;
    const int ww = std::clamp(windowWidth, 2, w), wh = std::clamp(windowHeight, 2, h);
    const int nx = std::max(1, int(std::lround(double(w) / ww))), ny = std::max(1, int(std::lround(double(h) / wh)));
    constexpr int kBins = 256;
    constexpr double kClip = 4.0; // Begrenzung je Säule (× Durchschnitt), sonst verstärkt sich Rauschen

    std::vector<float> lum(std::size_t(w) * h);
    std::vector<bool> used(std::size_t(w) * h);
    for (int y = 0; y < h; ++y) {
        const auto* line = reinterpret_cast<const QRgba64*>(out.constScanLine(y));
        for (int x = 0; x < w; ++x) {
            const double c[3] = {line[x].red() / 65535.0, line[x].green() / 65535.0, line[x].blue() / 65535.0};
            lum[std::size_t(y) * w + x] = float(luma(c));
            used[std::size_t(y) * w + x] = line[x].alpha() > 0;
        }
    }
    // Je Fenster eine Abbildung (kumuliertes, begrenztes Histogramm, kBins + 1 Stützstellen)
    std::vector<std::vector<double>> maps(std::size_t(nx) * ny);
    for (int ty = 0; ty < ny; ++ty)
        for (int tx = 0; tx < nx; ++tx) {
            const int x0 = tx * w / nx, x1 = (tx + 1) * w / nx, y0 = ty * h / ny, y1 = (ty + 1) * h / ny;
            std::vector<double> hist(kBins);
            double total = 0;
            for (int y = y0; y < y1; ++y)
                for (int x = x0; x < x1; ++x)
                    if (used[std::size_t(y) * w + x]) {
                        hist[std::size_t(std::clamp(int(lum[std::size_t(y) * w + x] * kBins), 0, kBins - 1))] += 1;
                        total += 1;
                    }
            std::vector<double>& map = maps[std::size_t(ty) * nx + tx];
            map.resize(kBins + 1);
            if (total <= 0) {
                for (int i = 0; i <= kBins; ++i)
                    map[std::size_t(i)] = double(i) / kBins;
                continue;
            }
            const double limit = kClip * total / kBins;
            double excess = 0;
            for (double& v : hist)
                if (v > limit) {
                    excess += v - limit;
                    v = limit;
                }
            for (double& v : hist)
                v += excess / kBins;
            double sum = 0;
            map[0] = 0;
            for (int i = 0; i < kBins; ++i) {
                sum += hist[std::size_t(i)];
                map[std::size_t(i + 1)] = sum / total;
            }
        }
    // Abbildung eines Fensters stufenlos (linear zwischen den Stützstellen)
    auto mapped = [&](int tx, int ty, double v) {
        const std::vector<double>& map = maps[std::size_t(ty) * nx + tx];
        const double pos = std::clamp(v, 0.0, 1.0) * kBins;
        const int i = std::min(int(pos), kBins - 1);
        return map[std::size_t(i)] + (map[std::size_t(i + 1)] - map[std::size_t(i)]) * (pos - i);
    };
    for (int y = 0; y < h; ++y) {
        auto* line = reinterpret_cast<QRgba64*>(out.scanLine(y));
        // Fenstermitten bilinear überblenden
        const double fy = std::clamp((y + 0.5) / h * ny - 0.5, 0.0, ny - 1.0);
        const int ty0 = int(fy), ty1 = std::min(ty0 + 1, ny - 1);
        const double ay = fy - ty0;
        for (int x = 0; x < w; ++x) {
            if (!used[std::size_t(y) * w + x])
                continue;
            const double fx = std::clamp((x + 0.5) / w * nx - 0.5, 0.0, nx - 1.0);
            const int tx0 = int(fx), tx1 = std::min(tx0 + 1, nx - 1);
            const double ax = fx - tx0;
            const double l = lum[std::size_t(y) * w + x];
            const double top = mapped(tx0, ty0, l) * (1 - ax) + mapped(tx1, ty0, l) * ax;
            const double bottom = mapped(tx0, ty1, l) * (1 - ax) + mapped(tx1, ty1, l) * ax;
            const double delta = top * (1 - ay) + bottom * ay - l;
            QRgba64& p = line[x];
            auto ch = [delta](quint16 v) { return quint16(std::lround(std::clamp(v / 65535.0 + delta, 0.0, 1.0) * 65535.0)); };
            p = QRgba64::fromRgba64(ch(p.red()), ch(p.green()), ch(p.blue()), p.alpha());
        }
    }
    return out;
}

// --- Proben-/Zielabgleich -----------------------------------------------------------------

QImage sampleTargetBalance(const QImage& image, const std::array<Color, 3>& samples, const std::array<Color, 3>& targets)
{
    QImage out = image.convertToFormat(QImage::Format_RGBA64);
    for (int c = 0; c < 3; ++c) {
        auto part = [c](const Color& color) { return double(c == 0 ? color.r : c == 1 ? color.g : color.b); };
        QList<QPointF> points;
        bool identity = true;
        for (int i = 0; i < 3; ++i) {
            points << QPointF(part(samples[std::size_t(i)]), part(targets[std::size_t(i)]));
            identity = identity && points.last().x() == points.last().y();
        }
        if (!identity)
            out = toneCurve(out, points, c);
    }
    return out;
}

// --- Selektive Farbe ----------------------------------------------------------------------

bool SelectiveColor::isIdentity() const
{
    for (const auto& range : cmyk)
        for (int v : range)
            if (v != 0)
                return false;
    return true;
}

double colorRangeWeight(double r, double g, double b, ColorRange range)
{
    const double mx = std::max({r, g, b}), mn = std::min({r, g, b});
    const double mid = r + g + b - mx - mn;
    switch (range) {
    // Grundfarben: der Kanal ist der grösste, Anteil = Abstand zum mittleren
    case ColorRange::Reds: return r == mx && mx > mid ? mx - mid : 0.0;
    case ColorRange::Greens: return g == mx && r != mx && mx > mid ? mx - mid : 0.0;
    case ColorRange::Blues: return b == mx && r != mx && g != mx && mx > mid ? mx - mid : 0.0;
    // Mischfarben: der fehlende Kanal ist der kleinste, Anteil = Abstand vom mittleren zum kleinsten
    case ColorRange::Cyans: return r == mn && mid > mn ? mid - mn : 0.0;
    case ColorRange::Magentas: return g == mn && r != mn && mid > mn ? mid - mn : 0.0;
    case ColorRange::Yellows: return b == mn && r != mn && g != mn && mid > mn ? mid - mn : 0.0;
    case ColorRange::Whites: return mn > 0.5 ? (mn - 0.5) * 2.0 : 0.0;
    case ColorRange::Blacks: return mx < 0.5 ? (0.5 - mx) * 2.0 : 0.0;
    case ColorRange::Neutrals: return std::max(0.0, 1.0 - (std::abs(mx - 0.5) + std::abs(mn - 0.5)));
    }
    return 0.0;
}

QImage selectiveColor(const QImage& image, const SelectiveColor& s)
{
    if (s.isIdentity())
        return image.convertToFormat(QImage::Format_RGBA64);
    return eachPixel(image, [&s](double* c) {
        double delta[3] = {0, 0, 0};
        for (int range = 0; range < kColorRanges; ++range) {
            const auto& adj = s.cmyk[std::size_t(range)];
            if (adj == std::array<int, 4>{})
                continue;
            const double w = colorRangeWeight(c[0], c[1], c[2], ColorRange(range));
            if (w <= 0)
                continue;
            for (int ch = 0; ch < 3; ++ch) {
                // Farbmenge (Cyan zu Rot, Magenta zu Grün, Gelb zu Blau); Schwarz wirkt auf alle drei
                const double ink = 1.0 - c[ch];
                const double amount = (adj[std::size_t(ch)] + adj[3]) / 100.0;
                // relativ: Wegnehmen anteilig zur vorhandenen Menge, Hinzufügen halb fest, halb anteilig
                const double scale = amount < 0 ? ink : 0.5 + 0.5 * ink;
                delta[ch] += w * (s.relative ? amount * scale : amount);
            }
        }
        for (int ch = 0; ch < 3; ++ch)
            c[ch] -= delta[ch];
    });
}

// --- Farben ersetzen ----------------------------------------------------------------------

QImage replaceColors(const QImage& image, const ReplaceColors& s)
{
    const Hsl from = toHsl(s.oldColor.r / 255.0, s.oldColor.g / 255.0, s.oldColor.b / 255.0);
    const Hsl to = toHsl(s.newColor.r / 255.0, s.newColor.g / 255.0, s.newColor.b / 255.0);
    const double o[3] = {s.oldColor.r / 255.0, s.oldColor.g / 255.0, s.oldColor.b / 255.0};
    // Bereich 100 reicht etwa über die halbe Raumdiagonale des Farbwürfels
    const double tolerance = std::clamp(s.range, 0, 100) / 100.0 * 0.6;
    const bool grayOld = from.s < 0.05;
    return eachPixel(image, [&](double* c) {
        const double d = std::sqrt((c[0] - o[0]) * (c[0] - o[0]) + (c[1] - o[1]) * (c[1] - o[1]) + (c[2] - o[2]) * (c[2] - o[2])) / std::sqrt(3.0);
        double w = 0;
        if (d <= tolerance * 0.5 || d < 1e-6)
            w = 1;
        else if (d < tolerance) {
            const double t = (d - tolerance * 0.5) / (tolerance * 0.5);
            w = 1 - t * t * (3 - 2 * t);
        }
        if (w <= 0)
            return;
        Hsl p = toHsl(c[0], c[1], c[2]);
        p.h = (grayOld ? to.h : p.h + to.h - from.h) + s.hue;
        p.s = std::clamp(p.s + to.s - from.s + s.saturation / 100.0, 0.0, 1.0);
        p.l = std::clamp(p.l + to.l - from.l + s.lightness / 100.0, 0.0, 1.0);
        double n[3];
        fromHsl(p, n);
        for (int ch = 0; ch < 3; ++ch)
            c[ch] += (n[ch] - c[ch]) * w;
    });
}

// --- Kanalmixer ---------------------------------------------------------------------------

bool ChannelMixer::isIdentity() const
{
    return rows == ChannelMixer().rows;
}

QImage channelMixer(const QImage& image, const ChannelMixer& m)
{
    if (m.isIdentity())
        return image.convertToFormat(QImage::Format_RGBA64);
    return eachPixel(image, [&m](double* c) {
        const double in[3] = {c[0], c[1], c[2]};
        for (int out = 0; out < 3; ++out) {
            const auto& row = m.rows[std::size_t(out)];
            c[out] = (row[0] * in[0] + row[1] * in[1] + row[2] * in[2] + row[3]) / 100.0;
        }
    });
}

} // namespace vek::adjust
