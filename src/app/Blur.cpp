#include "Blur.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace vek {

namespace {

// Breiten dreier Kastenfilter, deren Hintereinanderausführung einer Gausskurve mit sigma
// entspricht (Kovesi, "Fast Almost-Gaussian Filtering")
std::array<int, 3> boxSizes(double sigma)
{
    const int n = 3;
    const double ideal = std::sqrt(12.0 * sigma * sigma / n + 1.0);
    int lower = int(std::floor(ideal));
    if (lower % 2 == 0)
        --lower;
    const int upper = lower + 2;
    const double m = (12.0 * sigma * sigma - n * lower * lower - 4.0 * n * lower - 3.0 * n) / (-4.0 * lower - 4.0);
    const int small = int(std::lround(m));
    std::array<int, 3> sizes{};
    for (int i = 0; i < n; ++i)
        sizes[std::size_t(i)] = i < small ? lower : upper;
    return sizes;
}

// Ein Kastenfilter über eine Zeile oder Spalte; ausserhalb gilt Transparenz
void boxPass(const quint8* src, quint8* dst, int count, int stride, int radius)
{
    const int width = 2 * radius + 1;
    for (int c = 0; c < 4; ++c) {
        int sum = 0;
        for (int i = 0; i < std::min(radius, count); ++i)
            sum += src[i * stride + c];
        for (int i = 0; i < count; ++i) {
            const int add = i + radius;
            const int remove = i - radius - 1;
            if (add < count)
                sum += src[add * stride + c];
            if (remove >= 0)
                sum -= src[remove * stride + c];
            dst[i * stride + c] = quint8((sum + width / 2) / width);
        }
    }
}

} // namespace

void gaussianBlur(QImage& image, double sigma)
{
    if (sigma < 0.3 || image.isNull())
        return;
    Q_ASSERT(image.format() == QImage::Format_ARGB32_Premultiplied);
    const int w = image.width();
    const int h = image.height();
    const std::array<int, 3> sizes = boxSizes(sigma);
    std::vector<quint8> line(std::size_t(std::max(w, h)) * 4);
    std::vector<quint8> out(line.size());
    for (int size : sizes) {
        const int radius = (size - 1) / 2;
        for (int y = 0; y < h; ++y) {
            quint8* row = image.scanLine(y);
            boxPass(row, out.data(), w, 4, radius);
            std::copy_n(out.data(), std::size_t(w) * 4, row);
        }
        const qsizetype bpl = image.bytesPerLine();
        quint8* bits = image.bits();
        for (int x = 0; x < w; ++x) {
            for (int y = 0; y < h; ++y)
                std::copy_n(bits + y * bpl + x * 4, 4, line.data() + std::size_t(y) * 4);
            boxPass(line.data(), out.data(), h, 4, radius);
            for (int y = 0; y < h; ++y)
                std::copy_n(out.data() + std::size_t(y) * 4, 4, bits + y * bpl + x * 4);
        }
    }
}

void rampBlur(QImage& image, const std::function<double(double, double)>& sigmaAt, double minSigma, double maxSigma)
{
    if (maxSigma <= 0.0 || image.isNull())
        return;
    minSigma = std::clamp(minSigma, 0.0, maxSigma);
    // Stufen höchstens ~2 px Sigma auseinander, damit das Mischen nicht auffällt
    const int levels = std::clamp(int(std::ceil((maxSigma - minSigma) / 2.0)) + 1, 2, 10);
    std::vector<QImage> stages;
    stages.reserve(std::size_t(levels));
    for (int k = 0; k < levels; ++k) {
        QImage stage = image.copy();
        gaussianBlur(stage, minSigma + (maxSigma - minSigma) * k / (levels - 1));
        stages.push_back(std::move(stage));
    }
    const double span = maxSigma - minSigma;
    for (int y = 0; y < image.height(); ++y) {
        auto* out = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            const double s = std::clamp(sigmaAt(x + 0.5, y + 0.5), minSigma, maxSigma);
            const double f = span > 0.0 ? (s - minSigma) / span * (levels - 1) : 0.0;
            const int k = std::min(int(f), levels - 2);
            const double w = f - k;
            const QRgb a = reinterpret_cast<const QRgb*>(stages[std::size_t(k)].constScanLine(y))[x];
            const QRgb b = reinterpret_cast<const QRgb*>(stages[std::size_t(k + 1)].constScanLine(y))[x];
            auto mix = [w](int u, int v) { return int(std::lround(u + (v - u) * w)); };
            out[x] = qRgba(mix(qRed(a), qRed(b)), mix(qGreen(a), qGreen(b)), mix(qBlue(a), qBlue(b)),
                           mix(qAlpha(a), qAlpha(b)));
        }
    }
}

void applyOpacity(QImage& image, const std::function<double(double, double)>& opacityAt)
{
    for (int y = 0; y < image.height(); ++y) {
        auto* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(line[x]) == 0)
                continue;
            const double o = std::clamp(opacityAt(x + 0.5, y + 0.5), 0.0, 1.0);
            const QRgb c = line[x];
            // vormultipliziert: alle Kanäle gleich skalieren
            line[x] = qRgba(int(qRed(c) * o + 0.5), int(qGreen(c) * o + 0.5), int(qBlue(c) * o + 0.5),
                            int(qAlpha(c) * o + 0.5));
        }
    }
}

} // namespace vek
