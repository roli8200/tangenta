#include "CdrImport.h"

#include "CurveObject.h"
#include "FontMatch.h"
#include "GroupObject.h"
#include "ImageObject.h"
#include "ImagePool.h"
#include "Ramp.h"
#include "TextObject.h"

#include <libcdr/libcdr.h>
#include <librevenge-stream/librevenge-stream.h>
#include <librevenge/librevenge.h>

#include <QHash>
#include <QPainterPath>
#include <QTransform>

#include <algorithm>
#include <cmath>
#include <memory>

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
    return v ? QByteArray::fromBase64(QByteArray(v->getStr().cstr())) : QByteArray();
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

    void startLayer(const Props&) override { m_open.push_back(std::make_unique<GroupObject>()); }
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
        const QString key = imagepool::add(binary(p, "office:binary-data"));
        if (key.isEmpty())
            return;
        const double x = num(p, "svg:x") * kMm, y = num(p, "svg:y") * kMm;
        const double w = num(p, "svg:width") * kMm, h = num(p, "svg:height") * kMm;
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
        m_paragraphs.clear();
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
        auto text = std::make_unique<TextObject>(TextObject::Kind::Paragraph);
        text->setParagraphs(m_paragraphs);
        // Die Masse stammen aus der Zeichnung; etwas Luft, damit andere Schriftmetrik nicht umbricht
        // oder Zeilen versteckt
        const double lineMm = largest * 25.4 / 72.0 * 1.3;
        const double needed = lineMm * double(m_paragraphs.size());
        text->setFrameSize(QSizeF(std::max(m_textFrame.width() * 1.04, lineMm), std::max(m_textFrame.height(), needed) * 1.15));
        text->setTransform(QTransform::fromTranslate(m_textFrame.left(), m_textFrame.top()));
        vek::Style st;
        st.fill = Fill::solid(Color(0, 0, 0));
        st.stroke.reset();
        text->setStyle(st);
        TextObject* raw = text.get();
        attachDetachedChild(*parent(), std::move(text));
        for (const TextParagraph& par : std::as_const(m_paragraphs))
            for (const TextRun& run : par.runs)
                registerFont(run.format, m_rawFont.value(run.format.family, run.format.family), raw);
    }
    void openParagraph(const Props& p) override
    {
        m_paragraph = TextParagraph();
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
        if (m_paragraph.runs.isEmpty())
            m_paragraph.runs << TextRun{QString(), m_format};
        m_paragraphs << m_paragraph;
        m_paragraph = TextParagraph();
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
        m_format = CharFormat(family, size > 0 ? size : 12.0, bold || str(p, "fo:font-weight") == QLatin1String("bold"),
                              italic || str(p, "fo:font-style") == QLatin1String("italic"), false,
                              color(str(p, "fo:color")).value_or(Color(0, 0, 0)));
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
    GroupObject* parent() const
    {
        if (m_skip > 0 || pages.empty())
            return nullptr;
        return m_open.empty() ? pages.back().group.get() : m_open.back().get();
    }

    void closeOne()
    {
        if (m_open.empty())
            return;
        std::unique_ptr<GroupObject> group = std::move(m_open.back());
        m_open.pop_back();
        GroupObject* target = parent();
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
        if (!m_paragraph.runs.isEmpty() && m_paragraph.runs.last().format == m_format)
            m_paragraph.runs.last().text += s;
        else
            m_paragraph.runs << TextRun{s, m_format};
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
        curve->setStyle(style());
        applyOpacity(*curve);
        attachDetachedChild(*parent(), std::move(curve));
    }

    void applyOpacity(DrawObject& o) const
    {
        const double opacity = num(m_style, "draw:opacity", 1.0);
        if (opacity < 1.0)
            o.setEffect(Effect::Transparency, Ramp::uniform((1.0 - std::clamp(opacity, 0.0, 1.0)) * 100.0));
    }

    vek::Style style() const
    {
        vek::Style st;
        const QString fill = str(m_style, "draw:fill");
        if (fill == QLatin1String("solid")) {
            if (const auto c = color(str(m_style, "draw:fill-color")))
                st.fill = Fill::solid(*c);
        } else if (fill == QLatin1String("gradient")) {
            st.fill = gradient();
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

    Fill bitmapFill() const
    {
        const QString key = imagepool::add(binary(m_style, "draw:fill-image"));
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
    std::vector<std::unique_ptr<GroupObject>> m_open;
    int m_skip = 0;
    bool m_inText = false;
    QRectF m_textFrame;
    QList<TextParagraph> m_paragraphs;
    TextParagraph m_paragraph;
    CharFormat m_format;
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
