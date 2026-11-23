#include "Cutout.h"

#include <QPainter>
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace vek::cutout {

namespace {

using Plane = std::vector<float>;

// Summe über ein (2r+1)²-Fenster, ausserhalb des Bildes 0 (Ränder haben kleinere Fenster)
template <typename T>
void boxSum(const T* in, T* out, int w, int h, int r, std::vector<T>& tmp)
{
    tmp.resize(size_t(w) * h);
#pragma omp parallel for schedule(static)
    for (int y = 0; y < h; ++y) {
        const T* src = in + size_t(y) * w;
        T* dst = tmp.data() + size_t(y) * w;
        double s = 0;
        for (int x = 0; x <= std::min(r, w - 1); ++x)
            s += src[x];
        for (int x = 0; x < w; ++x) {
            dst[x] = T(s);
            if (x + r + 1 < w)
                s += src[x + r + 1];
            if (x - r >= 0)
                s -= src[x - r];
        }
    }
    // senkrecht: Spaltenblöcke parallel, laufende Zeilensumme
    constexpr int kBlock = 256;
    const int blocks = (w + kBlock - 1) / kBlock;
#pragma omp parallel for schedule(static)
    for (int b = 0; b < blocks; ++b) {
        const int x0 = b * kBlock;
        const int x1 = std::min(w, x0 + kBlock);
        std::vector<double> acc(size_t(x1 - x0), 0.0);
        for (int y = 0; y <= std::min(r, h - 1); ++y) {
            const T* row = tmp.data() + size_t(y) * w;
            for (int x = x0; x < x1; ++x)
                acc[size_t(x - x0)] += row[x];
        }
        for (int y = 0; y < h; ++y) {
            T* dst = out + size_t(y) * w;
            for (int x = x0; x < x1; ++x)
                dst[x] = T(acc[size_t(x - x0)]);
            if (y + r + 1 < h) {
                const T* add = tmp.data() + size_t(y + r + 1) * w;
                for (int x = x0; x < x1; ++x)
                    acc[size_t(x - x0)] += add[x];
            }
            if (y - r >= 0) {
                const T* sub = tmp.data() + size_t(y - r) * w;
                for (int x = x0; x < x1; ++x)
                    acc[size_t(x - x0)] -= sub[x];
            }
        }
    }
}

// Mittelwert über das Fenster (Ränder: über die vorhandenen Pixel)
template <typename T>
void boxMean(const T* in, T* out, int w, int h, int r, const std::vector<T>& count, std::vector<T>& tmp)
{
    boxSum(in, out, w, h, r, tmp);
    const size_t n = size_t(w) * h;
#pragma omp parallel for schedule(static)
    for (qsizetype i = 0; i < qsizetype(n); ++i)
        out[i] /= count[size_t(i)];
}

template <typename T>
struct RgbT {
    int w = 0, h = 0;
    std::vector<T> c[3];
};
using Rgb = RgbT<float>;
using RgbD = RgbT<double>;
using PlaneD = std::vector<double>;

RgbD toDouble(const Rgb& in)
{
    RgbD out;
    out.w = in.w;
    out.h = in.h;
    for (int c = 0; c < 3; ++c)
        out.c[c].assign(in.c[c].begin(), in.c[c].end());
    return out;
}

Rgb toPlanes(const QImage& image)
{
    Rgb out;
    const QImage src = image.convertToFormat(QImage::Format_RGBX64);
    out.w = src.width();
    out.h = src.height();
    for (Plane& p : out.c)
        p.resize(size_t(out.w) * out.h);
    for (int y = 0; y < out.h; ++y) {
        const auto* px = reinterpret_cast<const QRgba64*>(src.constScanLine(y));
        for (int x = 0; x < out.w; ++x) {
            const size_t i = size_t(y) * out.w + x;
            out.c[0][i] = px[x].red() / 65535.0f;
            out.c[1][i] = px[x].green() / 65535.0f;
            out.c[2][i] = px[x].blue() / 65535.0f;
        }
    }
    return out;
}

// Löst (L_uu + λ) x = −L_uk α_k + λ prior nur auf den unbekannten Pixeln. L ist der Matting-Laplace
// mit 3x3-Fenstern (Levin 2008), dünn gespeichert: je unbekanntem Pixel 25 Nachbarn (5x5). Konjugierte
// Gradienten mit Diagonal-Vorkonditionierung; Aufwand wächst mit der Breite des Bandes, nicht mit dem Bild.
bool solve(const RgbD& I, const std::vector<uchar>& tri, const PlaneD* prior, const Settings& s, PlaneD& alpha,
           const Progress& progress, int progressFrom, int progressTo, const PlaneD* knownValues = nullptr)
{
    const int w = I.w, h = I.h;
    const size_t n = size_t(w) * h;
    std::vector<int> index(n, -1);
    std::vector<int> pix;
    for (size_t i = 0; i < n; ++i) {
        if (tri[i] == Unknown) {
            index[i] = int(pix.size());
            pix.push_back(int(i));
        }
    }
    alpha.assign(n, 0.0);
    for (size_t i = 0; i < n; ++i)
        alpha[i] = knownValues ? (*knownValues)[i] : tri[i] == Foreground ? 1.0 : 0.0;
    const int U = int(pix.size());
    if (U == 0)
        return true;

    // Fensterdaten (Mittelwert, inverse regularisierte Kovarianz) für alle Fenster, die ein
    // unbekanntes Pixel berühren; nur Fenster ganz im Bild
    std::vector<int> winIndex(n, -1);
    std::vector<int> centers;
    for (int u = 0; u < U; ++u) {
        const int x = pix[size_t(u)] % w, y = pix[size_t(u)] / w;
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx) {
                const int cx = x + dx, cy = y + dy;
                if (cx < 1 || cy < 1 || cx > w - 2 || cy > h - 2)
                    continue;
                const size_t c = size_t(cy) * w + cx;
                if (winIndex[c] < 0) {
                    winIndex[c] = int(centers.size());
                    centers.push_back(int(c));
                }
            }
    }
    const int nw = int(centers.size());
    std::vector<double> win(size_t(nw) * 9);
    const double epsW = s.epsilon / 9.0;
#pragma omp parallel for schedule(static)
    for (int k = 0; k < nw; ++k) {
        const int cx = centers[size_t(k)] % w, cy = centers[size_t(k)] / w;
        double mu[3] = {0, 0, 0};
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
                for (int c = 0; c < 3; ++c)
                    mu[c] += I.c[c][size_t(cy + dy) * w + cx + dx];
        for (double& m : mu)
            m /= 9.0;
        double cov[6] = {0, 0, 0, 0, 0, 0};
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx) {
                const size_t j = size_t(cy + dy) * w + cx + dx;
                const double r = I.c[0][j] - mu[0], g = I.c[1][j] - mu[1], b = I.c[2][j] - mu[2];
                cov[0] += r * r;
                cov[1] += r * g;
                cov[2] += r * b;
                cov[3] += g * g;
                cov[4] += g * b;
                cov[5] += b * b;
            }
        const double a = cov[0] / 9 + epsW, bb = cov[1] / 9, c = cov[2] / 9;
        const double d = cov[3] / 9 + epsW, f = cov[4] / 9, g = cov[5] / 9 + epsW;
        const double A = d * g - f * f, B = c * f - bb * g, C = bb * f - c * d;
        const double D = a * g - c * c, E = bb * c - a * f, F = a * d - bb * bb;
        const double det = a * A + bb * B + c * C;
        const double inv = std::abs(det) > 1e-300 ? 1.0 / det : 0.0;
        double* out = &win[size_t(k) * 9];
        out[0] = mu[0];
        out[1] = mu[1];
        out[2] = mu[2];
        out[3] = A * inv;
        out[4] = B * inv;
        out[5] = C * inv;
        out[6] = D * inv;
        out[7] = E * inv;
        out[8] = F * inv;
    }

    // Zeilen von L: Beiträge aller Fenster, die das Pixel enthalten
    std::vector<double> L(size_t(U) * 25, 0.0);
    std::vector<int> nb(size_t(U) * 25, -1);
    const size_t nu = size_t(U);
    std::vector<double> rhs(nu), diag(nu);
    const double lam = prior ? s.priorWeight : 0.0;
#pragma omp parallel for schedule(static)
    for (int u = 0; u < U; ++u) {
        const int i = pix[size_t(u)];
        const int xi = i % w, yi = i / w;
        double* row = &L[size_t(u) * 25];
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                const int cx = xi + dx, cy = yi + dy;
                if (cx < 1 || cy < 1 || cx > w - 2 || cy > h - 2)
                    continue;
                const double* wd = &win[size_t(winIndex[size_t(cy) * w + cx]) * 9];
                const double di0 = I.c[0][size_t(i)] - wd[0], di1 = I.c[1][size_t(i)] - wd[1], di2 = I.c[2][size_t(i)] - wd[2];
                const double v0 = wd[3] * di0 + wd[4] * di1 + wd[5] * di2;
                const double v1 = wd[4] * di0 + wd[6] * di1 + wd[7] * di2;
                const double v2 = wd[5] * di0 + wd[7] * di1 + wd[8] * di2;
                for (int ey = -1; ey <= 1; ++ey) {
                    for (int ex = -1; ex <= 1; ++ex) {
                        const int xj = cx + ex, yj = cy + ey;
                        const size_t j = size_t(yj) * w + xj;
                        const double dj0 = I.c[0][j] - wd[0], dj1 = I.c[1][j] - wd[1], dj2 = I.c[2][j] - wd[2];
                        const double v = ((xj == xi && yj == yi) ? 1.0 : 0.0) - (1.0 + v0 * dj0 + v1 * dj1 + v2 * dj2) / 9.0;
                        row[(yj - yi + 2) * 5 + (xj - xi + 2)] += v;
                    }
                }
            }
        }
        double b = 0;
        for (int o = 0; o < 25; ++o) {
            const int xj = xi + o % 5 - 2, yj = yi + o / 5 - 2;
            if (xj < 0 || yj < 0 || xj >= w || yj >= h || row[o] == 0.0)
                continue;
            const size_t j = size_t(yj) * w + xj;
            if (index[j] >= 0)
                nb[size_t(u) * 25 + o] = index[j];
            else
                b -= row[o] * alpha[j];
        }
        rhs[size_t(u)] = b + (prior ? lam * (*prior)[size_t(i)] : 0.0);
        diag[size_t(u)] = row[12] + lam;
    }

    // Je Schritt nur wenig Arbeit: viele kurze Parallelabschnitte kosten mehr als sie bringen, besonders
    // wenn der Rechner ausgelastet ist (Threads warten aufeinander). Erst bei grossen Bändern parallel.
    const bool parallel = U > 300'000;
    auto apply = [&](const std::vector<double>& v, std::vector<double>& out) {
#pragma omp parallel for schedule(static) if (parallel)
        for (int u = 0; u < U; ++u) {
            const double* row = &L[size_t(u) * 25];
            const int* nr = &nb[size_t(u) * 25];
            double sum = lam * v[size_t(u)];
            for (int o = 0; o < 25; ++o)
                if (nr[o] >= 0)
                    sum += row[o] * v[size_t(nr[o])];
            out[size_t(u)] = sum;
        }
    };
    auto dotU = [U, parallel](const std::vector<double>& a, const std::vector<double>& b) {
        double sum = 0;
#pragma omp parallel for reduction(+ : sum) schedule(static) if (parallel)
        for (int u = 0; u < U; ++u)
            sum += a[size_t(u)] * b[size_t(u)];
        return sum;
    };
    std::vector<double> x(nu), r(nu), z(nu), d(nu), Ad(nu);
    for (int u = 0; u < U; ++u)
        x[size_t(u)] = prior ? (*prior)[size_t(pix[size_t(u)])]
                             : knownValues ? (*knownValues)[size_t(pix[size_t(u)])] : 0.5; // Start: bisheriges Ergebnis
    apply(x, Ad);
    for (int u = 0; u < U; ++u) {
        r[size_t(u)] = rhs[size_t(u)] - Ad[size_t(u)];
        z[size_t(u)] = diag[size_t(u)] > 0 ? r[size_t(u)] / diag[size_t(u)] : r[size_t(u)];
        d[size_t(u)] = z[size_t(u)];
    }
    double rz = dotU(r, z);
    const double r0 = std::max(dotU(r, r), 1e-300);
    const double goal = s.tolerance * s.tolerance * r0;
    const double logSpan = std::max(1e-9, std::log(r0 / std::max(goal, 1e-300)));
    double rr = r0;
    int it = 0;
    for (; it < s.maxIterations && rr > goal; ++it) {
        if (progress.cancel && progress.cancel->load())
            return false;
        apply(d, Ad);
        const double dAd = dotU(d, Ad);
        if (dAd <= 0)
            break;
        const double al = rz / dAd;
#pragma omp parallel for schedule(static) if (parallel)
        for (int u = 0; u < U; ++u) {
            x[size_t(u)] += al * d[size_t(u)];
            r[size_t(u)] -= al * Ad[size_t(u)];
            z[size_t(u)] = diag[size_t(u)] > 0 ? r[size_t(u)] / diag[size_t(u)] : r[size_t(u)];
        }
        const double rzNew = dotU(r, z);
        const double beta = rzNew / rz;
        rz = rzNew;
#pragma omp parallel for schedule(static) if (parallel)
        for (int u = 0; u < U; ++u)
            d[size_t(u)] = z[size_t(u)] + beta * d[size_t(u)];
        rr = dotU(r, r);
        if (progress.report && it % 20 == 0) {
            const double done = std::clamp(std::log(r0 / std::max(rr, 1e-300)) / logSpan, 0.0, 1.0);
            const double byCount = double(it) / s.maxIterations;
            progress.report(progressFrom + int((progressTo - progressFrom) * std::max(done, byCount)));
        }
    }
    if (qEnvironmentVariableIsSet("VEK_CUTOUT_DEBUG"))
        qInfo("cutout: %d px, %d unbekannt, %d it, r0 %g, rr %g", int(n), U, it, r0, rr);
    for (int u = 0; u < U; ++u)
        alpha[size_t(pix[size_t(u)])] = std::clamp(x[size_t(u)], 0.0, 1.0);
    return true;
}

// Geführtes Hochskalieren (Fast Guided Filter): lineare Koeffizienten α ≈ aᵀI + b klein rechnen,
// gross auf das Originalbild anwenden. Feine Strukturen des Originals bleiben so erhalten.
Plane guidedUpsample(const RgbD& small, const PlaneD& alpha, const Rgb& big, int r, double eps)
{
    const int w = small.w, h = small.h;
    const size_t n = size_t(w) * h;
    PlaneD one(n, 1.0), count(n), tmp;
    boxSum(one.data(), count.data(), w, h, r, tmp);
    PlaneD mu[3], ma(n), mIa[3], prod(n);
    boxMean(alpha.data(), ma.data(), w, h, r, count, tmp);
    for (int c = 0; c < 3; ++c) {
        mu[c].resize(n);
        mIa[c].resize(n);
        boxMean(small.c[c].data(), mu[c].data(), w, h, r, count, tmp);
        for (size_t i = 0; i < n; ++i)
            prod[i] = small.c[c][i] * alpha[i];
        boxMean(prod.data(), mIa[c].data(), w, h, r, count, tmp);
    }
    PlaneD cov[6];
    static const int pairs[6][2] = {{0, 0}, {0, 1}, {0, 2}, {1, 1}, {1, 2}, {2, 2}};
    for (int k = 0; k < 6; ++k) {
        const auto [a, b] = pairs[k];
        for (size_t i = 0; i < n; ++i)
            prod[i] = small.c[a][i] * small.c[b][i];
        cov[k].resize(n);
        boxMean(prod.data(), cov[k].data(), w, h, r, count, tmp);
        for (size_t i = 0; i < n; ++i)
            cov[k][i] -= mu[a][i] * mu[b][i];
    }
    PlaneD A[3], B(n);
    for (PlaneD& p : A)
        p.resize(n);
    for (size_t i = 0; i < n; ++i) {
        const double a = cov[0][i] + eps, b = cov[1][i], c = cov[2][i];
        const double d = cov[3][i] + eps, f = cov[4][i], g = cov[5][i] + eps;
        const double iA = d * g - f * f, iB = c * f - b * g, iC = b * f - c * d;
        const double iD = a * g - c * c, iE = b * c - a * f, iF = a * d - b * b;
        const double det = a * iA + b * iB + c * iC;
        const double inv = std::abs(det) > 1e-30 ? 1.0 / det : 0.0;
        const double x0 = mIa[0][i] - mu[0][i] * ma[i];
        const double x1 = mIa[1][i] - mu[1][i] * ma[i];
        const double x2 = mIa[2][i] - mu[2][i] * ma[i];
        A[0][i] = (iA * x0 + iB * x1 + iC * x2) * inv;
        A[1][i] = (iB * x0 + iD * x1 + iE * x2) * inv;
        A[2][i] = (iC * x0 + iE * x1 + iF * x2) * inv;
        B[i] = ma[i] - A[0][i] * mu[0][i] - A[1][i] * mu[1][i] - A[2][i] * mu[2][i];
    }
    PlaneD mA[3], mB(n);
    for (int c = 0; c < 3; ++c) {
        mA[c].resize(n);
        boxMean(A[c].data(), mA[c].data(), w, h, r, count, tmp);
    }
    boxMean(B.data(), mB.data(), w, h, r, count, tmp);
    // bilinear auf die grosse Fläche
    const int W = big.w, H = big.h;
    Plane out(size_t(W) * H);
    const double sx = double(w) / W, sy = double(h) / H;
#pragma omp parallel for schedule(static)
    for (int Y = 0; Y < H; ++Y) {
        const double fy = std::clamp((Y + 0.5) * sy - 0.5, 0.0, double(h - 1));
        const int y0 = int(fy), y1 = std::min(h - 1, y0 + 1);
        const double ty = fy - y0;
        for (int X = 0; X < W; ++X) {
            const double fx = std::clamp((X + 0.5) * sx - 0.5, 0.0, double(w - 1));
            const int x0 = int(fx), x1 = std::min(w - 1, x0 + 1);
            const double tx = fx - x0;
            auto lerp = [&](const PlaneD& p) {
                const double top = p[size_t(y0) * w + x0] * (1 - tx) + p[size_t(y0) * w + x1] * tx;
                const double bot = p[size_t(y1) * w + x0] * (1 - tx) + p[size_t(y1) * w + x1] * tx;
                return top * (1 - ty) + bot * ty;
            };
            const size_t i = size_t(Y) * W + X;
            out[i] = float(lerp(mA[0]) * big.c[0][i] + lerp(mA[1]) * big.c[1][i] + lerp(mA[2]) * big.c[2][i] + lerp(mB));
        }
    }
    return out;
}

QImage grayOf(const QImage& image, const QSize& size)
{
    QImage g = image.convertToFormat(QImage::Format_Grayscale8);
    return g.size() == size ? g : g.scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

} // namespace

QImage floodFill(const QImage& marker, const QPoint& seed)
{
    const QImage m = marker.convertToFormat(QImage::Format_Grayscale8);
    const int w = m.width(), h = m.height();
    QImage out(w, h, QImage::Format_Grayscale8);
    out.fill(0);
    if (!QRect(0, 0, w, h).contains(seed) || m.constScanLine(seed.y())[seed.x()] > 0)
        return {};
    std::vector<QPoint> stack{seed};
    while (!stack.empty()) {
        const QPoint p = stack.back();
        stack.pop_back();
        const uchar* mrow = m.constScanLine(p.y());
        uchar* orow = out.scanLine(p.y());
        if (orow[p.x()] || mrow[p.x()])
            continue;
        int x0 = p.x(), x1 = p.x();
        while (x0 > 0 && !mrow[x0 - 1] && !orow[x0 - 1])
            --x0;
        while (x1 < w - 1 && !mrow[x1 + 1] && !orow[x1 + 1])
            ++x1;
        std::fill(orow + x0, orow + x1 + 1, uchar(255));
        for (int ny : {p.y() - 1, p.y() + 1}) {
            if (ny < 0 || ny >= h)
                continue;
            const uchar* nm = m.constScanLine(ny);
            const uchar* no = out.constScanLine(ny);
            bool open = false;
            for (int x = x0; x <= x1; ++x) {
                const bool free = !nm[x] && !no[x];
                if (free && !open)
                    stack.emplace_back(x, ny);
                open = free;
            }
        }
    }
    return out;
}

QImage trimap(const QImage& marker, const QImage& fill, const QImage& keep, const QImage& remove)
{
    const QSize size = marker.size();
    QImage out(size, QImage::Format_Grayscale8);
    out.fill(Background);
    auto layer = [&size](const QImage& img) {
        return img.isNull() ? QImage() : img.convertToFormat(QImage::Format_Grayscale8).scaled(size);
    };
    const QImage m = layer(marker), f = layer(fill), k = layer(keep), r = layer(remove);
    for (int y = 0; y < size.height(); ++y) {
        uchar* o = out.scanLine(y);
        const uchar* mr = m.constScanLine(y);
        const uchar* fr = f.isNull() ? nullptr : f.constScanLine(y);
        const uchar* kr = k.isNull() ? nullptr : k.constScanLine(y);
        const uchar* rr = r.isNull() ? nullptr : r.constScanLine(y);
        for (int x = 0; x < size.width(); ++x) {
            uchar v = fr && fr[x] ? Foreground : Background;
            if (mr[x])
                v = Unknown;
            if (kr && kr[x])
                v = Foreground;
            if (rr && rr[x])
                v = Background;
            o[x] = v;
        }
    }
    return out;
}

QRect unknownBounds(const QImage& trimap)
{
    int x0 = trimap.width(), y0 = trimap.height(), x1 = -1, y1 = -1;
    for (int y = 0; y < trimap.height(); ++y) {
        const uchar* row = trimap.constScanLine(y);
        for (int x = 0; x < trimap.width(); ++x) {
            if (row[x] != Unknown)
                continue;
            x0 = std::min(x0, x);
            x1 = std::max(x1, x);
            y0 = std::min(y0, y);
            y1 = std::max(y1, y);
        }
    }
    return x1 < 0 ? QRect() : QRect(QPoint(x0, y0), QPoint(x1, y1));
}

QImage solveAlpha(const QImage& image, const QImage& trimapImage, const QImage& prior, const Settings& s,
                  const Progress& progress)
{
    const QImage tri = trimapImage.convertToFormat(QImage::Format_Grayscale8);
    QImage out = tri.copy();
    // Bekannte Pixel stehen fest; nur im Band rechnen
    for (int y = 0; y < out.height(); ++y) {
        uchar* row = out.scanLine(y);
        for (int x = 0; x < out.width(); ++x)
            row[x] = row[x] == Foreground ? 255 : 0;
    }
    const QRect band = unknownBounds(tri);
    if (band.isEmpty())
        return out;
    const int margin = 2 * s.radius + 8;
    const QRect crop = band.adjusted(-margin, -margin, margin, margin) & image.rect();
    // Arbeitsgrösse: Ausschnitt und Zahl der unbekannten Pixel begrenzen (Speicher, Zeit)
    qint64 unknownCount = 0;
    for (int y = crop.top(); y <= crop.bottom(); ++y) {
        const uchar* row = tri.constScanLine(y);
        for (int x = crop.left(); x <= crop.right(); ++x)
            unknownCount += row[x] == Unknown;
    }
    const double scale = std::min({1.0, std::sqrt(double(s.workPixels) / (double(crop.width()) * crop.height())),
                                   std::sqrt(double(s.maxUnknown) / double(std::max<qint64>(1, unknownCount)))});
    const QSize work(std::max(8, int(std::lround(crop.width() * scale))), std::max(8, int(std::lround(crop.height() * scale))));

    const QImage cropImage = image.copy(crop);
    const Rgb big = toPlanes(cropImage);
    const Rgb small = scale < 1.0 ? toPlanes(cropImage.scaled(work, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)) : Rgb{};
    const RgbD I = toDouble(scale < 1.0 ? small : big);
    const int w = I.w, h = I.h;

    // Trimap auf Arbeitsgrösse: bekannt nur, wo der ganze Quellbereich bekannt ist
    std::vector<uchar> t(size_t(w) * h);
    {
        const QImage triCrop = tri.copy(crop);
        QImage fg(triCrop.size(), QImage::Format_Grayscale8), bg(triCrop.size(), QImage::Format_Grayscale8);
        for (int y = 0; y < triCrop.height(); ++y) {
            const uchar* row = triCrop.constScanLine(y);
            uchar* f = fg.scanLine(y);
            uchar* b = bg.scanLine(y);
            for (int x = 0; x < triCrop.width(); ++x) {
                f[x] = row[x] == Foreground ? 255 : 0;
                b[x] = row[x] == Background ? 255 : 0;
            }
        }
        if (scale < 1.0) {
            fg = fg.scaled(work, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            bg = bg.scaled(work, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }
        for (int y = 0; y < h; ++y) {
            const uchar* f = fg.constScanLine(y);
            const uchar* b = bg.constScanLine(y);
            for (int x = 0; x < w; ++x)
                t[size_t(y) * w + x] = f[x] >= 250 ? Foreground : b[x] >= 250 ? Background : Unknown;
        }
    }
    PlaneD priorPlane;
    if (!prior.isNull() && s.priorWeight > 0) {
        const QImage p = grayOf(prior.copy(crop), QSize(w, h));
        priorPlane.resize(size_t(w) * h);
        for (int y = 0; y < h; ++y) {
            const uchar* row = p.constScanLine(y);
            for (int x = 0; x < w; ++x)
                priorPlane[size_t(y) * w + x] = row[x] / 255.0;
        }
    }
    if (progress.report)
        progress.report(5);
    PlaneD solved;
    if (!solve(I, t, priorPlane.empty() ? nullptr : &priorPlane, s, solved, progress, 5, scale < 1.0 ? 90 : 98))
        return {};
    Plane alpha;
    if (scale < 1.0)
        alpha = guidedUpsample(I, solved, big, 1, 1e-5);
    else
        alpha.assign(solved.begin(), solved.end());
    for (int y = 0; y < crop.height(); ++y) {
        const uchar* trow = tri.constScanLine(crop.y() + y);
        uchar* orow = out.scanLine(crop.y() + y);
        for (int x = 0; x < crop.width(); ++x) {
            if (trow[crop.x() + x] != Unknown)
                continue;
            orow[crop.x() + x] = uchar(std::clamp(int(std::lround(alpha[size_t(y) * crop.width() + x] * 255.0f)), 0, 255));
        }
    }
    if (progress.report)
        progress.report(100);
    return out;
}

QImage updateAlpha(const QImage& image, const QImage& trimapImage, const QImage& previous, const QRect& region,
                   const Settings& s, const Progress& progress)
{
    const QImage tri = trimapImage.convertToFormat(QImage::Format_Grayscale8);
    const QImage prev = previous.convertToFormat(QImage::Format_Grayscale8);
    if (prev.size() != tri.size() || region.isEmpty())
        return solveAlpha(image, tri, {}, s, progress);
    const QRect area = region.adjusted(-24, -24, 24, 24) & image.rect();
    const QRect crop = area.adjusted(-2, -2, 2, 2) & image.rect();
    // Neu gerechnet wird nur, was im Bereich unbekannt ist; der Rand behält das bisherige Ergebnis
    qint64 unknownCount = 0;
    for (int y = area.top(); y <= area.bottom(); ++y) {
        const uchar* row = tri.constScanLine(y);
        for (int x = area.left(); x <= area.right(); ++x)
            unknownCount += row[x] == Unknown;
    }
    if (unknownCount > s.maxUnknown)
        return solveAlpha(image, tri, {}, s, progress);
    QImage out = prev.copy();
    for (int y = 0; y < out.height(); ++y) {
        const uchar* t = tri.constScanLine(y);
        uchar* o = out.scanLine(y);
        for (int x = 0; x < out.width(); ++x)
            if (t[x] != Unknown)
                o[x] = t[x] == Foreground ? 255 : 0;
    }
    if (unknownCount == 0)
        return out;
    const RgbD I = toDouble(toPlanes(image.copy(crop)));
    const int w = I.w, h = I.h;
    std::vector<uchar> t(size_t(w) * h);
    PlaneD known(size_t(w) * h);
    for (int y = 0; y < h; ++y) {
        const uchar* trow = tri.constScanLine(crop.y() + y) + crop.x();
        const uchar* prow = prev.constScanLine(crop.y() + y) + crop.x();
        for (int x = 0; x < w; ++x) {
            const size_t i = size_t(y) * w + x;
            const bool inside = area.contains(crop.x() + x, crop.y() + y);
            if (trow[x] == Unknown && inside) {
                t[i] = Unknown;
                known[i] = prow[x] / 255.0; // Startwert
            } else {
                t[i] = trow[x] == Unknown ? Background : trow[x]; // Kennung egal, es zählt der Wert
                known[i] = trow[x] == Unknown ? prow[x] / 255.0 : trow[x] == Foreground ? 1.0 : 0.0;
            }
        }
    }
    PlaneD solved;
    if (!solve(I, t, nullptr, s, solved, progress, 5, 98, &known))
        return {};
    for (int y = 0; y < h; ++y) {
        uchar* orow = out.scanLine(crop.y() + y) + crop.x();
        for (int x = 0; x < w; ++x)
            if (t[size_t(y) * w + x] == Unknown)
                orow[x] = uchar(std::clamp(int(std::lround(solved[size_t(y) * w + x] * 255.0)), 0, 255));
    }
    if (progress.report)
        progress.report(100);
    return out;
}

QRect changedBounds(const QImage& a, const QImage& b)
{
    if (a.size() != b.size())
        return QRect(QPoint(0, 0), b.size());
    const QImage ga = a.convertToFormat(QImage::Format_Grayscale8), gb = b.convertToFormat(QImage::Format_Grayscale8);
    int x0 = ga.width(), y0 = ga.height(), x1 = -1, y1 = -1;
    for (int y = 0; y < ga.height(); ++y) {
        const uchar* ra = ga.constScanLine(y);
        const uchar* rb = gb.constScanLine(y);
        if (std::memcmp(ra, rb, size_t(ga.width())) == 0)
            continue;
        for (int x = 0; x < ga.width(); ++x) {
            if (ra[x] == rb[x])
                continue;
            x0 = std::min(x0, x);
            x1 = std::max(x1, x);
            y0 = std::min(y0, y);
            y1 = std::max(y1, y);
        }
    }
    return x1 < 0 ? QRect() : QRect(QPoint(x0, y0), QPoint(x1, y1));
}

QImage refineAlpha(const QImage& alphaImage, int shift, int feather, int hardness)
{
    QImage a = alphaImage.convertToFormat(QImage::Format_Grayscale8);
    const int w = a.width(), h = a.height();
    // Kante verschieben: abwechselnd Quadrat und Kreuz (achteckige Form statt Kästchen)
    for (int step = 0; step < std::abs(shift); ++step) {
        const bool grow = shift > 0;
        const bool square = step % 2 == 0;
        QImage src = a.copy();
        for (int y = 0; y < h; ++y) {
            uchar* o = a.scanLine(y);
            for (int x = 0; x < w; ++x) {
                int v = src.constScanLine(y)[x];
                for (int dy = -1; dy <= 1; ++dy) {
                    const int yy = y + dy;
                    if (yy < 0 || yy >= h)
                        continue;
                    const uchar* row = src.constScanLine(yy);
                    for (int dx = -1; dx <= 1; ++dx) {
                        const int xx = x + dx;
                        if (xx < 0 || xx >= w || (!square && dx != 0 && dy != 0))
                            continue;
                        v = grow ? std::max(v, int(row[xx])) : std::min(v, int(row[xx]));
                    }
                }
                o[x] = uchar(v);
            }
        }
    }
    if (feather > 0) {
        // zweimal Kastenfilter ≈ Gauss
        Plane p(size_t(w) * h), q(p.size()), one(p.size(), 1.0f), count(p.size()), tmp;
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
                p[size_t(y) * w + x] = a.constScanLine(y)[x];
        const int r = std::max(1, feather / 2);
        boxSum(one.data(), count.data(), w, h, r, tmp);
        boxMean(p.data(), q.data(), w, h, r, count, tmp);
        boxMean(q.data(), p.data(), w, h, r, count, tmp);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
                a.scanLine(y)[x] = uchar(std::clamp(int(std::lround(p[size_t(y) * w + x])), 0, 255));
    }
    if (hardness > 0) {
        const double k = 1.0 + std::clamp(hardness, 0, 100) * 0.09;
        uchar table[256];
        for (int i = 0; i < 256; ++i)
            table[i] = uchar(std::clamp(int(std::lround(((i / 255.0 - 0.5) * k + 0.5) * 255.0)), 0, 255));
        for (int y = 0; y < h; ++y) {
            uchar* row = a.scanLine(y);
            for (int x = 0; x < w; ++x)
                row[x] = table[row[x]];
        }
    }
    return a;
}

QImage compose(const QImage& image, const QImage& alphaImage, bool decontaminate)
{
    const QImage alpha = grayOf(alphaImage, image.size());
    const bool deep = image.depth() == 64;
    QImage out = image.convertToFormat(QImage::Format_RGBA64);
    const int w = out.width(), h = out.height();

    // Kantenbereich: 0 < α < 1
    QRect edge;
    for (int y = 0; y < h; ++y) {
        const uchar* row = alpha.constScanLine(y);
        for (int x = 0; x < w; ++x)
            if (row[x] > 0 && row[x] < 255)
                edge |= QRect(x, y, 1, 1);
    }
    if (decontaminate && !edge.isEmpty()) {
        // Vordergrundfarbe schätzen (Blur-Fusion, Forte & Pitié 2021): grob, dann fein
        const int r1 = std::max(8, int(std::lround(45.0 * std::max(w, h) / 1500.0)));
        const int r2 = std::max(2, r1 / 15);
        const QRect area = edge.adjusted(-r1 - 2, -r1 - 2, r1 + 2, r1 + 2) & out.rect();
        const int aw = area.width(), ah = area.height();
        const size_t n = size_t(aw) * ah;
        Plane I[3], F[3], B[3], A(n);
        for (int c = 0; c < 3; ++c) {
            I[c].resize(n);
        }
        for (int y = 0; y < ah; ++y) {
            const auto* px = reinterpret_cast<const QRgba64*>(out.constScanLine(area.y() + y)) + area.x();
            const uchar* ar = alpha.constScanLine(area.y() + y) + area.x();
            for (int x = 0; x < aw; ++x) {
                const size_t i = size_t(y) * aw + x;
                I[0][i] = px[x].red() / 65535.0f;
                I[1][i] = px[x].green() / 65535.0f;
                I[2][i] = px[x].blue() / 65535.0f;
                A[i] = ar[x] / 255.0f;
            }
        }
        for (int c = 0; c < 3; ++c) {
            F[c] = I[c];
            B[c] = I[c];
        }
        Plane one(n, 1.0f), tmp, prod(n), bA(n), bFA(n), bB1A(n);
        for (int r : {r1, r2}) {
            Plane count(n);
            boxSum(one.data(), count.data(), aw, ah, r, tmp);
            boxMean(A.data(), bA.data(), aw, ah, r, count, tmp);
            for (int c = 0; c < 3; ++c) {
                for (size_t i = 0; i < n; ++i)
                    prod[i] = F[c][i] * A[i];
                boxMean(prod.data(), bFA.data(), aw, ah, r, count, tmp);
                for (size_t i = 0; i < n; ++i)
                    prod[i] = B[c][i] * (1 - A[i]);
                boxMean(prod.data(), bB1A.data(), aw, ah, r, count, tmp);
                for (size_t i = 0; i < n; ++i) {
                    const float bF = bFA[i] / (bA[i] + 1e-5f);
                    const float bB = bB1A[i] / ((1 - bA[i]) + 1e-5f);
                    F[c][i] = std::clamp(bF + A[i] * (I[c][i] - A[i] * bF - (1 - A[i]) * bB), 0.0f, 1.0f);
                    B[c][i] = bB;
                }
            }
        }
        for (int y = 0; y < ah; ++y) {
            auto* px = reinterpret_cast<QRgba64*>(out.scanLine(area.y() + y)) + area.x();
            const uchar* ar = alpha.constScanLine(area.y() + y) + area.x();
            for (int x = 0; x < aw; ++x) {
                if (ar[x] == 0 || ar[x] == 255)
                    continue;
                const size_t i = size_t(y) * aw + x;
                px[x] = QRgba64::fromRgba64(quint16(std::lround(F[0][i] * 65535.0f)), quint16(std::lround(F[1][i] * 65535.0f)),
                                            quint16(std::lround(F[2][i] * 65535.0f)), px[x].alpha());
            }
        }
    }
    for (int y = 0; y < h; ++y) {
        auto* px = reinterpret_cast<QRgba64*>(out.scanLine(y));
        const uchar* ar = alpha.constScanLine(y);
        for (int x = 0; x < w; ++x)
            px[x].setAlpha(quint16(quint32(px[x].alpha()) * ar[x] / 255));
    }
    return deep ? out : out.convertToFormat(QImage::Format_ARGB32);
}

} // namespace vek::cutout
