#include "ColorTransfer.h"

#include <QColor>
#include <QColorSpace>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace vek::colortransfer {

namespace {

constexpr int kGrid = 17;      // Stützstellen je Farbachse
constexpr int kFitWidth = 256; // Arbeitsbreite für das Lernen der Farbabbildung
constexpr double kPrior = 4.0; // Gewicht des Polynoms in dünn besetzten Zellen

// Drei Farbebenen in Gleitkomma (0..1)
struct Planes {
    int w = 0, h = 0;
    std::vector<float> c[3];
    void resize(int width, int height)
    {
        w = width;
        h = height;
        for (auto& p : c)
            p.assign(size_t(w) * size_t(h), 0.0f);
    }
};

Planes planesOf(const QImage& image, int w, int h)
{
    const QImage img = image.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_RGBA64);
    Planes p;
    p.resize(w, h);
    for (int y = 0; y < h; ++y) {
        const auto* line = reinterpret_cast<const QRgba64*>(img.constScanLine(y));
        for (int x = 0; x < w; ++x) {
            const size_t i = size_t(y) * size_t(w) + size_t(x);
            p.c[0][i] = line[x].red() / 65535.0f;
            p.c[1][i] = line[x].green() / 65535.0f;
            p.c[2][i] = line[x].blue() / 65535.0f;
        }
    }
    return p;
}

// Gauss-Näherung: dreimal Kastenfilter, Ränder fortgesetzt
void boxPass(std::vector<float>& v, int w, int h, int r, bool horizontal)
{
    const int n = horizontal ? w : h;
    const int lines = horizontal ? h : w;
    const size_t len = size_t(n);
    std::vector<float> line(len), out(len);
    const float norm = 1.0f / float(2 * r + 1);
    for (int l = 0; l < lines; ++l) {
        auto at = [&](int k) -> float& { return horizontal ? v[size_t(l) * size_t(w) + size_t(k)] : v[size_t(k) * size_t(w) + size_t(l)]; };
        for (int k = 0; k < n; ++k)
            line[size_t(k)] = at(k);
        auto px = [&](int k) { return line[size_t(std::clamp(k, 0, n - 1))]; };
        double sum = 0.0;
        for (int k = -r; k <= r; ++k)
            sum += px(k);
        for (int k = 0; k < n; ++k) {
            out[size_t(k)] = float(sum) * norm;
            sum += px(k + r + 1) - px(k - r);
        }
        for (int k = 0; k < n; ++k)
            at(k) = out[size_t(k)];
    }
}

void blur(Planes& p, double sigma)
{
    // drei Durchgänge mit Radius r haben die Varianz r(r+1)
    const int r = std::max(1, int(std::lround((std::sqrt(1.0 + 4.0 * sigma * sigma) - 1.0) / 2.0)));
    for (auto& c : p.c)
        for (int pass = 0; pass < 3; ++pass) {
            boxPass(c, p.w, p.h, r, true);
            boxPass(c, p.w, p.h, r, false);
        }
}

constexpr int kTerms = 10;

std::array<double, kTerms> features(double r, double g, double b)
{
    return {1.0, r, g, b, r * r, g * g, b * b, r * g, r * b, g * b};
}

// Kleinste Quadrate über die Normalgleichungen (10x10, Gauss mit Pivot)
std::array<std::array<double, kTerms>, 3> fitPolynomial(const Planes& a, const Planes& b)
{
    double m[kTerms][kTerms] = {};
    double rhs[3][kTerms] = {};
    const size_t n = a.c[0].size();
    for (size_t i = 0; i < n; ++i) {
        const auto f = features(a.c[0][i], a.c[1][i], a.c[2][i]);
        for (int r = 0; r < kTerms; ++r) {
            for (int c = 0; c < kTerms; ++c)
                m[r][c] += f[size_t(r)] * f[size_t(c)];
            for (int ch = 0; ch < 3; ++ch)
                rhs[ch][r] += f[size_t(r)] * b.c[ch][i];
        }
    }
    for (int r = 0; r < kTerms; ++r)
        m[r][r] += 1e-6 * double(n);
    std::array<std::array<double, kTerms>, 3> coef{};
    for (int ch = 0; ch < 3; ++ch) {
        double a2[kTerms][kTerms + 1];
        for (int r = 0; r < kTerms; ++r) {
            for (int c = 0; c < kTerms; ++c)
                a2[r][c] = m[r][c];
            a2[r][kTerms] = rhs[ch][r];
        }
        for (int col = 0; col < kTerms; ++col) {
            int pivot = col;
            for (int r = col + 1; r < kTerms; ++r)
                if (std::abs(a2[r][col]) > std::abs(a2[pivot][col]))
                    pivot = r;
            for (int c = 0; c <= kTerms; ++c)
                std::swap(a2[col][c], a2[pivot][c]);
            if (std::abs(a2[col][col]) < 1e-12)
                continue;
            for (int r = 0; r < kTerms; ++r) {
                if (r == col)
                    continue;
                const double f = a2[r][col] / a2[col][col];
                for (int c = col; c <= kTerms; ++c)
                    a2[r][c] -= f * a2[col][c];
            }
        }
        for (int r = 0; r < kTerms; ++r)
            coef[size_t(ch)][size_t(r)] = std::abs(a2[r][r]) < 1e-12 ? 0.0 : a2[r][kTerms] / a2[r][r];
    }
    return coef;
}

struct Lut {
    std::vector<float> v; // (r * N + g) * N + b, je 3 Werte

    void lookup(float r, float g, float b, float out[3]) const
    {
        const float s = float(kGrid - 1);
        const float pr = std::clamp(r, 0.0f, 1.0f) * s, pg = std::clamp(g, 0.0f, 1.0f) * s, pb = std::clamp(b, 0.0f, 1.0f) * s;
        const int ir = std::min(int(pr), kGrid - 2), ig = std::min(int(pg), kGrid - 2), ib = std::min(int(pb), kGrid - 2);
        const float fr = pr - float(ir), fg = pg - float(ig), fb = pb - float(ib);
        out[0] = out[1] = out[2] = 0.0f;
        for (int dr = 0; dr < 2; ++dr)
            for (int dg = 0; dg < 2; ++dg)
                for (int db = 0; db < 2; ++db) {
                    const float w = (dr ? fr : 1 - fr) * (dg ? fg : 1 - fg) * (db ? fb : 1 - fb);
                    const float* cell = &v[size_t(((ir + dr) * kGrid + (ig + dg)) * kGrid + (ib + db)) * 3];
                    out[0] += w * cell[0];
                    out[1] += w * cell[1];
                    out[2] += w * cell[2];
                }
    }
};

Lut fitLut(const Planes& a, const Planes& b)
{
    const auto coef = fitPolynomial(a, b);
    const size_t cells = size_t(kGrid) * kGrid * kGrid;
    std::vector<double> acc(cells * 3, 0.0), weight(cells, 0.0);
    const float s = float(kGrid - 1);
    for (size_t i = 0; i < a.c[0].size(); ++i) {
        const float pr = a.c[0][i] * s, pg = a.c[1][i] * s, pb = a.c[2][i] * s;
        const int ir = std::clamp(int(pr), 0, kGrid - 2), ig = std::clamp(int(pg), 0, kGrid - 2), ib = std::clamp(int(pb), 0, kGrid - 2);
        const float fr = pr - float(ir), fg = pg - float(ig), fb = pb - float(ib);
        for (int dr = 0; dr < 2; ++dr)
            for (int dg = 0; dg < 2; ++dg)
                for (int db = 0; db < 2; ++db) {
                    const double w = (dr ? fr : 1 - fr) * (dg ? fg : 1 - fg) * (db ? fb : 1 - fb);
                    const size_t cell = size_t(((ir + dr) * kGrid + (ig + dg)) * kGrid + (ib + db));
                    weight[cell] += w;
                    for (int ch = 0; ch < 3; ++ch)
                        acc[cell * 3 + size_t(ch)] += w * b.c[ch][i];
                }
    }
    Lut lut;
    lut.v.resize(cells * 3);
    for (int r = 0; r < kGrid; ++r)
        for (int g = 0; g < kGrid; ++g)
            for (int bl = 0; bl < kGrid; ++bl) {
                const size_t cell = size_t((r * kGrid + g) * kGrid + bl);
                const auto f = features(r / s, g / s, bl / s);
                for (int ch = 0; ch < 3; ++ch) {
                    double poly = 0.0;
                    for (int t = 0; t < kTerms; ++t)
                        poly += coef[size_t(ch)][size_t(t)] * f[size_t(t)];
                    const double value = (acc[cell * 3 + size_t(ch)] + kPrior * poly) / (weight[cell] + kPrior);
                    lut.v[cell * 3 + size_t(ch)] = float(std::clamp(value, 0.0, 1.0));
                }
            }
    return lut;
}

// Helligkeitsgradienten (dx, dy) einer verkleinerten Fassung
std::vector<std::pair<float, float>> gradients(const QImage& image, int w, int h)
{
    const QImage g = image.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_Grayscale8);
    std::vector<std::pair<float, float>> out(size_t(w) * size_t(h), {0.0f, 0.0f});
    for (int y = 1; y < h - 1; ++y) {
        const uchar* up = g.constScanLine(y - 1);
        const uchar* mid = g.constScanLine(y);
        const uchar* down = g.constScanLine(y + 1);
        for (int x = 1; x < w - 1; ++x)
            out[size_t(y) * size_t(w) + size_t(x)] = {float(mid[x + 1] - mid[x - 1]), float(down[x] - up[x])};
    }
    return out;
}

} // namespace

double alignment(const QImage& a, const QImage& b)
{
    // Richtungsvergleich der Gradienten, gewichtet mit ihrer Stärke: unempfindlich gegen andere
    // Helligkeit, Kontrast und Farben, aber nicht gegen Verschieben oder Neuzeichnen
    if (a.isNull() || b.isNull())
        return 0.0;
    const int w = 192;
    const int h = std::max(8, int(std::lround(double(w) * a.height() / std::max(1, a.width()))));
    const auto ga = gradients(a, w, h), gb = gradients(b, w, h);
    double agree = 0.0, weight = 0.0;
    for (size_t i = 0; i < ga.size(); ++i) {
        const double ma = std::hypot(ga[i].first, ga[i].second), mb = std::hypot(gb[i].first, gb[i].second);
        if (ma < 2.0 || mb < 2.0)
            continue;
        const double wgt = std::sqrt(ma * mb);
        agree += wgt * (ga[i].first * gb[i].first + ga[i].second * gb[i].second) / (ma * mb);
        weight += wgt;
    }
    return weight > 0.0 ? std::clamp(agree / weight, 0.0, 1.0) : 0.0;
}

Result transfer(const QImage& original, const QImage& edited, double local)
{
    Result result;
    if (original.isNull() || edited.isNull()) {
        result.image = original;
        return result;
    }
    const int fw = std::min(kFitWidth, original.width());
    const int fh = std::max(1, int(std::lround(double(fw) * original.height() / original.width())));
    Planes a = planesOf(original, fw, fh);
    Planes b = planesOf(edited, fw, fh);
    // leicht glätten: kleine Verschiebungen und Rauschen der KI stören die Farbabbildung nicht
    blur(a, 1.0);
    blur(b, 1.0);
    const Lut lut = fitLut(a, b);

    result.alignment = alignment(original, edited);
    // bis 0,3 keine lokale Korrektur, ab 0,7 die volle
    result.localUsed = std::clamp(local, 0.0, 1.0) * std::clamp((result.alignment - 0.3) / 0.4, 0.0, 1.0);

    // Lokaler Rest in Arbeitsauflösung
    Planes residual;
    if (result.localUsed > 0.0) {
        const int rw = std::min(1024, original.width());
        const int rh = std::max(1, int(std::lround(double(rw) * original.height() / original.width())));
        Planes g = planesOf(original, rw, rh);
        for (size_t i = 0; i < g.c[0].size(); ++i) {
            float out[3];
            lut.lookup(g.c[0][i], g.c[1][i], g.c[2][i], out);
            for (int ch = 0; ch < 3; ++ch)
                g.c[ch][i] = out[ch];
        }
        Planes k = planesOf(edited, rw, rh);
        const double sigma = double(rw) / (40.0 + 210.0 * result.localUsed);
        blur(g, sigma);
        blur(k, sigma);
        residual.resize(rw, rh);
        for (int ch = 0; ch < 3; ++ch)
            for (size_t i = 0; i < residual.c[ch].size(); ++i)
                residual.c[ch][i] = k.c[ch][i] - g.c[ch][i];
    }

    const bool deep = original.depth() > 32;
    QImage src = original.convertToFormat(deep ? QImage::Format_RGBA64 : QImage::Format_ARGB32);
    QImage out(src.size(), src.format());
    out.setDotsPerMeterX(original.dotsPerMeterX());
    out.setDotsPerMeterY(original.dotsPerMeterY());
    out.setColorSpace(original.colorSpace());
    const int W = src.width(), H = src.height();
    const bool hasResidual = !residual.c[0].empty();
    const double sx = hasResidual ? double(residual.w) / W : 0.0, sy = hasResidual ? double(residual.h) / H : 0.0;
    auto sample = [&](int ch, int x, int y) {
        const double fx = std::clamp((x + 0.5) * sx - 0.5, 0.0, double(residual.w - 1));
        const double fy = std::clamp((y + 0.5) * sy - 0.5, 0.0, double(residual.h - 1));
        const int x0 = std::min(int(fx), residual.w - 2 < 0 ? 0 : residual.w - 2), y0 = std::min(int(fy), residual.h - 2 < 0 ? 0 : residual.h - 2);
        const int x1 = std::min(x0 + 1, residual.w - 1), y1 = std::min(y0 + 1, residual.h - 1);
        const double tx = fx - x0, ty = fy - y0;
        const auto& p = residual.c[ch];
        const size_t w = size_t(residual.w);
        return float((1 - ty) * ((1 - tx) * p[size_t(y0) * w + size_t(x0)] + tx * p[size_t(y0) * w + size_t(x1)])
                     + ty * ((1 - tx) * p[size_t(y1) * w + size_t(x0)] + tx * p[size_t(y1) * w + size_t(x1)]));
    };
#pragma omp parallel for schedule(static) if (W * H > 300000)
    for (int y = 0; y < H; ++y) {
        float mapped[3];
        if (deep) {
            const auto* in = reinterpret_cast<const QRgba64*>(src.constScanLine(y));
            auto* dst = reinterpret_cast<QRgba64*>(out.scanLine(y));
            for (int x = 0; x < W; ++x) {
                lut.lookup(in[x].red() / 65535.0f, in[x].green() / 65535.0f, in[x].blue() / 65535.0f, mapped);
                quint16 v[3];
                for (int ch = 0; ch < 3; ++ch) {
                    const float r = hasResidual ? sample(ch, x, y) : 0.0f;
                    v[ch] = quint16(std::lround(std::clamp(mapped[ch] + r, 0.0f, 1.0f) * 65535.0f));
                }
                dst[x] = QRgba64::fromRgba64(v[0], v[1], v[2], in[x].alpha());
            }
        } else {
            const auto* in = reinterpret_cast<const QRgb*>(src.constScanLine(y));
            auto* dst = reinterpret_cast<QRgb*>(out.scanLine(y));
            for (int x = 0; x < W; ++x) {
                lut.lookup(qRed(in[x]) / 255.0f, qGreen(in[x]) / 255.0f, qBlue(in[x]) / 255.0f, mapped);
                int v[3];
                for (int ch = 0; ch < 3; ++ch) {
                    const float r = hasResidual ? sample(ch, x, y) : 0.0f;
                    v[ch] = int(std::lround(std::clamp(mapped[ch] + r, 0.0f, 1.0f) * 255.0f));
                }
                dst[x] = qRgba(v[0], v[1], v[2], qAlpha(in[x]));
            }
        }
    }
    result.image = out;
    return result;
}

} // namespace vek::colortransfer
