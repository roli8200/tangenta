#include "ImagePool.h"

#include "ImageCodecs.h"
#include "RawImage.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QHash>
#include <QImageReader>

#include <algorithm>

namespace vek::imagepool {

namespace {

struct Entry {
    QByteArray encoded;
    QString suffix;
    QImage image; // RGBA64
    int bits = 8;
    QImage display; // erst bei Bedarf
};

// Bits je Kanal eines dekodierten Bildes
int bitsOf(const QImage& image)
{
    switch (image.format()) {
    case QImage::Format_RGBX64:
    case QImage::Format_RGBA64:
    case QImage::Format_RGBA64_Premultiplied:
    case QImage::Format_Grayscale16:
        return 16;
    case QImage::Format_RGBX16FPx4:
    case QImage::Format_RGBA16FPx4:
    case QImage::Format_RGBA16FPx4_Premultiplied:
        return 16;
    case QImage::Format_RGBX32FPx4:
    case QImage::Format_RGBA32FPx4:
    case QImage::Format_RGBA32FPx4_Premultiplied:
        return 32;
    case QImage::Format_BGR30:
    case QImage::Format_A2BGR30_Premultiplied:
    case QImage::Format_RGB30:
    case QImage::Format_A2RGB30_Premultiplied:
        return 10;
    default:
        return 8;
    }
}

QHash<QString, Entry>& pool()
{
    static QHash<QString, Entry> entries;
    return entries;
}

} // namespace

QString add(const QByteArray& encoded)
{
    const QString key = QString::fromLatin1(QCryptographicHash::hash(encoded, QCryptographicHash::Sha1).toHex());
    if (pool().contains(key))
        return key;
    QBuffer buffer;
    buffer.setData(encoded);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer);
    reader.setAutoTransform(true); // Ausrichtung aus EXIF wie in jedem Bildbetrachter
    QString format = QString::fromLatin1(reader.format()).toLower();
    // Photoshop und PCX lesen wir selbst (Qt kann sie unter Windows gar nicht)
    QString ownSuffix;
    QImage image = codecs::decode(encoded, &ownSuffix);
    if (!image.isNull())
        format = ownSuffix;
    // Rohdateien sehen für Qt oft wie TIFF aus (dann käme nur das kleine Vorschaubild) oder
    // werden von einem Zusatzmodul anders entwickelt: zuerst LibRaw fragen
    const bool ordinary = format == QLatin1String("jpeg") || format == QLatin1String("png") || format == QLatin1String("gif")
                          || format == QLatin1String("bmp") || format == QLatin1String("webp") || format == QLatin1String("svg");
    if (!ordinary && image.isNull()) {
        QString rawSuffix;
        image = raw::decode(encoded, &rawSuffix);
        if (!image.isNull())
            format = rawSuffix;
    }
    if (image.isNull())
        image = reader.read();
    if (image.isNull())
        return {};
    const int bits = bitsOf(image);
    pool().insert(key, Entry{encoded, format == QLatin1String("jpeg") ? QStringLiteral("jpg") : format,
                             image.convertToFormat(QImage::Format_RGBA64), bits, QImage()});
    return key;
}

QString addImage(const QImage& image)
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return add(bytes);
}

QStringList importSuffixes()
{
    QStringList list;
    for (const QByteArray& f : QImageReader::supportedImageFormats())
        list << QString::fromLatin1(f).toLower();
    for (const QString& s : raw::suffixes() + codecs::readSuffixes()) {
        if (!list.contains(s))
            list << s;
    }
    return list;
}

bool contains(const QString& key)
{
    return pool().contains(key);
}

QImage image(const QString& key)
{
    const auto it = pool().constFind(key);
    return it == pool().constEnd() ? QImage() : it->image;
}

QImage ditherTo8Bit(const QImage& image)
{
    // Bayer-Matrix 8 x 8: Schwellen 0..63, gleichmässig über die Fläche verteilt
    static constexpr int bayer[8][8] = {{0, 32, 8, 40, 2, 34, 10, 42},  {48, 16, 56, 24, 50, 18, 58, 26},
                                        {12, 44, 4, 36, 14, 46, 6, 38}, {60, 28, 52, 20, 62, 30, 54, 22},
                                        {3, 35, 11, 43, 1, 33, 9, 41},  {51, 19, 59, 27, 49, 17, 57, 25},
                                        {15, 47, 7, 39, 13, 45, 5, 37}, {63, 31, 55, 23, 61, 29, 53, 21}};
    const QImage src = image.convertToFormat(QImage::Format_RGBA64_Premultiplied);
    QImage out(src.size(), QImage::Format_ARGB32_Premultiplied);
    out.setDotsPerMeterX(src.dotsPerMeterX());
    out.setDotsPerMeterY(src.dotsPerMeterY());
    for (int y = 0; y < src.height(); ++y) {
        const auto* in = reinterpret_cast<const QRgba64*>(src.constScanLine(y));
        auto* line = reinterpret_cast<QRgb*>(out.scanLine(y));
        for (int x = 0; x < src.width(); ++x) {
            // Wert / 257 liegt zwischen zwei 8-Bit-Stufen; die Schwelle entscheidet, welche.
            // Genau auf einer Stufe (8-Bit-Quelle) bleibt er unverändert.
            const double t = (bayer[y & 7][x & 7] + 0.5) / 64.0;
            auto q = [t](quint16 v) { return std::min(255, int(v / 257.0 + t)); };
            const int a = q(in[x].alpha());
            line[x] = qRgba(std::min(a, q(in[x].red())), std::min(a, q(in[x].green())), std::min(a, q(in[x].blue())), a);
        }
    }
    return out;
}

QImage displayImage(const QString& key)
{
    const auto it = pool().find(key);
    if (it == pool().end())
        return QImage();
    if (it->display.isNull())
        it->display = it->bits > 8 ? ditherTo8Bit(it->image) : it->image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    return it->display;
}

int sourceBits(const QString& key)
{
    const auto it = pool().constFind(key);
    return it == pool().constEnd() ? 0 : it->bits;
}

QByteArray encoded(const QString& key)
{
    const auto it = pool().constFind(key);
    return it == pool().constEnd() ? QByteArray() : it->encoded;
}

QString suffix(const QString& key)
{
    const auto it = pool().constFind(key);
    return it == pool().constEnd() ? QString() : it->suffix;
}

} // namespace vek::imagepool
