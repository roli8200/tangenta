#include "ImageCodecs.h"

#include <QHash>
#include <QStringList>
#include <QtEndian>

#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <vector>

namespace vek::codecs {

namespace {

// --- Lesehilfen (gross- bzw. kleinendig) ---------------------------------------------------------

struct Reader {
    const QByteArray& d;
    qsizetype pos = 0;
    bool ok = true;
    bool need(qsizetype n)
    {
        if (pos + n > d.size())
            ok = false;
        return ok;
    }
    quint8 u8()
    {
        if (!need(1))
            return 0;
        return quint8(d[pos++]);
    }
    quint16 be16()
    {
        if (!need(2))
            return 0;
        const quint16 v = qFromBigEndian<quint16>(d.constData() + pos);
        pos += 2;
        return v;
    }
    quint32 be32()
    {
        if (!need(4))
            return 0;
        const quint32 v = qFromBigEndian<quint32>(d.constData() + pos);
        pos += 4;
        return v;
    }
    quint16 le16()
    {
        if (!need(2))
            return 0;
        const quint16 v = qFromLittleEndian<quint16>(d.constData() + pos);
        pos += 2;
        return v;
    }
    void skip(qsizetype n)
    {
        if (need(n))
            pos += n;
    }
};

void putBe16(QByteArray& out, quint16 v)
{
    out.append(char(v >> 8)).append(char(v & 0xff));
}

void putBe32(QByteArray& out, quint32 v)
{
    putBe16(out, quint16(v >> 16));
    putBe16(out, quint16(v & 0xffff));
}

void putLe16(QByteArray& out, quint16 v)
{
    out.append(char(v & 0xff)).append(char(v >> 8));
}

// PackBits (PSD) bzw. dessen Gegenstück
bool unpackBits(Reader& r, qsizetype packed, QByteArray& out, qsizetype want)
{
    const qsizetype end = r.pos + packed;
    while (r.ok && r.pos < end && out.size() < want) {
        const qint8 n = qint8(r.u8());
        if (n >= 0) {
            for (int i = 0; i <= n && r.ok; ++i)
                out.append(char(r.u8()));
        } else if (n != -128) {
            const char v = char(r.u8());
            out.append(QByteArray(1 - n, v));
        }
    }
    r.pos = end;
    out.resize(want, 0);
    return r.ok;
}

} // namespace

// --- PSD ---------------------------------------------------------------------------------------

QImage readPsd(const QByteArray& data)
{
    Reader r{data};
    if (data.size() < 26 || !data.startsWith("8BPS"))
        return {};
    r.skip(4);
    if (r.be16() != 1) // 2 = PSB (grosse Dateien) wird nicht unterstützt
        return {};
    r.skip(6);
    const int channels = r.be16();
    const int height = int(r.be32());
    const int width = int(r.be32());
    const int depth = r.be16();
    const int mode = r.be16();
    if (!r.ok || width <= 0 || height <= 0 || channels <= 0 || (depth != 8 && depth != 16 && depth != 1)
        || qint64(width) * height > 400LL * 1000 * 1000)
        return {};
    // Farbtabelle (indiziert), Bildressourcen, Ebenen: Längen überspringen
    const quint32 colorLen = r.be32();
    QByteArray palette = data.mid(r.pos, colorLen);
    r.skip(colorLen);
    r.skip(r.be32());
    r.skip(r.be32());
    const int compression = r.be16();
    if (!r.ok || compression > 1)
        return {};
    const int bytesPerSample = depth == 16 ? 2 : 1;
    const qsizetype rowBytes = depth == 1 ? (width + 7) / 8 : qsizetype(width) * bytesPerSample;
    // Kanäle planar einlesen
    std::vector<QByteArray> planes(static_cast<std::size_t>(channels));
    if (compression == 0) {
        for (int c = 0; c < channels; ++c) {
            planes[std::size_t(c)] = data.mid(r.pos, rowBytes * height);
            r.skip(rowBytes * height);
        }
    } else {
        std::vector<quint16> counts(static_cast<std::size_t>(channels) * height);
        for (quint16& n : counts)
            n = r.be16();
        for (int c = 0; c < channels; ++c) {
            QByteArray& plane = planes[std::size_t(c)];
            plane.reserve(rowBytes * height);
            for (int y = 0; y < height && r.ok; ++y) {
                QByteArray row;
                unpackBits(r, counts[std::size_t(c) * height + y], row, rowBytes);
                plane += row;
            }
        }
    }
    if (!r.ok && planes.back().size() < rowBytes * height)
        return {};
    auto sample = [&](int c, int x, int y) -> double { // 0..1
        const QByteArray& p = planes[std::size_t(std::min(c, channels - 1))];
        const qsizetype i = qsizetype(y) * rowBytes + qsizetype(x) * bytesPerSample;
        if (i + bytesPerSample > p.size())
            return 0.0;
        if (depth == 16)
            return qFromBigEndian<quint16>(p.constData() + i) / 65535.0;
        return quint8(p[i]) / 255.0;
    };
    const int base = mode == 4 ? 4 : mode == 3 ? 3 : 1;
    const bool alpha = channels > base && (mode == 3 || mode == 1 || mode == 4);
    QImage img(width, height, depth == 16 ? QImage::Format_RGBA64 : QImage::Format_ARGB32);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            double rr = 0, gg = 0, bb = 0, aa = 1;
            switch (mode) {
            case 0: { // Bitmap: 1 Bit, 1 = schwarz
                const quint8 byte = quint8(planes[0][qsizetype(y) * rowBytes + x / 8]);
                rr = gg = bb = (byte >> (7 - x % 8)) & 1 ? 0.0 : 1.0;
                break;
            }
            case 2: { // indiziert
                const int i = quint8(planes[0][qsizetype(y) * rowBytes + x]);
                if (palette.size() >= 768) {
                    rr = quint8(palette[i]) / 255.0;
                    gg = quint8(palette[256 + i]) / 255.0;
                    bb = quint8(palette[512 + i]) / 255.0;
                }
                break;
            }
            case 3: rr = sample(0, x, y); gg = sample(1, x, y); bb = sample(2, x, y); break;
            case 4: { // CMYK, in PSD invertiert gespeichert
                const double c = 1 - sample(0, x, y), m = 1 - sample(1, x, y), ye = 1 - sample(2, x, y), k = 1 - sample(3, x, y);
                rr = (1 - c) * (1 - k);
                gg = (1 - m) * (1 - k);
                bb = (1 - ye) * (1 - k);
                break;
            }
            default: rr = gg = bb = sample(0, x, y); break; // Graustufen, Duplex, Mehrkanal
            }
            if (alpha)
                aa = sample(base, x, y);
            if (depth == 16)
                reinterpret_cast<QRgba64*>(img.scanLine(y))[x] =
                    QRgba64::fromRgba64(quint16(rr * 65535 + 0.5), quint16(gg * 65535 + 0.5), quint16(bb * 65535 + 0.5), quint16(aa * 65535 + 0.5));
            else
                reinterpret_cast<QRgb*>(img.scanLine(y))[x] = qRgba(int(rr * 255 + 0.5), int(gg * 255 + 0.5), int(bb * 255 + 0.5), int(aa * 255 + 0.5));
        }
    }
    return img;
}

QByteArray writePsd(const QImage& source)
{
    const bool deep = source.depth() > 32;
    const QImage img = source.convertToFormat(deep ? QImage::Format_RGBA64 : QImage::Format_ARGB32);
    const bool alpha = img.hasAlphaChannel();
    const int channels = alpha ? 4 : 3;
    QByteArray out("8BPS");
    putBe16(out, 1);
    out.append(QByteArray(6, 0));
    putBe16(out, quint16(channels));
    putBe32(out, quint32(img.height()));
    putBe32(out, quint32(img.width()));
    putBe16(out, deep ? 16 : 8);
    putBe16(out, 3); // RGB
    putBe32(out, 0); // Farbtabelle
    putBe32(out, 0); // Ressourcen
    putBe32(out, 0); // Ebenen und Masken
    putBe16(out, 0); // unkomprimiert
    for (int c = 0; c < channels; ++c)
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x) {
                if (deep) {
                    const QRgba64 px = reinterpret_cast<const QRgba64*>(img.constScanLine(y))[x];
                    putBe16(out, c == 0 ? px.red() : c == 1 ? px.green() : c == 2 ? px.blue() : px.alpha());
                } else {
                    const QRgb px = reinterpret_cast<const QRgb*>(img.constScanLine(y))[x];
                    out.append(char(c == 0 ? qRed(px) : c == 1 ? qGreen(px) : c == 2 ? qBlue(px) : qAlpha(px)));
                }
            }
    return out;
}

// --- PCX ---------------------------------------------------------------------------------------

QImage readPcx(const QByteArray& data)
{
    if (data.size() < 128 || quint8(data[0]) != 0x0A || quint8(data[2]) > 1)
        return {};
    Reader r{data};
    r.skip(1);
    r.u8(); // Version
    const bool rle = r.u8() == 1;
    const int bpp = r.u8();
    const int xmin = r.le16(), ymin = r.le16(), xmax = r.le16(), ymax = r.le16();
    r.skip(4); // Auflösung
    const QByteArray egaPalette = data.mid(16, 48);
    r.pos = 65;
    const int planes = r.u8();
    const int bytesPerLine = r.le16();
    const int width = xmax - xmin + 1, height = ymax - ymin + 1;
    if (width <= 0 || height <= 0 || bytesPerLine <= 0 || planes <= 0 || qint64(width) * height > 400LL * 1000 * 1000)
        return {};
    r.pos = 128;
    const qsizetype lineSize = qsizetype(bytesPerLine) * planes;
    QByteArray pixels;
    pixels.reserve(lineSize * height);
    while (pixels.size() < lineSize * height && r.pos < data.size()) {
        const quint8 c = r.u8();
        if (rle && (c & 0xC0) == 0xC0) {
            const int count = c & 0x3F;
            pixels.append(QByteArray(count, char(r.u8())));
        } else {
            pixels.append(char(c));
        }
    }
    pixels.resize(lineSize * height, 0);
    // 256er-Palette am Dateiende (0x0C + 768 Bytes)
    QByteArray palette;
    if (bpp == 8 && planes == 1 && data.size() >= 769 && quint8(data[data.size() - 769]) == 0x0C)
        palette = data.right(768);
    QImage img(width, height, QImage::Format_RGB32);
    for (int y = 0; y < height; ++y) {
        const char* line = pixels.constData() + qsizetype(y) * lineSize;
        auto* out = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < width; ++x) {
            if (bpp == 8 && planes >= 3) {
                out[x] = qRgb(quint8(line[x]), quint8(line[bytesPerLine + x]), quint8(line[2 * bytesPerLine + x]));
            } else if (bpp == 8) {
                const int i = quint8(line[x]);
                out[x] = palette.size() == 768 ? qRgb(quint8(palette[3 * i]), quint8(palette[3 * i + 1]), quint8(palette[3 * i + 2])) : qRgb(i, i, i);
            } else if (bpp == 1) {
                // 1 Bit, eine bis vier Ebenen (EGA 16 Farben)
                int index = 0;
                for (int p = 0; p < planes; ++p)
                    index |= ((quint8(line[p * bytesPerLine + x / 8]) >> (7 - x % 8)) & 1) << p;
                if (planes == 1)
                    out[x] = index ? qRgb(255, 255, 255) : qRgb(0, 0, 0);
                else
                    out[x] = qRgb(quint8(egaPalette[3 * index]), quint8(egaPalette[3 * index + 1]), quint8(egaPalette[3 * index + 2]));
            } else {
                out[x] = qRgb(0, 0, 0);
            }
        }
    }
    return img;
}

QByteArray writePcx(const QImage& source)
{
    const QImage img = source.convertToFormat(QImage::Format_RGB32);
    const int width = img.width(), height = img.height();
    const int bytesPerLine = (width + 1) & ~1;
    QByteArray out(128, 0);
    out[0] = char(0x0A);
    out[1] = 5;
    out[2] = 1;
    out[3] = 8;
    auto set16 = [&out](int at, int v) {
        out[at] = char(v & 0xff);
        out[at + 1] = char((v >> 8) & 0xff);
    };
    set16(8, width - 1);
    set16(10, height - 1);
    set16(12, std::max(1, int(std::lround(img.dotsPerMeterX() * 0.0254))));
    set16(14, std::max(1, int(std::lround(img.dotsPerMeterY() * 0.0254))));
    out[65] = 3;
    set16(66, bytesPerLine);
    set16(68, 1);
    for (int y = 0; y < height; ++y) {
        const auto* line = reinterpret_cast<const QRgb*>(img.constScanLine(y));
        for (int plane = 0; plane < 3; ++plane) {
            QByteArray raw(bytesPerLine, 0);
            for (int x = 0; x < width; ++x)
                raw[x] = char(plane == 0 ? qRed(line[x]) : plane == 1 ? qGreen(line[x]) : qBlue(line[x]));
            // Lauflängen: höchstens 63 gleiche Bytes; Werte ab 0xC0 immer als Lauf
            for (int i = 0; i < bytesPerLine;) {
                int run = 1;
                while (i + run < bytesPerLine && run < 63 && raw[i + run] == raw[i])
                    ++run;
                if (run > 1 || (quint8(raw[i]) & 0xC0) == 0xC0)
                    out.append(char(0xC0 | run));
                out.append(raw[i]);
                i += run;
            }
        }
    }
    return out;
}

// --- GIF ---------------------------------------------------------------------------------------

namespace {

// Median-Cut: Farben in Boxen teilen, bis es count sind
QList<QRgb> medianCut(const std::vector<QRgb>& colors, int count)
{
    struct Box {
        std::vector<QRgb> c;
    };
    std::vector<Box> boxes{{colors}};
    auto range = [](const Box& b, int ch) {
        int lo = 255, hi = 0;
        for (QRgb p : b.c) {
            const int v = ch == 0 ? qRed(p) : ch == 1 ? qGreen(p) : qBlue(p);
            lo = std::min(lo, v);
            hi = std::max(hi, v);
        }
        return hi - lo;
    };
    while (int(boxes.size()) < count) {
        int best = -1, bestRange = 0, bestCh = 0;
        for (int i = 0; i < int(boxes.size()); ++i) {
            if (boxes[std::size_t(i)].c.size() < 2)
                continue;
            for (int ch = 0; ch < 3; ++ch)
                if (const int r = range(boxes[std::size_t(i)], ch); r > bestRange) {
                    bestRange = r;
                    best = i;
                    bestCh = ch;
                }
        }
        if (best < 0)
            break;
        std::vector<QRgb>& c = boxes[std::size_t(best)].c;
        std::sort(c.begin(), c.end(), [bestCh](QRgb a, QRgb b) {
            return (bestCh == 0 ? qRed(a) : bestCh == 1 ? qGreen(a) : qBlue(a)) < (bestCh == 0 ? qRed(b) : bestCh == 1 ? qGreen(b) : qBlue(b));
        });
        Box upper{std::vector<QRgb>(c.begin() + c.size() / 2, c.end())};
        c.resize(c.size() / 2);
        boxes.push_back(std::move(upper));
    }
    QList<QRgb> palette;
    for (const Box& b : boxes) {
        if (b.c.empty())
            continue;
        qint64 r = 0, g = 0, bl = 0;
        for (QRgb p : b.c) {
            r += qRed(p);
            g += qGreen(p);
            bl += qBlue(p);
        }
        const qint64 n = qint64(b.c.size());
        palette << qRgb(int(r / n), int(g / n), int(bl / n));
    }
    return palette;
}

// LZW-Datenstrom für GIF (Codes mit wachsender Breite, Unterblöcke zu 255 Bytes)
QByteArray lzw(const std::vector<quint8>& indices, int minCodeSize)
{
    QByteArray bytes;
    quint32 bitBuffer = 0;
    int bitCount = 0;
    int codeSize = minCodeSize + 1;
    const int clear = 1 << minCodeSize, end = clear + 1;
    int next = end + 1;
    QHash<quint32, int> dict;
    auto put = [&](int code) {
        bitBuffer |= quint32(code) << bitCount;
        bitCount += codeSize;
        while (bitCount >= 8) {
            bytes.append(char(bitBuffer & 0xff));
            bitBuffer >>= 8;
            bitCount -= 8;
        }
    };
    put(clear);
    int prefix = -1;
    for (quint8 k : indices) {
        if (prefix < 0) {
            prefix = k;
            continue;
        }
        const quint32 key = (quint32(prefix) << 8) | k;
        if (const auto it = dict.constFind(key); it != dict.constEnd()) {
            prefix = *it;
            continue;
        }
        put(prefix);
        if (next < 4096) {
            dict.insert(key, next++);
            if (next > (1 << codeSize) && codeSize < 12)
                ++codeSize;
        } else {
            put(clear);
            dict.clear();
            codeSize = minCodeSize + 1;
            next = end + 1;
        }
        prefix = k;
    }
    if (prefix >= 0)
        put(prefix);
    put(end);
    if (bitCount > 0)
        bytes.append(char(bitBuffer & 0xff));
    QByteArray out;
    for (qsizetype i = 0; i < bytes.size(); i += 255) {
        const QByteArray chunk = bytes.mid(i, 255);
        out.append(char(chunk.size()));
        out.append(chunk);
    }
    out.append(char(0));
    return out;
}

} // namespace

QByteArray writeGif(const QImage& source)
{
    const QImage img = source.convertToFormat(QImage::Format_ARGB32);
    const int w = img.width(), h = img.height();
    bool transparent = false;
    std::vector<QRgb> sample;
    const int step = std::max(1, int(std::sqrt(double(w) * h / 200000.0))); // grosse Bilder nur stichprobenartig
    for (int y = 0; y < h; y += step)
        for (int x = 0; x < w; x += step) {
            const QRgb p = img.pixel(x, y);
            if (qAlpha(p) < 128)
                transparent = true;
            else
                sample.push_back(p | 0xff000000);
        }
    // Erst prüfen, ob es ohnehin wenige Farben sind
    QHash<QRgb, int> distinct;
    for (QRgb p : sample) {
        distinct.insert(p, 0);
        if (distinct.size() > 256)
            break;
    }
    const int maxColors = transparent ? 255 : 256;
    QList<QRgb> palette;
    if (distinct.size() <= maxColors)
        palette = distinct.keys();
    else
        palette = medianCut(sample, maxColors);
    if (palette.isEmpty())
        palette << qRgb(0, 0, 0);
    const int transparentIndex = transparent ? int(palette.size()) : -1;
    if (transparent)
        palette << qRgb(0, 0, 0);
    while (palette.size() < 256)
        palette << qRgb(0, 0, 0);
    QHash<QRgb, quint8> nearest;
    std::vector<quint8> indices(std::size_t(w) * h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const QRgb p = img.pixel(x, y);
            if (transparent && qAlpha(p) < 128) {
                indices[std::size_t(y) * w + x] = quint8(transparentIndex);
                continue;
            }
            const QRgb key = p | 0xff000000;
            auto it = nearest.find(key);
            if (it == nearest.end()) {
                int best = 0;
                int bestD = INT_MAX;
                const int usable = transparent ? transparentIndex : int(palette.size());
                for (int i = 0; i < usable; ++i) {
                    const int dr = qRed(key) - qRed(palette[i]), dg = qGreen(key) - qGreen(palette[i]), db = qBlue(key) - qBlue(palette[i]);
                    const int d = dr * dr * 3 + dg * dg * 4 + db * db * 2;
                    if (d < bestD) {
                        bestD = d;
                        best = i;
                    }
                }
                it = nearest.insert(key, quint8(best));
            }
            indices[std::size_t(y) * w + x] = *it;
        }
    QByteArray out("GIF89a");
    putLe16(out, quint16(w));
    putLe16(out, quint16(h));
    out.append(char(0xF7)); // globale Farbtabelle, 8 Bit, 256 Einträge
    out.append(char(transparent ? transparentIndex : 0));
    out.append(char(0));
    for (QRgb c : std::as_const(palette))
        out.append(char(qRed(c))).append(char(qGreen(c))).append(char(qBlue(c)));
    if (transparent) {
        // Grafiksteuerung: Transparenzindex
        out.append(char(0x21)).append(char(0xF9)).append(char(4)).append(char(0x01));
        putLe16(out, 0);
        out.append(char(transparentIndex)).append(char(0));
    }
    out.append(char(0x2C));
    putLe16(out, 0);
    putLe16(out, 0);
    putLe16(out, quint16(w));
    putLe16(out, quint16(h));
    out.append(char(0));
    out.append(char(8)); // LZW-Mindestbreite
    out.append(lzw(indices, 8));
    out.append(char(0x3B));
    return out;
}

// --- Erkennung ---------------------------------------------------------------------------------

QImage decode(const QByteArray& data, QString* suffix)
{
    if (data.startsWith("8BPS")) {
        if (suffix)
            *suffix = QStringLiteral("psd");
        return readPsd(data);
    }
    if (data.size() >= 128 && quint8(data[0]) == 0x0A && quint8(data[1]) <= 5 && quint8(data[2]) <= 1
        && (quint8(data[3]) == 1 || quint8(data[3]) == 8)) {
        if (suffix)
            *suffix = QStringLiteral("pcx");
        return readPcx(data);
    }
    return {};
}

QStringList readSuffixes()
{
    return {QStringLiteral("psd"), QStringLiteral("pcx")};
}

QStringList writeSuffixes()
{
    return {QStringLiteral("psd"), QStringLiteral("pcx"), QStringLiteral("gif")};
}

} // namespace vek::codecs
