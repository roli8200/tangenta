#include "Output.h"

#include "Canvas.h"
#include "DrawObject.h"
#include "CurveObject.h"
#include "Document.h"
#include "DxfImport.h"
#include "Editor.h"
#include "Page.h"
#include "PathOps.h"
#include "ImageCodecs.h"
#include "VectorRecorder.h"

#include <QGradient>
#include "Labels.h"
#include "Options.h"

#include <QBuffer>
#include <QDateTime>
#include <QDir>
#include <QRegularExpression>

#include <array>
#include <QFile>
#include <QFileInfo>
#include <QImageWriter>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QPrinter>
#include <QSvgGenerator>

namespace vek::output {

namespace {

const Document& doc(const Canvas& canvas)
{
    return canvas.editor().document();
}

QSizeF pageSize(const Canvas& canvas, int index)
{
    return doc(canvas).page(std::clamp(index, 0, doc(canvas).pageCount() - 1)).size();
}

QPageLayout layoutFor(const QSizeF& mm)
{
    // Papier immer hochkant angeben, die Ausrichtung dreht es (sonst tauscht Qt Querformat zurück)
    const QSizeF portrait(std::min(mm.width(), mm.height()), std::max(mm.width(), mm.height()));
    const QPageSize size(portrait, QPageSize::Millimeter, QString(), QPageSize::ExactMatch);
    return QPageLayout(size, mm.width() > mm.height() ? QPageLayout::Landscape : QPageLayout::Portrait, QMarginsF(),
                       QPageLayout::Millimeter);
}

void setError(QString* error, const QString& text)
{
    if (error)
        *error = text;
}

} // namespace

QSizeF layoutSplit(int layoutType)
{
    // Spalten x Zeilen: Ganze Seite, Buch, Heft, Klappkarte, Faltkarte
    // horizontal/vertikal, Parallelfalz
    static const QList<QSizeF> kSplit = {QSizeF(1, 1), QSizeF(2, 1), QSizeF(2, 1), QSizeF(1, 2),
                                         QSizeF(2, 2), QSizeF(2, 2), QSizeF(3, 1)};
    return kSplit.value(layoutType, QSizeF(1, 1));
}

Imposition impose(const Canvas& canvas, const QList<int>& pages)
{
    const Options& o = Options::instance();
    Imposition result;
    if (pages.isEmpty())
        return result;
    if (o.integer(QStringLiteral("doc/label/labels")) == 1 && o.integer(QStringLiteral("doc/label/cols")) >= 1) {
        // Etiketten: jede Seite auf das nächste Etikett des Bogens
        LabelStock stock;
        const QString name = o.value(QStringLiteral("doc/label/name")).toString();
        for (const LabelStock& s : labelStocks())
            if (s.maker + QLatin1Char(' ') + s.name == name)
                stock.sheet = s.sheet;
        stock.label = QSizeF(o.number(QStringLiteral("doc/label/w")), o.number(QStringLiteral("doc/label/h")));
        stock.columns = std::max(1, o.integer(QStringLiteral("doc/label/cols")));
        stock.rows = std::max(1, o.integer(QStringLiteral("doc/label/rows")));
        stock.left = o.number(QStringLiteral("doc/label/left"));
        stock.top = o.number(QStringLiteral("doc/label/top"));
        stock.hGap = o.number(QStringLiteral("doc/label/hgap"));
        stock.vGap = o.number(QStringLiteral("doc/label/vgap"));
        result.sheetSize = stock.sheet;
        for (int i = 0; i < pages.size(); ++i) {
            if (i % stock.perSheet() == 0)
                result.sheets.append(Sheet());
            result.sheets.last().pages.append({pages[i], stock.labelOrigin(i)});
        }
        return result;
    }
    const QSizeF split = layoutSplit(o.integer(QStringLiteral("doc/layout/type")));
    const int cols = int(split.width()), rows = int(split.height()), perSheet = cols * rows;
    const QSizeF page = pageSize(canvas, pages.first());
    result.sheetSize = QSizeF(page.width() * cols, page.height() * rows);
    for (int i = 0; i < pages.size(); ++i) {
        if (i % perSheet == 0)
            result.sheets.append(Sheet());
        const int slot = i % perSheet;
        result.sheets.last().pages.append({pages[i], QPointF((slot % cols) * page.width(), (slot / cols) * page.height())});
    }
    if (perSheet == 1) {
        // Ganze Seite: jeder Bogen so gross wie seine Seite
        result.sheetSize = page;
    }
    return result;
}

void paintSheet(const Canvas& canvas, QPainter& p, const Imposition& imposition, int sheet, double pxPerMm)
{
    for (const auto& [index, origin] : imposition.sheets.value(sheet).pages) {
        p.save();
        const QSizeF size = pageSize(canvas, index);
        p.setClipRect(QRectF(origin * pxPerMm, size * pxPerMm));
        canvas.renderPage(p, index, pxPerMm, origin * pxPerMm, true);
        p.restore();
    }
}

bool print(const Canvas& canvas, QPrinter& printer, const QList<int>& pages, QString* error)
{
    const Imposition imp = impose(canvas, pages);
    if (imp.sheets.isEmpty()) {
        setError(error, QObject::tr("Keine Seiten zum Drucken."));
        return false;
    }
    printer.setFullPage(true);
    // "Drucker kann Dokumentseitengrössen anpassen": Papier = Bogen, sonst auf das Papier einpassen
    const bool fit = !Options::instance().flag(QStringLiteral("global/print/pageSize"));
    if (!fit)
        printer.setPageLayout(layoutFor(imp.sheetSize));
    QPainter p;
    if (!p.begin(&printer)) {
        setError(error, QObject::tr("Der Drucker lässt sich nicht öffnen."));
        return false;
    }
    for (int s = 0; s < imp.sheets.size(); ++s) {
        if (s > 0 && !printer.newPage()) {
            setError(error, QObject::tr("Neue Seite fehlgeschlagen."));
            return false;
        }
        double pxPerMm = printer.resolution() / 25.4;
        p.save();
        if (fit) {
            // Bogen mittig und wenn nötig verkleinert aufs Papier
            const QRectF paper = printer.pageLayout().fullRect(QPageLayout::Millimeter);
            const double scale = std::min({1.0, paper.width() / imp.sheetSize.width(), paper.height() / imp.sheetSize.height()});
            p.translate((paper.width() - imp.sheetSize.width() * scale) / 2.0 * pxPerMm,
                        (paper.height() - imp.sheetSize.height() * scale) / 2.0 * pxPerMm);
            pxPerMm *= scale;
        }
        paintSheet(canvas, p, imp, s, pxPerMm);
        p.restore();
    }
    p.end();
    return true;
}


void paintImposedSide(const Canvas& canvas, QPainter& p, const impose::Result& result, const impose::Side& side, const QList<int>& pages,
                      double pxPerMm, const QString& info)
{
    for (const impose::Placement& pl : side.pages) {
        if (pl.page < 0 || pl.page >= pages.size())
            continue; // Leerseite
        const int index = pages[pl.page];
        const QSizeF size = pageSize(canvas, index);
        p.save();
        p.setClipRect(QRectF(pl.trim.marginsAdded(pl.bleed).topLeft() * pxPerMm, pl.trim.marginsAdded(pl.bleed).size() * pxPerMm));
        // Seitenmitte auf die Mitte des Endformats, Bundzuwachs waagrecht auf dem Bogen
        p.translate((pl.trim.center() + QPointF(pl.shift, 0)) * pxPerMm);
        p.rotate(pl.rotation);
        p.translate(-size.width() / 2 * pxPerMm, -size.height() / 2 * pxPerMm);
        canvas.renderPage(p, index, pxPerMm, QPointF(0, 0), true);
        p.restore();
    }
    // Marken in Passerschwarz, 0,25 pt
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen(Qt::black, 0.25 * 25.4 / 72.0 * pxPerMm);
    pen.setCapStyle(Qt::FlatCap);
    p.setPen(pen);
    for (const QLineF& l : side.cropMarks)
        p.drawLine(QLineF(l.p1() * pxPerMm, l.p2() * pxPerMm));
    pen.setDashPattern({4, 3});
    p.setPen(pen);
    for (const QLineF& l : side.foldMarks)
        p.drawLine(QLineF(l.p1() * pxPerMm, l.p2() * pxPerMm));
    if (!info.isEmpty() && !side.cropMarks.isEmpty()) {
        QFont f(QStringLiteral("Arial"));
        f.setPixelSize(std::max(1, int(std::lround(2.2 * pxPerMm)))); // ~6 pt
        p.setFont(f);
        p.setPen(Qt::black);
        p.drawText(QPointF(4.0 * pxPerMm, (result.sheet.height() - 2.5) * pxPerMm), info);
    }
    p.restore();
}

void setupImposedPrinter(QPrinter& printer, const impose::Settings& settings, const impose::Result& result)
{
    printer.setFullPage(true);
    printer.setPageLayout(layoutFor(result.sheet));
    if (result.duplex)
        printer.setDuplex(settings.flip == impose::Flip::ShortEdge ? QPrinter::DuplexShortSide : QPrinter::DuplexLongSide);
    else
        printer.setDuplex(QPrinter::DuplexNone);
}

namespace {

QString sideInfo(const QString& title, int sheet, int sheets, bool back)
{
    return QObject::tr("%1 – Bogen %2/%3 %4 – %5")
        .arg(title)
        .arg(sheet + 1)
        .arg(sheets)
        .arg(back ? QObject::tr("Rückseite") : QObject::tr("Vorderseite"))
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("dd.MM.yyyy HH:mm")));
}

// Gemeinsam für Drucker und PDF: Bogenseiten der Reihe nach
bool paintImposed(const Canvas& canvas, QPagedPaintDevice& device, const impose::Result& result, const QList<int>& pages, const QString& title,
                  double resolution, QString* error)
{
    QPainter p;
    if (!p.begin(&device)) {
        setError(error, QObject::tr("Die Ausgabe lässt sich nicht öffnen."));
        return false;
    }
    const double pxPerMm = resolution / 25.4;
    bool first = true;
    for (int s = 0; s < result.sheets.size(); ++s) {
        const impose::Sheet& sheet = result.sheets[s];
        for (bool back : {false, true}) {
            if (back && !sheet.hasBack)
                continue;
            if (!first && !device.newPage()) {
                setError(error, QObject::tr("Neue Seite fehlgeschlagen."));
                return false;
            }
            first = false;
            paintImposedSide(canvas, p, result, back ? sheet.back : sheet.front, pages, pxPerMm,
                             title.isEmpty() ? QString() : sideInfo(title, s, int(result.sheets.size()), back));
        }
    }
    p.end();
    return true;
}

} // namespace

bool printImposed(const Canvas& canvas, QPrinter& printer, const impose::Result& result, const QList<int>& pages, const QString& title,
                  QString* error)
{
    if (!result.ok()) {
        setError(error, result.error);
        return false;
    }
    return paintImposed(canvas, printer, result, pages, title, printer.resolution(), error);
}

bool exportImposedPdf(const Canvas& canvas, const QString& path, const impose::Result& result, const QList<int>& pages, const QString& title,
                      QString* error)
{
    if (!result.ok()) {
        setError(error, result.error);
        return false;
    }
    QPdfWriter writer(path);
    writer.setResolution(600);
    writer.setCreator(QStringLiteral("TANGENTA"));
    writer.setTitle(title);
    writer.setPageLayout(layoutFor(result.sheet));
    return paintImposed(canvas, writer, result, pages, title, writer.resolution(), error);
}

bool exportPdf(const Canvas& canvas, const QString& path, const QList<int>& pages, QString* error)
{
    if (pages.isEmpty()) {
        setError(error, QObject::tr("Keine Seiten zum Exportieren."));
        return false;
    }
    QPdfWriter writer(path);
    writer.setResolution(600);
    writer.setCreator(QStringLiteral("TANGENTA"));
    writer.setTitle(QFileInfo(path).completeBaseName());
    writer.setPageLayout(layoutFor(pageSize(canvas, pages.first())));
    QPainter p;
    if (!p.begin(&writer)) {
        setError(error, QObject::tr("%1 lässt sich nicht schreiben.").arg(path));
        return false;
    }
    const double pxPerMm = writer.resolution() / 25.4;
    for (int i = 0; i < pages.size(); ++i) {
        if (i > 0) {
            writer.setPageLayout(layoutFor(pageSize(canvas, pages[i])));
            writer.newPage();
        }
        canvas.renderPage(p, pages[i], pxPerMm, QPointF(0, 0), true);
    }
    p.end();
    return true;
}

bool exportBitmap(const Canvas& canvas, const QString& path, int page, double dpi, bool transparent, QString* error)
{
    const QSizeF mm = pageSize(canvas, page);
    const double pxPerMm = dpi / 25.4;
    const QSize size(std::max(1, int(std::lround(mm.width() * pxPerMm))), std::max(1, int(std::lround(mm.height() * pxPerMm))));
    if (qint64(size.width()) * size.height() > 400LL * 1000 * 1000) {
        setError(error, QObject::tr("Das Bild wäre mit %1 x %2 Pixeln zu gross.").arg(size.width()).arg(size.height()));
        return false;
    }
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    const QByteArray format = QFileInfo(path).suffix().toLower().toLatin1();
    const bool alpha = transparent && (format == "png" || format == "tif" || format == "tiff" || format == "webp");
    image.fill(alpha ? Qt::transparent : Qt::white);
    image.setDotsPerMeterX(int(std::lround(pxPerMm * 1000.0)));
    image.setDotsPerMeterY(int(std::lround(pxPerMm * 1000.0)));
    {
        QPainter p(&image);
        canvas.renderPage(p, page, pxPerMm, QPointF(0, 0), !alpha);
    }
    QImageWriter writer(path, format);
    if (format == "jpg" || format == "jpeg")
        writer.setQuality(92);
    if (!writer.write(alpha ? image : image.convertToFormat(QImage::Format_RGB32))) {
        setError(error, writer.errorString());
        return false;
    }
    return true;
}

QRectF exportArea(const Canvas& canvas, const ExportOptions& options)
{
    if (options.only.isEmpty())
        return QRectF(QPointF(0, 0), pageSize(canvas, options.page));
    QRectF area;
    for (NodeId id : options.only) {
        const auto* o = doc(canvas).findAs<DrawObject>(id);
        if (!o)
            continue;
        // Kontur, Unschärfe und Schatten ragen über das Begrenzungsrechteck hinaus
        double grow = o->style().stroke ? o->style().strokeWidth / 2.0 : 0.0;
        grow += 3.0 * o->blur();
        QRectF r = o->boundingRect().adjusted(-grow, -grow, grow, grow);
        if (o->shadow().active)
            r = r.united(Canvas::shadowTransform(o->boundingRect(), o->shadow()).mapRect(o->boundingRect())
                             .adjusted(-2 * o->shadow().feather, -2 * o->shadow().feather, 2 * o->shadow().feather, 2 * o->shadow().feather));
        area = area.isNull() ? r : area.united(r);
    }
    return area.isNull() ? QRectF(QPointF(0, 0), pageSize(canvas, options.page)) : area;
}

namespace {

// Seite bzw. Auswahl in einen Maler zeichnen; Bereich area (mm) landet bei (0,0)
void renderArea(const Canvas& canvas, QPainter& p, const ExportOptions& options, const QRectF& area, double pxPerMm, bool background)
{
    auto& c = const_cast<Canvas&>(canvas);
    c.setRenderFilter(options.only);
    canvas.renderPage(p, options.page, pxPerMm, -area.topLeft() * pxPerMm, background && options.only.isEmpty());
    c.setRenderFilter({});
}

} // namespace

QImage renderImage(const Canvas& canvas, const ExportOptions& options, QString* error)
{
    const QRectF area = exportArea(canvas, options);
    const double pxPerMm = options.dpi / 25.4;
    const QSize size(std::max(1, int(std::lround(area.width() * pxPerMm))), std::max(1, int(std::lround(area.height() * pxPerMm))));
    if (qint64(size.width()) * size.height() > 400LL * 1000 * 1000) {
        setError(error, QObject::tr("Das Bild wäre mit %1 x %2 Pixeln zu gross.").arg(size.width()).arg(size.height()));
        return {};
    }
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(options.transparent ? Qt::transparent : Qt::white);
    image.setDotsPerMeterX(int(std::lround(pxPerMm * 1000.0)));
    image.setDotsPerMeterY(int(std::lround(pxPerMm * 1000.0)));
    {
        QPainter p(&image);
        renderArea(canvas, p, options, area, pxPerMm, !options.transparent);
    }
    switch (options.colorMode) {
    case ExportOptions::ColorMode::Grayscale:
        if (!options.transparent)
            image = image.convertToFormat(QImage::Format_Grayscale8);
        break;
    case ExportOptions::ColorMode::BlackWhite:
        image = image.convertToFormat(QImage::Format_Mono, Qt::ThresholdDither | Qt::AvoidDither);
        break;
    default: break;
    }
    return image;
}

QStringList imageExportSuffixes()
{
    // Nur Formate, die Qt wirklich schreiben kann: Ein Plugin kann vorhanden sein, aber nicht laden
    // (z.B. TIFF aus dem offiziellen Qt, das libtiff.so.5 verlangt). PSD und PCX schreibt TANGENTA selbst.
    static const QStringList suffixes = [] {
        QStringList result;
        for (const char* format : {"png", "jpg", "tif", "webp", "bmp", "gif", "ico"}) {
            QBuffer buffer;
            buffer.open(QIODevice::WriteOnly);
            QImageWriter writer(&buffer, format);
            if (writer.canWrite())
                result << QString::fromLatin1(format);
        }
        return result << QStringLiteral("psd") << QStringLiteral("pcx");
    }();
    return suffixes;
}

bool exportImage(const Canvas& canvas, const QString& path, const ExportOptions& options, QString* error)
{
    QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == QLatin1String("jpeg"))
        suffix = QStringLiteral("jpg");
    if (suffix == QLatin1String("tiff"))
        suffix = QStringLiteral("tif");
    // Formate ohne Alphakanal bekommen einen weissen Hintergrund
    ExportOptions o = options;
    const bool alphaCapable = suffix == QLatin1String("png") || suffix == QLatin1String("tif") || suffix == QLatin1String("webp")
                              || suffix == QLatin1String("gif") || suffix == QLatin1String("ico") || suffix == QLatin1String("psd");
    o.transparent = options.transparent && alphaCapable;
    QImage image = renderImage(canvas, o, error);
    if (image.isNull())
        return false;
    QByteArray own;
    if (suffix == QLatin1String("psd"))
        own = codecs::writePsd(image);
    else if (suffix == QLatin1String("pcx"))
        own = codecs::writePcx(image);
    else if (suffix == QLatin1String("gif"))
        own = codecs::writeGif(image);
    if (!own.isEmpty()) {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly) || f.write(own) != own.size()) {
            setError(error, f.errorString());
            return false;
        }
        return true;
    }
    if (suffix == QLatin1String("ico")) {
        // Symbol: höchstens 256 x 256, quadratisch mit transparentem Rand
        const int side = std::min(256, std::max(image.width(), image.height()));
        QImage icon(side, side, QImage::Format_ARGB32);
        icon.fill(Qt::transparent);
        QPainter p(&icon);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        const QImage scaled = image.scaled(side, side, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        p.drawImage((side - scaled.width()) / 2, (side - scaled.height()) / 2, scaled);
        p.end();
        image = icon;
    }
    const QByteArray format = suffix == QLatin1String("jpg") ? QByteArray("jpeg") : suffix == QLatin1String("tif") ? QByteArray("tiff") : suffix.toLatin1();
    QImageWriter writer(path, format);
    if (format == "jpeg" || format == "webp")
        writer.setQuality(options.quality);
    if (format == "tiff")
        writer.setCompression(1); // LZW
    if (!o.transparent && image.hasAlphaChannel())
        image = image.convertToFormat(QImage::Format_RGB32);
    if (!writer.write(image)) {
        setError(error, writer.errorString());
        return false;
    }
    return true;
}

bool exportPdf(const Canvas& canvas, const QString& path, const ExportOptions& options, QString* error)
{
    if (options.only.isEmpty())
        return exportPdf(canvas, path, options.pages.isEmpty() ? QList<int>{options.page} : options.pages, error);
    // Nur markierte Objekte: eine Seite in der Grösse ihres Rechtecks
    const QRectF area = exportArea(canvas, options);
    QPdfWriter writer(path);
    writer.setCreator(QStringLiteral("TANGENTA"));
    writer.setResolution(1200);
    writer.setPageLayout(layoutFor(area.size()));
    QPainter p;
    if (!p.begin(&writer)) {
        setError(error, QObject::tr("Die PDF-Datei lässt sich nicht schreiben."));
        return false;
    }
    renderArea(canvas, p, options, area, 1200.0 / 25.4, false);
    p.end();
    return true;
}

QByteArray areaSvg(const Canvas& canvas, const ExportOptions& options)
{
    const QRectF area = exportArea(canvas, options);
    const double pxPerMm = 96.0 / 25.4;
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly);
    QSvgGenerator gen;
    gen.setOutputDevice(&buffer);
    gen.setResolution(96);
    gen.setSize(QSize(int(std::ceil(area.width() * pxPerMm)), int(std::ceil(area.height() * pxPerMm))));
    gen.setViewBox(QRectF(0, 0, area.width() * pxPerMm, area.height() * pxPerMm));
    gen.setTitle(QStringLiteral("TANGENTA"));
    {
        QPainter p(&gen);
        renderArea(canvas, p, options, area, pxPerMm, true);
    }
    return buffer.data();
}

bool exportSvg(const Canvas& canvas, const QString& path, const ExportOptions& options, QString* error)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(areaSvg(canvas, options)) < 0) {
        setError(error, f.errorString());
        return false;
    }
    return true;
}

namespace {

// --- PostScript --------------------------------------------------------------------------------

QString num(double v)
{
    QString s = QString::number(v, 'f', 3);
    while (s.contains(QLatin1Char('.')) && (s.endsWith(QLatin1Char('0')) || s.endsWith(QLatin1Char('.'))))
        s.chop(1);
    return s == QLatin1String("-0") ? QStringLiteral("0") : s;
}

QString psPath(const QPainterPath& path, double height)
{
    QString out;
    auto pt = [height](const QPointF& p) { return num(p.x()) + QLatin1Char(' ') + num(height - p.y()); };
    for (int i = 0; i < path.elementCount(); ++i) {
        const QPainterPath::Element e = path.elementAt(i);
        if (e.isMoveTo()) {
            out += pt(e) + QStringLiteral(" m\n");
        } else if (e.isLineTo()) {
            out += pt(e) + QStringLiteral(" l\n");
        } else if (e.isCurveTo() && i + 2 < path.elementCount()) {
            out += pt(e) + QLatin1Char(' ') + pt(path.elementAt(i + 1)) + QLatin1Char(' ') + pt(path.elementAt(i + 2)) + QStringLiteral(" c\n");
            i += 2;
        }
        // geschlossen: der nächste moveto oder ein ausdrücklicher Schluss
        const bool lastOfSub = i + 1 >= path.elementCount() || path.elementAt(i + 1).isMoveTo();
        if (lastOfSub && i > 0) {
            // Teilpfad geschlossen, wenn Ende = Anfang
            int start = i;
            while (start > 0 && !path.elementAt(start).isMoveTo())
                --start;
            if (QPointF(path.elementAt(start)) == QPointF(path.elementAt(i)))
                out += QStringLiteral("h\n");
        }
    }
    return out;
}

QString rgb(const QColor& c)
{
    return num(c.redF()) + QLatin1Char(' ') + num(c.greenF()) + QLatin1Char(' ') + num(c.blueF()) + QStringLiteral(" rg\n");
}

// Verlauf als Level-3-Schattierung (Typ 2 linear, Typ 3 radial); leer, wenn es keiner ist
QString psShading(const QBrush& brush, double height)
{
    const QGradient* g = brush.gradient();
    if (!g || (g->type() != QGradient::LinearGradient && g->type() != QGradient::RadialGradient) || g->stops().size() < 2)
        return {};
    const QTransform t = brush.transform();
    auto flip = [&](const QPointF& p) {
        const QPointF d = t.map(p);
        return QPointF(d.x(), height - d.y());
    };
    QString coords;
    if (g->type() == QGradient::LinearGradient) {
        const auto* lg = static_cast<const QLinearGradient*>(g);
        const QPointF a = flip(lg->start()), b = flip(lg->finalStop());
        coords = QStringLiteral("/ShadingType 2 /Coords [%1 %2 %3 %4]").arg(num(a.x()), num(a.y()), num(b.x()), num(b.y()));
    } else {
        const auto* rg = static_cast<const QRadialGradient*>(g);
        const QPointF c = flip(rg->center()), f = flip(rg->focalPoint());
        const double r = rg->radius() * std::sqrt(std::abs(t.determinant()));
        coords = QStringLiteral("/ShadingType 3 /Coords [%1 %2 0 %3 %4 %5]").arg(num(f.x()), num(f.y()), num(c.x()), num(c.y()), num(r));
    }
    const QGradientStops stops = g->stops();
    QString functions, bounds, encode;
    for (int i = 0; i + 1 < stops.size(); ++i) {
        const QColor a = stops[i].second, b = stops[i + 1].second;
        functions += QStringLiteral("<< /FunctionType 2 /Domain [0 1] /C0 [%1 %2 %3] /C1 [%4 %5 %6] /N 1 >> ")
                         .arg(num(a.redF()), num(a.greenF()), num(a.blueF()), num(b.redF()), num(b.greenF()), num(b.blueF()));
        if (i > 0)
            bounds += num(stops[i].first) + QLatin1Char(' ');
        encode += QStringLiteral("0 1 ");
    }
    return QStringLiteral("<< %1 /ColorSpace /DeviceRGB /Extend [true true] /Function << /FunctionType 3 /Domain [%2 %3] /Functions [%4] /Bounds [%5] /Encode [%6] >> >> shfill\n")
        .arg(coords, num(stops.first().first), num(stops.last().first), functions, bounds.trimmed(), encode.trimmed());
}

// Bild mit Farbschlüssel-Maske (durchsichtige Punkte) als Hexadezimaldaten
QString psImage(const QImage& source, const QTransform& toDevice, double height)
{
    const QImage img = source.convertToFormat(QImage::Format_ARGB32);
    const int w = img.width(), h = img.height();
    if (w <= 0 || h <= 0)
        return {};
    const QTransform unit = QTransform(w, 0, 0, -h, 0, h) * toDevice * QTransform(1, 0, 0, -1, 0, height);
    QString out = QStringLiteral("gsave\n[%1 %2 %3 %4 %5 %6] concat\n/DeviceRGB setcolorspace\n")
                      .arg(num(unit.m11()), num(unit.m12()), num(unit.m21()), num(unit.m22()), num(unit.dx()), num(unit.dy()));
    out += QStringLiteral("<< /ImageType 4 /Width %1 /Height %2 /BitsPerComponent 8 /Decode [0 1 0 1 0 1] /ImageMatrix [%1 0 0 -%2 0 %2] "
                          "/MaskColor [1 1 2 2 3 3] /DataSource currentfile /ASCIIHexDecode filter >> image\n")
               .arg(w)
               .arg(h);
    QByteArray hex;
    hex.reserve(qsizetype(w) * h * 6 + h * 2);
    int col = 0;
    for (int y = 0; y < h; ++y) {
        const auto* line = reinterpret_cast<const QRgb*>(img.constScanLine(y));
        for (int x = 0; x < w; ++x) {
            const QRgb p = line[x];
            int r = 1, g = 2, b = 3; // Schlüssel = durchsichtig
            if (qAlpha(p) >= 128) {
                const double a = qAlpha(p) / 255.0; // halbdurchsichtig: auf Weiss
                r = int(std::lround(qRed(p) * a + 255 * (1 - a)));
                g = int(std::lround(qGreen(p) * a + 255 * (1 - a)));
                b = int(std::lround(qBlue(p) * a + 255 * (1 - a)));
                if (r == 1 && g == 2 && b == 3)
                    r = 0;
            }
            hex += QByteArray::number(r | 0x100, 16).mid(1) + QByteArray::number(g | 0x100, 16).mid(1) + QByteArray::number(b | 0x100, 16).mid(1);
            if (++col == 12) {
                hex += '\n';
                col = 0;
            }
        }
    }
    out += QString::fromLatin1(hex) + QStringLiteral(">\ngrestore\n");
    return out;
}

} // namespace

QByteArray areaEps(const Canvas& canvas, const ExportOptions& options)
{
    const QRectF area = exportArea(canvas, options);
    const double ptPerMm = 72.0 / 25.4;
    const QSizeF size(area.width() * ptPerMm, area.height() * ptPerMm);
    VectorRecorder recorder(size, 72.0);
    {
        QPainter p(&recorder);
        renderArea(canvas, p, options, area, ptPerMm, true);
    }
    const double H = size.height();
    // Transparenzreduzierung: fertiges Ergebnis einmal rastern, daraus die Stellen ausschneiden
    QImage flat;
    double pxPerPt = 0;
    const bool anyBlending = std::any_of(recorder.items().begin(), recorder.items().end(), [](const RecordedItem& i) { return i.needsBlending(); });
    if (anyBlending) {
        double dpi = std::clamp(options.flattenDpi, 72.0, 1200.0);
        const double maxPixels = 200.0 * 1000 * 1000;
        const double pixels = size.width() * size.height() * (dpi / 72.0) * (dpi / 72.0);
        if (pixels > maxPixels)
            dpi *= std::sqrt(maxPixels / pixels);
        pxPerPt = dpi / 72.0;
        flat = QImage(QSize(std::max(1, int(std::ceil(size.width() * pxPerPt))), std::max(1, int(std::ceil(size.height() * pxPerPt)))),
                      QImage::Format_ARGB32_Premultiplied);
        flat.fill(Qt::white);
        QPainter p(&flat);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        renderArea(canvas, p, options, area, ptPerMm * pxPerPt, true);
    }
    QString ps;
    ps += QStringLiteral("%!PS-Adobe-3.0 EPSF-3.0\n%%BoundingBox: 0 0 %1 %2\n%%HiResBoundingBox: 0 0 %3 %4\n")
              .arg(int(std::ceil(size.width())))
              .arg(int(std::ceil(size.height())))
              .arg(num(size.width()), num(size.height()));
    ps += QStringLiteral("%%Creator: TANGENTA\n%%LanguageLevel: 3\n%%Pages: 1\n%%EndComments\n");
    ps += QStringLiteral("%%BeginProlog\n/m {moveto} bind def /l {lineto} bind def /c {curveto} bind def /h {closepath} bind def\n"
                         "/rg {setrgbcolor} bind def\n%%EndProlog\n%%Page: 1 1\n");
    for (const RecordedItem& item : recorder.items()) {
        if (anyBlending && item.needsBlending()) {
            // Genau die bedeckte Fläche mit dem gerasterten Endergebnis füllen
            const QPainterPath shape = item.coverage();
            const QRectF bounds = shape.boundingRect().intersected(QRectF(QPointF(0, 0), size));
            if (bounds.isEmpty())
                continue;
            const QRect px = QRectF(bounds.topLeft() * pxPerPt, bounds.bottomRight() * pxPerPt).toAlignedRect().adjusted(-1, -1, 1, 1)
                                 .intersected(flat.rect());
            const QImage crop = flat.copy(px);
            const QTransform toDevice = QTransform::fromScale(1 / pxPerPt, 1 / pxPerPt) * QTransform::fromTranslate(px.left() / pxPerPt, px.top() / pxPerPt);
            ps += QStringLiteral("gsave newpath\n") + psPath(shape, H)
                  + (shape.fillRule() == Qt::OddEvenFill ? QStringLiteral("eoclip newpath\n") : QStringLiteral("clip newpath\n"))
                  + psImage(crop, toDevice, H) + QStringLiteral("grestore\n");
            continue;
        }
        ps += QStringLiteral("gsave\n");
        if (item.clip) {
            ps += QStringLiteral("newpath\n") + psPath(*item.clip, H)
                  + (item.clip->fillRule() == Qt::OddEvenFill ? QStringLiteral("eoclip newpath\n") : QStringLiteral("clip newpath\n"));
        }
        if (item.kind == RecordedItem::Kind::Image) {
            ps += psImage(item.image, item.imageTransform, H);
        } else {
            const QString geometry = psPath(item.path, H);
            const bool evenOdd = item.path.fillRule() == Qt::OddEvenFill;
            if (item.fill) {
                const QBrush& b = *item.fill;
                if (b.style() == Qt::SolidPattern) {
                    ps += QStringLiteral("newpath\n") + geometry + rgb(b.color()) + (evenOdd ? QStringLiteral("eofill\n") : QStringLiteral("fill\n"));
                } else if (const QString shading = psShading(b, H); !shading.isEmpty()) {
                    ps += QStringLiteral("gsave newpath\n") + geometry + (evenOdd ? QStringLiteral("eoclip\n") : QStringLiteral("clip\n")) + shading
                          + QStringLiteral("grestore\n");
                } else {
                    // Muster, Maschen, konische Verläufe: als Bild in der Form
                    const QRectF r = item.path.boundingRect();
                    const double scale = 300.0 / 72.0;
                    QImage img(QSize(std::max(1, int(std::ceil(r.width() * scale))), std::max(1, int(std::ceil(r.height() * scale)))),
                               QImage::Format_ARGB32_Premultiplied);
                    img.fill(Qt::transparent);
                    {
                        QPainter ip(&img);
                        ip.setRenderHint(QPainter::Antialiasing);
                        ip.scale(scale, scale);
                        ip.translate(-r.topLeft());
                        ip.fillPath(item.path, b);
                    }
                    ps += QStringLiteral("gsave newpath\n") + geometry + (evenOdd ? QStringLiteral("eoclip\n") : QStringLiteral("clip\n"))
                          + psImage(img, QTransform::fromScale(1 / scale, 1 / scale) * QTransform::fromTranslate(r.left(), r.top()), H)
                          + QStringLiteral("grestore\n");
                }
            }
            if (item.stroke) {
                const QPen& pen = *item.stroke;
                const QColor c = pen.brush().gradient() && !pen.brush().gradient()->stops().isEmpty() ? pen.brush().gradient()->stops().first().second
                                                                                                     : pen.color();
                ps += QStringLiteral("newpath\n") + geometry + rgb(c);
                ps += num(pen.widthF()) + QStringLiteral(" setlinewidth ");
                ps += QString::number(pen.capStyle() == Qt::RoundCap ? 1 : pen.capStyle() == Qt::SquareCap ? 2 : 0) + QStringLiteral(" setlinecap ");
                ps += QString::number(pen.joinStyle() == Qt::RoundJoin ? 1 : pen.joinStyle() == Qt::BevelJoin ? 2 : 0) + QStringLiteral(" setlinejoin\n");
                if (pen.style() != Qt::SolidLine) {
                    QString dashes;
                    for (double d : pen.dashPattern())
                        dashes += num(d * pen.widthF()) + QLatin1Char(' ');
                    ps += QStringLiteral("[%1] 0 setdash\n").arg(dashes.trimmed());
                }
                ps += QStringLiteral("stroke\n");
            }
        }
        ps += QStringLiteral("grestore\n");
    }
    ps += QStringLiteral("showpage\n%%Trailer\n%%EOF\n");
    return ps.toLatin1();
}

bool exportEps(const Canvas& canvas, const QString& path, const ExportOptions& options, QString* error)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(areaEps(canvas, options)) < 0) {
        setError(error, f.errorString());
        return false;
    }
    return true;
}

namespace {

// AutoCAD-2004-DXF in die Vorlage resources/dxf/acad2004.dxf schreiben (tools/dxf-template.py)
class DxfWriter {
public:
    explicit DxfWriter(double height) : m_height(height)
    {
        QFile f(QStringLiteral(":/dxf/acad2004.dxf"));
        if (f.open(QIODevice::ReadOnly))
            m_template = QString::fromUtf8(f.readAll());
        // Kennwerte der Vorlage aus ihrer ersten Kommentarzeile
        static const QRegularExpression keys(QStringLiteral("Modellbereich=(\\w+) Ebenentabelle=(\\w+) Bildverzeichnis=(\\w+) Handles ab=(\\w+)"));
        const QRegularExpressionMatch m = keys.match(m_template);
        m_modelSpace = m.captured(1);
        m_layerTable = m.captured(2);
        m_imageDict = m.captured(3);
        m_next = m.captured(4).toUInt(nullptr, 16);
        static const QRegularExpression plotStyle(QStringLiteral("AcDbLayerTableRecord\\n  2\\n0\\n(?:.*\\n)*?390\\n(\\w+)"));
        m_plotStyle = plotStyle.match(m_template).captured(1);
    }

    bool valid() const { return !m_modelSpace.isEmpty() && m_next > 0; }

    void addLayer(const QString& name)
    {
        if (name == QLatin1String("0") || m_layerNames.contains(name))
            return;
        m_layerNames << name;
        QString& o = m_layers;
        put(o, 0, QStringLiteral("LAYER"));
        put(o, 5, handle());
        put(o, 330, m_layerTable);
        put(o, 100, QStringLiteral("AcDbSymbolTableRecord"));
        put(o, 100, QStringLiteral("AcDbLayerTableRecord"));
        put(o, 2, name);
        put(o, 70, QStringLiteral("0"));
        put(o, 62, QStringLiteral("7"));
        put(o, 6, QStringLiteral("Continuous"));
        put(o, 370, QStringLiteral("-3"));
        if (!m_plotStyle.isEmpty())
            put(o, 390, m_plotStyle);
    }

    // Gefüllte Fläche als Schraffur (einfarbig oder zweifarbiger Verlauf)
    void hatch(const QPainterPath& devicePath, const QString& layer, const QColor& color, double alpha,
               const std::optional<QGradient>& gradient = std::nullopt, const QTransform& gradientToDevice = QTransform())
    {
        QPainterPath path = devicePath;
        if (path.fillRule() == Qt::WindingFill && path.toSubpathPolygons().size() > 1) {
            // Ungerade-Regel der Schraffur: überlappende Teile erst vereinigen
            QPainterPath merged = path.simplified();
            path = CurveObject::toPainterPath(pathops::fitRegion(merged, 0.05));
        }
        QString o;
        // Verläufe: Mischfarbe als Ersatz für Programme ohne Verlaufsschraffur
        QColor shown = color;
        if (gradient) {
            const QColor a = gradient->stops().first().second, b = gradient->stops().last().second;
            shown = QColor((a.red() + b.red()) / 2, (a.green() + b.green()) / 2, (a.blue() + b.blue()) / 2);
        }
        entityHead(o, QStringLiteral("HATCH"), layer, shown, alpha);
        put(o, 100, QStringLiteral("AcDbHatch"));
        put(o, 10, QStringLiteral("0.0"));
        put(o, 20, QStringLiteral("0.0"));
        put(o, 30, QStringLiteral("0.0"));
        put(o, 210, QStringLiteral("0.0"));
        put(o, 220, QStringLiteral("0.0"));
        put(o, 230, QStringLiteral("1.0"));
        put(o, 2, QStringLiteral("SOLID"));
        put(o, 70, QStringLiteral("1"));
        put(o, 71, QStringLiteral("0"));
        const QList<Segments> loops = segments(path);
        put(o, 91, QString::number(loops.size()));
        for (const Segments& loop : loops) {
            put(o, 92, QStringLiteral("1"));
            put(o, 93, QString::number(loop.size()));
            for (const Segment& s : loop) {
                if (!s.curve) {
                    put(o, 72, QStringLiteral("1"));
                    point(o, 10, s.p[0]);
                    point(o, 11, s.p[3]);
                } else {
                    put(o, 72, QStringLiteral("4"));
                    put(o, 94, QStringLiteral("3"));
                    put(o, 73, QStringLiteral("0"));
                    put(o, 74, QStringLiteral("0"));
                    put(o, 95, QStringLiteral("8"));
                    put(o, 96, QStringLiteral("4"));
                    for (const char* k : {"0.0", "0.0", "0.0", "0.0", "1.0", "1.0", "1.0", "1.0"})
                        put(o, 40, QLatin1String(k));
                    for (const QPointF& p : s.p)
                        point(o, 10, p);
                }
            }
            put(o, 97, QStringLiteral("0"));
        }
        put(o, 75, QStringLiteral("0"));
        put(o, 76, QStringLiteral("1"));
        put(o, 98, QStringLiteral("0"));
        if (gradient) {
            const QGradientStops stops = gradient->stops();
            put(o, 450, QStringLiteral("1"));
            put(o, 451, QStringLiteral("0"));
            double angle = 0;
            QString name = QStringLiteral("SPHERICAL");
            if (gradient->type() == QGradient::LinearGradient) {
                const auto* lin = static_cast<const QLinearGradient*>(&*gradient);
                const QPointF a = toDxf(gradientToDevice.map(lin->start())), b = toDxf(gradientToDevice.map(lin->finalStop()));
                angle = std::atan2(b.y() - a.y(), b.x() - a.x());
                name = QStringLiteral("LINEAR");
            }
            put(o, 460, num(angle));
            put(o, 461, QStringLiteral("0.0"));
            put(o, 452, QStringLiteral("0"));
            put(o, 462, QStringLiteral("0.0"));
            put(o, 453, QStringLiteral("2"));
            put(o, 463, QStringLiteral("0"));
            put(o, 421, QString::number(stops.first().second.rgb() & 0xFFFFFF));
            put(o, 463, QStringLiteral("1"));
            put(o, 421, QString::number(stops.last().second.rgb() & 0xFFFFFF));
            put(o, 470, name);
        }
        m_entities += o;
    }

    // Kontur als Mittellinie: Linienzug oder Spline (exakte Bézierkurven)
    void line(const QPainterPath& devicePath, const QString& layer, const QColor& color, double alpha, double widthMm)
    {
        static const int weights[] = {0, 5, 9, 13, 15, 18, 20, 25, 30, 35, 40, 50, 53, 60, 70, 80, 90, 100, 106, 120, 140, 158, 200, 211};
        int weight = 0;
        for (int w : weights)
            if (std::abs(w - widthMm * 100) < std::abs(weight - widthMm * 100))
                weight = w;
        for (const Segments& loop : segments(devicePath, false)) {
            const bool closed = loop.closed;
            QString o;
            entityHead(o, std::any_of(loop.begin(), loop.end(), [](const Segment& s) { return s.curve; }) ? QStringLiteral("SPLINE")
                                                                                                         : QStringLiteral("LWPOLYLINE"),
                       layer, color, alpha, weight);
            if (std::none_of(loop.begin(), loop.end(), [](const Segment& s) { return s.curve; })) {
                put(o, 100, QStringLiteral("AcDbPolyline"));
                const int n = int(loop.size()) + (closed ? 0 : 1);
                put(o, 90, QString::number(n));
                put(o, 70, closed ? QStringLiteral("1") : QStringLiteral("0"));
                for (const Segment& s : loop)
                    point(o, 10, s.p[0]);
                if (!closed)
                    point(o, 10, loop.last().p[3]);
            } else {
                // Kubischer B-Spline mit dreifachen Knoten: jedes Stück ist genau eine Bézierkurve
                put(o, 100, QStringLiteral("AcDbSpline"));
                put(o, 70, QStringLiteral("8"));
                put(o, 71, QStringLiteral("3"));
                const int n = int(loop.size());
                put(o, 72, QString::number(3 * n + 5));
                put(o, 73, QString::number(3 * n + 1));
                put(o, 74, QStringLiteral("0"));
                for (int k = 0; k < 4; ++k)
                    put(o, 40, QStringLiteral("0.0"));
                for (int i = 1; i < n; ++i)
                    for (int k = 0; k < 3; ++k)
                        put(o, 40, QString::number(i) + QStringLiteral(".0"));
                for (int k = 0; k < 4; ++k)
                    put(o, 40, QString::number(n) + QStringLiteral(".0"));
                point3(o, loop.first().p[0]);
                for (const Segment& s : loop)
                    for (int k = 1; k < 4; ++k)
                        point3(o, s.p[k]);
            }
            m_entities += o;
        }
    }

    // Bild als IMAGE, Pixel in einer PNG-Datei neben der Zeichnung; pixelToDevice bildet Bildpunkte
    // (Ursprung oben links) aufs Gerät ab
    void image(const QImage& img, const QTransform& pixelToDevice, const QString& layer, double alpha, const QString& fileName)
    {
        m_images << std::pair{fileName, img};
        const QString def = handle(), reactor = handle(), self = handle();
        const int w = img.width(), h = img.height();
        // Einfügepunkt = linke untere Ecke; u entlang der unteren Kante, v nach oben
        const QPointF insert = toDxf(pixelToDevice.map(QPointF(0, h)));
        const QPointF u = toDxf(pixelToDevice.map(QPointF(1, h))) - insert;
        const QPointF v = toDxf(pixelToDevice.map(QPointF(0, h - 1))) - insert;
        QString o;
        put(o, 0, QStringLiteral("IMAGE"));
        put(o, 5, self);
        put(o, 330, m_modelSpace);
        put(o, 100, QStringLiteral("AcDbEntity"));
        put(o, 8, layer);
        if (alpha < 0.999)
            put(o, 440, QString::number(0x02000000 | int(std::lround(std::clamp(alpha, 0.0, 1.0) * 255))));
        put(o, 100, QStringLiteral("AcDbRasterImage"));
        put(o, 90, QStringLiteral("0"));
        point3(o, insert, 10);
        put(o, 11, num(u.x()));
        put(o, 21, num(u.y()));
        put(o, 31, QStringLiteral("0.0"));
        put(o, 12, num(v.x()));
        put(o, 22, num(v.y()));
        put(o, 32, QStringLiteral("0.0"));
        put(o, 13, QString::number(w) + QStringLiteral(".0"));
        put(o, 23, QString::number(h) + QStringLiteral(".0"));
        put(o, 340, def);
        put(o, 70, img.hasAlphaChannel() ? QStringLiteral("11") : QStringLiteral("3")); // zeigen, schräg zeigen, (Transparenz)
        put(o, 280, QStringLiteral("0"));
        put(o, 281, QStringLiteral("50"));
        put(o, 282, QStringLiteral("50"));
        put(o, 283, QStringLiteral("0"));
        put(o, 360, reactor);
        put(o, 71, QStringLiteral("1"));
        put(o, 91, QStringLiteral("2"));
        put(o, 14, QStringLiteral("-0.5"));
        put(o, 24, QStringLiteral("-0.5"));
        put(o, 14, num(w - 0.5));
        put(o, 24, num(h - 0.5));
        m_entities += o;
        // Bilddefinition im Bildverzeichnis, Rückverweis über den Reaktor
        put(m_dictEntries, 3, QFileInfo(fileName).completeBaseName());
        put(m_dictEntries, 350, def);
        QString& d = m_objects;
        put(d, 0, QStringLiteral("IMAGEDEF"));
        put(d, 5, def);
        put(d, 102, QStringLiteral("{ACAD_REACTORS"));
        put(d, 330, m_imageDict);
        put(d, 330, reactor);
        put(d, 102, QStringLiteral("}"));
        put(d, 330, m_imageDict);
        put(d, 100, QStringLiteral("AcDbRasterImageDef"));
        put(d, 90, QStringLiteral("0"));
        put(d, 1, fileName);
        put(d, 10, QString::number(w) + QStringLiteral(".0"));
        put(d, 20, QString::number(h) + QStringLiteral(".0"));
        const double mmPerPixel = QLineF(QPointF(0, 0), u).length();
        put(d, 11, num(mmPerPixel));
        put(d, 21, num(mmPerPixel));
        put(d, 280, QStringLiteral("1"));
        put(d, 281, QStringLiteral("0"));
        put(d, 0, QStringLiteral("IMAGEDEF_REACTOR"));
        put(d, 5, reactor);
        put(d, 330, self);
        put(d, 100, QStringLiteral("AcDbRasterImageDefReactor"));
        put(d, 90, QStringLiteral("2"));
        put(d, 330, self);
    }

    QByteArray finish(const QSizeF& extentMm)
    {
        QString t = m_template;
        auto marker = [](const char* name) { return QStringLiteral("@%1@\n").arg(QLatin1String(name)); };
        t.replace(marker("LAYERS"), m_layers);
        t.replace(marker("ENTITIES"), m_entities);
        t.replace(marker("IMAGEDICT"), m_dictEntries);
        t.replace(marker("OBJECTS"), m_objects);
        t.replace(QStringLiteral("@EXTMIN_X@"), QStringLiteral("0.0"));
        t.replace(QStringLiteral("@EXTMIN_Y@"), QStringLiteral("0.0"));
        t.replace(QStringLiteral("@EXTMAX_X@"), num(extentMm.width()));
        t.replace(QStringLiteral("@EXTMAX_Y@"), num(extentMm.height()));
        t.replace(QStringLiteral("@HANDSEED@"), QString::number(m_next, 16).toUpper());
        return t.toUtf8();
    }

    const QList<std::pair<QString, QImage>>& images() const { return m_images; }

private:
    struct Segment {
        bool curve = false;
        std::array<QPointF, 4> p; // DXF-Koordinaten (mm, y nach oben)
    };
    struct Segments : QList<Segment> {
        bool closed = false;
    };

    static constexpr double kMmPerPt = 25.4 / 72.0;

    QPointF toDxf(const QPointF& device) const { return QPointF(device.x() * kMmPerPt, (m_height - device.y()) * kMmPerPt); }

    // Teilpfade als Linien- und Kurvenstücke; für Flächen immer geschlossen
    QList<Segments> segments(const QPainterPath& path, bool forceClosed = true) const
    {
        QList<Segments> out;
        QPointF start, last;
        auto closeLoop = [&] {
            if (out.isEmpty() || out.last().isEmpty())
                return;
            Segments& s = out.last();
            const bool atStart = QLineF(last, start).length() < 1e-9;
            if (!atStart && forceClosed)
                s << Segment{false, {last, last, start, start}};
            s.closed = forceClosed || atStart;
        };
        for (int i = 0; i < path.elementCount(); ++i) {
            const QPainterPath::Element e = path.elementAt(i);
            const QPointF p = toDxf(e);
            if (e.isMoveTo()) {
                closeLoop();
                out << Segments();
                start = last = p;
            } else if (e.isLineTo()) {
                if (QLineF(last, p).length() > 1e-9) {
                    const QPointF third = (p - last) / 3.0;
                    out.last() << Segment{false, {last, last + third, last + 2 * third, p}};
                }
                last = p;
            } else if (e.isCurveTo() && i + 2 < path.elementCount()) {
                const QPointF c2 = toDxf(path.elementAt(i + 1)), end = toDxf(path.elementAt(i + 2));
                out.last() << Segment{true, {last, p, c2, end}};
                last = end;
                i += 2;
            }
        }
        closeLoop();
        out.removeIf([](const Segments& s) { return s.isEmpty(); });
        return out;
    }

    void entityHead(QString& o, const QString& type, const QString& layer, const QColor& color, double alpha, int lineWeight = -1)
    {
        put(o, 0, type);
        put(o, 5, handle());
        put(o, 330, m_modelSpace);
        put(o, 100, QStringLiteral("AcDbEntity"));
        put(o, 8, layer);
        if (color.isValid()) {
            put(o, 62, QString::number(dxf::nearestAci(color)));
            put(o, 420, QString::number(color.rgb() & 0xFFFFFF));
        }
        if (lineWeight >= 0)
            put(o, 370, QString::number(lineWeight));
        if (alpha < 0.999)
            put(o, 440, QString::number(0x02000000 | int(std::lround(std::clamp(alpha, 0.0, 1.0) * 255))));
    }

    void point(QString& o, int code, const QPointF& p) const
    {
        put(o, code, num(p.x()));
        put(o, code + 10, num(p.y()));
    }
    void point3(QString& o, const QPointF& p, int code = 10) const
    {
        point(o, code, p);
        put(o, code + 20, QStringLiteral("0.0"));
    }
    static void put(QString& o, int code, const QString& v)
    {
        o += QString::number(code).rightJustified(3) + QLatin1Char('\n') + v + QLatin1Char('\n');
    }
    static QString num(double v) { return QString::number(v, 'f', 6); }
    QString handle() { return QString::number(m_next++, 16).toUpper(); }

    double m_height;
    QString m_template;
    QString m_modelSpace, m_layerTable, m_imageDict, m_plotStyle;
    quint32 m_next = 0;
    QStringList m_layerNames;
    QString m_layers, m_entities, m_dictEntries, m_objects;
    QList<std::pair<QString, QImage>> m_images;
};

QString dxfLayerName(QString name)
{
    // In Ebenennamen verboten: < > / \ " : ; ? * | = `
    static const QRegularExpression bad(QStringLiteral("[<>/\\\\\":;?*|=`]"));
    name.replace(bad, QStringLiteral("_"));
    name = name.trimmed();
    return name.isEmpty() ? QStringLiteral("TANGENTA") : name;
}

// Einfarbig oder zweifarbiger Verlauf mit gleichmässiger Deckung: als Schraffur darstellbar
bool hatchable(const QBrush& b)
{
    if (b.style() == Qt::SolidPattern)
        return true;
    const QGradient* g = b.gradient();
    if (!g || (g->type() != QGradient::LinearGradient && g->type() != QGradient::RadialGradient))
        return false;
    const QGradientStops stops = g->stops();
    if (stops.size() < 2)
        return false;
    for (const auto& s : stops)
        if (s.second.alpha() != stops.first().second.alpha())
            return false;
    // Zwischenstopps nur, wenn sie auf der Linie zwischen Anfang und Ende liegen
    for (qsizetype i = 1; i + 1 < stops.size(); ++i) {
        const double t = stops[i].first;
        const QColor a = stops.first().second, e = stops.last().second, m = stops[i].second;
        if (std::abs(a.red() + (e.red() - a.red()) * t - m.red()) > 6 || std::abs(a.green() + (e.green() - a.green()) * t - m.green()) > 6
            || std::abs(a.blue() + (e.blue() - a.blue()) * t - m.blue()) > 6)
            return false;
    }
    return true;
}

} // namespace

QByteArray areaDxf(const Canvas& canvas, const ExportOptions& options, QList<std::pair<QString, QImage>>* images, const QString& imageBaseName)
{
    const QRectF area = exportArea(canvas, options);
    const double ptPerMm = 72.0 / 25.4;
    const QSizeF size(area.width() * ptPerMm, area.height() * ptPerMm);
    DxfWriter w(size.height());
    if (!w.valid())
        return {};
    // Je Ebene des Dokuments eine DXF-Ebene (Hauptseite zuerst, wie beim Zeichnen)
    const Document& doc = canvas.editor().document();
    QList<std::pair<QString, QList<NodeId>>> layers;
    for (const Page* page : {&doc.masterPage(), &doc.page(options.page)}) {
        for (int l = 0; l < page->childCount(); ++l) {
            const auto* layer = node_cast<Layer>(page->child(l));
            if (!layer || !layer->visible() || !layer->printable())
                continue;
            QList<NodeId> ids;
            for (int i = 0; i < layer->childCount(); ++i)
                if (options.only.isEmpty() || options.only.contains(layer->child(i)->id()))
                    ids << layer->child(i)->id();
            if (!ids.isEmpty())
                layers << std::pair{dxfLayerName(layer->name()), ids};
        }
    }
    // Gerastertes Endergebnis für das, was DXF nicht kann (Mischmodi, Muster, mehrfarbige Verläufe)
    QImage flat;
    double pxPerPt = 0;
    auto flatImage = [&]() -> const QImage& {
        if (flat.isNull()) {
            double dpi = std::clamp(options.flattenDpi, 72.0, 1200.0);
            const double maxPixels = 200.0 * 1000 * 1000;
            const double pixels = size.width() * size.height() * (dpi / 72.0) * (dpi / 72.0);
            if (pixels > maxPixels)
                dpi *= std::sqrt(maxPixels / pixels);
            pxPerPt = dpi / 72.0;
            flat = QImage(QSize(std::max(1, int(std::ceil(size.width() * pxPerPt))), std::max(1, int(std::ceil(size.height() * pxPerPt)))),
                          QImage::Format_ARGB32_Premultiplied);
            flat.fill(Qt::transparent);
            QPainter p(&flat);
            p.setRenderHint(QPainter::Antialiasing);
            p.setRenderHint(QPainter::SmoothPixmapTransform);
            renderArea(canvas, p, options, area, ptPerMm * pxPerPt, false);
        }
        return flat;
    };
    int imageCount = 0;
    auto nextImageName = [&] { return QStringLiteral("%1_bild%2.png").arg(imageBaseName.isEmpty() ? QStringLiteral("tangenta") : imageBaseName).arg(++imageCount); };
    for (const auto& [layerName, ids] : std::as_const(layers)) {
        w.addLayer(layerName);
        ExportOptions part = options;
        part.only = ids;
        VectorRecorder recorder(size, 72.0);
        {
            QPainter p(&recorder);
            renderArea(canvas, p, part, area, ptPerMm, false);
        }
        for (const RecordedItem& item : recorder.items()) {
            const double opacity = item.opacity;
            const bool blend = item.composition != QPainter::CompositionMode_SourceOver;
            if (item.kind == RecordedItem::Kind::Image && !blend) {
                QImage img = item.image.convertToFormat(QImage::Format_ARGB32);
                if (item.clip) {
                    // Beschneidung (SuperClip) in den Alphakanal
                    QImage mask(img.size(), QImage::Format_ARGB32_Premultiplied);
                    mask.fill(Qt::transparent);
                    QPainter mp(&mask);
                    mp.setRenderHint(QPainter::Antialiasing);
                    mp.fillPath(item.imageTransform.inverted().map(*item.clip), Qt::black);
                    mp.end();
                    QPainter ip(&img);
                    ip.setCompositionMode(QPainter::CompositionMode_DestinationIn);
                    ip.drawImage(0, 0, mask);
                }
                w.image(img, item.imageTransform, layerName, opacity, nextImageName());
                continue;
            }
            const bool fillOk = !item.fill || hatchable(*item.fill);
            const bool strokeOk = !item.stroke || item.stroke->brush().style() == Qt::SolidPattern;
            if (blend || !fillOk || !strokeOk || item.kind == RecordedItem::Kind::Image) {
                // Endergebnis an dieser Stelle als Bild, ausserhalb der Form durchsichtig
                const QImage& full = flatImage();
                const QPainterPath shape = item.coverage();
                const QRectF bounds = shape.boundingRect().intersected(QRectF(QPointF(0, 0), size));
                if (bounds.isEmpty())
                    continue;
                const QRect px = QRectF(bounds.topLeft() * pxPerPt, bounds.bottomRight() * pxPerPt).toAlignedRect().adjusted(-1, -1, 1, 1)
                                     .intersected(full.rect());
                QImage crop = full.copy(px).convertToFormat(QImage::Format_ARGB32);
                QImage mask(crop.size(), QImage::Format_ARGB32_Premultiplied);
                mask.fill(Qt::transparent);
                {
                    QPainter mp(&mask);
                    mp.setRenderHint(QPainter::Antialiasing);
                    mp.translate(-px.topLeft());
                    mp.scale(pxPerPt, pxPerPt);
                    mp.fillPath(shape, Qt::black);
                }
                {
                    QPainter ip(&crop);
                    ip.setCompositionMode(QPainter::CompositionMode_DestinationIn);
                    ip.drawImage(0, 0, mask);
                }
                const QTransform toDevice = QTransform::fromScale(1 / pxPerPt, 1 / pxPerPt) * QTransform::fromTranslate(px.left() / pxPerPt, px.top() / pxPerPt);
                w.image(crop, toDevice, layerName, 1.0, nextImageName());
                continue;
            }
            if (item.fill) {
                QPainterPath path = item.path;
                if (item.clip)
                    path = path.intersected(*item.clip);
                const QBrush& b = *item.fill;
                if (b.style() == Qt::SolidPattern) {
                    w.hatch(path, layerName, b.color(), b.color().alphaF() * opacity);
                } else {
                    const QGradient* g = b.gradient();
                    w.hatch(path, layerName, QColor(), g->stops().first().second.alphaF() * opacity, *g, b.transform());
                }
            }
            if (item.stroke) {
                const QPen& pen = *item.stroke;
                const double widthMm = pen.widthF() * 25.4 / 72.0;
                const QColor c = pen.color();
                if (widthMm > 2.11 + 1e-6) {
                    // Breiter als jede DXF-Linienstärke: Kontur als Fläche
                    QPainterPathStroker stroker(pen);
                    QPainterPath outline = stroker.createStroke(item.path);
                    outline.setFillRule(Qt::WindingFill);
                    if (item.clip)
                        outline = outline.intersected(*item.clip);
                    w.hatch(outline, layerName, c, c.alphaF() * opacity);
                } else {
                    w.line(item.path, layerName, c, c.alphaF() * opacity, widthMm);
                }
            }
        }
    }
    if (images)
        *images = w.images();
    return w.finish(area.size());
}

bool exportDxf(const Canvas& canvas, const QString& path, const ExportOptions& options, QString* error)
{
    QList<std::pair<QString, QImage>> images;
    const QByteArray data = areaDxf(canvas, options, &images, QFileInfo(path).completeBaseName());
    QFile f(path);
    if (data.isEmpty() || !f.open(QIODevice::WriteOnly) || f.write(data) < 0) {
        setError(error, data.isEmpty() ? QObject::tr("DXF-Vorlage fehlt.") : f.errorString());
        return false;
    }
    // Bilder liegen als PNG neben der Zeichnung (DXF verweist nur auf sie)
    for (const auto& [name, image] : images)
        if (!image.save(QFileInfo(path).dir().filePath(name), "PNG")) {
            setError(error, QObject::tr("%1 konnte nicht gespeichert werden.").arg(name));
            return false;
        }
    return true;
}

QPainterPath selectionOutline(const Canvas& canvas, const ExportOptions& options)
{
    // Gezeichnete Flächen und Konturen der Auswahl als eine Fläche (mm, Ursprung oben links)
    const QRectF area = exportArea(canvas, options);
    const double ptPerMm = 72.0 / 25.4;
    VectorRecorder recorder(QSizeF(area.width() * ptPerMm, area.height() * ptPerMm), 72.0);
    {
        QPainter p(&recorder);
        renderArea(canvas, p, options, area, ptPerMm, false);
    }
    QPainterPath result;
    result.setFillRule(Qt::WindingFill);
    for (const RecordedItem& item : recorder.items()) {
        if (item.kind != RecordedItem::Kind::Path || item.opacity <= 0.0)
            continue;
        QPainterPath shape;
        if (item.fill && item.fill->style() != Qt::NoBrush)
            shape = item.path;
        if (item.stroke && item.stroke->style() != Qt::NoPen && item.stroke->widthF() > 0) {
            QPainterPathStroker stroker(*item.stroke);
            QPainterPath outline = stroker.createStroke(item.path);
            outline.setFillRule(Qt::WindingFill);
            shape = shape.isEmpty() ? outline : shape.united(outline);
        }
        if (item.clip)
            shape = shape.intersected(*item.clip);
        if (!shape.isEmpty())
            result = result.isEmpty() ? shape : result.united(shape);
    }
    return QTransform::fromScale(1.0 / ptPerMm, 1.0 / ptPerMm).map(result);
}

bool exportAi(const Canvas& canvas, const QString& path, const ExportOptions& options, QString* error)
{
    // Illustrator speichert seit Version 9 PDF-kompatibel; jedes Programm, das AI liest, liest dies
    return exportPdf(canvas, path, options, error);
}

QByteArray pageSvg(const Canvas& canvas, int page)
{
    const QSizeF mm = pageSize(canvas, page);
    // 96 dpi wie im Web; QSvgGenerator schreibt Breite/Höhe in mm aus der Auflösung
    const double pxPerMm = 96.0 / 25.4;
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly);
    QSvgGenerator gen;
    gen.setOutputDevice(&buffer);
    gen.setResolution(96);
    gen.setSize(QSize(int(std::ceil(mm.width() * pxPerMm)), int(std::ceil(mm.height() * pxPerMm))));
    gen.setViewBox(QRectF(0, 0, mm.width() * pxPerMm, mm.height() * pxPerMm));
    gen.setTitle(QObject::tr("Seite %1").arg(page + 1));
    {
        QPainter p(&gen);
        canvas.renderPage(p, page, pxPerMm, QPointF(0, 0), true);
    }
    return buffer.data();
}

bool exportSvg(const Canvas& canvas, const QString& path, int page, QString* error)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(pageSvg(canvas, page)) < 0) {
        setError(error, f.errorString());
        return false;
    }
    return true;
}

bool exportHtml(const Canvas& canvas, const QString& path, const QString& title, QString* error)
{
    const int count = doc(canvas).pageCount();
    QString html;
    html += QStringLiteral("<!DOCTYPE html>\n<html lang=\"de\">\n<head>\n<meta charset=\"utf-8\">\n"
                           "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n<title>%1</title>\n"
                           "<meta name=\"generator\" content=\"TANGENTA\">\n<style>\n"
                           "body{margin:0;background:#e9e9e9;font-family:system-ui,sans-serif}\n"
                           "nav{position:sticky;top:0;background:#fff;border-bottom:1px solid #ccc;padding:8px 16px;z-index:1}\n"
                           "nav a{margin-right:12px;color:#1f5fae;text-decoration:none}\n"
                           "section{margin:24px auto;width:max-content;max-width:calc(100% - 32px)}\n"
                           "section svg{display:block;max-width:100%;height:auto;background:#fff;box-shadow:0 1px 4px rgba(0,0,0,.25)}\n"
                           "</style>\n</head>\n<body>\n")
                .arg(title.toHtmlEscaped());
    if (count > 1) {
        html += QStringLiteral("<nav>");
        for (int i = 0; i < count; ++i)
            html += QStringLiteral("<a href=\"#seite-%1\">%2</a>").arg(i + 1).arg(doc(canvas).page(i).name().isEmpty()
                                                                               ? QObject::tr("Seite %1").arg(i + 1).toHtmlEscaped()
                                                                               : doc(canvas).page(i).name().toHtmlEscaped());
        html += QStringLiteral("</nav>\n");
    }
    for (int i = 0; i < count; ++i) {
        QString svg = QString::fromUtf8(pageSvg(canvas, i));
        // XML-Kopf weg, das SVG steht direkt im HTML
        const int start = svg.indexOf(QLatin1String("<svg"));
        if (start > 0)
            svg = svg.mid(start);
        html += QStringLiteral("<section id=\"seite-%1\">\n%2\n</section>\n").arg(i + 1).arg(svg);
    }
    html += QStringLiteral("</body>\n</html>\n");
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(html.toUtf8()) < 0) {
        setError(error, f.errorString());
        return false;
    }
    return true;
}

} // namespace vek::output
