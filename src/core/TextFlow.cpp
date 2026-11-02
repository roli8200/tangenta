#include "TextFlow.h"

#include "TextObject.h"

#include <QFontMetricsF>
#include <QGlyphRun>
#include <QRawFont>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextLayout>

#include <algorithm>
#include <climits>
#include <cmath>

namespace vek {

namespace {

constexpr double kUnbounded = 1e7; // Layout-Einheiten, passt noch in QFixed
constexpr double kFitTolerance = 0.5;

int frameOrder(int frame)
{
    return frame < 0 ? INT_MAX : frame;
}

} // namespace

TextFlow::TextFlow(const QTextDocument& doc, const QList<QSizeF>& frames, bool wrap)
{
    const double u = TextObject::kUnitsPerMm;
    if (wrap) {
        for (const QSizeF& f : frames)
            m_frameSizes.emplace_back(std::max(1.0, f.width() * u), f.height() * u);
    }
    if (m_frameSizes.empty())
        m_frameSizes.emplace_back(kUnbounded, kUnbounded);
    m_endPosition = std::max(0, doc.characterCount() - 1);

    // Ohne Umbruch zweimal: erst die breiteste Zeile messen, dann darin ausrichten
    auto run = [&](double artisticWidth) {
        m_layouts.clear();
        m_extras.clear();
        m_blockStart.clear();
        m_firstLine.clear();
        m_lines.clear();
        m_overflow = false;
        m_width = 0.0;
        int frame = 0;
        double y = 0.0;
        int blockIndex = 0;
        for (QTextBlock block = doc.begin(); block.isValid(); block = block.next(), ++blockIndex) {
            const QTextBlockFormat bf = block.blockFormat();
            QTextOption option(bf.alignment());
            option.setWrapMode(wrap ? QTextOption::WrapAtWordBoundaryOrAnywhere : QTextOption::NoWrap);
            option.setTextDirection(Qt::LeftToRight);
            option.setTabStopDistance(TextObject::kDefaultTabMm * u);
            option.setTabs(bf.tabPositions());
            auto layout = std::make_unique<QTextLayout>(block.text(), block.charFormat().font());
            layout->setTextOption(option);
            layout->setFormats(block.textFormats());
            layout->setCacheEnabled(true);
            // Aufzählungszeichen und Initiale: Schrift des ersten Zeichens
            BlockExtra extra;
            const QTextCharFormat firstFormat = block.begin() != block.end() && block.begin().fragment().isValid()
                                                    ? block.begin().fragment().charFormat()
                                                    : block.charFormat();
            const QFont font = firstFormat.font();
            if (firstFormat.hasProperty(QTextFormat::ForegroundBrush) && firstFormat.foreground().style() != Qt::NoBrush)
                extra.color = Color::fromQColor(firstFormat.foreground().color());
            const double em = std::max(1, font.pixelSize());
            if (bf.property(TextObject::kBulletProperty).toBool()) {
                const QRawFont raw = QRawFont::fromFont(font);
                const QList<quint32> g = raw.glyphIndexesForString(QStringLiteral("•"));
                if (!g.isEmpty())
                    extra.bullet = raw.pathForGlyph(g.first());
                extra.bulletX = em * 0.25;
                extra.bulletIndent = em * 1.1;
            }
            if (const int lines = bf.property(TextObject::kDropCapProperty).toInt(); lines >= 2 && !block.text().isEmpty()) {
                // Initiale: Grossbuchstabe von der ersten Oberlänge bis zur Grundlinie der n-ten Zeile
                const QFontMetricsF fm(font);
                const double capH = std::max(1.0, fm.capHeight());
                const double big = (lines - 1) * fm.lineSpacing() + capH;
                QFont capFont = font;
                capFont.setPixelSize(std::max(1, int(std::lround(em * big / capH))));
                const QRawFont raw = QRawFont::fromFont(capFont);
                const QList<quint32> g = raw.glyphIndexesForString(block.text().left(1));
                if (!g.isEmpty()) {
                    extra.cap = raw.pathForGlyph(g.first());
                    const QList<QPointF> adv = raw.advancesForGlyphIndexes(g);
                    extra.capIndent = (adv.isEmpty() ? capFont.pixelSize() * 0.7 : adv.first().x()) + em * 0.15;
                    extra.dropLines = lines;
                }
            }
            m_blockStart.push_back(block.position());
            m_firstLine.push_back(int(m_lines.size()));
            layout->beginLayout();
            for (int index = 0;; ++index) {
                QTextLine line = layout->createLine();
                if (!line.isValid())
                    break;
                line.setLeadingIncluded(true);
                // Einzug: Aufzählung hängend, Initiale für ihre Zeilen
                const double indent = extra.bulletIndent + (index < extra.dropLines ? extra.capIndent : 0.0);
                int placed = 0;
                if (!wrap) {
                    line.setLineWidth(artisticWidth);
                    m_width = std::max(m_width, line.naturalTextWidth() + indent);
                } else {
                    placed = -1;
                    while (frame < frameCount()) {
                        const QSizeF& size = m_frameSizes[std::size_t(frame)];
                        line.setLineWidth(std::max(1.0, size.width() - indent));
                        if (y + line.height() <= size.height() + kFitTolerance) {
                            placed = frame;
                            break;
                        }
                        ++frame;
                        y = 0.0;
                    }
                    if (placed < 0)
                        m_overflow = true;
                }
                // Erstes Zeichen einer Initiale wird nicht gezeichnet: Zeile um seine Breite nach links
                const double hidden = index == 0 && extra.dropLines > 0 ? line.cursorToX(1) - line.cursorToX(0) : 0.0;
                line.setPosition(QPointF(indent - hidden, y));
                y += line.height();
                m_lines.push_back(Line{blockIndex, index, placed});
            }
            layout->endLayout();
            m_layouts.push_back(std::move(layout));
            m_extras.push_back(extra);
        }
        m_height = y;
    };
    if (wrap) {
        run(0.0);
    } else {
        run(kUnbounded);
        run(std::max(1.0, std::ceil(m_width)));
    }
}

TextFlow::~TextFlow() = default;

QList<TextPiece> TextFlow::pieces(int frame) const
{
    // Glyphen je Formatbereich holen, damit jede Farbe ihren eigenen Pfad bekommt
    QList<TextPiece> pieces;
    auto pieceFor = [&pieces](const std::optional<Color>& color) -> TextPiece& {
        for (TextPiece& p : pieces) {
            if (p.color == color)
                return p;
        }
        pieces << TextPiece{QPainterPath(), color, {}, {}};
        pieces.last().path.setFillRule(Qt::WindingFill);
        pieces.last().extras.setFillRule(Qt::WindingFill);
        return pieces.last();
    };
    for (const Line& l : m_lines) {
        if (l.frame != frame)
            continue;
        const QTextLayout& layout = *m_layouts[std::size_t(l.block)];
        const QTextLine line = layout.lineAt(l.index);
        const int lineFrom = line.textStart();
        const int lineTo = lineFrom + line.textLength();
        const BlockExtra& extra = m_extras[std::size_t(l.block)];
        if (l.index == 0) {
            const double baseline = line.y() + line.ascent();
            if (!extra.bullet.isEmpty()) {
                TextPiece& piece = pieceFor(extra.color);
                piece.path.addPath(extra.bullet.translated(extra.bulletX, baseline));
                piece.extras.addPath(extra.bullet.translated(extra.bulletX, baseline));
            }
            if (extra.dropLines > 0) {
                // Grundlinie der letzten Zeile, über die die Initiale reicht
                const QTextLine last = layout.lineAt(std::min(extra.dropLines, layout.lineCount()) - 1);
                TextPiece& piece = pieceFor(extra.color);
                piece.path.addPath(extra.cap.translated(extra.bulletIndent, last.y() + last.ascent()));
                piece.extras.addPath(extra.cap.translated(extra.bulletIndent, last.y() + last.ascent()));
            }
        }
        for (const QTextLayout::FormatRange& range : layout.formats()) {
            // Das erste Zeichen steht als Initiale schon da
            const int from = std::max(extra.dropLines > 0 ? 1 : 0, std::max(lineFrom, range.start));
            const int to = std::min(lineTo, range.start + range.length);
            if (from >= to)
                continue;
            const QTextCharFormat& cf = range.format;
            std::optional<Color> color;
            if (cf.hasProperty(QTextFormat::ForegroundBrush) && cf.foreground().style() != Qt::NoBrush)
                color = Color::fromQColor(cf.foreground().color());
            TextPiece& target = pieceFor(color);
            for (const QGlyphRun& run : line.glyphRuns(from, to - from)) {
                const QRawFont font = run.rawFont();
                const QList<quint32> glyphs = run.glyphIndexes();
                const QList<QPointF> positions = run.positions();
                for (int i = 0; i < glyphs.size() && i < positions.size(); ++i)
                    target.path.addPath(font.pathForGlyph(glyphs[i]).translated(positions[i]));
                target.runs << run;
                if (cf.fontUnderline() && !positions.isEmpty()) {
                    const QRectF box = run.boundingRect();
                    const double baseline = positions.first().y();
                    const double thickness = std::max(1.0, font.lineThickness());
                    const QRectF bar(box.left(), baseline + font.underlinePosition(), box.width(), thickness);
                    target.path.addRect(bar);
                    target.extras.addRect(bar);
                }
            }
        }
    }
    const QTransform toMm = QTransform::fromScale(1.0 / TextObject::kUnitsPerMm, 1.0 / TextObject::kUnitsPerMm);
    for (TextPiece& p : pieces) {
        p.path = toMm.map(p.path);
        p.extras = toMm.map(p.extras);
    }
    return pieces;
}

QList<TextGlyph> TextFlow::glyphs(int frame) const
{
    QList<TextGlyph> out;
    const double u = TextObject::kUnitsPerMm;
    for (const Line& l : m_lines) {
        if (l.frame != frame)
            continue;
        const QTextLayout& layout = *m_layouts[std::size_t(l.block)];
        const QTextLine line = layout.lineAt(l.index);
        const double baseline = line.y() + line.ascent();
        for (const QTextLayout::FormatRange& range : layout.formats()) {
            const int from = std::max(line.textStart(), range.start);
            const int to = std::min(line.textStart() + line.textLength(), range.start + range.length);
            if (from >= to)
                continue;
            std::optional<Color> color;
            if (range.format.hasProperty(QTextFormat::ForegroundBrush) && range.format.foreground().style() != Qt::NoBrush)
                color = Color::fromQColor(range.format.foreground().color());
            for (const QGlyphRun& run : line.glyphRuns(from, to - from)) {
                const QRawFont font = run.rawFont();
                const QList<quint32> indexes = run.glyphIndexes();
                const QList<QPointF> positions = run.positions();
                const QList<QPointF> advances = font.advancesForGlyphIndexes(indexes);
                for (int i = 0; i < indexes.size() && i < positions.size(); ++i) {
                    const double adv = i < advances.size() ? advances[i].x() : 0.0;
                    QPainterPath p = font.pathForGlyph(indexes[i]).translated(positions[i]);
                    p = QTransform::fromScale(1.0 / u, 1.0 / u).map(p);
                    out << TextGlyph{p, color, (positions[i].x() + adv / 2.0) / u, baseline / u};
                }
            }
        }
    }
    return out;
}

QRectF TextFlow::extent() const
{
    return QRectF(0, 0, m_width / TextObject::kUnitsPerMm, m_height / TextObject::kUnitsPerMm);
}

std::pair<int, int> TextFlow::frameRange(int frame) const
{
    int first = -1;
    int last = -1;
    int after = m_endPosition;
    for (const Line& l : m_lines) {
        const QTextLine line = m_layouts[std::size_t(l.block)]->lineAt(l.index);
        const int start = m_blockStart[std::size_t(l.block)] + line.textStart();
        if (l.frame == frame) {
            if (first < 0)
                first = start;
            last = start + line.textLength();
        } else if (frameOrder(l.frame) > frame) {
            after = start;
            break;
        }
    }
    if (first < 0)
        return {after, after};
    // Bis zum Anfang des nächsten Rahmens, damit Absatzenden mitkommen
    return {first, std::max(last, after)};
}

int TextFlow::lineOf(int position) const
{
    if (m_lines.empty())
        return -1;
    position = std::clamp(position, 0, m_endPosition);
    const auto it = std::upper_bound(m_blockStart.begin(), m_blockStart.end(), position);
    const std::size_t block = std::size_t(std::max<std::ptrdiff_t>(0, (it - m_blockStart.begin()) - 1));
    const QTextLayout& layout = *m_layouts[block];
    const QTextLine line = layout.lineForTextPosition(position - m_blockStart[block]);
    const int index = line.isValid() ? line.lineNumber() : std::max(0, layout.lineCount() - 1);
    return m_firstLine[block] + index;
}

TextFlow::Caret TextFlow::caret(int position) const
{
    const int li = lineOf(position);
    if (li < 0)
        return {};
    const Line& l = m_lines[std::size_t(li)];
    const QTextLine line = m_layouts[std::size_t(l.block)]->lineAt(l.index);
    const double u = TextObject::kUnitsPerMm;
    const double x = line.cursorToX(std::clamp(position, 0, m_endPosition) - m_blockStart[std::size_t(l.block)]);
    return Caret{l.frame, QPointF(x / u, line.y() / u), line.height() / u};
}

int TextFlow::positionAt(int frame, const QPointF& local) const
{
    const QPointF p = local * TextObject::kUnitsPerMm;
    int best = -1;
    double bestDistance = 0.0;
    for (std::size_t i = 0; i < m_lines.size(); ++i) {
        if (m_lines[i].frame != frame)
            continue;
        const QTextLine line = m_layouts[std::size_t(m_lines[i].block)]->lineAt(m_lines[i].index);
        const double d = p.y() < line.y() ? line.y() - p.y() : std::max(0.0, p.y() - line.y() - line.height());
        if (best < 0 || d < bestDistance) {
            best = int(i);
            bestDistance = d;
        }
    }
    if (best < 0) // leerer Rahmen: dort, wo der nächste Rahmen beginnt
        return frameRange(frame).first;
    const Line& l = m_lines[std::size_t(best)];
    const QTextLine line = m_layouts[std::size_t(l.block)]->lineAt(l.index);
    return m_blockStart[std::size_t(l.block)] + line.xToCursor(p.x());
}

QPainterPath TextFlow::selection(int frame, int from, int to) const
{
    QPainterPath marked;
    for (const Line& l : m_lines) {
        if (l.frame != frame)
            continue;
        const QTextLine line = m_layouts[std::size_t(l.block)]->lineAt(l.index);
        const int blockStart = m_blockStart[std::size_t(l.block)];
        const int lineStart = blockStart + line.textStart();
        const int lineEnd = lineStart + line.textLength();
        const int a = std::max(from, lineStart);
        const int b = std::min(to, lineEnd);
        if (a > b || (a == b && b != lineEnd))
            continue;
        const double x1 = line.cursorToX(a - blockStart);
        double x2 = line.cursorToX(b - blockStart);
        if (to > lineEnd) // Zeilenende mit markieren, wie in jedem Textprogramm
            x2 += line.height() * 0.25;
        marked.addRect(QRectF(QPointF(std::min(x1, x2), line.y()), QPointF(std::max(x1, x2), line.y() + line.height())));
    }
    const double s = 1.0 / TextObject::kUnitsPerMm;
    return QTransform::fromScale(s, s).map(marked);
}

int TextFlow::verticalMove(int position, int delta) const
{
    const int li = lineOf(position);
    const int target = li + delta;
    if (li < 0 || target < 0 || target >= int(m_lines.size()))
        return position;
    const Line& from = m_lines[std::size_t(li)];
    const Line& to = m_lines[std::size_t(target)];
    const double x = m_layouts[std::size_t(from.block)]->lineAt(from.index).cursorToX(position - m_blockStart[std::size_t(from.block)]);
    return m_blockStart[std::size_t(to.block)] + m_layouts[std::size_t(to.block)]->lineAt(to.index).xToCursor(x);
}

int TextFlow::lineStart(int position) const
{
    const int li = lineOf(position);
    if (li < 0)
        return position;
    const Line& l = m_lines[std::size_t(li)];
    return m_blockStart[std::size_t(l.block)] + m_layouts[std::size_t(l.block)]->lineAt(l.index).textStart();
}

int TextFlow::lineEnd(int position) const
{
    const int li = lineOf(position);
    if (li < 0)
        return position;
    const Line& l = m_lines[std::size_t(li)];
    const QTextLayout& layout = *m_layouts[std::size_t(l.block)];
    const QTextLine line = layout.lineAt(l.index);
    int end = line.textStart() + line.textLength();
    // Umbrochene Zeile: vor dem Leerzeichen, an dem sie bricht
    if (l.index + 1 < layout.lineCount() && end > line.textStart() && layout.text().at(end - 1).isSpace())
        --end;
    return m_blockStart[std::size_t(l.block)] + end;
}

} // namespace vek
