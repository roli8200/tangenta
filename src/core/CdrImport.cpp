#include "CdrImport.h"

#include "CurveObject.h"
#include "FontMatch.h"
#include "GroupObject.h"
#include "ImageObject.h"
#include "ImagePool.h"
#include "Ramp.h"
#include "SuperClipObject.h"
#include "TextObject.h"

#include <libcdr/libcdr.h>
#include <librevenge-stream/librevenge-stream.h>
#include <librevenge/librevenge.h>

#include <QBuffer>
#include <QFont>
#include <QFontMetricsF>
#include <QHash>
#include <QImage>
#include <QPainterPath>
#include <QTransform>
#include <QtMath>
#include <QtEndian>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>

namespace librevenge {
// in third_party/cdr-support/patches/patch.cmake ergänzt: Bilddaten ohne Base64
const RVNGBinaryData* tangentaBinaryData(const RVNGProperty* property);
}

namespace vek::cdr {

namespace {

using Props = librevenge::RVNGPropertyList;

constexpr double kMm = 25.4; // libcdr liefert Längen in Zoll

double num(const Props& p, const char* key, double fallback = 0.0)
{
    const librevenge::RVNGProperty* v = p[key];
    return v ? v->getDouble() : fallback;
}

QString str(const Props& p, const char* key)
{
    const librevenge::RVNGProperty* v = p[key];
    return v ? QString::fromUtf8(v->getStr().cstr()) : QString();
}

bool flag(const Props& p, const char* key)
{
    const librevenge::RVNGProperty* v = p[key];
    return v && v->getInt() != 0;
}

std::optional<Color> color(const QString& hex, double opacity = 1.0)
{
    std::optional<Color> c = Color::fromHex(hex);
    if (c && opacity < 1.0)
        c->a = std::uint8_t(std::lround(std::clamp(opacity, 0.0, 1.0) * 255.0));
    return c;
}

QByteArray binary(const Props& p, const char* key)
{
    const librevenge::RVNGProperty* v = p[key];
    if (!v)
        return {};
    if (const librevenge::RVNGBinaryData* d = librevenge::tangentaBinaryData(v))
        return QByteArray(reinterpret_cast<const char*>(d->getDataBuffer()), qsizetype(d->size()));
    return QByteArray::fromBase64(QByteArray(v->getStr().cstr()));
}

// libcdr liefert Bilder als BMP mit 32 Bit je Pixel; eine Maske aus der Datei steht im vierten
// Byte (siehe bitmap-mask-apply.inc), sonst ist es überall 0. Qt liest den Alphakanal eines
// solchen BMP nicht, darum werden maskierte Bilder als PNG übernommen.
QByteArray withMask(const QByteArray& bmp)
{
    if (bmp.size() < 54 || !bmp.startsWith("BM"))
        return bmp;
    const auto* data = reinterpret_cast<const uchar*>(bmp.constData());
    const qint64 offset = qFromLittleEndian<quint32>(data + 10);
    const int width = qFromLittleEndian<qint32>(data + 18);
    const int height = qFromLittleEndian<qint32>(data + 22);
    if (qFromLittleEndian<quint16>(data + 28) != 32 || qFromLittleEndian<quint32>(data + 30) != 0 || width <= 0
        || height <= 0 || offset + qint64(width) * height * 4 > bmp.size())
        return bmp;
    const uchar* pixels = data + offset;
    bool masked = false;
    for (qint64 i = 0; i < qint64(width) * height && !masked; ++i)
        masked = pixels[i * 4 + 3] != 0;
    if (!masked)
        return bmp;
    QImage image(width, height, QImage::Format_ARGB32);
    for (int y = 0; y < height; ++y) // BMP: unterste Zeile zuerst; Bytes B, G, R, A wie ARGB32
        std::memcpy(image.scanLine(y), pixels + qint64(height - 1 - y) * width * 4, std::size_t(width) * 4);
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return png;
}

// Zeichnet libcdr in TANGENTA-Objekte: jede Seite eine Gruppe, Gruppen der Zeichnung als Gruppen
class Collector final : public librevenge::RVNGDrawingInterface {
public:
    std::vector<svg::Import> pages;
    std::vector<ImportedFont> fonts;

    void startDocument(const Props&) override {}
    void endDocument() override {}
    void setDocumentMetaData(const Props&) override {}
    void defineEmbeddedFont(const Props&) override {}

    void startPage(const Props& p) override
    {
        svg::Import page;
        page.group = std::make_unique<GroupObject>();
        page.size = QSizeF(num(p, "svg:width") * kMm, num(p, "svg:height") * kMm);
        pages.push_back(std::move(page));
        m_open.clear();
    }
    void endPage() override { closeAll(); }
    // Masterseiten zeichnet libcdr nicht gesondert; falls doch, landen sie auf der aktuellen Seite nicht
    void startMasterPage(const Props&) override { ++m_skip; }
    void endMasterPage() override { --m_skip; }

    void setStyle(const Props& p) override { m_style = p; }

    // Gruppen kommen als Ebenen. PowerClip (powerclip-collect.inc): erstes Kind ist der Rahmen,
    // danach die Inhalte
    void startLayer(const Props& p) override
    {
        if (p["tangenta:powerclip"])
            m_open.push_back(std::make_unique<SuperClipObject>());
        else
            m_open.push_back(std::make_unique<GroupObject>());
    }
    void endLayer() override { closeOne(); }
    void openGroup(const Props& p) override { startLayer(p); }
    void closeGroup() override { closeOne(); }
    void startEmbeddedGraphics(const Props&) override {}
    void endEmbeddedGraphics() override {}

    void drawRectangle(const Props&) override {}
    void drawEllipse(const Props&) override {}
    void drawPolygon(const Props& p) override { drawPoints(p, true); }
    void drawPolyline(const Props& p) override { drawPoints(p, false); }
    void drawConnector(const Props&) override {}

    void drawPath(const Props& p) override
    {
        const librevenge::RVNGPropertyListVector* d = p.child("svg:d");
        if (!d || !parent())
            return;
        // Als SVG-Pfaddaten in mm aufschreiben; der SVG-Leser kennt auch Bögen
        QString data;
        auto xy = [&](const Props& e, const char* x, const char* y) {
            return QStringLiteral("%1 %2 ").arg(num(e, x) * kMm, 0, 'g', 10).arg(num(e, y) * kMm, 0, 'g', 10);
        };
        for (unsigned long i = 0; i < d->count(); ++i) {
            const Props& e = (*d)[i];
            const QString action = str(e, "librevenge:path-action");
            if (action == QLatin1String("M") || action == QLatin1String("L")) {
                data += action + xy(e, "svg:x", "svg:y");
            } else if (action == QLatin1String("C")) {
                data += QStringLiteral("C") + xy(e, "svg:x1", "svg:y1") + xy(e, "svg:x2", "svg:y2") + xy(e, "svg:x", "svg:y");
            } else if (action == QLatin1String("Q")) {
                data += QStringLiteral("Q") + xy(e, "svg:x1", "svg:y1") + xy(e, "svg:x", "svg:y");
            } else if (action == QLatin1String("A")) {
                data += QStringLiteral("A%1 %2 %3 %4 %5 ")
                            .arg(num(e, "svg:rx") * kMm, 0, 'g', 10)
                            .arg(num(e, "svg:ry") * kMm, 0, 'g', 10)
                            .arg(num(e, "librevenge:rotate"), 0, 'g', 10)
                            .arg(flag(e, "librevenge:large-arc") ? 1 : 0)
                            .arg(flag(e, "librevenge:sweep") ? 1 : 0)
                        + xy(e, "svg:x", "svg:y");
            } else if (action == QLatin1String("Z")) {
                data += QStringLiteral("Z ");
            }
        }
        QPainterPath path = svg::parsePathData(data);
        addShape(path);
    }

    void drawGraphicObject(const Props& p) override
    {
        if (!parent())
            return;
        const QString key = imagepool::add(withMask(binary(p, "office:binary-data")));
        const double x = num(p, "svg:x") * kMm, y = num(p, "svg:y") * kMm;
        const double w = num(p, "svg:width") * kMm, h = num(p, "svg:height") * kMm;
        if (key.isEmpty())
            return;
        if (w <= 0 || h <= 0)
            return;
        auto image = std::make_unique<ImageObject>(key, QSizeF(w, h));
        // Spiegeln und Drehen um die Mitte, wie libcdr es meint (Grad im Uhrzeigersinn)
        const QPointF mid(x + w / 2.0, y + h / 2.0);
        QTransform rotate;
        rotate.rotate(num(p, "librevenge:rotate"));
        image->setTransform(QTransform::fromTranslate(x, y) * QTransform::fromTranslate(-mid.x(), -mid.y()) * rotate
                            * QTransform::fromScale(flag(p, "draw:mirror-horizontal") ? -1 : 1, flag(p, "draw:mirror-vertical") ? -1 : 1)
                            * QTransform::fromTranslate(mid.x(), mid.y()));
        applyOpacity(*image);
        attachDetachedChild(*parent(), std::move(image));
    }

    // --- Text: ein Rahmen mit Absätzen und Läufen ---------------------------------------
    void startTextObject(const Props& p) override
    {
        m_textFrame = QRectF(num(p, "svg:x") * kMm, num(p, "svg:y") * kMm, num(p, "svg:width") * kMm, num(p, "svg:height") * kMm);
        // Abbildung lokal (Zoll, y nach oben) -> Seite (Zoll, y nach unten), siehe text-transform.inc;
        // hier umgerechnet auf TANGENTA-Koordinaten (mm, y nach unten)
        m_textMatrix.reset();
        m_textBox.reset();
        if (p["tangenta:m11"]) {
            m_textMatrix = QTransform(num(p, "tangenta:m11"), num(p, "tangenta:m12"), -num(p, "tangenta:m21"),
                                      -num(p, "tangenta:m22"), num(p, "tangenta:dx") * kMm, num(p, "tangenta:dy") * kMm);
            if (p["tangenta:box-w"])
                m_textBox = QRectF(num(p, "tangenta:box-x") * kMm, -num(p, "tangenta:box-y") * kMm,
                                   num(p, "tangenta:box-w") * kMm, num(p, "tangenta:box-h") * kMm);
        }
        m_paragraphs.clear();
        m_shifts.clear();
        m_textStroke.reset();
        m_textStrokeWidth = 0.0;
        m_textNoFill = false;
        m_inText = true;
    }
    void endTextObject() override
    {
        m_inText = false;
        if (!parent())
            return;
        bool any = false;
        double largest = 0.0;
        for (const TextParagraph& par : std::as_const(m_paragraphs))
            for (const TextRun& run : par.runs) {
                any = any || !run.text.trimmed().isEmpty();
                largest = std::max(largest, run.format.sizePt);
            }
        if (!any)
            return;
        // Läufe ohne Füllung haben keine eigene Farbe und zeigen die (leere) Füllung des Objekts;
        // die Kontur der Zeichen wird die Kontur des Textobjekts
        vek::Style st;
        st.fill = m_textNoFill ? Fill::none() : Fill::solid(Color(0, 0, 0));
        st.stroke = m_textStroke;
        if (m_textStroke)
            st.strokeWidth = std::max(m_textStrokeWidth * kMm, 0.076);
        if (shiftedLine()) {
            auto group = std::make_unique<GroupObject>();
            for (std::unique_ptr<TextObject>& piece : shiftedPieces()) {
                piece->setStyle(st);
                applyOpacity(*piece);
                TextObject* raw = piece.get();
                attachDetachedChild(*group, std::move(piece));
                for (const TextRun& run : raw->paragraphs().first().runs)
                    registerFont(run.format, m_rawFont.value(run.format.family, run.format.family), raw);
            }
            attachDetachedChild(*parent(), std::move(group));
            return;
        }
        mergeRuns();
        std::unique_ptr<TextObject> text = m_textMatrix ? placedText(largest) : framedText(largest);
        text->setStyle(st);
        applyOpacity(*text);
        TextObject* raw = text.get();
        attachDetachedChild(*parent(), std::move(text));
        for (const TextParagraph& par : std::as_const(m_paragraphs))
            for (const TextRun& run : par.runs)
                registerFont(run.format, m_rawFont.value(run.format.family, run.format.family), raw);
    }

    // Horizontal verschobene Zeichen (z. B. Tageszahlen eines Kalenders als ein Text, jede Zahl in
    // ihrer Spalte): Nur eine Zeile Grafiktext wird zerlegt, Mengentext behält die Läufe ohne Verschiebung
    bool shiftedLine() const
    {
        if (!m_textMatrix || m_textBox || m_paragraphs.size() != 1 || m_shifts.size() != 1)
            return false;
        return std::any_of(m_shifts.first().cbegin(), m_shifts.first().cend(), [](double s) { return std::abs(s) > 0.01; });
    }

    // Ohne Verschiebung gehören gleich formatierte Nachbarläufe wieder zusammen
    void mergeRuns()
    {
        for (TextParagraph& par : m_paragraphs) {
            QList<TextRun> merged;
            for (const TextRun& run : std::as_const(par.runs)) {
                if (!merged.isEmpty() && merged.last().format == run.format)
                    merged.last().text += run.text;
                else
                    merged << run;
            }
            par.runs = merged;
        }
    }

    static QFont runFont(const CharFormat& format)
    {
        const QString installed = fontmatch::installedFamily(format.family);
        QFont font(installed.isEmpty() ? format.family : installed);
        font.setPixelSize(std::max(1, int(std::lround(format.sizePt * 25.4 / 72.0 * TextObject::kUnitsPerMm))));
        font.setBold(format.bold);
        font.setItalic(format.italic);
        return font;
    }

    // Je Lauf ein Grafiktext an seiner natürlichen Stelle plus Verschiebung (Anteil der Schriftgrösse),
    // alle auf derselben Grundlinie
    std::vector<std::unique_ptr<TextObject>> shiftedPieces() const
    {
        const TextParagraph& par = m_paragraphs.first();
        const QList<double>& shifts = m_shifts.first();
        std::vector<double> advances;
        double total = 0.0;
        for (const TextRun& run : par.runs) {
            advances.push_back(QFontMetricsF(runFont(run.format)).horizontalAdvance(run.text) / TextObject::kUnitsPerMm);
            total += advances.back();
        }
        double originX = 0.0;
        if (par.align == TextParagraph::Align::Center)
            originX = total / 2.0;
        else if (par.align == TextParagraph::Align::Right)
            originX = total;
        std::vector<std::unique_ptr<TextObject>> pieces;
        double x = 0.0;
        for (qsizetype i = 0; i < par.runs.size(); ++i) {
            const TextRun& run = par.runs[i];
            if (!run.text.trimmed().isEmpty()) {
                TextParagraph single;
                single.runs << run;
                auto piece = std::make_unique<TextObject>(TextObject::Kind::Artistic);
                piece->setParagraphs({single});
                const double ascent = QFontMetricsF(runFont(run.format)).ascent() / TextObject::kUnitsPerMm;
                const double em = run.format.sizePt * 25.4 / 72.0;
                piece->setTransform(QTransform::fromTranslate(x + shifts.value(i) * em - originX - piece->frameRect().left(), -ascent)
                                    * *m_textMatrix);
                pieces.push_back(std::move(piece));
            }
            x += advances[size_t(i)];
        }
        return pieces;
    }
    // Ältere Ausgabe ohne Abbildung: Rahmen achsenparallel aus den Seitenmassen
    std::unique_ptr<TextObject> framedText(double largest) const
    {
        auto text = std::make_unique<TextObject>(TextObject::Kind::Paragraph);
        text->setParagraphs(m_paragraphs);
        // Die Masse stammen aus der Zeichnung; etwas Luft, damit andere Schriftmetrik nicht umbricht
        // oder Zeilen versteckt
        const double lineMm = largest * 25.4 / 72.0 * 1.3;
        const double needed = lineMm * double(m_paragraphs.size());
        text->setFrameSize(QSizeF(std::max(m_textFrame.width() * widthSlack(), lineMm), std::max(m_textFrame.height(), needed) * 1.15));
        text->setTransform(QTransform::fromTranslate(m_textFrame.left(), m_textFrame.top()));
        return text;
    }

    // Luft in der Rahmenbreite: Mit den Originalschriften bricht der Text genau wie in der Datei um
    // (sonst passt z. B. eine Adresszeile knapp und läuft in die Nachbarspalte); nur Ersatzschriften
    // mit anderer Laufweite bekommen etwas mehr Platz, damit sie nicht zusätzlich umbrechen
    double widthSlack() const
    {
        for (const TextParagraph& par : std::as_const(m_paragraphs))
            for (const TextRun& run : par.runs)
                if (!run.text.trimmed().isEmpty() && fontmatch::installedFamily(run.format.family).isEmpty())
                    return 1.04;
        return 1.002;
    }

    // Text mit der Abbildung aus der Datei: Drehung, Stauchung und Spiegelung bleiben erhalten.
    // Mengentext bekommt seinen lokalen Rahmen, Grafiktext steht mit der ersten Grundlinie am
    // Ursprung (links, Mitte oder rechts je nach Ausrichtung).
    std::unique_ptr<TextObject> placedText(double largest) const
    {
        if (m_textBox) {
            auto text = std::make_unique<TextObject>(TextObject::Kind::Paragraph);
            text->setParagraphs(m_paragraphs);
            const double lineMm = largest * 25.4 / 72.0 * 1.3;
            const double needed = lineMm * double(m_paragraphs.size());
            text->setFrameSize(QSizeF(std::max(m_textBox->width() * widthSlack(), lineMm), std::max(m_textBox->height(), needed) * 1.15));
            text->setTransform(QTransform::fromTranslate(m_textBox->left(), m_textBox->top()) * *m_textMatrix);
            return text;
        }
        auto text = std::make_unique<TextObject>(TextObject::Kind::Artistic);
        text->setParagraphs(m_paragraphs);
        const QRectF block = text->frameRect();
        double ascent = 0.0;
        for (const TextRun& run : m_paragraphs.first().runs) {
            const QString installed = fontmatch::installedFamily(run.format.family);
            QFont font(installed.isEmpty() ? run.format.family : installed);
            font.setPixelSize(std::max(1, int(std::lround(run.format.sizePt * 25.4 / 72.0 * TextObject::kUnitsPerMm))));
            font.setBold(run.format.bold);
            font.setItalic(run.format.italic);
            ascent = std::max(ascent, QFontMetricsF(font).ascent() / TextObject::kUnitsPerMm);
        }
        double originX = block.left();
        const TextParagraph::Align align = m_paragraphs.first().align;
        if (align == TextParagraph::Align::Center)
            originX = block.center().x();
        else if (align == TextParagraph::Align::Right)
            originX = block.right();
        QTransform local = QTransform::fromTranslate(-originX, -ascent);
        // Ersatzschriften laufen oft breiter: ungedrehten Text auf die Breite in der Datei stauchen
        const bool upright = std::abs(m_textMatrix->m12()) < 1e-6 && std::abs(m_textMatrix->m21()) < 1e-6;
        const double shown = block.width() * std::abs(m_textMatrix->m11());
        if (upright && m_textFrame.width() > 0.0 && shown > m_textFrame.width() * 1.02)
            local *= QTransform::fromScale(m_textFrame.width() / shown, 1.0);
        text->setTransform(local * *m_textMatrix);
        return text;
    }

    void openParagraph(const Props& p) override
    {
        m_paragraph = TextParagraph();
        m_runShifts.clear();
        const QString align = str(p, "fo:text-align");
        if (align == QLatin1String("center"))
            m_paragraph.align = TextParagraph::Align::Center;
        else if (align == QLatin1String("end") || align == QLatin1String("right"))
            m_paragraph.align = TextParagraph::Align::Right;
        else if (align == QLatin1String("justify") || align == QLatin1String("full"))
            m_paragraph.align = TextParagraph::Align::Justify;
    }
    void closeParagraph() override
    {
        if (m_paragraph.runs.isEmpty()) {
            m_paragraph.runs << TextRun{QString(), m_format};
            m_runShifts << 0.0;
        }
        m_paragraphs << m_paragraph;
        m_shifts << m_runShifts;
        m_paragraph = TextParagraph();
        m_runShifts.clear();
    }
    void openSpan(const Props& p) override
    {
        const QString raw = str(p, "style:font-name");
        bool bold = false, italic = false;
        QString family = raw.isEmpty() ? QStringLiteral("Arial") : fontmatch::familyFromBaseName(raw, &bold, &italic);
        if (family.isEmpty())
            family = raw;
        m_rawFont.insert(family, raw.isEmpty() ? family : raw);
        const double size = num(p, "fo:font-size", 12.0); // in Punkt
        const bool noFill = flag(p, "tangenta:fill-none");
        m_format = CharFormat(family, size > 0 ? size : 12.0, bold || str(p, "fo:font-weight") == QLatin1String("bold"),
                              italic || str(p, "fo:font-style") == QLatin1String("italic"), false,
                              noFill ? std::nullopt : std::optional<Color>(color(str(p, "fo:color")).value_or(Color(0, 0, 0))));
        m_textNoFill = m_textNoFill || noFill;
        m_shift = num(p, "tangenta:h-shift");
        // Breite 0 heisst bei Text: keine Kontur (so steht es auch bei gewöhnlichem Text in der Datei)
        if (const auto c = color(str(p, "tangenta:stroke-color")); c && !m_textStroke && num(p, "tangenta:stroke-width") > 0.0) {
            m_textStroke = *c;
            m_textStrokeWidth = num(p, "tangenta:stroke-width");
        }
    }
    void closeSpan() override {}
    void insertText(const librevenge::RVNGString& text) override { append(QString::fromUtf8(text.cstr())); }
    void insertTab() override { append(QStringLiteral("\t")); }
    void insertSpace() override { append(QStringLiteral(" ")); }
    void insertLineBreak() override
    {
        // Zeilenumbruch: neuer Absatz mit derselben Ausrichtung
        const TextParagraph::Align align = m_paragraph.align;
        closeParagraph();
        m_paragraph.align = align;
    }
    void insertField(const Props&) override {}
    void openOrderedListLevel(const Props&) override {}
    void openUnorderedListLevel(const Props&) override {}
    void closeOrderedListLevel() override {}
    void closeUnorderedListLevel() override {}
    void openListElement(const Props& p) override { openParagraph(p); }
    void closeListElement() override { closeParagraph(); }
    void defineParagraphStyle(const Props&) override {}
    void defineCharacterStyle(const Props&) override {}
    void openLink(const Props&) override {}
    void closeLink() override {}

    void startTableObject(const Props&) override {}
    void openTableRow(const Props&) override {}
    void closeTableRow() override {}
    void openTableCell(const Props&) override {}
    void closeTableCell() override {}
    void insertCoveredTableCell(const Props&) override {}
    void endTableObject() override {}

private:
    Node* parent() const
    {
        if (m_skip > 0 || pages.empty())
            return nullptr;
        return m_open.empty() ? pages.back().group.get() : m_open.back().get();
    }

    void closeOne()
    {
        if (m_open.empty())
            return;
        std::unique_ptr<DrawObject> group = std::move(m_open.back());
        m_open.pop_back();
        Node* target = parent();
        if (target && group->childCount() > 0)
            attachDetachedChild(*target, std::move(group));
    }

    void closeAll()
    {
        while (!m_open.empty())
            closeOne();
    }

    void append(const QString& s)
    {
        if (!m_inText)
            return;
        if (!m_paragraph.runs.isEmpty() && m_paragraph.runs.last().format == m_format && m_runShifts.last() == m_shift) {
            m_paragraph.runs.last().text += s;
        } else {
            m_paragraph.runs << TextRun{s, m_format};
            m_runShifts << m_shift;
        }
    }

    void drawPoints(const Props& p, bool closed)
    {
        const librevenge::RVNGPropertyListVector* points = p.child("svg:points");
        if (!points || points->count() < 2)
            return;
        QPainterPath path;
        for (unsigned long i = 0; i < points->count(); ++i) {
            const QPointF pt(num((*points)[i], "svg:x") * kMm, num((*points)[i], "svg:y") * kMm);
            i == 0 ? path.moveTo(pt) : path.lineTo(pt);
        }
        if (closed)
            path.closeSubpath();
        addShape(path);
    }

    void addShape(QPainterPath path)
    {
        if (path.isEmpty() || !parent())
            return;
        path.setFillRule(str(m_style, "svg:fill-rule") == QLatin1String("nonzero") ? Qt::WindingFill : Qt::OddEvenFill);
        std::unique_ptr<CurveObject> curve = CurveObject::fromPath(path);
        if (curve->subPaths().isEmpty())
            return;
        curve->setStyle(style(curve->localPath().boundingRect()));
        applyOpacity(*curve);
        attachDetachedChild(*parent(), std::move(curve));
    }

    void applyOpacity(DrawObject& o) const
    {
        const double opacity = num(m_style, "draw:opacity", 1.0);
        if (opacity < 1.0)
            o.setEffect(Effect::Transparency, Ramp::uniform((1.0 - std::clamp(opacity, 0.0, 1.0)) * 100.0));
        if (const std::optional<Ramp> ramp = transparency(o))
            o.setEffect(Effect::Transparency, *ramp);
        if (int(num(m_style, "tangenta:transparency-kind")) == 5)
            o.setBlendMode(blendMode(int(num(m_style, "tangenta:transparency-mode"))));
    }

    // Verlauf der Transparenz aus einer Füllung mit Grauwerten (transparency-output.inc): Geometrie wie
    // bei Farbverläufen im Begrenzungsrechteck auf der Seite; die Ramp kennt nur Anfang und Ende,
    // also gelten der erste und der letzte Wert (radial: erster Wert aussen)
    std::optional<Ramp> transparencyGradient(const DrawObject& o) const
    {
        const librevenge::RVNGPropertyListVector* v = m_style.child("tangenta:transparency-stops");
        if (!v || v->count() < 2)
            return std::nullopt;
        const double first = std::clamp(num((*v)[0], "tangenta:transparency"), 0.0, 100.0);
        const double last = std::clamp(num((*v)[v->count() - 1], "tangenta:transparency"), 0.0, 100.0);
        const QRectF page = o.path().boundingRect();
        const QRectF bounds = o.localPath().boundingRect();
        if (page.width() <= 0.0 || page.height() <= 0.0 || bounds.width() <= 0.0 || bounds.height() <= 0.0)
            return Ramp::uniform((first + last) / 2.0);
        const QTransform toLocal = o.transform().inverted();
        auto rel = [&](const QPointF& p) {
            const QPointF l = toLocal.map(p);
            return QPointF((l.x() - bounds.left()) / bounds.width(), (l.y() - bounds.top()) / bounds.height());
        };
        const int type = int(num(m_style, "tangenta:transparency-gradient-type"));
        const double angle = qDegreesToRadians(num(m_style, "tangenta:transparency-gradient-angle"));
        const double edge = std::clamp(num(m_style, "tangenta:transparency-gradient-edge"), 0.0, 49.0) / 100.0;
        const QPointF dir(std::cos(angle), -std::sin(angle));
        Ramp r;
        if (type != 2 && type != 3 && type != 4) {
            const double half = (std::abs(page.width() * dir.x()) + std::abs(page.height() * dir.y())) / 2.0 * (1.0 - 2.0 * edge);
            r.type = Ramp::Type::Linear;
            r.from = first;
            r.to = last;
            r.start = rel(page.center() - dir * half);
            r.end = rel(page.center() + dir * half);
            return r;
        }
        const QPointF m(page.center().x() + num(m_style, "tangenta:transparency-gradient-cx") / 100.0 * page.width() / 2.0,
                        page.center().y() - num(m_style, "tangenta:transparency-gradient-cy") / 100.0 * page.height() / 2.0);
        double radius = 0.0;
        for (const QPointF& corner : {page.topLeft(), page.topRight(), page.bottomLeft(), page.bottomRight()})
            radius = std::max(radius, std::hypot(corner.x() - m.x(), corner.y() - m.y()));
        r.start = rel(m);
        if (type == 3) {
            r.type = Ramp::Type::Conical;
            r.from = first;
            r.to = last;
            r.end = rel(m + dir * radius);
        } else {
            r.type = Ramp::Type::Radial;
            r.from = last;
            r.to = first;
            r.end = rel(m + QPointF(radius * (1.0 - edge), 0.0));
        }
        return r;
    }

    // Mischmodus der Transparenz; Nummern wie in der Skript-Schnittstelle des Originals (0-24).
    // Die später hinzugekommenen in der Reihenfolge des Menüs; 28 (Bildschirm) und 29 (Überlagern)
    // sind an Dateien mit Vorschaubild geprüft, die übrigen folgen daraus
    static BlendMode blendMode(int mode)
    {
        switch (mode) {
        case 1: return BlendMode::And;
        case 2: return BlendMode::Or;
        case 3: return BlendMode::Xor;
        case 6: return BlendMode::Invert;
        case 7: return BlendMode::Add;
        case 8: return BlendMode::Subtract;
        case 9: return BlendMode::Multiply;
        case 10: return BlendMode::Divide;
        case 11: return BlendMode::Lighten;
        case 12: return BlendMode::Darken;
        case 13: return BlendMode::Multiply; // Texturieren
        case 14: return BlendMode::Color;
        case 15: return BlendMode::Hue;
        case 16: return BlendMode::Saturation;
        case 17: return BlendMode::Luminosity;
        case 18: return BlendMode::Red;
        case 19: return BlendMode::Green;
        case 20: return BlendMode::Blue;
        case 24: return BlendMode::Difference;
        case 27: return BlendMode::Behind;
        case 28: return BlendMode::Screen;
        case 29: return BlendMode::Overlay;
        case 30: return BlendMode::SoftLight;
        case 31: return BlendMode::HardLight;
        case 32: return BlendMode::ColorDodge;
        case 33: return BlendMode::ColorBurn;
        case 34: return BlendMode::Exclusion;
        default: return BlendMode::Normal;
        }
    }

    // Transparenz aus der Datei (transparency-read.inc): gleichmässig oder als linearer Verlauf
    // zwischen zwei Anfassern in Seitenkoordinaten; die Ramp rechnet relativ zur Grundform
    std::optional<Ramp> transparency(const DrawObject& o) const
    {
        const int kind = int(num(m_style, "tangenta:transparency-kind"));
        const double from = std::clamp(num(m_style, "tangenta:transparency-from"), 0.0, 100.0);
        const double to = std::clamp(num(m_style, "tangenta:transparency-to"), 0.0, 100.0);
        if (kind == 1) {
            if (int(num(m_style, "tangenta:transparency-subtype")) != 0 || from <= 0.0)
                return std::nullopt;
            return Ramp::uniform(from);
        }
        if (kind != 5)
            return std::nullopt;
        // Die lokalen Anfasser mit der Abbildung des Objekts treffen; die Punkte "in der Zeichnung"
        // (tangenta:transparency-x1 …) sind das Begrenzungsrechteck
        if (!m_style["tangenta:transparency-lx1"] && m_style.child("tangenta:transparency-stops"))
            return transparencyGradient(o);
        if (!m_style["tangenta:transparency-lx1"])
            return from > 0.0 ? std::optional<Ramp>(Ramp::uniform(from)) : std::nullopt;
        const QPointF a(num(m_style, "tangenta:transparency-lx1") * kMm, num(m_style, "tangenta:transparency-ly1") * kMm);
        const QPointF b(num(m_style, "tangenta:transparency-lx2") * kMm, num(m_style, "tangenta:transparency-ly2") * kMm);
        const QRectF bounds = o.localPath().boundingRect();
        if (bounds.width() <= 0.0 || bounds.height() <= 0.0)
            return Ramp::uniform((from + to) / 2.0);
        const QTransform toLocal = o.transform().inverted();
        auto rel = [&](const QPointF& p) {
            const QPointF l = toLocal.map(p);
            return QPointF((l.x() - bounds.left()) / bounds.width(), (l.y() - bounds.top()) / bounds.height());
        };
        Ramp r;
        r.type = Ramp::Type::Linear;
        r.from = from;
        r.to = to;
        r.start = rel(a);
        r.end = rel(b);
        return r;
    }

    // Maschenfüllung (mesh-output.inc): Knoten zeilenweise auf der Seite, hier relativ zum
    // Begrenzungsrechteck der Grundform
    std::optional<Fill> meshFill(const QRectF& bounds) const
    {
        const librevenge::RVNGPropertyListVector* nodes = m_style.child("tangenta:mesh-nodes");
        const int rows = int(num(m_style, "tangenta:mesh-rows")), columns = int(num(m_style, "tangenta:mesh-columns"));
        if (!nodes || rows < 2 || columns < 2 || nodes->count() != unsigned(rows * columns) || bounds.width() <= 0.0
            || bounds.height() <= 0.0)
            return std::nullopt;
        MeshFill mesh;
        mesh.rows = rows - 1;
        mesh.columns = columns - 1;
        for (unsigned long i = 0; i < nodes->count(); ++i) {
            const Props& n = (*nodes)[i];
            const QPointF p(num(n, "svg:x") * kMm, num(n, "svg:y") * kMm);
            mesh.points.append(QPointF((p.x() - bounds.left()) / bounds.width(), (p.y() - bounds.top()) / bounds.height()));
            mesh.colors.append(color(str(n, "tangenta:color")).value_or(Color()));
        }
        if (!mesh.isValid())
            return std::nullopt;
        return Fill::meshFill(mesh);
    }

    vek::Style style(const QRectF& bounds = QRectF()) const
    {
        vek::Style st;
        const QString fill = str(m_style, "draw:fill");
        if (const std::optional<Fill> mesh = meshFill(bounds)) {
            st.fill = *mesh;
        } else if (fill == QLatin1String("solid")) {
            if (const auto c = color(str(m_style, "draw:fill-color")))
                st.fill = Fill::solid(*c);
        } else if (fill == QLatin1String("gradient")) {
            st.fill = bounds.isValid() && m_style["tangenta:gradient-type"] ? cdrGradient(bounds) : gradient();
        } else if (fill == QLatin1String("bitmap")) {
            st.fill = bitmapFill();
        } else {
            st.fill = Fill::none();
        }
        const QString stroke = str(m_style, "draw:stroke");
        const std::optional<Color> strokeColor = color(str(m_style, "svg:stroke-color"), num(m_style, "svg:stroke-opacity", 1.0));
        if (stroke.isEmpty() || stroke == QLatin1String("none") || !strokeColor) {
            st.stroke.reset();
        } else {
            st.stroke = *strokeColor;
            // Breite 0 ist in der Zeichnung eine Haarlinie
            st.strokeWidth = std::max(num(m_style, "svg:stroke-width") * kMm, 0.076);
            const QString cap = str(m_style, "svg:stroke-linecap"), join = str(m_style, "svg:stroke-linejoin");
            st.cap = cap == QLatin1String("round") ? vek::Style::Cap::Round : cap == QLatin1String("square") ? vek::Style::Cap::Square : vek::Style::Cap::Flat;
            st.join = join == QLatin1String("round") ? vek::Style::Join::Round : join == QLatin1String("bevel") ? vek::Style::Join::Bevel : vek::Style::Join::Miter;
            if (stroke == QLatin1String("dash"))
                st.dash = 1;
        }
        return st;
    }

    // Verlauf wie im SVG-Generator von librevenge: draw:angle gegen den Uhrzeigersinn,
    // 0 = von oben nach unten; Radialverlauf mit Mittelpunkt svg:cx/svg:cy und Radius 1 - draw:border
    Fill gradient() const
    {
        const QString kind = str(m_style, "draw:style");
        QList<GradientStop> stops;
        if (const librevenge::RVNGPropertyListVector* v = m_style.child("svg:linearGradient")) {
            for (unsigned long i = 0; i < v->count(); ++i) {
                const Props& s = (*v)[i];
                if (const auto c = color(str(s, "svg:stop-color"), num(s, "svg:stop-opacity", 1.0)))
                    stops << GradientStop{std::clamp(num(s, "svg:offset"), 0.0, 1.0), *c};
            }
        }
        const bool radial = kind == QLatin1String("radial") || kind == QLatin1String("square") || kind == QLatin1String("rectangular")
                            || kind == QLatin1String("ellipsoid");
        const std::optional<Color> from = color(str(m_style, "draw:start-color")), to = color(str(m_style, "draw:end-color"));
        if (stops.isEmpty() && from && to) {
            if (radial)
                stops = {GradientStop{0.0, *to}, GradientStop{1.0, *from}};
            else if (kind == QLatin1String("axial"))
                stops = {GradientStop{0.0, *to}, GradientStop{0.5, *from}, GradientStop{1.0, *to}};
            else
                stops = {GradientStop{0.0, *from}, GradientStop{1.0, *to}};
        }
        if (stops.isEmpty())
            return Fill::none();
        if (stops.size() == 1)
            return Fill::solid(stops.first().color);
        Fill f;
        f.stops = stops;
        if (radial) {
            f.type = Fill::Type::Radial;
            const QPointF c(num(m_style, "svg:cx", 0.5), num(m_style, "svg:cy", 0.5));
            const double r = std::max(0.01, 1.0 - num(m_style, "draw:border"));
            f.start = c;
            f.end = c + QPointF(r, 0.0);
        } else {
            f.type = Fill::Type::Linear;
            double angle = -num(m_style, "draw:angle");
            while (angle < 0)
                angle += 360;
            while (angle >= 360)
                angle -= 360;
            if (std::abs(angle - 270.0) < 1e-6) {
                f.start = QPointF(0.0, 0.5);
                f.end = QPointF(1.0, 0.5);
            } else {
                // (0,0)->(0,1) im Rechteck der Form, um die Mitte gedreht
                QTransform rot;
                rot.translate(0.5, 0.5);
                rot.rotate(angle);
                rot.translate(-0.5, -0.5);
                f.start = rot.map(QPointF(0.0, 0.0));
                f.end = rot.map(QPointF(0.0, 1.0));
            }
        }
        f.sortStops();
        return f;
    }

    // Verlauf aus den Angaben der Datei (gradient-output.inc), bezogen auf das Begrenzungsrechteck
    // der Form auf der Seite: linear über die ganze Ausdehnung in Richtung des Winkels, radial und
    // quadratisch vom (verschobenen) Mittelpunkt bis zur entferntesten Ecke, konisch um den
    // Mittelpunkt. Randabstand verkürzt den Übergang, Farbe 0 liegt bei radial aussen.
    Fill cdrGradient(const QRectF& r) const
    {
        QList<GradientStop> stops;
        if (const librevenge::RVNGPropertyListVector* v = m_style.child("tangenta:gradient-stops"))
            for (unsigned long i = 0; i < v->count(); ++i)
                if (const auto c = color(str((*v)[i], "svg:stop-color")))
                    stops << GradientStop{std::clamp(num((*v)[i], "svg:offset"), 0.0, 1.0), *c};
        if (stops.isEmpty())
            return Fill::none();
        if (stops.size() == 1 || r.width() <= 0.0 || r.height() <= 0.0)
            return Fill::solid(stops.first().color);
        const double mid = num(m_style, "tangenta:gradient-midpoint", 0.5);
        if (stops.size() == 2 && mid > 0.02 && mid < 0.98 && std::abs(mid - 0.5) > 0.01) {
            const Color a = stops.first().color, b = stops.last().color;
            auto mix = [](int x, int y) { return std::uint8_t((x + y) / 2); };
            stops.insert(1, GradientStop{mid, Color(mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b), mix(a.a, b.a))});
        }
        const int type = int(num(m_style, "tangenta:gradient-type"));
        const double angle = qDegreesToRadians(num(m_style, "tangenta:gradient-angle"));
        const double edge = std::clamp(num(m_style, "tangenta:gradient-edge"), 0.0, 49.0) / 100.0;
        const QPointF dir(std::cos(angle), -std::sin(angle)); // y nach unten
        auto rel = [&](const QPointF& p) { return QPointF((p.x() - r.left()) / r.width(), (p.y() - r.top()) / r.height()); };
        Fill f;
        if (type != 2 && type != 3 && type != 4) {
            const double half = (std::abs(r.width() * dir.x()) + std::abs(r.height() * dir.y())) / 2.0 * (1.0 - 2.0 * edge);
            f.type = Fill::Type::Linear;
            f.start = rel(r.center() - dir * half);
            f.end = rel(r.center() + dir * half);
        } else {
            const QPointF m(r.center().x() + num(m_style, "tangenta:gradient-cx") / 100.0 * r.width() / 2.0,
                            r.center().y() - num(m_style, "tangenta:gradient-cy") / 100.0 * r.height() / 2.0);
            double radius = 0.0;
            for (const QPointF& corner : {r.topLeft(), r.topRight(), r.bottomLeft(), r.bottomRight()})
                radius = std::max(radius, std::hypot(corner.x() - m.x(), corner.y() - m.y()));
            f.start = rel(m);
            if (type == 3) {
                f.type = Fill::Type::Conical;
                f.end = rel(m + dir * radius);
            } else {
                f.type = Fill::Type::Radial;
                f.end = rel(m + QPointF(radius * (1.0 - edge), 0.0));
                for (GradientStop& st : stops)
                    st.position = 1.0 - st.position;
            }
        }
        f.stops = stops;
        f.sortStops();
        return f;
    }

    Fill bitmapFill() const
    {
        const QString key = imagepool::add(withMask(binary(m_style, "draw:fill-image")));
        if (key.isEmpty()) {
            if (const auto c = color(str(m_style, "draw:fill-color")))
                return Fill::solid(*c);
            return Fill::none();
        }
        PatternFill pattern;
        pattern.kind = PatternFill::Kind::Bitmap;
        pattern.imageKey = key;
        // Kachelgrösse in Zoll; relative Angaben (Anteil der Form) sind kleiner als 2 und gelten als Standard
        const double tile = num(m_style, "svg:width") * kMm;
        pattern.tile = tile > 1.0 ? tile : 10.0;
        return Fill::patternFill(pattern);
    }

    void registerFont(const CharFormat& format, const QString& raw, TextObject* text)
    {
        auto it = std::find_if(fonts.begin(), fonts.end(), [&](const ImportedFont& f) {
            return f.family == format.family && f.bold == format.bold && f.italic == format.italic;
        });
        if (it == fonts.end()) {
            ImportedFont f;
            f.baseName = raw;
            f.family = format.family;
            f.bold = format.bold;
            f.italic = format.italic;
            f.installed = fontmatch::installedFamily(format.family);
            fonts.push_back(std::move(f));
            it = std::prev(fonts.end());
        }
        if (!it->texts.contains(text))
            it->texts << text;
        if (it->sample.size() < 40)
            it->sample += text->plainText().left(40 - it->sample.size());
    }

    Props m_style;
    std::vector<std::unique_ptr<DrawObject>> m_open;
    int m_skip = 0;
    bool m_inText = false;
    QRectF m_textFrame;
    std::optional<QTransform> m_textMatrix;
    std::optional<QRectF> m_textBox;
    std::optional<Color> m_textStroke; // Kontur der Zeichen
    double m_textStrokeWidth = 0.0;    // in Zoll
    bool m_textNoFill = false;
    QList<TextParagraph> m_paragraphs;
    TextParagraph m_paragraph;
    CharFormat m_format;
    double m_shift = 0.0;            // horizontale Verschiebung des Laufs, Anteil der Schriftgrösse
    QList<double> m_runShifts;       // je Lauf des offenen Absatzes
    QList<QList<double>> m_shifts;   // je Absatz
    QHash<QString, QString> m_rawFont; // Familie -> Name in der Datei
};

} // namespace

bool isSupported(const QByteArray& data)
{
    if (data.isEmpty())
        return false;
    librevenge::RVNGStringStream input(reinterpret_cast<const unsigned char*>(data.constData()), unsigned(data.size()));
    try {
        return libcdr::CDRDocument::isSupported(&input) || libcdr::CMXDocument::isSupported(&input);
    } catch (...) {
        return false;
    }
}

std::vector<svg::Import> read(const QByteArray& data, QString* error)
{
    Collector collector;
    bool ok = false;
    try {
        librevenge::RVNGStringStream input(reinterpret_cast<const unsigned char*>(data.constData()), unsigned(data.size()));
        if (libcdr::CDRDocument::isSupported(&input)) {
            input.seek(0, librevenge::RVNG_SEEK_SET);
            ok = libcdr::CDRDocument::parse(&input, &collector);
        } else {
            input.seek(0, librevenge::RVNG_SEEK_SET);
            if (libcdr::CMXDocument::isSupported(&input)) {
                input.seek(0, librevenge::RVNG_SEEK_SET);
                ok = libcdr::CMXDocument::parse(&input, &collector);
            } else if (error) {
                *error = QObject::tr("Unbekanntes oder nicht unterstütztes Format.");
                return {};
            }
        }
    } catch (...) {
        ok = false;
    }
    std::vector<svg::Import> pages = std::move(collector.pages);
    if (!ok && pages.empty()) {
        if (error)
            *error = QObject::tr("Die Datei ist beschädigt oder in einer nicht unterstützten Version.");
        return {};
    }
    if (pages.empty()) {
        if (error)
            *error = QObject::tr("Die Datei enthält keine Seiten.");
        return {};
    }
    pages.front().fonts = std::move(collector.fonts);
    return pages;
}

} // namespace vek::cdr
