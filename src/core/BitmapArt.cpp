#include "BitmapArt.h"

#include <QtMath>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <thread>
#include <vector>

namespace vek::filters {

namespace {

using Px = std::array<float, 4>;
using Rgb = std::array<float, 3>;
template <int N>
using Plane = std::vector<std::array<float, N>>;

// Bild als Gleitkomma-Puffer, Farben vormultipliziert (wie in BitmapFilters.cpp)
struct Buf {
    int w = 0, h = 0;
    std::vector<Px> px;

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

    Px& at(int x, int y) { return px[std::size_t(y) * w + x]; }
    const Px& at(int x, int y) const { return px[std::size_t(y) * w + x]; }

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

float luminance(const Rgb& c)
{
    return 0.299f * c[0] + 0.587f * c[1] + 0.114f * c[2];
}

float clamp01(float v)
{
    return std::clamp(v, 0.f, 1.f);
}

float smoothstep(float a, float b, float v)
{
    const float t = clamp01((v - a) / (b - a));
    return t * t * (3 - 2 * t);
}

// Zeilen auf alle Kerne verteilen; jede Zeile unabhängig, daher reproduzierbar
template <class F>
void parallelRows(int count, F fn)
{
    const int threads = std::clamp(int(std::thread::hardware_concurrency()), 1, 16);
    if (threads == 1 || count < 32) {
        for (int i = 0; i < count; ++i)
            fn(i);
        return;
    }
    std::atomic<int> next{0};
    std::vector<std::thread> pool;
    for (int t = 0; t < threads; ++t)
        pool.emplace_back([&] {
            for (int i = next.fetch_add(1); i < count; i = next.fetch_add(1))
                fn(i);
        });
    for (auto& t : pool)
        t.join();
}

// --- Zufall und Rauschen ---------------------------------------------------------------------

quint32 hash3(quint32 x, quint32 y, quint32 s)
{
    quint32 h = x * 0x8da6b343u ^ y * 0xd8163841u ^ s * 0xcb1ab31fu;
    h ^= h >> 16;
    h *= 0x7feb352du;
    h ^= h >> 15;
    h *= 0x846ca68bu;
    h ^= h >> 16;
    return h;
}

// Gleichverteilt 0..1 je Gitterpunkt
float rnd(int x, int y, quint32 s)
{
    return float(hash3(quint32(x), quint32(y), s) >> 8) * (1.f / 16777216.f);
}

// Glattes Wertrauschen 0..1
float valueNoise(float x, float y, quint32 s)
{
    const float fx0 = std::floor(x), fy0 = std::floor(y);
    const int ix = int(fx0), iy = int(fy0);
    float fx = x - fx0, fy = y - fy0;
    fx = fx * fx * (3 - 2 * fx);
    fy = fy * fy * (3 - 2 * fy);
    const float a = rnd(ix, iy, s), b = rnd(ix + 1, iy, s), c = rnd(ix, iy + 1, s), d = rnd(ix + 1, iy + 1, s);
    return (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy;
}

// Fraktales Rauschen 0..1
float fbm(float x, float y, quint32 s, int octaves)
{
    float sum = 0, amp = 0.5f, norm = 0;
    for (int i = 0; i < octaves; ++i) {
        sum += amp * valueNoise(x, y, s + quint32(i) * 977u);
        norm += amp;
        x = x * 2.03f + 17.1f;
        y = y * 2.03f + 5.3f;
        amp *= 0.5f;
    }
    return sum / norm;
}

// Zufallsfolge für Pinselstriche
struct Rng {
    quint32 seed, n = 0;
    explicit Rng(quint32 s) : seed(s) {}
    float operator()() { return float(hash3(n++, 0x51ed27u, seed) >> 8) * (1.f / 16777216.f); }
};

// --- Weichzeichnen ---------------------------------------------------------------------------

// Eine Zeile oder Spalte herauskopieren, filtern, zurückschreiben
template <int N, class F>
void linePass(Plane<N>& v, int w, int h, bool horizontal, F filter)
{
    const int len = horizontal ? w : h, lines = horizontal ? h : w;
    const std::size_t step = horizontal ? 1 : std::size_t(w);
    parallelRows(lines, [&](int line) {
        const std::size_t base = horizontal ? std::size_t(line) * w : std::size_t(line);
        std::vector<std::array<float, N>> src(len);
        for (int i = 0; i < len; ++i)
            src[i] = v[base + i * step];
        filter(src, [&](int i, const std::array<float, N>& p) { v[base + i * step] = p; });
    });
}

template <int N>
void boxPass(Plane<N>& v, int w, int h, int r, bool horizontal)
{
    linePass<N>(v, w, h, horizontal, [r](const std::vector<std::array<float, N>>& src, auto put) {
        const int len = int(src.size());
        auto get = [&](int i) -> const std::array<float, N>& { return src[std::clamp(i, 0, len - 1)]; };
        std::array<double, N> acc{};
        for (int i = -r; i <= r; ++i)
            for (int c = 0; c < N; ++c)
                acc[c] += get(i)[c];
        const double inv = 1.0 / (2 * r + 1);
        for (int i = 0; i < len; ++i) {
            std::array<float, N> o;
            for (int c = 0; c < N; ++c)
                o[c] = float(acc[c] * inv);
            put(i, o);
            const auto& add = get(i + r + 1);
            const auto& sub = get(i - r);
            for (int c = 0; c < N; ++c)
                acc[c] += double(add[c]) - sub[c];
        }
    });
}

template <int N>
void gaussPass(Plane<N>& v, int w, int h, const std::vector<float>& k, bool horizontal)
{
    const int r = int(k.size()) / 2;
    linePass<N>(v, w, h, horizontal, [&k, r](const std::vector<std::array<float, N>>& src, auto put) {
        const int len = int(src.size());
        for (int i = 0; i < len; ++i) {
            std::array<float, N> acc{};
            for (int j = -r; j <= r; ++j) {
                const auto& p = src[std::clamp(i + j, 0, len - 1)];
                for (int c = 0; c < N; ++c)
                    acc[c] += p[c] * k[j + r];
            }
            put(i, acc);
        }
    });
}

// Gauss: klein exakt, gross als dreifacher Kastenfilter (Laufzeit unabhängig vom Radius)
template <int N>
void blurPlane(Plane<N>& v, int w, int h, double sigma)
{
    if (sigma < 0.3 || w == 0 || h == 0)
        return;
    if (sigma < 3) {
        const int r = std::max(1, int(std::ceil(sigma * 3)));
        std::vector<float> k(2 * r + 1);
        float sum = 0;
        for (int i = -r; i <= r; ++i)
            sum += k[i + r] = float(std::exp(-i * i / (2 * sigma * sigma)));
        for (float& f : k)
            f /= sum;
        gaussPass<N>(v, w, h, k, true);
        gaussPass<N>(v, w, h, k, false);
        return;
    }
    const int r = std::max(1, int(std::lround((std::sqrt(1 + 4 * sigma * sigma) - 1) / 2)));
    for (int i = 0; i < 3; ++i)
        boxPass<N>(v, w, h, r, true);
    for (int i = 0; i < 3; ++i)
        boxPass<N>(v, w, h, r, false);
}

Buf blurredBuf(const Buf& b, double sigma)
{
    Buf out = b;
    blurPlane<4>(out.px, b.w, b.h, sigma);
    return out;
}

// Unvormultiplizierte Farben
Plane<3> colorsOf(const Buf& b)
{
    Plane<3> c(b.px.size());
    parallelRows(b.h, [&](int y) {
        for (int x = 0; x < b.w; ++x) {
            const std::size_t i = std::size_t(y) * b.w + x;
            const Px& p = b.px[i];
            c[i] = p[3] > 1e-5f ? Rgb{p[0] / p[3], p[1] / p[3], p[2] / p[3]} : Rgb{0, 0, 0};
        }
    });
    return c;
}

// Farben mit der Deckkraft des Originals zusammensetzen
QImage compose(const Plane<3>& c, const Buf& alpha)
{
    Buf out(alpha.w, alpha.h);
    parallelRows(alpha.h, [&](int y) {
        for (int x = 0; x < alpha.w; ++x) {
            const std::size_t i = std::size_t(y) * alpha.w + x;
            const float a = alpha.px[i][3];
            out.px[i] = {clamp01(c[i][0]) * a, clamp01(c[i][1]) * a, clamp01(c[i][2]) * a, a};
        }
    });
    return out.image();
}

Rgb sampleRgb(const Plane<3>& c, int w, int h, double x, double y)
{
    x = std::clamp(x, 0.0, w - 1.0);
    y = std::clamp(y, 0.0, h - 1.0);
    const int x0 = int(x), y0 = int(y);
    const int x1 = std::min(x0 + 1, w - 1), y1 = std::min(y0 + 1, h - 1);
    const float fx = float(x - x0), fy = float(y - y0);
    auto at = [&](int xx, int yy) -> const Rgb& { return c[std::size_t(yy) * w + xx]; };
    Rgb r;
    for (int k = 0; k < 3; ++k)
        r[k] = (at(x0, y0)[k] * (1 - fx) + at(x1, y0)[k] * fx) * (1 - fy) + (at(x0, y1)[k] * (1 - fx) + at(x1, y1)[k] * fx) * fy;
    return r;
}

// Helligkeit −1..1: aufhellen zu Weiss, abdunkeln zu Schwarz
float brighten(float v, float b)
{
    return b >= 0 ? v + (1 - v) * b : v * (1 + b);
}

// Sättigung um den Faktor f ändern
Rgb saturate(const Rgb& c, float f)
{
    const float l = luminance(c);
    return {l + (c[0] - l) * f, l + (c[1] - l) * f, l + (c[2] - l) * f};
}

// Kuwahara-Filter: je Pixel das Mittel der ruhigsten Quadranten (weich gewichtet)
Plane<3> kuwahara(const Plane<3>& c, int w, int h, int r)
{
    // Kastenmittel über [x, x+r] × [y, y+r] von R, G, B, L, L²
    Plane<5> m(c.size());
    for (std::size_t i = 0; i < c.size(); ++i) {
        const float l = luminance(c[i]);
        m[i] = {c[i][0], c[i][1], c[i][2], l, l * l};
    }
    const int k = r + 1;
    for (int pass = 0; pass < 2; ++pass)
        linePass<5>(m, w, h, pass == 0, [k](const std::vector<std::array<float, 5>>& src, auto put) {
            const int len = int(src.size());
            auto get = [&](int i) -> const std::array<float, 5>& { return src[std::clamp(i, 0, len - 1)]; };
            std::array<double, 5> acc{};
            for (int i = 0; i < k; ++i)
                for (int ch = 0; ch < 5; ++ch)
                    acc[ch] += get(i)[ch];
            for (int i = 0; i < len; ++i) {
                std::array<float, 5> o;
                for (int ch = 0; ch < 5; ++ch)
                    o[ch] = float(acc[ch] / k);
                put(i, o);
                for (int ch = 0; ch < 5; ++ch)
                    acc[ch] += double(get(i + k)[ch]) - get(i)[ch];
            }
        });
    Plane<3> out(c.size());
    parallelRows(h, [&](int y) {
        for (int x = 0; x < w; ++x) {
            const int xs[2] = {std::max(0, x - r), x}, ys[2] = {std::max(0, y - r), y};
            double sum[3] = {0, 0, 0}, wsum = 0;
            for (int qy = 0; qy < 2; ++qy)
                for (int qx = 0; qx < 2; ++qx) {
                    const auto& q = m[std::size_t(ys[qy]) * w + xs[qx]];
                    const double var = std::max(0.f, q[4] - q[3] * q[3]);
                    const double wt = 1.0 / std::pow(1e-4 + var, 2.0);
                    for (int ch = 0; ch < 3; ++ch)
                        sum[ch] += q[ch] * wt;
                    wsum += wt;
                }
            out[std::size_t(y) * w + x] = {float(sum[0] / wsum), float(sum[1] / wsum), float(sum[2] / wsum)};
        }
    });
    return out;
}

} // namespace

// --- Farbänderung ----------------------------------------------------------------------------

QImage posterize(const QImage& in, int levels)
{
    const Buf b(in);
    const float n = float(std::clamp(levels, 2, 32) - 1);
    Plane<3> c = colorsOf(b);
    for (Rgb& p : c)
        for (float& v : p)
            v = std::round(clamp01(v) * n) / n;
    return compose(c, b);
}

QImage soften(const QImage& in, double percent)
{
    const float t = float(std::clamp(percent, 0.0, 100.0) / 100);
    Buf b(in);
    if (t <= 0)
        return b.image();
    // Kleiner Weichzeichner, nur anteilig beigemischt
    const Buf s = blurredBuf(b, 0.6 + 0.9 * t);
    // Deckkraft bleibt die des Originals, nur die Farbe wird weicher
    const float k = 0.85f * t;
    for (std::size_t i = 0; i < b.px.size(); ++i) {
        Px m;
        for (int c = 0; c < 4; ++c)
            m[c] = b.px[i][c] + (s.px[i][c] - b.px[i][c]) * k;
        const float a = b.px[i][3];
        const float f = m[3] > 1e-5f ? a / m[3] : 0.f;
        b.px[i] = {m[0] * f, m[1] * f, m[2] * f, a};
    }
    return b.image();
}

// --- Künstlerische Striche -------------------------------------------------------------------

QImage impressionism(const QImage& in, double strokeSize, double coloration, double brightness, quint32 seed)
{
    const Buf src(in);
    const int w = src.w, h = src.h;
    if (w == 0 || h == 0)
        return src.image();
    const double s = std::clamp(strokeSize, 3.0, 100.0);
    const float colo = float(std::clamp(coloration, 0.0, 100.0) / 100);
    const float bright = float(std::clamp(brightness, -100.0, 100.0) / 100);

    // Strichrichtung entlang der Bildstruktur (Strukturtensor)
    Plane<3> tensor(src.px.size());
    {
        const Plane<3> g = colorsOf(blurredBuf(src, s * 0.35));
        parallelRows(h, [&](int y) {
            for (int x = 0; x < w; ++x) {
                auto l = [&](int xx, int yy) { return luminance(g[std::size_t(std::clamp(yy, 0, h - 1)) * w + std::clamp(xx, 0, w - 1)]); };
                const float gx = l(x + 1, y) - l(x - 1, y), gy = l(x, y + 1) - l(x, y - 1);
                tensor[std::size_t(y) * w + x] = {gx * gx, gx * gy, gy * gy};
            }
        });
        blurPlane<3>(tensor, w, h, s * 0.9);
    }

    // Grundierung: stark geglättetes Bild, damit die Leinwand voll bedeckt ist
    Plane<3> canvas = colorsOf(blurredBuf(src, s * 1.5));
    std::vector<float> height(src.px.size(), 0.f);
    Rng rng(seed);
    quint32 strokeId = 0;
    const double sizes[3] = {s * 1.5, s, std::max(1.5, s * 0.5)};
    const int layers = s < 6 ? 2 : 3; // feinste Lage nur bei grossen Strichen
    for (int layer = 0; layer < layers; ++layer) {
        const double R = sizes[layer]; // Strichbreite
        const Plane<3> ref = colorsOf(blurredBuf(src, R * 0.35));
        const double step = std::max(3.0, R * 0.85);
        struct Spot {
            float x, y;
        };
        std::vector<Spot> spots;
        for (double gy = step * 0.5; gy < h; gy += step)
            for (double gx = step * 0.5; gx < w; gx += step) {
                const float px = float(gx + (rng() - 0.5) * step), py = float(gy + (rng() - 0.5) * step);
                if (layer > 0) {
                    // Feinere Striche nur, wo die Leinwand noch vom Bild abweicht
                    float err = 0;
                    const float o = float(step * 0.5);
                    const float dxs[5] = {0, -o, o, 0, 0}, dys[5] = {0, 0, 0, -o, o};
                    for (int k = 0; k < 5; ++k) {
                        const int ix = std::clamp(int(px + dxs[k]), 0, w - 1), iy = std::clamp(int(py + dys[k]), 0, h - 1);
                        const Rgb& a = canvas[std::size_t(iy) * w + ix];
                        const Rgb& b = ref[std::size_t(iy) * w + ix];
                        err += std::abs(a[0] - b[0]) + std::abs(a[1] - b[1]) + std::abs(a[2] - b[2]);
                    }
                    if (err / 5 < (layer == 1 ? 0.08f : 0.14f))
                        continue;
                }
                spots.push_back({px, py});
            }
        for (std::size_t i = spots.size(); i > 1; --i)
            std::swap(spots[i - 1], spots[std::size_t(rng() * i) % i]);

        for (const Spot& sp : spots) {
            const int ix = std::clamp(int(sp.x), 0, w - 1), iy = std::clamp(int(sp.y), 0, h - 1);
            const std::size_t idx = std::size_t(iy) * w + ix;
            const auto& t = tensor[idx];
            const float tr = t[0] + t[2];
            const float coherence = tr > 1e-9f ? std::sqrt((t[0] - t[2]) * (t[0] - t[2]) + 4 * t[1] * t[1]) / tr : 0.f;
            double ang;
            if (rng() < coherence * 1.6f)
                ang = 0.5 * std::atan2(2.0 * t[1], double(t[0] - t[2])) + M_PI / 2 + (rng() - 0.5) * 0.35;
            else
                ang = (fbm(sp.x * 0.004f, sp.y * 0.004f, seed + 9, 2) - 0.5) * 2.4 + M_PI / 4 + (rng() - 0.5) * 0.5; // ruhige Flächen: fliessende Richtung
            const double ca = std::cos(ang), sa = std::sin(ang);
            const float hw = float(R * 0.5 * (0.8 + 0.4 * rng()));
            const float hl = float(R * (0.8 + 0.7 * rng()));
            const float bend = (rng() - 0.5f) * 0.8f;
            // Farbe gebrochen: jeder Tupfer etwas anders in Helligkeit und Ton
            Rgb col = ref[idx];
            const float shift = (rng() - 0.5f) * (0.04f + 0.12f * colo);
            for (float& v : col)
                v += shift + (rng() - 0.5f) * 0.26f * colo;
            col = saturate(col, 1 + 0.6f * colo);
            const int nb = 3 + int(hw * 0.6f); // Borsten
            const quint32 id = strokeId++;
            const float ex = float(std::abs(ca) * hl + std::abs(sa) * hw) + 2, ey = float(std::abs(sa) * hl + std::abs(ca) * hw) + 2;
            const int x0 = std::max(0, int(sp.x - ex)), x1 = std::min(w - 1, int(sp.x + ex));
            const int y0 = std::max(0, int(sp.y - ey)), y1 = std::min(h - 1, int(sp.y + ey));
            for (int y = y0; y <= y1; ++y)
                for (int x = x0; x <= x1; ++x) {
                    const float dx = x + 0.5f - sp.x, dy = y + 0.5f - sp.y;
                    const float u = float(dx * ca + dy * sa);
                    float v = float(-dx * sa + dy * ca);
                    const float un = u / hl;
                    v -= bend * un * un * hw;
                    const float a = std::max(std::abs(u) - (hl - hw), 0.f);
                    const float dist = std::sqrt(a * a + v * v);
                    const float cov = clamp01(hw - dist + 0.5f);
                    if (cov <= 0)
                        continue;
                    const float vb = clamp01(v / hw * 0.5f + 0.5f) * nb;
                    const int bi = std::min(int(vb), nb - 1);
                    const float bristle = rnd(int(id), bi, seed);
                    // trockenes Strichende
                    const float tail = clamp01((un * 0.5f + 0.5f - 0.7f) / 0.3f);
                    const float op = cov * (1 - 0.8f * tail * bristle);
                    const float tex = 1 + 0.16f * (bristle - 0.5f);
                    Rgb& cv = canvas[std::size_t(y) * w + x];
                    for (int c = 0; c < 3; ++c)
                        cv[c] += (col[c] * tex - cv[c]) * op;
                    const float fb = vb - bi;
                    const float ridge = 0.4f + 2.4f * fb * (1 - fb) * (0.6f + 0.4f * bristle);
                    float& hh = height[std::size_t(y) * w + x];
                    hh += (ridge - hh) * op;
                }
        }
    }

    // Pastoser Farbauftrag: Licht von links oben
    Plane<3> out(canvas.size());
    parallelRows(h, [&](int y) {
        for (int x = 0; x < w; ++x) {
            auto hg = [&](int xx, int yy) { return height[std::size_t(std::clamp(yy, 0, h - 1)) * w + std::clamp(xx, 0, w - 1)]; };
            const float shade = 1 + 0.3f * (hg(x - 1, y - 1) - hg(x + 1, y + 1));
            const Rgb& c = canvas[std::size_t(y) * w + x];
            for (int k = 0; k < 3; ++k)
                out[std::size_t(y) * w + x][k] = brighten(clamp01(c[k] * shade), bright * 0.7f);
        }
    });
    return compose(out, src);
}

QImage watercolor(const QImage& in, double brushSize, double granulation, double water, double brightness, quint32 seed)
{
    const Buf src(in);
    const int w = src.w, h = src.h;
    if (w == 0 || h == 0)
        return src.image();
    const double bs = std::clamp(brushSize, 1.0, 50.0);
    const float gran = float(std::clamp(granulation, 0.0, 100.0) / 100);
    const float wet = float(std::clamp(water, 0.0, 100.0) / 100);
    const float bright = float(std::clamp(brightness, -100.0, 100.0) / 100);
    const int r = std::max(1, int(std::lround(1 + bs * 1.2)));

    // Flächen zusammenfassen
    Plane<3> flat = colorsOf(blurredBuf(src, 0.5 + r * 0.2));
    flat = kuwahara(flat, w, h, r);
    flat = kuwahara(flat, w, h, std::max(1, r / 2));
    // Verlaufene Ränder: wellig verschoben und weich
    Plane<3> wetC(flat.size());
    {
        const double amp = (0.3 + 1.2 * wet) * r;
        const float sc = float(1.0 / (r * 4.0));
        parallelRows(h, [&](int y) {
            for (int x = 0; x < w; ++x) {
                const float nx = fbm(x * sc, y * sc, seed, 3) - 0.5f, ny = fbm(x * sc, y * sc, seed + 101, 3) - 0.5f;
                wetC[std::size_t(y) * w + x] = sampleRgb(flat, w, h, x + nx * 2 * amp, y + ny * 2 * amp);
            }
        });
        blurPlane<3>(wetC, w, h, 0.4 + wet * r * 0.35);
    }
    // Pigmentränder: Abweichung von der weiträumigen Umgebung
    Plane<3> wide = wetC;
    blurPlane<3>(wide, w, h, r * 0.9 + 1);

    Plane<3> out(flat.size());
    parallelRows(h, [&](int y) {
        for (int x = 0; x < w; ++x) {
            const std::size_t i = std::size_t(y) * w + x;
            const Rgb& c = wetC[i];
            const float edge = std::abs(luminance(c) - luminance(wide[i]));
            const float rim = 1 + std::min(1.f, edge * 7.f) * 0.8f;
            // Papier: Pigment setzt sich in den Vertiefungen ab; dazu Wolken vom nassen Auftrag
            const float grain = rnd(x, y, seed + 7) - 0.5f;
            const float pores = fbm(x * 0.16f, y * 0.16f, seed + 11, 3) - 0.5f;
            const float clouds = fbm(x * 0.012f, y * 0.012f, seed + 13, 4) - 0.5f;
            const float g = 1 + gran * (1.3f * pores + 0.12f * grain) + (0.2f + 0.45f * wet) * clouds;
            const float paper = 1 - gran * 0.06f * clamp01(pores + 0.5f);
            Rgb o;
            for (int k = 0; k < 3; ++k) {
                // Lasierend: Pigmentdichte statt Farbe
                const float d = clamp01((1 - c[k]) * 0.88f * rim * g);
                o[k] = brighten(paper * (1 - d), bright * 0.7f);
            }
            out[i] = o;
        }
    });
    return compose(out, src);
}

QImage scraperboard(const QImage& in, double density, double size, bool color)
{
    const Buf src(in);
    const int w = src.w, h = src.h;
    if (w == 0 || h == 0)
        return src.image();
    const float d = float(std::clamp(density, 1.0, 100.0) / 100);
    const float spacing = 16 - 12.5f * d;
    const float gain = 0.3f + 1.2f * float(std::clamp(size, 1.0, 100.0) / 100);
    const quint32 seed = 0x5c4a;

    const Plane<3> col = colorsOf(blurredBuf(src, spacing * 0.3));
    // Helligkeit mit verstärktem örtlichem Kontrast
    Plane<1> lum(col.size());
    for (std::size_t i = 0; i < col.size(); ++i)
        lum[i] = {luminance(col[i])};
    Plane<1> wide = lum;
    blurPlane<1>(wide, w, h, spacing * 5);

    Plane<3> out(col.size());
    const float k = float(M_SQRT1_2);
    parallelRows(h, [&](int y) {
        for (int x = 0; x < w; ++x) {
            const std::size_t i = std::size_t(y) * w + x;
            const float l = clamp01(lum[i][0] + 0.7f * (lum[i][0] - wide[i][0]));
            // Linien leicht geschwungen, Ränder gekratzt
            const float sc = 1.f / (spacing * 30);
            const float warp = (fbm(x * sc, y * sc, seed, 2) - 0.5f) * spacing * 6;
            const float rough = (valueNoise(x * 0.5f, y * 0.5f, seed + 3) - 0.5f) * 0.25f;
            auto line = [&](float phase, float width) {
                const float f = phase / spacing;
                const float dist = std::abs(f - std::floor(f) - 0.5f) * spacing;
                const float hw = width * spacing * 0.5f + rough * std::min(width, 1 - width) * 2;
                return clamp01(hw - dist + 0.5f);
            };
            const float w1 = clamp01(std::pow(l, 1.5f) * gain);
            const float cov = w1 > 0.02f ? line((x + y) * k + warp, w1) : 0.f;
            Rgb ink{1, 1, 1};
            if (color)
                ink = saturate({std::pow(clamp01(col[i][0]), 0.75f), std::pow(clamp01(col[i][1]), 0.75f), std::pow(clamp01(col[i][2]), 0.75f)},
                               1.15f);
            out[i] = {ink[0] * cov, ink[1] * cov, ink[2] * cov};
        }
    });
    return compose(out, src);
}

// --- Struktur --------------------------------------------------------------------------------

QImage wavyPaper(const QImage& in, double pressure, bool color)
{
    const Buf src(in);
    const int w = src.w, h = src.h;
    if (w == 0 || h == 0)
        return src.image();
    const float p = float(std::clamp(pressure, 0.0, 100.0) / 100);
    const quint32 seed = 0x3a7e;
    // Wellenlänge nach Bildgrösse, damit die Vorschau dem Ergebnis gleicht
    const float period = std::clamp(std::min(w, h) / 16.f, 8.f, 400.f);
    const Plane<3> col = colorsOf(src);

    // Papieroberfläche: kurze, leicht gebogene Fasern auf feinem Korn
    Plane<1> paper(col.size());
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            paper[std::size_t(y) * w + x] = {0.5f + 0.35f * (rnd(x, y, seed) - 0.5f)};
    {
        Rng rng(seed + 1);
        const long fibres = long(w) * h / 40;
        for (long f = 0; f < fibres; ++f) {
            float fx = rng() * w, fy = rng() * h;
            double ang = rng() * M_PI;
            const int len = 6 + int(rng() * 18);
            const float val = rng() < 0.6f ? 0.9f : 0.15f; // hell oder dunkel
            const double turn = (rng() - 0.5) * 0.15;
            for (int k = 0; k < len; ++k) {
                const int ix = int(fx), iy = int(fy);
                if (ix >= 0 && iy >= 0 && ix < w && iy < h) {
                    float& v = paper[std::size_t(iy) * w + ix][0];
                    v += (val - v) * 0.55f;
                }
                fx += float(std::cos(ang));
                fy += float(std::sin(ang));
                ang += turn;
            }
        }
    }
    blurPlane<1>(paper, w, h, 0.6);

    Plane<3> out(col.size());
    const float sc = 1.f / (period * 4);
    const double a1 = qDegreesToRadians(12.0), a2 = qDegreesToRadians(-58.0);
    const float c1 = float(std::cos(a1)), s1 = float(std::sin(a1)), c2 = float(std::cos(a2)), s2 = float(std::sin(a2));
    const float k1 = 2 * float(M_PI) / period, k2 = 2 * float(M_PI) / (period * 0.45f);
    parallelRows(h, [&](int y) {
        for (int x = 0; x < w; ++x) {
            const std::size_t i = std::size_t(y) * w + x;
            // Höhenfeld aus zwei unregelmässigen Wellen; Steigung in Lichtrichtung (von links oben)
            const float warp = (fbm(x * sc, y * sc, seed + 5, 3) - 0.5f) * period * 4.f;
            const float ph1 = (x * c1 + y * s1 + warp) * k1, ph2 = (x * c2 + y * s2 - warp * 0.7f) * k2;
            const float g1 = std::cos(ph1) * k1, g2 = std::cos(ph2) * k2 * 0.2f;
            const float gx = g1 * c1 + g2 * c2, gy = g1 * s1 + g2 * s2;
            const float slope = -(gx + gy) * 0.7071f * period / (2 * float(M_PI));
            const Rgb c = sampleRgb(col, w, h, x + gx * period * 0.15f * p, y + gy * period * 0.15f * p);
            Rgb o = c;
            if (!color) {
                const float l = luminance(c);
                o = {l, l, l};
            }
            // Farbe sitzt auf den Fasern: helle Fasern nehmen weniger auf, dunkle mehr
            const float t = paper[i][0] - 0.5f;
            const float shade = 1 + (0.03f + 0.12f * p) * slope;
            const float spec = 0.04f * p * std::max(0.f, slope);
            for (int k = 0; k < 3; ++k) {
                const float d = (1 - o[k]) * (1 - (0.25f + 0.55f * p) * t * 2);
                o[k] = clamp01((1 - clamp01(d)) * shade + spec + 0.08f * p * t);
            }
            out[i] = o;
        }
    });
    return compose(out, src);
}

QImage fabric(const QImage& in, double cellSize, double complexity, double brightness, double angleDeg)
{
    const Buf src(in);
    const int w = src.w, h = src.h;
    if (w == 0 || h == 0)
        return src.image();
    const float cell = float(std::clamp(cellSize, 3.0, 200.0));
    const float cx = float(std::clamp(complexity, 0.0, 100.0) / 100);
    const float bright = float(std::clamp(brightness, -100.0, 100.0) / 100);
    const double a = qDegreesToRadians(angleDeg);
    const float ca = float(std::cos(a)), sa = float(std::sin(a));
    const quint32 seed = 0xfab;
    const Plane<3> col = colorsOf(blurredBuf(src, cell * 0.4));
    const int plies = 1 + int(std::lround(cx * 3)); // Fäden aus mehreren Garnen
    const float twist = 0.6f + cx * 1.4f;

    Plane<3> out(col.size());
    parallelRows(h, [&](int y) {
        for (int x = 0; x < w; ++x) {
            const float u = (x * ca + y * sa) / cell, v = (-x * sa + y * ca) / cell;
            const float fi = std::floor(u), fj = std::floor(v);
            const int i = int(fi), j = int(fj);
            const float fu = u - fi, fv = v - fj;
            const bool warpTop = ((i + j) & 1) == 0; // Leinwandbindung
            const float across = warpTop ? fu : fv, along = warpTop ? fv : fu;
            const int thread = warpTop ? i : j;
            // Fadenbreite leicht unregelmässig
            const float tw = 0.84f + 0.1f * cx * (rnd(thread, warpTop, seed) - 0.5f);
            const float t = (across - 0.5f) / (tw * 0.5f);
            // Farbe längs der Fadenmitte, damit der Faden durchgehend wirkt
            const float mu = warpTop ? (fi + 0.5f) * cell : u * cell, mv = warpTop ? v * cell : (fj + 0.5f) * cell;
            Rgb c = sampleRgb(col, w, h, mu * ca - mv * sa, mu * sa + mv * ca);
            const float tint = 1 + 0.10f * cx * (rnd(thread, warpTop + 2, seed) - 0.5f);
            float shade, spec = 0;
            if (std::abs(t) <= 1) {
                const float round = std::sqrt(1 - t * t);
                // taucht an beiden Enden unter dem Querfaden ab
                const float dip = std::pow(std::sin(float(M_PI) * along), 0.5f);
                // Garne verdrillt: schräge Rillen
                const float pf = (t * 0.5f + 0.5f) * plies + along * twist * plies;
                const float ply = std::sin(float(M_PI) * (pf - std::floor(pf)));
                const float fibre = 1 + 0.12f * cx * (valueNoise(along * 18, (t + 1) * 6 + thread * 3.1f, seed) - 0.5f);
                shade = (0.3f + 0.7f * std::sqrt(round)) * (0.6f + 0.4f * dip) * (0.78f + 0.22f * std::sqrt(ply)) * fibre * tint;
                spec = 0.12f * std::pow(round * ply, 4.f) * dip;
            } else {
                // Lücke: darunterliegender Querfaden im Schatten
                shade = 0.25f + 0.15f * std::sin(float(M_PI) * along);
            }
            Rgb o;
            for (int k = 0; k < 3; ++k)
                o[k] = brighten(clamp01(c[k] * shade * 1.3f + spec), bright * 0.7f);
            out[std::size_t(y) * w + x] = o;
        }
    });
    return compose(out, src);
}

// --- Kreativ ---------------------------------------------------------------------------------

QImage glassBlock(const QImage& in, double blockWidth, double blockHeight)
{
    const Buf src(in);
    const int w = src.w, h = src.h;
    if (w == 0 || h == 0)
        return src.image();
    const float bw = float(std::clamp(blockWidth, 4.0, 2000.0)), bh = float(std::clamp(blockHeight, 4.0, 2000.0));
    const Plane<3> col = colorsOf(src);
    const float mortar = std::max(1.f, std::min(bw, bh) * 0.035f);
    const float bevel = std::max(1.5f, std::min(bw, bh) * 0.10f);

    Plane<3> out(col.size());
    parallelRows(h, [&](int y) {
        for (int x = 0; x < w; ++x) {
            const float fi = std::floor(x / bw), fj = std::floor(y / bh);
            const float fx = x / bw - fi, fy = y / bh - fj;
            const float lx = fx - 0.5f, ly = fy - 0.5f;
            // Gewölbte Glasfläche vergrössert den Inhalt, innen feine Wellen
            const float cxp = (fi + 0.5f) * bw, cyp = (fj + 0.5f) * bh;
            const float mag = 0.62f;
            float sx = cxp + lx * bw * mag, sy = cyp + ly * bh * mag;
            sx += bw * (0.022f * std::sin(float(M_PI) * 2 * (2.5f * fy + 0.3f * fi)) - 0.06f * std::sin(float(M_PI) * 2 * lx));
            sy += bh * (0.022f * std::sin(float(M_PI) * 2 * (2.5f * fx + 0.3f * fj)) - 0.06f * std::sin(float(M_PI) * 2 * ly));
            Rgb c = sampleRgb(col, w, h, sx, sy);
            // Kanten: Fuge, Fase hell oben links, dunkel unten rechts
            const float el = fx * bw, er = (1 - fx) * bw, et = fy * bh, eb = (1 - fy) * bh;
            const float e = std::min({el, er, et, eb});
            float lightAdd = 0, darkMul = 1;
            if (e < bevel + mortar) {
                const float f = 1 - smoothstep(mortar, mortar + bevel, e);
                if (std::min(el, et) <= std::min(er, eb))
                    lightAdd = 0.10f * f;
                else
                    darkMul = 1 - 0.16f * f;
            }
            const float m = 1 - smoothstep(mortar * 0.5f, mortar, e);
            Rgb o;
            for (int k = 0; k < 3; ++k) {
                const float v = c[k] * darkMul + lightAdd;
                o[k] = clamp01(v + (c[k] * 0.35f + 0.08f - v) * m * 0.75f); // Fuge dunkel
            }
            out[std::size_t(y) * w + x] = o;
        }
    });
    return compose(out, src);
}

QImage smokedGlass(const QImage& in, const Color& tint, double tintPercent, double blurPercent)
{
    const Buf src(in);
    const int w = src.w, h = src.h;
    if (w == 0 || h == 0)
        return src.image();
    const float t = float(std::clamp(tintPercent, 0.0, 100.0) / 100);
    const float bp = float(std::clamp(blurPercent, 0.0, 100.0) / 100);
    const Rgb tc{tint.r / 255.f, tint.g / 255.f, tint.b / 255.f};
    const quint32 seed = 0x5e0c;
    // Unschärfe nach Bildgrösse, damit die Vorschau dem Ergebnis gleicht
    const double sigma = bp * std::max(w, h) * 0.02;

    // Mattiertes Glas: Licht wird gestreut (zufällig versetzt), dann weich
    Buf frosted(w, h);
    const float scatter = float(sigma * 0.7);
    parallelRows(h, [&](int y) {
        for (int x = 0; x < w; ++x) {
            int sx = x, sy = y;
            if (scatter >= 0.5f) {
                sx = std::clamp(int(std::lround(x + (rnd(x, y, seed) - 0.5f) * 2 * scatter)), 0, w - 1);
                sy = std::clamp(int(std::lround(y + (rnd(x, y, seed + 1) - 0.5f) * 2 * scatter)), 0, h - 1);
            }
            frosted.at(x, y) = src.at(sx, sy);
        }
    });
    blurPlane<4>(frosted.px, w, h, sigma);
    const Plane<3> c = colorsOf(frosted);
    // Sprenkel der mattierten Oberfläche (2 bis 3 Pixel gross)
    Plane<1> frost(c.size());
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            frost[std::size_t(y) * w + x] = {rnd(x, y, seed + 2)};
    blurPlane<1>(frost, w, h, 0.8);
    for (auto& f : frost)
        f[0] = clamp01((f[0] - 0.5f) * 3.f + 0.5f);

    Plane<3> out(c.size());
    const float veil = 0.30f; // Anteil gestreutes Licht im Glas (Schleier)
    parallelRows(h, [&](int y) {
        for (int x = 0; x < w; ++x) {
            const std::size_t i = std::size_t(y) * w + x;
            // Oberfläche: feine Körnung, schwacher Lichtreflex von oben links
            const float grain = (frost[i][0] - 0.5f) * 0.10f * std::sqrt(bp) * (0.3f + luminance(c[i]));
            const float gx = float(x) / w, gy = float(y) / h;
            const float sheen = 0.035f * t * clamp01(1 - (gx + gy) * 0.8f);
            Rgb o;
            for (int k = 0; k < 3; ++k) {
                // Getöntes Glas filtert (multiplikativ) und trübt (Schleier in Glasfarbe)
                const float filtered = c[i][k] * tc[k];
                const float glass = filtered * (1 - veil) + tc[k] * veil;
                o[k] = clamp01(c[i][k] + (glass - c[i][k]) * t + grain + sheen);
            }
            out[i] = o;
        }
    });
    return compose(out, src);
}

} // namespace vek::filters
