#include "RawImage.h"

#include "libraw/libraw.h"

#include <cstring>
#include <memory>

namespace vek::raw {

namespace {

// Endung nach Hersteller, damit die Datei im Dokument wieder so heisst wie von der Kamera
QString suffixFor(const libraw_iparams_t& id)
{
    if (id.dng_version)
        return QStringLiteral("dng");
    const QString make = QString::fromLatin1(id.make).toLower();
    struct Pair {
        const char* make;
        const char* suffix;
    };
    for (const Pair& p : {Pair{"nikon", "nef"}, Pair{"canon", "cr2"}, Pair{"sony", "arw"}, Pair{"fujifilm", "raf"},
                          Pair{"olympus", "orf"}, Pair{"om digital", "orf"}, Pair{"panasonic", "rw2"},
                          Pair{"pentax", "pef"}, Pair{"ricoh", "pef"}, Pair{"samsung", "srw"}, Pair{"leica", "rwl"},
                          Pair{"hasselblad", "3fr"}, Pair{"phase one", "iiq"}, Pair{"sigma", "x3f"}}) {
        if (make.startsWith(QLatin1String(p.make)))
            return QLatin1String(p.suffix);
    }
    return QStringLiteral("raw");
}

} // namespace

QStringList suffixes()
{
    return {QStringLiteral("nef"), QStringLiteral("nrw"), QStringLiteral("cr2"), QStringLiteral("cr3"),
            QStringLiteral("crw"), QStringLiteral("arw"), QStringLiteral("srf"), QStringLiteral("sr2"),
            QStringLiteral("dng"), QStringLiteral("raf"), QStringLiteral("orf"), QStringLiteral("rw2"),
            QStringLiteral("pef"), QStringLiteral("srw"), QStringLiteral("rwl"), QStringLiteral("3fr"),
            QStringLiteral("iiq"), QStringLiteral("x3f"), QStringLiteral("erf"), QStringLiteral("kdc"),
            QStringLiteral("mrw"), QStringLiteral("raw")};
}

QImage decode(const QByteArray& data, QString* suffix)
{
    if (data.isEmpty())
        return {};
    // LibRaw ist gross (mehrere hundert KB), also nicht auf den Stapel
    auto processor = std::make_unique<LibRaw>();
    if (processor->open_buffer(data.constData(), std::size_t(data.size())) != LIBRAW_SUCCESS)
        return {};
    libraw_output_params_t& p = processor->imgdata.params;
    p.output_bps = 16;    // 16 Bit je Kanal
    p.use_camera_wb = 1;  // Weissabgleich wie in der Kamera eingestellt
    p.output_color = 1;   // sRGB
    p.gamm[0] = 1.0 / 2.4; // sRGB-Gamma statt BT.709
    p.gamm[1] = 12.92;
    p.user_qual = 3;      // AHD-Interpolation
    if (processor->unpack() != LIBRAW_SUCCESS || processor->dcraw_process() != LIBRAW_SUCCESS)
        return {};
    int error = 0;
    libraw_processed_image_t* developed = processor->dcraw_make_mem_image(&error);
    if (!developed)
        return {};
    QImage image;
    if (developed->type == LIBRAW_IMAGE_BITMAP && developed->colors == 3 && developed->bits == 16) {
        image = QImage(developed->width, developed->height, QImage::Format_RGBX64);
        const auto* src = reinterpret_cast<const quint16*>(developed->data);
        for (int y = 0; y < image.height(); ++y) {
            auto* line = reinterpret_cast<QRgba64*>(image.scanLine(y));
            const quint16* row = src + std::size_t(y) * developed->width * 3;
            for (int x = 0; x < image.width(); ++x)
                line[x] = QRgba64::fromRgba64(row[3 * x], row[3 * x + 1], row[3 * x + 2], 65535);
        }
        // Fotos mit 300 dpi platzieren (Rohdateien haben keine Druckauflösung)
        image.setDotsPerMeterX(int(300 / 0.0254));
        image.setDotsPerMeterY(int(300 / 0.0254));
    }
    LibRaw::dcraw_clear_mem(developed);
    if (suffix && !image.isNull())
        *suffix = suffixFor(processor->imgdata.idata);
    return image;
}

} // namespace vek::raw
