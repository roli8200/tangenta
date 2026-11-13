#include "Mosaic.h"

#include <QDir>
#include <QImageReader>
#include <QPainter>
#include <QTransform>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <random>

namespace vek::mosaics {

namespace {

QImage to8Bit(const QImage& image)
{
    return image.format() == QImage::Format_ARGB32_Premultiplied ? image : image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}

double luminance(const QColor& c)
{
    return (0.299 * c.red() + 0.587 * c.green() + 0.114 * c.blue()) / 255.0;
}

} // namespace

QColor meanColor(const QImage& source, const QRect& area)
{
    const QImage image = to8Bit(source);
    const QRect r = (area.isNull() ? image.rect() : area).intersected(image.rect());
    if (r.isEmpty())
        return QColor(0, 0, 0, 0);
    // Vormultipliziert summieren: durchsichtige Punkte zählen nicht zur Farbe
    double sr = 0, sg = 0, sb = 0, sa = 0;
    for (int y = r.top(); y <= r.bottom(); ++y) {
        const auto* line = reinterpret_cast<const QRgb*>(image.constScanLine(y));
        for (int x = r.left(); x <= r.right(); ++x) {
            const QRgb p = line[x];
            sr += qRed(p);
            sg += qGreen(p);
            sb += qBlue(p);
            sa += qAlpha(p);
        }
    }
    const double n = double(r.width()) * r.height();
    if (sa <= 0)
        return QColor(0, 0, 0, 0);
    auto ch = [sa](double v) { return std::clamp(int(std::lround(v / sa * 255.0)), 0, 255); };
    return QColor(ch(sr), ch(sg), ch(sb), std::clamp(int(std::lround(sa / n)), 0, 255));
}

double colorDistance(const QColor& a, const QColor& b)
{
    // "Redmean": Gewichte je nach Rotanteil, nah an der Wahrnehmung, aber schnell
    const double rm = (a.red() + b.red()) / 2.0;
    const double dr = a.red() - b.red(), dg = a.green() - b.green(), db = a.blue() - b.blue();
    return std::sqrt((2.0 + rm / 256.0) * dr * dr + 4.0 * dg * dg + (2.0 + (255.0 - rm) / 256.0) * db * db);
}

QStringList imageFiles(const QString& folder)
{
    QStringList patterns;
    for (const QByteArray& suffix : QImageReader::supportedImageFormats())
        patterns << QStringLiteral("*.") + QString::fromLatin1(suffix);
    QDir dir(folder);
    QStringList out;
    for (const QString& name : dir.entryList(patterns, QDir::Files | QDir::Readable, QDir::Name | QDir::IgnoreCase))
        out << dir.filePath(name);
    return out;
}

QList<Tile> loadTiles(const QString& folder, int thumbSize, const std::function<bool(int, int)>& progress)
{
    QList<Tile> tiles;
    const QStringList files = imageFiles(folder);
    thumbSize = std::max(4, thumbSize);
    for (int i = 0; i < files.size(); ++i) {
        if (progress && !progress(i, int(files.size())))
            break;
        QImageReader reader(files[i]);
        reader.setAutoTransform(true);
        // Grosse Fotos gleich verkleinert lesen (JPEG kann das beim Dekodieren)
        const QSize full = reader.size();
        if (full.isValid() && std::min(full.width(), full.height()) > thumbSize * 2)
            reader.setScaledSize(full.scaled(thumbSize * 2, thumbSize * 2, Qt::KeepAspectRatioByExpanding));
        QImage image = reader.read();
        if (image.isNull())
            continue;
        // Quadrat aus der Mitte
        const int side = std::min(image.width(), image.height());
        image = image.copy((image.width() - side) / 2, (image.height() - side) / 2, side, side);
        Tile t;
        t.path = files[i];
        t.thumb = to8Bit(image.scaled(thumbSize, thumbSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
        t.mean = meanColor(t.thumb);
        tiles << t;
    }
    return tiles;
}

int autoRows(const QSize& image, int columns)
{
    if (image.width() <= 0 || columns <= 0)
        return 1;
    return std::max(1, int(std::lround(double(columns) * image.height() / image.width())));
}

QList<Cell> matchTiles(const QImage& source, const QList<Tile>& tiles, const PhotoSettings& settings)
{
    QList<Cell> cells;
    const QImage target = to8Bit(source);
    if (target.isNull())
        return cells;
    const int cols = std::clamp(settings.columns, 1, 1000);
    const int rows = settings.rows > 0 ? std::clamp(settings.rows, 1, 1000) : autoRows(target.size(), cols);
    QList<int> used(tiles.size(), 0);
    QList<int> grid(qsizetype(rows) * cols, -1);
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            Cell c;
            c.row = row;
            c.column = col;
            c.rect = QRectF(double(col) / cols, double(row) / rows, 1.0 / cols, 1.0 / rows);
            const QRect px(int(std::floor(c.rect.left() * target.width())), int(std::floor(c.rect.top() * target.height())),
                           std::max(1, int(std::ceil(c.rect.width() * target.width()))),
                           std::max(1, int(std::ceil(c.rect.height() * target.height()))));
            c.target = meanColor(target, px);
            if (c.target.alpha() >= 128 && !tiles.isEmpty()) {
                const int left = col > 0 ? grid[row * cols + col - 1] : -1;
                const int up = row > 0 ? grid[(row - 1) * cols + col] : -1;
                auto pick = [&](bool limit, bool neighbors) {
                    int best = -1;
                    double bestD = 1e300;
                    for (int i = 0; i < tiles.size(); ++i) {
                        if (limit && settings.maxRepeats > 0 && used[i] >= settings.maxRepeats)
                            continue;
                        if (neighbors && (i == left || i == up))
                            continue;
                        const double d = colorDistance(c.target, tiles[i].mean);
                        if (d < bestD) {
                            bestD = d;
                            best = i;
                        }
                    }
                    return best;
                };
                const bool neighbors = settings.avoidNeighbors && tiles.size() > 2;
                int best = pick(true, neighbors);
                if (best < 0)
                    best = pick(true, false);
                if (best < 0)
                    best = pick(false, false); // alle verbraucht: Grenze aufheben
                c.tile = best;
                if (best >= 0)
                    ++used[best];
                grid[row * cols + col] = best;
            }
            cells << c;
        }
    }
    return cells;
}

QImage tileImage(const Tile& tile, const QSize& size, const QColor& target, double tint)
{
    if (tile.thumb.isNull() || size.isEmpty())
        return {};
    // Aus der Mitte auf das Seitenverhältnis der Zelle schneiden
    const QImage& t = tile.thumb;
    const double aspect = double(size.width()) / size.height();
    QRect crop = t.rect();
    if (aspect >= 1.0)
        crop.setHeight(std::max(1, int(std::lround(t.width() / aspect))));
    else
        crop.setWidth(std::max(1, int(std::lround(t.height() * aspect))));
    crop.moveCenter(t.rect().center());
    QImage out = to8Bit(t.copy(crop).scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    const double k = std::clamp(tint, 0.0, 100.0) / 100.0;
    if (k > 0) {
        // Farbangleichung: alle Punkte um den Unterschied der Mittelfarben verschieben
        const int dr = int(std::lround((target.red() - tile.mean.red()) * k));
        const int dg = int(std::lround((target.green() - tile.mean.green()) * k));
        const int db = int(std::lround((target.blue() - tile.mean.blue()) * k));
        for (int y = 0; y < out.height(); ++y) {
            auto* line = reinterpret_cast<QRgb*>(out.scanLine(y));
            for (int x = 0; x < out.width(); ++x) {
                const int a = qAlpha(line[x]);
                line[x] = qRgba(std::clamp(qRed(line[x]) + dr * a / 255, 0, a), std::clamp(qGreen(line[x]) + dg * a / 255, 0, a),
                                std::clamp(qBlue(line[x]) + db * a / 255, 0, a), a);
            }
        }
    }
    return out;
}

QImage renderMosaic(const QImage& target, const QList<Tile>& tiles, const QList<Cell>& cells, const PhotoSettings& settings,
                    int cellPixels)
{
    if (cells.isEmpty() || target.isNull())
        return {};
    int cols = 0, rows = 0;
    for (const Cell& c : cells) {
        cols = std::max(cols, c.column + 1);
        rows = std::max(rows, c.row + 1);
    }
    cellPixels = std::max(1, cellPixels);
    // Zellen haben das Seitenverhältnis von Vorlage / Raster
    const double cellAspect = (double(target.height()) / rows) / (double(target.width()) / cols);
    const int cw = cellPixels, ch = std::max(1, int(std::lround(cellPixels * cellAspect)));
    QImage out(cw * cols, ch * rows, QImage::Format_ARGB32_Premultiplied);
    out.fill(Qt::transparent);
    QPainter p(&out);
    for (const Cell& c : cells) {
        if (c.tile < 0 || c.tile >= tiles.size())
            continue;
        p.drawImage(QPoint(c.column * cw, c.row * ch), tileImage(tiles[c.tile], QSize(cw, ch), c.target, settings.tint));
    }
    p.end();
    return out;
}

QList<Dot> pointillize(const QImage& source, const QSizeF& sizeMm, const PointSettings& s)
{
    QList<Dot> dots;
    if (source.isNull() || sizeMm.width() <= 0 || sizeMm.height() <= 0)
        return dots;
    const double cell = std::max(0.05, s.cell);
    const int cols = int(std::ceil(sizeMm.width() / cell)), rows = int(std::ceil(sizeMm.height() / cell));
    if (double(cols) * rows > 4e6)
        return dots;
    // Grosse Bilder so verkleinern, dass eine Zelle etwa 8 Pixel breit ist (schneller)
    QImage image = to8Bit(source);
    const double pxPerCell = image.width() / (sizeMm.width() / cell);
    if (pxPerCell > 16.0)
        image = to8Bit(image.scaled(std::max(1, int(image.width() * 8.0 / pxPerCell)), std::max(1, int(image.height() * 8.0 / pxPerCell)),
                                    Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    const double sx = image.width() / sizeMm.width(), sy = image.height() / sizeMm.height();
    // Raster um die Bildmitte gedreht, so gross, dass es das Bild ganz bedeckt
    const QPointF center(sizeMm.width() / 2.0, sizeMm.height() / 2.0);
    const double reach = std::hypot(sizeMm.width(), sizeMm.height()) / 2.0;
    const int n = int(std::ceil(reach / cell)) + 1;
    const double a = qDegreesToRadians(s.angle);
    const QPointF ex(std::cos(a), std::sin(a)), ey(-std::sin(a), std::cos(a));
    std::mt19937 rng(s.seed);
    std::uniform_real_distribution<double> jitter(-0.5, 0.5);
    const double maxSize = cell * std::clamp(s.scale, 1.0, 400.0) / 100.0;
    const double minK = std::clamp(s.minSize, 0.0, 100.0) / 100.0;
    // Ohne Winkel genau am Bild ausrichten (erste Zelle oben links)
    const bool aligned = std::abs(std::fmod(s.angle, 360.0)) < 1e-9;
    const int i0 = aligned ? 0 : -n, i1 = aligned ? cols - 1 : n;
    const int j0 = aligned ? 0 : -n, j1 = aligned ? rows - 1 : n;
    for (int j = j0; j <= j1; ++j) {
        for (int i = i0; i <= i1; ++i) {
            QPointF p = aligned ? QPointF((i + 0.5) * cell, (j + 0.5) * cell) : center + ex * (i * cell) + ey * (j * cell);
            if (s.jitter > 0) {
                const double k = std::clamp(s.jitter, 0.0, 100.0) / 100.0 * cell;
                p += QPointF(jitter(rng) * k, jitter(rng) * k);
            }
            if (p.x() < 0 || p.y() < 0 || p.x() > sizeMm.width() || p.y() > sizeMm.height())
                continue;
            // Pixel, deren Mitte in der Zelle liegt
            const int x0 = int(std::lround((p.x() - cell / 2) * sx)), x1 = int(std::lround((p.x() + cell / 2) * sx));
            const int y0 = int(std::lround((p.y() - cell / 2) * sy)), y1 = int(std::lround((p.y() + cell / 2) * sy));
            const QRect area(x0, y0, std::max(1, x1 - x0), std::max(1, y1 - y0));
            const QColor c = meanColor(image, area);
            if (c.alpha() < 128)
                continue;
            Dot d;
            d.center = p;
            d.rotation = s.angle + s.rotation;
            double dark = 1.0 - luminance(c);
            if (s.invert)
                dark = 1.0 - dark;
            switch (s.mode) {
            case PointSettings::Mode::Color:
                d.size = maxSize;
                d.color = Color(std::uint8_t(c.red()), std::uint8_t(c.green()), std::uint8_t(c.blue()));
                break;
            case PointSettings::Mode::Size:
                d.size = maxSize * (minK + (1.0 - minK) * dark);
                d.color = s.color;
                break;
            case PointSettings::Mode::ColorAndSize:
                d.size = maxSize * (minK + (1.0 - minK) * dark);
                d.color = Color(std::uint8_t(c.red()), std::uint8_t(c.green()), std::uint8_t(c.blue()));
                break;
            }
            if (d.size < 1e-3)
                continue;
            dots << d;
        }
    }
    // Farben begrenzen: k-Means über die Punktfarben
    const int k = s.limitColors;
    if (k > 0 && s.mode != PointSettings::Mode::Size && !dots.isEmpty()) {
        QList<int> order(dots.size());
        for (int i = 0; i < order.size(); ++i)
            order[i] = i;
        auto lum = [&](int i) { return 0.299 * dots[i].color.r + 0.587 * dots[i].color.g + 0.114 * dots[i].color.b; };
        std::sort(order.begin(), order.end(), [&](int x, int y) { return lum(x) < lum(y); });
        struct C {
            double r, g, b;
        };
        QList<C> centers;
        for (int c = 0; c < k; ++c) {
            const Dot& d = dots[order[std::min<qsizetype>(order.size() - 1, (2 * c + 1) * order.size() / (2 * k))]];
            centers << C{double(d.color.r), double(d.color.g), double(d.color.b)};
        }
        QList<int> label(dots.size(), 0);
        for (int iter = 0; iter < 10; ++iter) {
            QList<C> sum(k, C{0, 0, 0});
            QList<int> count(k, 0);
            for (int i = 0; i < dots.size(); ++i) {
                double best = 1e300;
                for (int c = 0; c < k; ++c) {
                    const double dr = dots[i].color.r - centers[c].r, dg = dots[i].color.g - centers[c].g, db = dots[i].color.b - centers[c].b;
                    const double dd = dr * dr + dg * dg + db * db;
                    if (dd < best) {
                        best = dd;
                        label[i] = c;
                    }
                }
                sum[label[i]].r += dots[i].color.r;
                sum[label[i]].g += dots[i].color.g;
                sum[label[i]].b += dots[i].color.b;
                ++count[label[i]];
            }
            for (int c = 0; c < k; ++c)
                if (count[c] > 0)
                    centers[c] = C{sum[c].r / count[c], sum[c].g / count[c], sum[c].b / count[c]};
        }
        for (int i = 0; i < dots.size(); ++i) {
            const C& c = centers[label[i]];
            dots[i].color = Color(std::uint8_t(std::lround(c.r)), std::uint8_t(std::lround(c.g)), std::uint8_t(std::lround(c.b)));
        }
    }
    return dots;
}

QPainterPath dotPath(const Dot& dot, PointSettings::Shape shape, const QPainterPath& custom)
{
    QPainterPath unit;
    const double h = dot.size / 2.0;
    switch (shape) {
    case PointSettings::Shape::Circle:
        unit.addEllipse(QPointF(0, 0), h, h);
        return unit.translated(dot.center);
    case PointSettings::Shape::Square:
        unit.addRect(QRectF(-h, -h, dot.size, dot.size));
        break;
    case PointSettings::Shape::Custom: {
        const QRectF b = custom.boundingRect();
        if (custom.isEmpty() || b.width() <= 0 || b.height() <= 0) {
            unit.addEllipse(QPointF(0, 0), h, h);
            return unit.translated(dot.center);
        }
        // Proportional in das Quadrat einpassen
        const double k = dot.size / std::max(b.width(), b.height());
        unit = QTransform::fromTranslate(-b.center().x(), -b.center().y()).map(custom);
        unit = QTransform::fromScale(k, k).map(unit);
        break;
    }
    }
    QTransform t;
    t.translate(dot.center.x(), dot.center.y());
    t.rotate(dot.rotation);
    return t.map(unit);
}

} // namespace vek::mosaics
