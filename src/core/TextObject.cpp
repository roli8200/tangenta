#include "TextObject.h"

#include "Hyphenator.h"

#include <QFont>
#include <QHash>
#include <QJsonArray>
#include <QLineF>
#include <QPolygonF>
#include <QTransform>
#include <QSet>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextDocumentFragment>

#include <algorithm>
#include <cmath>

namespace vek {

namespace {

// Schriftgrösse in pt bleibt exakt erhalten, auch wenn die Pixelgrösse gerundet ist
constexpr int kSizePtProperty = QTextFormat::UserProperty + 1;

int pixelSize(double sizePt)
{
    return std::max(1, int(std::lround(sizePt * 25.4 / 72.0 * TextObject::kUnitsPerMm)));
}

QTextCharFormat toQtImpl(const CharFormat& f)
{
    QTextCharFormat cf;
    QFont font;
    font.setFamilies({f.family});
    font.setPixelSize(pixelSize(f.sizePt));
    font.setBold(f.bold);
    font.setItalic(f.italic);
    font.setUnderline(f.underline);
    font.setHintingPreference(QFont::PreferNoHinting);
    font.setKerning(true);
    cf.setFont(font, QTextCharFormat::FontPropertiesAll);
    cf.setProperty(kSizePtProperty, f.sizePt);
    if (f.color)
        cf.setForeground(f.color->toQColor());
    return cf;
}

CharFormat fromQtImpl(const QTextCharFormat& cf)
{
    CharFormat f;
    const QFont font = cf.font();
    if (!font.families().isEmpty())
        f.family = font.families().first();
    f.sizePt = cf.hasProperty(kSizePtProperty) ? cf.doubleProperty(kSizePtProperty)
                                               : font.pixelSize() / TextObject::kUnitsPerMm / 25.4 * 72.0;
    f.bold = font.bold();
    f.italic = font.italic();
    f.underline = cf.fontUnderline();
    if (cf.hasProperty(QTextFormat::ForegroundBrush) && cf.foreground().style() != Qt::NoBrush)
        f.color = Color::fromQColor(cf.foreground().color());
    return f;
}

Qt::Alignment alignToQt(TextParagraph::Align a)
{
    switch (a) {
    case TextParagraph::Align::Center: return Qt::AlignHCenter;
    case TextParagraph::Align::Right: return Qt::AlignRight;
    case TextParagraph::Align::Justify: return Qt::AlignJustify;
    default: return Qt::AlignLeft;
    }
}

TextParagraph::Align alignFromQt(Qt::Alignment a)
{
    if (a & Qt::AlignHCenter)
        return TextParagraph::Align::Center;
    if (a & Qt::AlignRight)
        return TextParagraph::Align::Right;
    if (a & Qt::AlignJustify)
        return TextParagraph::Align::Justify;
    return TextParagraph::Align::Left;
}

const char* alignName(TextParagraph::Align a)
{
    switch (a) {
    case TextParagraph::Align::Center: return "center";
    case TextParagraph::Align::Right: return "right";
    case TextParagraph::Align::Justify: return "justify";
    default: return "left";
    }
}

QTextBlockFormat blockFormat(const TextParagraph& p)
{
    QTextBlockFormat bf;
    bf.setAlignment(alignToQt(p.align));
    QList<QTextOption::Tab> tabs;
    for (double mm : p.tabs)
        tabs << QTextOption::Tab(mm * TextObject::kUnitsPerMm, QTextOption::LeftTab);
    bf.setTabPositions(tabs);
    if (p.bullet)
        bf.setProperty(TextObject::kBulletProperty, true);
    if (p.dropCap > 0)
        bf.setProperty(TextObject::kDropCapProperty, p.dropCap);
    return bf;
}

QJsonObject formatToJson(const CharFormat& f)
{
    QJsonObject o;
    o[QStringLiteral("font")] = f.family;
    o[QStringLiteral("size")] = f.sizePt;
    if (f.bold)
        o[QStringLiteral("bold")] = true;
    if (f.italic)
        o[QStringLiteral("italic")] = true;
    if (f.underline)
        o[QStringLiteral("underline")] = true;
    if (f.color)
        o[QStringLiteral("color")] = f.color->toHex();
    return o;
}

CharFormat formatFromJson(const QJsonObject& o)
{
    CharFormat f;
    f.family = o.value(QStringLiteral("font")).toString(f.family);
    f.sizePt = o.value(QStringLiteral("size")).toDouble(f.sizePt);
    f.bold = o.value(QStringLiteral("bold")).toBool();
    f.italic = o.value(QStringLiteral("italic")).toBool();
    f.underline = o.value(QStringLiteral("underline")).toBool();
    if (o.contains(QStringLiteral("color")))
        f.color = Color::fromHex(o.value(QStringLiteral("color")).toString());
    return f;
}

} // namespace

QTextCharFormat TextObject::toQtFormat(const CharFormat& format)
{
    return toQtImpl(format);
}

CharFormat TextObject::fromQtFormat(const QTextCharFormat& format)
{
    return fromQtImpl(format);
}

Qt::Alignment TextObject::toQtAlignment(TextParagraph::Align align)
{
    return alignToQt(align);
}

TextParagraph::Align TextObject::fromQtAlignment(Qt::Alignment align)
{
    return alignFromQt(align);
}

TextObject::TextObject(Kind kind, const QSizeF& frame) : m_kind(kind), m_frame(frame)
{
    setPlainText(QString(), CharFormat{QStringLiteral("Arial"), kind == Kind::Paragraph ? 12.0 : 24.0});
}

void TextObject::setFrameSize(const QSizeF& size)
{
    m_frame = size;
    invalidate();
}

void TextObject::setParagraphs(const QList<TextParagraph>& paragraphs)
{
    m_paragraphs = paragraphs;
    if (m_paragraphs.isEmpty())
        m_paragraphs << TextParagraph{};
    for (TextParagraph& p : m_paragraphs) {
        if (p.runs.isEmpty())
            p.runs << TextRun{};
    }
    invalidate();
}

void TextObject::setPlainText(const QString& text, const CharFormat& format)
{
    QList<TextParagraph> list;
    for (const QString& line : text.split(QLatin1Char('\n'))) {
        TextParagraph p;
        p.runs << TextRun{line, format};
        list << p;
    }
    setParagraphs(list);
}

QString TextObject::plainText() const
{
    QStringList lines;
    for (const TextParagraph& p : m_paragraphs) {
        QString line;
        for (const TextRun& r : p.runs)
            line += r.text;
        lines << line;
    }
    return lines.join(QLatin1Char('\n'));
}

void TextObject::applyToAll(const std::function<void(CharFormat&)>& fn)
{
    for (TextParagraph& p : m_paragraphs) {
        for (TextRun& r : p.runs)
            fn(r.format);
    }
    invalidate();
}

void TextObject::setAlignment(TextParagraph::Align align)
{
    for (TextParagraph& p : m_paragraphs)
        p.align = align;
    invalidate();
}

void TextObject::setHyphenate(bool on)
{
    m_hyphenate = on;
    invalidate();
}

std::unique_ptr<QTextDocument> TextObject::toDocument(bool withHyphens) const
{
    const bool hyphens = withHyphens && m_hyphenate && m_kind == Kind::Paragraph;
    auto doc = std::make_unique<QTextDocument>();
    doc->setUndoRedoEnabled(false);
    doc->setDocumentMargin(0);
    QTextCursor cursor(doc.get());
    for (int i = 0; i < m_paragraphs.size(); ++i) {
        const TextParagraph& p = m_paragraphs[i];
        const QTextCharFormat first = toQtImpl(p.runs.isEmpty() ? CharFormat{} : p.runs.first().format);
        if (i == 0) {
            cursor.setBlockFormat(blockFormat(p));
            cursor.setBlockCharFormat(first);
        } else {
            cursor.insertBlock(blockFormat(p), first);
        }
        for (const TextRun& r : p.runs)
            cursor.insertText(hyphens ? Hyphenator::german().withSoftHyphens(r.text) : r.text, toQtImpl(r.format));
    }
    prepareLayout(*doc);
    return doc;
}

void TextObject::prepareLayout(QTextDocument& doc) const
{
    QTextOption option = doc.defaultTextOption();
    option.setTabStopDistance(kDefaultTabMm * kUnitsPerMm);
    if (m_kind == Kind::Paragraph) {
        option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        doc.setDefaultTextOption(option);
        doc.setTextWidth(std::max(1.0, m_frame.width() * kUnitsPerMm));
    } else {
        option.setWrapMode(QTextOption::NoWrap);
        doc.setDefaultTextOption(option);
        doc.setTextWidth(-1);
    }
}

void TextObject::setFromDocument(const QTextDocument& doc)
{
    QList<TextParagraph> list;
    for (QTextBlock block = doc.begin(); block.isValid(); block = block.next()) {
        TextParagraph p;
        const QTextBlockFormat bf = block.blockFormat();
        p.align = alignFromQt(bf.alignment());
        for (const QTextOption::Tab& t : bf.tabPositions())
            p.tabs << t.position / kUnitsPerMm;
        p.bullet = bf.property(kBulletProperty).toBool();
        p.dropCap = bf.property(kDropCapProperty).toInt();
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            if (!fragment.isValid())
                continue;
            const CharFormat f = fromQtImpl(fragment.charFormat());
            // Nachbarn mit gleichem Format zusammenfassen
            if (!p.runs.isEmpty() && p.runs.last().format == f)
                p.runs.last().text += fragment.text();
            else
                p.runs << TextRun{fragment.text(), f};
        }
        if (p.runs.isEmpty())
            p.runs << TextRun{QString(), fromQtImpl(block.charFormat())};
        list << p;
    }
    setParagraphs(list);
}

const TextObject::Cache& TextObject::cache() const
{
    if (!m_cache) {
        auto c = std::make_shared<Cache>();
        const std::unique_ptr<QTextDocument> doc = toDocument(true);
        QList<QSizeF> frames;
        if (m_kind == Kind::Paragraph)
            frames << m_frame << m_followerSizes;
        const TextFlow flow(*doc, frames, m_kind == Kind::Paragraph);
        for (int i = 0; i < flow.frameCount(); ++i) {
            FrameContent f;
            f.pieces = flow.pieces(i);
            f.all.setFillRule(Qt::WindingFill);
            for (const Piece& p : f.pieces)
                f.all.addPath(p.path);
            c->frames << f;
        }
        if (flow.frameCount() > 0)
            c->glyphs = flow.glyphs(0);
        c->extent = flow.extent();
        c->overflow = flow.overflows();
        m_cache = c;
    }
    return *m_cache;
}

const TextObject::FrameContent* TextObject::content() const
{
    const Cache& c = m_source ? *m_source : cache();
    const int index = m_source ? m_chainIndex : 0;
    return index < c.frames.size() ? &c.frames[index] : nullptr;
}

const QList<TextObject::Piece>& TextObject::pieces() const
{
    static const QList<Piece> none;
    const FrameContent* c = content();
    return c ? c->pieces : none;
}

QPainterPath TextObject::localPath() const
{
    const FrameContent* c = content();
    return c ? c->all : QPainterPath();
}

bool TextObject::overflows() const
{
    return m_kind == Kind::Paragraph && (m_source ? m_source->overflow : cache().overflow);
}

void TextObject::clearChain() const
{
    if (!m_followerSizes.isEmpty()) {
        m_followerSizes.clear();
        invalidate();
    }
    m_chain.reset();
    m_chainIndex = 0;
    m_source.reset();
}

QList<QList<TextParagraph>> TextObject::paragraphsByFrame() const
{
    // Bereiche aus dem Satz mit Trennstellen, die weichen Trennstriche danach wieder entfernen
    const std::unique_ptr<QTextDocument> doc = toDocument(true);
    QList<QSizeF> frames{m_frame};
    frames << m_followerSizes;
    const TextFlow flow(*doc, frames, true);
    QList<QList<TextParagraph>> result;
    for (int i = 0; i < flow.frameCount(); ++i) {
        auto [from, to] = flow.frameRange(i);
        if (i == flow.frameCount() - 1)
            to = doc->characterCount() - 1;
        if (to > from && doc->findBlock(to).position() == to)
            --to; // Absatzende zwischen zwei Rahmen gehört zu keinem
        QTextCursor cursor(doc.get());
        cursor.setPosition(from);
        cursor.setPosition(std::max(from, to), QTextCursor::KeepAnchor);
        QTextDocument part;
        QTextCursor(&part).insertFragment(cursor.selection());
        TextObject piece(Kind::Paragraph, m_frame);
        piece.setFromDocument(part);
        QList<TextParagraph> paragraphs = piece.paragraphs();
        for (TextParagraph& p : paragraphs) {
            for (TextRun& r : p.runs)
                r.text.remove(QChar(QChar::SoftHyphen));
        }
        // Leerer Rahmen: Format des Kettenendes für die erste Zeile
        if (from == to && !m_paragraphs.isEmpty())
            paragraphs = {TextParagraph{{TextRun{QString(), m_paragraphs.last().runs.last().format}}, m_paragraphs.last().align, {}}};
        result << paragraphs;
    }
    return result;
}

void TextObject::linkChains(std::vector<const TextObject*> texts)
{
    // Feste Reihenfolge: Bei zwei Vorgängern (z.B. nach dem Kopieren) gewinnt der ältere
    std::sort(texts.begin(), texts.end(), [](const TextObject* a, const TextObject* b) { return a->id() < b->id(); });
    QHash<NodeId, const TextObject*> byId;
    for (const TextObject* t : texts) {
        if (t->kind() == Kind::Paragraph)
            byId.insert(t->id(), t);
    }
    QHash<NodeId, NodeId> previous;
    for (const TextObject* t : texts) {
        if (t->kind() == Kind::Paragraph && t->m_next != t->id() && byId.contains(t->m_next) && !previous.contains(t->m_next))
            previous.insert(t->m_next, t->id());
    }
    QSet<NodeId> linked;
    for (const TextObject* head : texts) {
        if (head->kind() != Kind::Paragraph || previous.contains(head->id()))
            continue;
        QList<NodeId> ids{head->id()};
        QList<const TextObject*> followers;
        QList<QSizeF> sizes;
        for (const TextObject* cur = head; cur->m_next != 0;) {
            const TextObject* next = byId.value(cur->m_next);
            if (!next || previous.value(next->id()) != cur->id() || ids.contains(next->id()))
                break;
            ids << next->id();
            followers << next;
            sizes << next->m_frame;
            cur = next;
        }
        if (followers.isEmpty())
            continue;
        if (head->m_followerSizes != sizes) {
            head->m_followerSizes = sizes;
            head->invalidate();
        }
        auto chain = std::make_shared<const QList<NodeId>>(ids);
        head->m_chain = chain;
        head->m_chainIndex = 0;
        head->m_source.reset();
        head->cache();
        for (int i = 0; i < followers.size(); ++i) {
            followers[i]->m_chain = chain;
            followers[i]->m_chainIndex = i + 1;
            followers[i]->m_source = head->m_cache;
        }
        for (NodeId id : ids)
            linked.insert(id);
    }
    for (const TextObject* t : texts) {
        if (!linked.contains(t->id()))
            t->clearChain();
    }
}

QRectF TextObject::frameRect() const
{
    if (m_kind == Kind::Paragraph)
        return QRectF(QPointF(0, 0), m_frame);
    return cache().extent;
}

void TextObject::setPathFit(NodeId pathObject, double offset, double distance, bool flip)
{
    m_pathId = pathObject;
    m_pathOffset = offset;
    m_pathDistance = distance;
    m_pathFlip = flip;
    if (!pathObject)
        m_pathShape = QPainterPath();
}

void TextObject::resolvePath(const std::function<const DrawObject*(NodeId)>& find) const
{
    const DrawObject* o = m_pathId && m_pathId != id() ? find(m_pathId) : nullptr;
    m_pathShape = o ? o->path() : QPainterPath();
}

const TextObject::FrameContent& TextObject::fittedContent() const
{
    const Cache& c = cache();
    FitCache& f = m_fit;
    if (f.source == m_cache && f.shape == m_pathShape && f.offset == m_pathOffset && f.distance == m_pathDistance && f.flip == m_pathFlip)
        return f.content;
    f.source = m_cache;
    f.shape = m_pathShape;
    f.offset = m_pathOffset;
    f.distance = m_pathDistance;
    f.flip = m_pathFlip;
    f.content = FrameContent();
    f.content.all.setFillRule(Qt::WindingFill);
    // Pfad fein als Linienzug mit Bogenlängen
    const QPainterPath fine = warpPath(m_pathShape, [](const QPointF& p) { return p; }, 0.2);
    const QList<QPolygonF> polys = fine.toSubpathPolygons();
    if (polys.isEmpty() || polys.first().size() < 2)
        return f.content;
    QPolygonF poly = polys.first();
    if (m_pathFlip)
        std::reverse(poly.begin(), poly.end());
    QList<double> len{0.0};
    for (int i = 1; i < poly.size(); ++i)
        len << len.last() + QLineF(poly[i - 1], poly[i]).length();
    // Geschlossene Formen: der Text läuft über den Anfang hinweg weiter
    const bool closed = poly.size() > 2 && QLineF(poly.first(), poly.last()).length() < 1e-6 && len.last() > 0;
    // Punkt und Richtung bei Bogenlänge s (offen: ausserhalb geradlinig weiter)
    auto at = [&](double s, QPointF* tangent) {
        if (closed)
            s -= std::floor(s / len.last()) * len.last();
        int i = 1;
        while (i < poly.size() - 1 && len[i] < s)
            ++i;
        const QPointF a = poly[i - 1], b = poly[i];
        const double seg = std::max(1e-9, len[i] - len[i - 1]);
        *tangent = (b - a) / seg;
        return a + *tangent * (s - len[i - 1]);
    };
    const double y0 = c.glyphs.isEmpty() ? 0.0 : c.glyphs.first().baseline;
    for (const TextGlyph& g : c.glyphs) {
        QPointF t;
        const QPointF p = at(m_pathOffset + g.center, &t);
        const QPointF up(t.y(), -t.x()); // links der Laufrichtung = oben
        const QTransform m = QTransform::fromTranslate(-g.center, -y0) * QTransform().rotateRadians(std::atan2(t.y(), t.x()))
                             * QTransform::fromTranslate(p.x() + up.x() * m_pathDistance, p.y() + up.y() * m_pathDistance);
        const QPainterPath placed = m.map(g.path);
        Piece* target = nullptr;
        for (Piece& piece : f.content.pieces)
            if (piece.color == g.color)
                target = &piece;
        if (!target) {
            f.content.pieces << Piece{QPainterPath(), g.color, {}, {}};
            f.content.pieces.last().path.setFillRule(Qt::WindingFill);
            target = &f.content.pieces.last();
        }
        target->path.addPath(placed);
        f.content.all.addPath(placed);
    }
    return f.content;
}

QPainterPath TextObject::path() const
{
    if (fitted())
        return fittedContent().all;
    return DrawObject::path();
}

QList<TextObject::Piece> TextObject::shownPieces() const
{
    if (fitted())
        return fittedContent().pieces;
    QList<Piece> list = pieces();
    if (hasEnvelope())
        for (Piece& p : list)
            p.path = enveloped(p.path);
    return list;
}

QPainterPath TextObject::snapPath() const
{
    if (hasEnvelope() || fitted()) {
        // Verformter Text: das Rechteck um die Glyphen genügt zum Einrasten
        QPainterPath box;
        box.addRect(path().boundingRect());
        return box;
    }
    QPainterPath frame;
    frame.addPolygon(transform().map(QPolygonF(frameRect())));
    frame.closeSubpath();
    return frame;
}

QRectF TextObject::boundingRect() const
{
    if (hasEnvelope() || fitted())
        return path().boundingRect();
    return transform().map(QPolygonF(frameRect())).boundingRect();
}

bool TextObject::hitTest(const QPointF& point, double tolerance, bool treatAsFilled) const
{
    if (hasEnvelope() || fitted())
        return path().boundingRect().adjusted(-tolerance, -tolerance, tolerance, tolerance).contains(point);
    // Wie gewohnt trifft man Text überall in seinem Rahmen, nicht nur auf den Glyphen
    const QPolygonF frame = transform().map(QPolygonF(frameRect().adjusted(-tolerance, -tolerance, tolerance, tolerance)));
    return frame.containsPoint(point, Qt::OddEvenFill) || DrawObject::hitTest(point, tolerance, treatAsFilled);
}

void TextObject::applyTransform(const QTransform& m)
{
    // Text an Pfad folgt nur dem Pfad
    if (m_pathId != 0)
        return;
    DrawObject::applyTransform(m);
    if (m_kind == Kind::Paragraph) {
        const QSizeF before = m_frame;
        absorbScale(m_frame);
        if (before != m_frame)
            invalidate();
    }
}

void TextObject::writeProperties(QJsonObject& out) const
{
    DrawObject::writeProperties(out);
    out[QStringLiteral("kind")] = m_kind == Kind::Paragraph ? QStringLiteral("paragraph") : QStringLiteral("artistic");
    if (m_kind == Kind::Paragraph) {
        out[QStringLiteral("width")] = m_frame.width();
        out[QStringLiteral("height")] = m_frame.height();
        if (m_hyphenate)
            out[QStringLiteral("hyphenate")] = true;
        if (m_next != 0)
            out[QStringLiteral("next")] = QString::number(m_next); // als Text wie die Ids
    }
    QJsonArray paragraphs;
    for (const TextParagraph& p : m_paragraphs) {
        QJsonObject po;
        if (p.align != TextParagraph::Align::Left)
            po[QStringLiteral("align")] = QLatin1String(alignName(p.align));
        if (!p.tabs.isEmpty()) {
            QJsonArray tabs;
            for (double t : p.tabs)
                tabs << t;
            po[QStringLiteral("tabs")] = tabs;
        }
        if (p.bullet)
            po[QStringLiteral("bullet")] = true;
        if (p.dropCap > 0)
            po[QStringLiteral("dropCap")] = p.dropCap;
        QJsonArray runs;
        for (const TextRun& r : p.runs) {
            QJsonObject ro = formatToJson(r.format);
            ro[QStringLiteral("text")] = r.text;
            runs << ro;
        }
        po[QStringLiteral("runs")] = runs;
        paragraphs << po;
    }
    out[QStringLiteral("paragraphs")] = paragraphs;
    if (m_pathId != 0) {
        out[QStringLiteral("onPath")] = QString::number(m_pathId);
        out[QStringLiteral("pathOffset")] = m_pathOffset;
        out[QStringLiteral("pathDistance")] = m_pathDistance;
        if (m_pathFlip)
            out[QStringLiteral("pathFlip")] = true;
    }
}

void TextObject::readProperties(const QJsonObject& in)
{
    DrawObject::readProperties(in);
    m_kind = in.value(QStringLiteral("kind")).toString() == QLatin1String("paragraph") ? Kind::Paragraph : Kind::Artistic;
    m_frame = QSizeF(in.value(QStringLiteral("width")).toDouble(), in.value(QStringLiteral("height")).toDouble());
    m_hyphenate = in.value(QStringLiteral("hyphenate")).toBool();
    m_next = in.value(QStringLiteral("next")).toString().toULongLong();
    m_pathId = in.value(QStringLiteral("onPath")).toString().toULongLong();
    m_pathOffset = in.value(QStringLiteral("pathOffset")).toDouble();
    m_pathDistance = in.value(QStringLiteral("pathDistance")).toDouble();
    m_pathFlip = in.value(QStringLiteral("pathFlip")).toBool();
    m_pathShape = QPainterPath();
    QList<TextParagraph> list;
    for (const QJsonValue& pv : in.value(QStringLiteral("paragraphs")).toArray()) {
        const QJsonObject po = pv.toObject();
        TextParagraph p;
        const QString align = po.value(QStringLiteral("align")).toString();
        p.align = align == QLatin1String("center")    ? TextParagraph::Align::Center
                  : align == QLatin1String("right")   ? TextParagraph::Align::Right
                  : align == QLatin1String("justify") ? TextParagraph::Align::Justify
                                                      : TextParagraph::Align::Left;
        for (const QJsonValue& t : po.value(QStringLiteral("tabs")).toArray())
            p.tabs << t.toDouble();
        p.bullet = po.value(QStringLiteral("bullet")).toBool();
        p.dropCap = std::clamp(po.value(QStringLiteral("dropCap")).toInt(), 0, 10);
        for (const QJsonValue& rv : po.value(QStringLiteral("runs")).toArray()) {
            const QJsonObject ro = rv.toObject();
            p.runs << TextRun{ro.value(QStringLiteral("text")).toString(), formatFromJson(ro)};
        }
        list << p;
    }
    setParagraphs(list);
}

} // namespace vek
