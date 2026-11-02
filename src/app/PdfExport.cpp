#include "PdfExport.h"

#include "Canvas.h"
#include "Document.h"
#include "DrawObject.h"
#include "Editor.h"
#include "GroupObject.h"
#include "ImageObject.h"
#include "ImagePool.h"
#include "Output.h"
#include "Page.h"
#include "PdfImport.h"
#include "TextObject.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QRandomGenerator>
#include <QRawFont>
#include <QTemporaryDir>

#include <cmath>
#include <functional>

namespace vek::pdfexport {

namespace {

constexpr double kPtPerMm = 72.0 / 25.4;

// Eine auszugebende PDF-Seite
struct OutPage {
    int page = 0;              // Dokumentseite
    QRectF trim;               // Endformat in Seitenkoordinaten (mm)
    QList<NodeId> only;        // leer = alle Objekte
    QString name;              // Lesezeichen
};

QList<OutPage> outputPages(const Canvas& canvas, const PdfSettings& s)
{
    const Editor& ed = canvas.editor();
    const Document& doc = ed.document();
    QList<OutPage> out;
    auto whole = [&](int index) {
        OutPage p;
        p.page = index;
        p.trim = QRectF(QPointF(0, 0), doc.page(index).size());
        p.name = doc.page(index).name().isEmpty() ? QObject::tr("Seite %1").arg(index + 1) : doc.page(index).name();
        return p;
    };
    switch (s.range) {
    case PdfSettings::Range::Document:
        for (int i = 0; i < doc.pageCount(); ++i)
            out << whole(i);
        break;
    case PdfSettings::Range::CurrentPage:
        out << whole(ed.currentPageIndex());
        break;
    case PdfSettings::Range::Pages:
        for (int i : parsePageList(s.pages, doc.pageCount()))
            out << whole(i);
        break;
    case PdfSettings::Range::Selection: {
        if (ed.selection().isEmpty())
            break;
        OutPage p = whole(ed.currentPageIndex());
        p.only = ed.selection();
        if (s.sizeFromSelection) {
            output::ExportOptions o;
            o.page = p.page;
            o.only = p.only;
            p.trim = output::exportArea(canvas, o);
        }
        out << p;
        break;
    }
    }
    return out;
}

// Alle Zeichenobjekte einer Seite (auch in Gruppen), gefiltert auf die oberste Ebene von only
void forEachObject(const Document& doc, const OutPage& page, const std::function<void(const DrawObject&)>& visit)
{
    std::function<void(const Node&)> walk = [&](const Node& n) {
        for (int i = 0; i < n.childCount(); ++i)
            if (const auto* d = node_cast<DrawObject>(n.child(i))) {
                visit(*d);
                walk(*d);
            }
    };
    for (const Page* p : {&doc.masterPage(), &doc.page(page.page)})
        for (int l = 0; l < p->childCount(); ++l) {
            const auto* layer = node_cast<Layer>(p->child(l));
            if (!layer || !layer->visible() || !layer->printable())
                continue;
            for (int i = 0; i < layer->childCount(); ++i) {
                const auto* d = node_cast<DrawObject>(layer->child(i));
                if (!d || (!page.only.isEmpty() && !page.only.contains(d->id())))
                    continue;
                visit(*d);
                walk(*d);
            }
        }
}

bool hasTransparency(const DrawObject& o)
{
    return o.transparency().isActive() || o.blur() > 0 || o.shadow().active || o.lens().isActive();
}

QString psString(const QString& text)
{
    // PostScript-Zeichenkette als UTF-16BE (mit BOM), damit Umlaute im PDF ankommen
    QString hex = QStringLiteral("<FEFF");
    for (QChar c : text)
        hex += QStringLiteral("%1").arg(int(c.unicode()), 4, 16, QLatin1Char('0')).toUpper();
    return hex + QLatin1Char('>');
}

QString num(double v)
{
    return QString::number(v, 'f', 3);
}

// Druckermarken in den Rand um das Endformat (Painter in mm, Ursprung = Medienecke)
void paintMarks(QPainter& p, const QRectF& trim, double bleed, const PdfSettings& s, const QString& info)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen(Qt::black, 0.25 / kPtPerMm);
    pen.setCapStyle(Qt::FlatCap);
    p.setPen(pen);
    const double gap = bleed + 1.5, len = 6.0;
    if (s.cropMarks) {
        for (const QPointF& c : {trim.topLeft(), trim.topRight(), trim.bottomLeft(), trim.bottomRight()}) {
            const double sx = c.x() < trim.center().x() ? -1 : 1, sy = c.y() < trim.center().y() ? -1 : 1;
            p.drawLine(QPointF(c.x() + sx * gap, c.y()), QPointF(c.x() + sx * (gap + len), c.y()));
            p.drawLine(QPointF(c.x(), c.y() + sy * gap), QPointF(c.x(), c.y() + sy * (gap + len)));
        }
    }
    if (s.registrationMarks) {
        // Passkreuze mittig an allen vier Seiten
        const double r = 2.5, off = gap + len / 2;
        for (const QPointF& c : {QPointF(trim.center().x(), trim.top() - off), QPointF(trim.center().x(), trim.bottom() + off),
                                 QPointF(trim.left() - off, trim.center().y()), QPointF(trim.right() + off, trim.center().y())}) {
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(c, r, r);
            p.drawEllipse(c, r * 0.5, r * 0.5);
            p.drawLine(c - QPointF(r * 1.4, 0), c + QPointF(r * 1.4, 0));
            p.drawLine(c - QPointF(0, r * 1.4), c + QPointF(0, r * 1.4));
        }
    }
    if (s.densitometer) {
        // Graukeil 0..100 % und die Druckfarben unter dem Endformat
        const double size = 4.0;
        const double y = trim.bottom() + gap + len + 1.0;
        double x = trim.left();
        for (int k = 0; k <= 10; ++k, x += size)
            p.fillRect(QRectF(x, y, size, size), QColor::fromRgbF(1 - k / 10.0, 1 - k / 10.0, 1 - k / 10.0));
        x += size;
        for (const QColor& c : {QColor(0, 255, 255), QColor(255, 0, 255), QColor(255, 255, 0), QColor(0, 0, 0)}) {
            p.fillRect(QRectF(x, y, size, size), c);
            x += size;
        }
        p.setBrush(Qt::NoBrush);
        p.drawRect(QRectF(trim.left(), y, x - trim.left(), size));
    }
    if (s.fileInfo && !info.isEmpty()) {
        QFont f(QStringLiteral("Arial"));
        f.setPixelSize(1); // nicht ganzzahlig skalieren: Schrift über die Transformation
        p.setFont(f);
        p.save();
        p.translate(trim.left(), trim.top() - gap - len - 1.0);
        p.scale(2.4, 2.4); // ~7 pt
        p.drawText(QPointF(0, 0), info);
        p.restore();
    }
    p.restore();
}

int permissions(const PdfSettings& s)
{
    // PDF-Berechtigungen (Bit 3 drucken, 4 ändern, 5 kopieren, 6 Kommentare, 9 Formulare,
    // 10 Zugänglichkeit, 11 zusammenstellen, 12 hochaufgelöst drucken); übrige Bits gesetzt
    quint32 p = 0xFFFFF0C0u;
    if (s.printing != PdfSettings::Printing::None)
        p |= 1u << 2;
    if (s.printing == PdfSettings::Printing::HighResolution)
        p |= 1u << 11;
    switch (s.changes) {
    case PdfSettings::Changes::None: break;
    case PdfSettings::Changes::Assembly: p |= 1u << 10; break;
    case PdfSettings::Changes::FormFill: p |= (1u << 8) | (1u << 10); break;
    case PdfSettings::Changes::Comments: p |= (1u << 5) | (1u << 8) | (1u << 10); break;
    case PdfSettings::Changes::AllButExtract: p |= (1u << 3) | (1u << 5) | (1u << 8) | (1u << 10); break;
    }
    if (s.allowCopy)
        p |= (1u << 4) | (1u << 9);
    return int(qint32(p));
}

// ICC-Profil aus Ghostscript: in der Windows-Bibliothek eingebaut (%rom%), unter Linux als Datei
QString iccProfile(const QString& name)
{
#ifndef Q_OS_WIN
    QStringList dirs{QStringLiteral("/usr/share/color/icc/ghostscript"), QStringLiteral("/usr/local/share/color/icc/ghostscript")};
    for (const QString& root : {QStringLiteral("/usr/share/ghostscript"), QStringLiteral("/usr/local/share/ghostscript")})
        for (const QString& version : QDir(root).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed))
            dirs << root + QLatin1Char('/') + version + QStringLiteral("/iccprofiles");
    for (const QString& dir : std::as_const(dirs))
        if (QFileInfo::exists(dir + QLatin1Char('/') + name))
            return QFileInfo(dir + QLatin1Char('/') + name).canonicalFilePath();
#endif
    return QStringLiteral("%rom%iccprofiles/") + name;
}

bool ghostscriptAvailable()
{
    return !pdf::ghostscript().isEmpty();
}

// Was nur mit Ghostscript geht
QStringList needsGhostscript(const PdfSettings& s)
{
    QStringList list;
    if (s.colors == PdfSettings::Colors::Cmyk)
        list << QObject::tr("CMYK-Ausgabe");
    if (s.colors == PdfSettings::Colors::Gray)
        list << QObject::tr("Graustufen-Ausgabe");
    if (s.isPdfX())
        list << QObject::tr("PDF/X");
    if (s.compatibility == PdfSettings::Compatibility::PdfA2b)
        list << QObject::tr("PDF/A-2b");
    if (s.openPassword || s.permissionPassword)
        list << QObject::tr("Kennwortschutz");
    if (!s.author.isEmpty() || !s.keywords.isEmpty())
        list << QObject::tr("Autor und Stichwörter");
    if (s.bookmarks)
        list << QObject::tr("Lesezeichen");
    if (s.thumbnails)
        list << QObject::tr("Skizzen");
    if (s.fastWebView)
        list << QObject::tr("schnelle Webanzeige");
    if (s.downsampleColor || s.downsampleGray || s.downsampleMono)
        list << QObject::tr("Bitmaps herunterrechnen");
    return list;
}

} // namespace

int problemCount(const QList<Issue>& issues)
{
    return int(std::count_if(issues.begin(), issues.end(), [](const Issue& i) { return i.severity != Issue::Severity::Info; }));
}

QList<Issue> preflight(const Canvas& canvas, const PdfSettings& s)
{
    QList<Issue> issues;
    using S = Issue::Severity;
    const Document& doc = canvas.editor().document();
    const QList<OutPage> pages = outputPages(canvas, s);
    if (pages.isEmpty()) {
        issues << Issue{S::Error, QObject::tr("Nichts auszugeben"),
                        s.range == PdfSettings::Range::Selection ? QObject::tr("Es sind keine Objekte markiert.") : QObject::tr("Die Seitenangabe ergibt keine Seite.")};
        return issues;
    }
    if ((s.openPassword || s.permissionPassword) && (s.isPdfX() || s.isPdfA()))
        issues << Issue{S::Error, QObject::tr("Kennwortschutz bei PDF/X oder PDF/A"),
                        QObject::tr("PDF/X und PDF/A erlauben keine Verschlüsselung; der Kennwortschutz wird weggelassen.")};
    if (s.openPassword && s.openPasswordText.isEmpty())
        issues << Issue{S::Error, QObject::tr("Kennwort fehlt"), QObject::tr("Für das Öffnen ist kein Kennwort eingetragen.")};
    if (s.permissionPassword && s.permissionPasswordText.isEmpty())
        issues << Issue{S::Error, QObject::tr("Berechtigungskennwort fehlt"), QObject::tr("Es ist kein Berechtigungskennwort eingetragen.")};
    if (!ghostscriptAvailable()) {
        const QStringList missing = needsGhostscript(s);
        if (!missing.isEmpty())
            issues << Issue{S::Warning, QObject::tr("Ghostscript fehlt"),
                            QObject::tr("Ohne Ghostscript (gsdll64.dll im Programmordner) entfällt: %1.").arg(missing.join(QStringLiteral(", ")))};
    }
    if (s.textAsCurves)
        issues << Issue{S::Info, QObject::tr("Text als Kurven"), QObject::tr("Der Text lässt sich im PDF weder suchen noch kopieren.")};

    int lowRes = 0, hairlines = 0, overhang = 0, transparent = 0, rgb = 0;
    double minDpi = 1e9;
    QSet<QString> missingFonts, restrictedFonts;
    for (const OutPage& page : pages) {
        const QRectF pageRect(QPointF(0, 0), doc.page(page.page).size());
        forEachObject(doc, page, [&](const DrawObject& o) {
            if (const auto* img = node_cast<ImageObject>(&o)) {
                const QSizeF pixels = imagepool::image(img->key()).size();
                const QRectF mm = o.transform().mapRect(QRectF(QPointF(0, 0), img->size()));
                if (mm.width() > 0 && pixels.width() > 0) {
                    const double dpi = pixels.width() / (mm.width() / 25.4);
                    minDpi = std::min(minDpi, dpi);
                    if (dpi < 150)
                        ++lowRes;
                }
                ++rgb;
            }
            const vek::Style& st = o.style();
            if (st.stroke && (st.strokeWidth <= 0.0 || st.strokeWidth < 0.1) && !node_cast<TextObject>(&o))
                ++hairlines;
            if (!st.fill.isNone() || st.stroke)
                ++rgb;
            if (hasTransparency(o))
                ++transparent;
            if (!s.bleedLimit && !pageRect.contains(o.boundingRect().adjusted(0.01, 0.01, -0.01, -0.01)) && page.only.isEmpty())
                ++overhang;
            if (const auto* text = node_cast<TextObject>(&o))
                for (const TextParagraph& par : text->paragraphs())
                    for (const TextRun& run : par.runs) {
                        if (!QFontDatabase::hasFamily(run.format.family)) {
                            missingFonts << run.format.family;
                            continue;
                        }
                        if (s.textAsCurves || restrictedFonts.contains(run.format.family))
                            continue;
                        // OS/2 fsType: Bit 1 = Einbetten untersagt
                        QFont f(run.format.family);
                        f.setPixelSize(100);
                        const QByteArray os2 = QRawFont::fromFont(f).fontTable("OS/2");
                        if (os2.size() > 9 && ((quint8(os2[8]) << 8 | quint8(os2[9])) & 0x000F) == 0x0002)
                            restrictedFonts << run.format.family;
                    }
        });
    }
    const bool printing = s.colors == PdfSettings::Colors::Cmyk || s.isPdfX();
    if (lowRes > 0 && printing)
        issues << Issue{S::Warning, QObject::tr("%n Bitmap(s) mit weniger als 150 dpi", nullptr, lowRes),
                        QObject::tr("Die niedrigste wirksame Auflösung ist %1 dpi; im Druck wirken solche Bilder unscharf.").arg(int(minDpi))};
    if (hairlines > 0 && printing)
        issues << Issue{S::Warning, QObject::tr("%n Haarlinie(n)", nullptr, hairlines),
                        QObject::tr("Konturen unter 0,1 mm verschwinden im Offsetdruck fast ganz.")};
    if (overhang > 0)
        issues << Issue{S::Warning, QObject::tr("%n Objekt(e) über den Seitenrand", nullptr, overhang),
                        QObject::tr("Die Randanschnittgrenze ist aus: Was über die Seite ragt, wird am Seitenrand abgeschnitten. "
                                    "Für randabfallenden Druck unter Druckvorstufe die Randanschnittgrenze einschalten.")};
    if (transparent > 0 && (s.compatibility == PdfSettings::Compatibility::PdfX1a || s.compatibility == PdfSettings::Compatibility::PdfA1b))
        issues << Issue{S::Warning, QObject::tr("Transparenz bei %1", nullptr).arg(s.compatibility == PdfSettings::Compatibility::PdfX1a ? QStringLiteral("PDF/X-1a")
                                                                                                                                 : QStringLiteral("PDF/A-1b")),
                        QObject::tr("%n Objekt(e) mit Transparenz, Schatten oder Unschärfe werden gerastert (der Standard erlaubt keine Transparenz).", nullptr,
                                    transparent)};
    if (rgb > 0 && s.colors == PdfSettings::Colors::Cmyk)
        issues << Issue{S::Info, QObject::tr("RGB-Farben werden in CMYK umgerechnet"),
                        QObject::tr("Leuchtende RGB-Farben können im Druck matter wirken.")};
    for (const QString& f : std::as_const(missingFonts))
        issues << Issue{S::Warning, QObject::tr("Schrift fehlt: %1", nullptr).arg(f), QObject::tr("Sie ist nicht installiert und wird ersetzt.")};
    for (const QString& f : std::as_const(restrictedFonts))
        issues << Issue{S::Warning, QObject::tr("Schrift darf nicht eingebettet werden: %1", nullptr).arg(f),
                        QObject::tr("Die Lizenz der Schrift untersagt das Einbetten; besser »Allen Text als Kurven exportieren«.")};
    return issues;
}

bool exportPdf(const Canvas& canvas, const QString& path, const PdfSettings& s, QString* error, QStringList* notes)
{
    auto fail = [&](const QString& text) {
        if (error)
            *error = text;
        return false;
    };
    const QList<OutPage> pages = outputPages(canvas, s);
    if (pages.isEmpty())
        return fail(QObject::tr("Keine Seiten zum Exportieren."));
    const bool gs = ghostscriptAvailable();
    if (!gs && notes)
        *notes = needsGhostscript(s);

    // 1. Seiten mit Qt schreiben: Medienformat = Endformat + Anschnitt + Platz für Marken
    const double bleed = s.bleedLimit ? std::max(0.0, s.bleed) : 0.0;
    const double slug = s.marksNeeded() ? 15.0 : 0.0;
    const double margin = bleed + slug;
    QTemporaryDir tmp;
    const QString stage1 = gs ? tmp.filePath(QStringLiteral("qt.pdf")) : path;
    {
        QPdfWriter writer(stage1);
        writer.setResolution(1200);
        writer.setCreator(QStringLiteral("TANGENTA"));
        writer.setTitle(QFileInfo(path).completeBaseName());
        if (!gs && s.isPdfA())
            writer.setPdfVersion(QPagedPaintDevice::PdfVersion_A1b);
        auto layoutFor = [&](const QRectF& trim) {
            const QSizeF media = trim.size() + QSizeF(2 * margin, 2 * margin);
            const QSizeF portrait(std::min(media.width(), media.height()), std::max(media.width(), media.height()));
            return QPageLayout(QPageSize(portrait, QPageSize::Millimeter, QString(), QPageSize::ExactMatch),
                               media.width() > media.height() ? QPageLayout::Landscape : QPageLayout::Portrait, QMarginsF(), QPageLayout::Millimeter);
        };
        writer.setPageLayout(layoutFor(pages.first().trim));
        QPainter p;
        if (!p.begin(&writer))
            return fail(QObject::tr("%1 lässt sich nicht schreiben.").arg(stage1));
        auto& c = const_cast<Canvas&>(canvas);
        c.setTextAsGlyphs(!s.textAsCurves);
        c.setComplexFillsAsBitmaps(s.complexFillsAsBitmaps);
        const double pxPerMm = writer.resolution() / 25.4;
        const QString base = QFileInfo(path).fileName();
        for (int i = 0; i < pages.size(); ++i) {
            const OutPage& op = pages[i];
            if (i > 0) {
                writer.setPageLayout(layoutFor(op.trim));
                writer.newPage();
            }
            p.save();
            // Inhalt: Endformat liegt um margin eingerückt; sichtbar bis zum Anschnitt
            const QRectF bleedBox(QPointF(slug, slug), op.trim.size() + QSizeF(2 * bleed, 2 * bleed));
            p.setClipRect(QRectF(bleedBox.topLeft() * pxPerMm, bleedBox.size() * pxPerMm));
            c.setRenderFilter(op.only);
            canvas.renderPage(p, op.page, pxPerMm, (QPointF(margin, margin) - op.trim.topLeft()) * pxPerMm, op.only.isEmpty());
            c.setRenderFilter({});
            p.restore();
            if (s.marksNeeded()) {
                p.save();
                p.scale(pxPerMm, pxPerMm);
                const QString info = QObject::tr("%1  –  %2  –  Seite %3  –  %4")
                                         .arg(base, op.name)
                                         .arg(i + 1)
                                         .arg(QDateTime::currentDateTime().toString(QStringLiteral("dd.MM.yyyy HH:mm")));
                paintMarks(p, QRectF(QPointF(margin, margin), op.trim.size()), bleed, s, info);
                p.restore();
            }
        }
        p.end();
        c.setTextAsGlyphs(false);
        c.setComplexFillsAsBitmaps(false);
    }
    if (!gs)
        return true;

    // 2. Ghostscript: Normalisieren und alle Einstellungen anwenden
    QString pre, post;
    QString iccPath;
    // Bildkomprimierung über die Distiller-Parameter
    if (s.compression == PdfSettings::Compression::Jpeg) {
        const double q = std::clamp(0.05 + (100 - s.jpegQuality) / 100.0 * 1.8, 0.05, 2.5);
        pre += QStringLiteral("<< /ColorImageDict << /QFactor %1 /Blend 1 /HSamples [1 1 1 1] /VSamples [1 1 1 1] >> "
                              "/GrayImageDict << /QFactor %1 /Blend 1 /HSamples [1 1 1 1] /VSamples [1 1 1 1] >> >> setdistillerparams\n")
                   .arg(num(q));
    }
    if (!s.embedBase14)
        pre += QStringLiteral("<< /NeverEmbed [/Courier /Courier-Bold /Courier-Oblique /Courier-BoldOblique /Helvetica /Helvetica-Bold "
                              "/Helvetica-Oblique /Helvetica-BoldOblique /Times-Roman /Times-Bold /Times-Italic /Times-BoldItalic /Symbol "
                              "/ZapfDingbats] >> setdistillerparams\n");
    // Ausgabebedingung für PDF/X und PDF/A (Profile aus Ghostscript)
    const bool cmyk = s.colors == PdfSettings::Colors::Cmyk || s.isPdfX();
    const bool gray = s.colors == PdfSettings::Colors::Gray && !s.isPdfX();
    if (s.isPdfX() || s.isPdfA()) {
        const QString profile = cmyk ? QStringLiteral("default_cmyk.icc") : gray ? QStringLiteral("default_gray.icc") : QStringLiteral("srgb.icc");
        const int n = cmyk ? 4 : gray ? 1 : 3;
        const QString subtype = s.isPdfX() ? QStringLiteral("/GTS_PDFX") : QStringLiteral("/GTS_PDFA1");
        const QString condition = cmyk ? QStringLiteral("CGATS TR 001") : gray ? QStringLiteral("Graustufen") : QStringLiteral("sRGB");
        iccPath = iccProfile(profile);
        pre += QStringLiteral("[/_objdef {icc} /type /stream /OBJ pdfmark\n[{icc} << /N %1 >> /PUT pdfmark\n"
                              "[{icc} (%2) (r) file /PUT pdfmark\n"
                              "[/_objdef {intent} /type /dict /OBJ pdfmark\n"
                              "[{intent} << /Type /OutputIntent /S %3 /DestOutputProfile {icc} /OutputConditionIdentifier (%4) "
                              "/RegistryName (http://www.color.org) /Info (%4) >> /PUT pdfmark\n"
                              "[{Catalog} << /OutputIntents [ {intent} ] >> /PUT pdfmark\n")
                   .arg(n)
                   .arg(iccPath, subtype, condition);
    }
    // Dokumentinfo, Startansicht, Lesezeichen und Skizzen nach den Seiten
    QStringList info{QStringLiteral("/Title ") + psString(QFileInfo(path).completeBaseName()), QStringLiteral("/Creator (TANGENTA)")};
    if (!s.author.isEmpty())
        info << QStringLiteral("/Author ") + psString(s.author);
    if (!s.keywords.isEmpty())
        info << QStringLiteral("/Keywords ") + psString(s.keywords);
    if (!s.subject.isEmpty())
        info << QStringLiteral("/Subject ") + psString(s.subject);
    if (s.isPdfX())
        info << QStringLiteral("/GTS_PDFXVersion (PDF/X-3:2002) /Trapped /False");
    post += QStringLiteral("[ %1 /DOCINFO pdfmark\n").arg(info.join(QLatin1Char(' ')));
    // Endformat- und Anschnittrahmen je Seite (PDF-Punkte, Ursprung unten links); nach den Seiten,
    // ältere Ghostscript-Fassungen lehnen /PAGES vor der ersten Seite ab
    if (margin > 0)
        for (int i = 0; i < pages.size(); ++i) {
            const QSizeF trim = pages[i].trim.size();
            const double m = margin * kPtPerMm, b = (margin - bleed) * kPtPerMm;
            const double w = (trim.width() + 2 * margin) * kPtPerMm, h = (trim.height() + 2 * margin) * kPtPerMm;
            post += QStringLiteral("[ {Page%1} << /TrimBox [%2 %3 %4 %5] /BleedBox [%6 %7 %8 %9] >> /PUT pdfmark\n")
                        .arg(i + 1)
                        .arg(num(m), num(m), num(w - m), num(h - m), num(b), num(b), num(w - b), num(h - b));
        }
    const char* modes[] = {"/UseNone", "/FullScreen", "/UseOutlines", "/UseThumbs"};
    post += QStringLiteral("[ /PageMode %1 /DOCVIEW pdfmark\n").arg(QLatin1String(modes[int(s.startView)]));
    if (s.bookmarks)
        for (int i = 0; i < pages.size(); ++i)
            post += QStringLiteral("[ /Title %1 /Page %2 /OUT pdfmark\n").arg(psString(pages[i].name)).arg(i + 1);
    if (s.thumbnails) {
        // Skizzen: je Seite ein kleines RGB-Bild als Rohdaten, über pdfmark an die Seite gehängt
        for (int i = 0; i < pages.size(); ++i) {
            const OutPage& op = pages[i];
            const double scale = 106.0 / std::max(op.trim.width(), op.trim.height());
            QImage img(QSize(std::max(1, int(op.trim.width() * scale)), std::max(1, int(op.trim.height() * scale))), QImage::Format_RGB888);
            img.fill(Qt::white);
            {
                QPainter tp(&img);
                auto& c = const_cast<Canvas&>(canvas);
                c.setRenderFilter(op.only);
                canvas.renderPage(tp, op.page, scale, -op.trim.topLeft() * scale, true);
                c.setRenderFilter({});
            }
            const QString raw = tmp.filePath(QStringLiteral("thumb%1.raw").arg(i + 1));
            QFile f(raw);
            if (!f.open(QIODevice::WriteOnly))
                continue;
            // Skizze im Farbraum der Ausgabe (PDF/X und CMYK ohne RGB)
            const bool toCmyk = s.colors == PdfSettings::Colors::Cmyk || s.isPdfX();
            const bool toGray = s.colors == PdfSettings::Colors::Gray && !s.isPdfX();
            for (int y = 0; y < img.height(); ++y) {
                const uchar* line = img.constScanLine(y);
                QByteArray row;
                for (int x = 0; x < img.width(); ++x) {
                    const int r = line[3 * x], g = line[3 * x + 1], b = line[3 * x + 2];
                    if (toGray) {
                        row.append(char(qGray(r, g, b)));
                    } else if (toCmyk) {
                        const int k = 255 - std::max({r, g, b});
                        const double d = 255.0 - k;
                        row.append(char(d > 0 ? int((d - r) / d * 255) : 0)).append(char(d > 0 ? int((d - g) / d * 255) : 0));
                        row.append(char(d > 0 ? int((d - b) / d * 255) : 0)).append(char(k));
                    } else {
                        row.append(reinterpret_cast<const char*>(line + 3 * x), 3);
                    }
                }
                f.write(row);
            }
            f.close();
            const QString space = toGray ? QStringLiteral("/DeviceGray") : toCmyk ? QStringLiteral("/DeviceCMYK") : QStringLiteral("/DeviceRGB");
            post += QStringLiteral("[/_objdef {thumb%1} /type /stream /OBJ pdfmark\n"
                                   "[{thumb%1} << /Width %2 /Height %3 /ColorSpace %5 /BitsPerComponent 8 >> /PUT pdfmark\n"
                                   "[{thumb%1} (%4) (r) file /PUT pdfmark\n"
                                   "[{Page%1} << /Thumb {thumb%1} >> /PUT pdfmark\n")
                        .arg(i + 1)
                        .arg(img.width())
                        .arg(img.height())
                        .arg(QDir::fromNativeSeparators(raw), space);
        }
    }
    const QString preFile = tmp.filePath(QStringLiteral("pre.ps")), postFile = tmp.filePath(QStringLiteral("post.ps"));
    for (const auto& [file, text] : {std::pair{preFile, pre}, std::pair{postFile, post}}) {
        QFile f(file);
        if (!f.open(QIODevice::WriteOnly) || f.write(text.toLatin1()) < 0)
            return fail(QObject::tr("Zwischendatei lässt sich nicht schreiben."));
    }
    QStringList args{QStringLiteral("-dNOPAUSE"), QStringLiteral("-dBATCH"), QStringLiteral("-dQUIET"), QStringLiteral("-sDEVICE=pdfwrite"),
                     QStringLiteral("--permit-file-read=") + QDir::fromNativeSeparators(tmp.path()) + QLatin1Char('/'),
                     QStringLiteral("-dCompatibilityLevel=") + s.compatibilityLevel()};
    args << QStringLiteral("-dSAFER");
    if (!iccPath.startsWith(QLatin1Char('%')))
        args << QStringLiteral("--permit-file-read=") + iccPath;
    switch (s.colors) {
    case PdfSettings::Colors::Native:
        args << (s.isPdfX() ? QStringLiteral("-sColorConversionStrategy=CMYK") : QStringLiteral("-sColorConversionStrategy=LeaveColorUnchanged"));
        break;
    case PdfSettings::Colors::Rgb:
        args << QStringLiteral("-sColorConversionStrategy=RGB") << QStringLiteral("-sProcessColorModel=DeviceRGB");
        break;
    case PdfSettings::Colors::Cmyk:
        args << QStringLiteral("-sColorConversionStrategy=CMYK") << QStringLiteral("-sProcessColorModel=DeviceCMYK");
        break;
    case PdfSettings::Colors::Gray:
        args << QStringLiteral("-sColorConversionStrategy=Gray") << QStringLiteral("-sProcessColorModel=DeviceGray");
        break;
    }
    if (s.isPdfX() && s.colors == PdfSettings::Colors::Native)
        args << QStringLiteral("-sProcessColorModel=DeviceCMYK");
    if (s.isPdfX())
        args << QStringLiteral("-dPDFX");
    if (s.compatibility == PdfSettings::Compatibility::PdfA1b)
        args << QStringLiteral("-dPDFA=1") << QStringLiteral("-dPDFACompatibilityPolicy=1");
    if (s.compatibility == PdfSettings::Compatibility::PdfA2b)
        args << QStringLiteral("-dPDFA=2") << QStringLiteral("-dPDFACompatibilityPolicy=1");
    // PDF/X-1a und PDF/A-1 kennen keine Transparenz: Ghostscript rastert sie
    if (s.compatibility == PdfSettings::Compatibility::PdfX1a || s.compatibility == PdfSettings::Compatibility::PdfA1b)
        args << QStringLiteral("-dHaveTransparency=false");
    // Bitmaps
    auto sampling = [&](const QString& kind, bool on, int dpi, const QString& type) {
        args << QStringLiteral("-dDownsample%1Images=%2").arg(kind, on ? QStringLiteral("true") : QStringLiteral("false"));
        if (on)
            args << QStringLiteral("-d%1ImageResolution=%2").arg(kind).arg(dpi) << QStringLiteral("-d%1ImageDownsampleType=%2").arg(kind, type)
                 << QStringLiteral("-d%1ImageDownsampleThreshold=1.0").arg(kind);
    };
    sampling(QStringLiteral("Color"), s.downsampleColor, s.colorDpi, QStringLiteral("/Bicubic"));
    sampling(QStringLiteral("Gray"), s.downsampleGray, s.grayDpi, QStringLiteral("/Bicubic"));
    sampling(QStringLiteral("Mono"), s.downsampleMono, s.monoDpi, QStringLiteral("/Subsample"));
    switch (s.compression) {
    case PdfSettings::Compression::None:
        args << QStringLiteral("-dEncodeColorImages=false") << QStringLiteral("-dEncodeGrayImages=false");
        break;
    case PdfSettings::Compression::Zip:
        args << QStringLiteral("-dAutoFilterColorImages=false") << QStringLiteral("-dAutoFilterGrayImages=false")
             << QStringLiteral("-dColorImageFilter=/FlateEncode") << QStringLiteral("-dGrayImageFilter=/FlateEncode");
        break;
    case PdfSettings::Compression::Jpeg:
        args << QStringLiteral("-dAutoFilterColorImages=false") << QStringLiteral("-dAutoFilterGrayImages=false")
             << QStringLiteral("-dColorImageFilter=/DCTEncode") << QStringLiteral("-dGrayImageFilter=/DCTEncode");
        break;
    }
    // Text und Schriften
    args << QStringLiteral("-dCompressPages=%1").arg(s.compressText ? QStringLiteral("true") : QStringLiteral("false"))
         << QStringLiteral("-dEmbedAllFonts=%1").arg(s.embedFonts ? QStringLiteral("true") : QStringLiteral("false"))
         << QStringLiteral("-dSubsetFonts=%1").arg(s.subsetFonts ? QStringLiteral("true") : QStringLiteral("false"))
         << QStringLiteral("-dMaxSubsetPct=%1").arg(s.subsetBelow);
    if (s.ascii85)
        args << QStringLiteral("-dASCII85EncodePages=true");
    if (s.fastWebView)
        args << QStringLiteral("-dFastWebView=true");
    if (!s.keepOverprint)
        args << QStringLiteral("-dPreserveOverprintSettings=false");
    // Verschlüsselung (nicht bei PDF/X und PDF/A)
    if ((s.openPassword || s.permissionPassword) && !s.isPdfX() && !s.isPdfA()) {
        QString owner = s.permissionPassword ? s.permissionPasswordText : QString();
        if (owner.isEmpty()) // ohne Berechtigungskennwort: zufälliges, damit niemand die Rechte ändert
            owner = QString::number(QRandomGenerator::global()->generate64(), 36);
        args << QStringLiteral("-sOwnerPassword=") + owner << QStringLiteral("-dEncryptionR=3") << QStringLiteral("-dKeyLength=128")
             << QStringLiteral("-dPermissions=%1").arg(permissions(s));
        if (s.openPassword)
            args << QStringLiteral("-sUserPassword=") + s.openPasswordText;
    }
    args << QStringLiteral("-sOutputFile=") + path << preFile << stage1 << postFile;
    // Fehlersuche: Zwischenstufen und Aufruf aufbewahren
    if (const QString keep = qEnvironmentVariable("VEK_PDF_KEEP"); !keep.isEmpty()) {
        QDir().mkpath(keep);
        for (const QString& f : {preFile, stage1, postFile})
            QFile::copy(f, QDir(keep).filePath(QFileInfo(f).fileName()));
        QFile list(QDir(keep).filePath(QStringLiteral("args.txt")));
        if (list.open(QIODevice::WriteOnly))
            list.write(args.join(QLatin1Char('\n')).toUtf8());
    }
    QString gsError;
    if (!pdf::runGhostscript(args, &gsError))
        return fail(gsError);
    return true;
}

} // namespace vek::pdfexport
