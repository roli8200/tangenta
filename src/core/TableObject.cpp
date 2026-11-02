#include "TableObject.h"

#include <QJsonArray>
#include <QObject>
#include <QPolygonF>

#include <numeric>

namespace vek {

namespace {

QList<double> even(double total, int n)
{
    return QList<double>(std::max(1, n), total / std::max(1, n));
}

double sum(const QList<double>& v)
{
    return std::accumulate(v.begin(), v.end(), 0.0);
}

} // namespace

TableObject::TableObject(const QSizeF& size, int rows, int columns)
    : m_colWidths(even(size.width(), columns)), m_rowHeights(even(size.height(), rows)),
      m_cells(std::max(1, rows) * std::max(1, columns))
{
    Style s;
    s.fill = Fill::none();
    s.stroke = Color(0, 0, 0);
    s.strokeWidth = 0.2;
    setStyle(s);
}

QSizeF TableObject::size() const
{
    return QSizeF(sum(m_colWidths), sum(m_rowHeights));
}

QPainterPath TableObject::localPath() const
{
    QPainterPath p;
    p.addRect(QRectF(QPointF(0, 0), size()));
    return p;
}

bool TableObject::hitTest(const QPointF& point, double tolerance, bool) const
{
    const QRectF r = transform().mapRect(QRectF(QPointF(0, 0), size())).adjusted(-tolerance, -tolerance, tolerance, tolerance);
    return transform().map(QPolygonF(QRectF(QPointF(0, 0), size()))).containsPoint(point, Qt::OddEvenFill) || r.contains(point);
}

void TableObject::applyTransform(const QTransform& m)
{
    DrawObject::applyTransform(m);
    QSizeF s = size();
    const QSizeF before = s;
    absorbScale(s);
    if (s != before && before.width() > 0 && before.height() > 0) {
        for (double& w : m_colWidths)
            w *= s.width() / before.width();
        for (double& h : m_rowHeights)
            h *= s.height() / before.height();
        invalidate();
    }
}

void TableObject::setRows(int rows)
{
    rows = std::clamp(rows, 1, 200);
    if (rows == this->rows())
        return;
    const double height = sum(m_rowHeights);
    const int cols = columns();
    QList<Cell> cells(rows * cols);
    for (int r = 0; r < std::min(rows, this->rows()); ++r)
        for (int c = 0; c < cols; ++c)
            cells[r * cols + c] = m_cells[index(r, c)];
    m_cells = cells;
    m_rowHeights = even(height, rows);
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c)
            m_cells[r * cols + c].rowSpan = std::min(m_cells[r * cols + c].rowSpan, rows - r);
    invalidate();
}

void TableObject::setColumns(int columns)
{
    columns = std::clamp(columns, 1, 200);
    if (columns == this->columns())
        return;
    const double width = sum(m_colWidths);
    QList<Cell> cells(rows() * columns);
    for (int r = 0; r < rows(); ++r)
        for (int c = 0; c < std::min(columns, this->columns()); ++c)
            cells[r * columns + c] = m_cells[index(r, c)];
    m_cells = cells;
    m_colWidths = even(width, columns);
    for (int r = 0; r < rows(); ++r)
        for (int c = 0; c < columns; ++c)
            m_cells[r * columns + c].colSpan = std::min(m_cells[r * columns + c].colSpan, columns - c);
    invalidate();
}

TableObject::Cell TableObject::cell(int row, int column) const
{
    if (row < 0 || column < 0 || row >= rows() || column >= columns())
        return {};
    return m_cells.value(index(row, column));
}

void TableObject::setCellText(int row, int column, const QString& text)
{
    if (row < 0 || column < 0 || row >= rows() || column >= columns())
        return;
    m_cells[index(row, column)].text = text;
    invalidate();
}

void TableObject::setCellFill(int row, int column, std::optional<Color> fill)
{
    if (row < 0 || column < 0 || row >= rows() || column >= columns())
        return;
    m_cells[index(row, column)].fill = fill;
}

void TableObject::setTextFormat(const CharFormat& f)
{
    m_format = f;
    invalidate();
}

QRectF TableObject::spanRect(int row, int column, int rowSpan, int colSpan) const
{
    double x = 0, y = 0, w = 0, h = 0;
    for (int c = 0; c < column; ++c)
        x += m_colWidths.value(c);
    for (int r = 0; r < row; ++r)
        y += m_rowHeights.value(r);
    for (int c = column; c < column + colSpan; ++c)
        w += m_colWidths.value(c);
    for (int r = row; r < row + rowSpan; ++r)
        h += m_rowHeights.value(r);
    return QRectF(x, y, w, h);
}

QRectF TableObject::cellRect(int row, int column) const
{
    const auto [ar, ac] = anchorOf(row, column);
    if (ar < 0)
        return QRectF();
    const Cell& a = m_cells[index(ar, ac)];
    return spanRect(ar, ac, a.rowSpan, a.colSpan);
}

std::pair<int, int> TableObject::anchorOf(int row, int column) const
{
    if (row < 0 || column < 0 || row >= rows() || column >= columns())
        return {-1, -1};
    // Eine verbundene Zelle weiter oben/links, die bis hierher reicht?
    for (int r = row; r >= 0; --r) {
        for (int c = column; c >= 0; --c) {
            if (r == row && c == column)
                continue;
            const Cell& k = m_cells[index(r, c)];
            if ((k.rowSpan > 1 || k.colSpan > 1) && r + k.rowSpan > row && c + k.colSpan > column)
                return {r, c};
        }
    }
    return {row, column};
}

bool TableObject::isCovered(int row, int column) const
{
    return anchorOf(row, column) != std::pair{row, column};
}

void TableObject::insertRow(int at)
{
    at = std::clamp(at, 0, rows());
    const int cols = columns();
    const double height = m_rowHeights.value(std::min(at, rows() - 1), 10.0);
    // Verbundene Zellen, in die eingefügt wird, wachsen mit
    for (int r = 0; r < rows(); ++r)
        for (int c = 0; c < cols; ++c) {
            Cell& k = m_cells[index(r, c)];
            if (r < at && r + k.rowSpan > at)
                ++k.rowSpan;
        }
    QList<Cell> row(cols);
    for (int c = cols - 1; c >= 0; --c)
        m_cells.insert(at * cols, row[c]);
    m_rowHeights.insert(at, height);
    invalidate();
}

void TableObject::insertColumn(int at)
{
    at = std::clamp(at, 0, columns());
    const int oldCols = columns();
    const double width = m_colWidths.value(std::min(at, oldCols - 1), 20.0);
    for (int r = 0; r < rows(); ++r)
        for (int c = 0; c < oldCols; ++c) {
            Cell& k = m_cells[r * oldCols + c];
            if (c < at && c + k.colSpan > at)
                ++k.colSpan;
        }
    QList<Cell> cells;
    for (int r = 0; r < rows(); ++r)
        for (int c = 0; c <= oldCols; ++c)
            cells << (c == at ? Cell() : m_cells[r * oldCols + (c > at ? c - 1 : c)]);
    m_cells = cells;
    m_colWidths.insert(at, width);
    invalidate();
}

void TableObject::removeRow(int at)
{
    if (rows() <= 1 || at < 0 || at >= rows())
        return;
    const int cols = columns();
    for (int r = 0; r < rows(); ++r)
        for (int c = 0; c < cols; ++c) {
            Cell& k = m_cells[index(r, c)];
            if (r < at && r + k.rowSpan > at)
                --k.rowSpan;
            else if (r == at && k.rowSpan > 1 && at + 1 < rows()) {
                // Anker wandert eine Zeile tiefer
                Cell moved = k;
                --moved.rowSpan;
                m_cells[index(at + 1, c)] = moved;
            }
        }
    m_cells.remove(at * cols, cols);
    m_rowHeights.removeAt(at);
    invalidate();
}

void TableObject::removeColumn(int at)
{
    if (columns() <= 1 || at < 0 || at >= columns())
        return;
    const int oldCols = columns();
    for (int r = 0; r < rows(); ++r)
        for (int c = 0; c < oldCols; ++c) {
            Cell& k = m_cells[r * oldCols + c];
            if (c < at && c + k.colSpan > at)
                --k.colSpan;
            else if (c == at && k.colSpan > 1 && at + 1 < oldCols) {
                Cell moved = k;
                --moved.colSpan;
                m_cells[r * oldCols + at + 1] = moved;
            }
        }
    QList<Cell> cells;
    for (int r = 0; r < rows(); ++r)
        for (int c = 0; c < oldCols; ++c)
            if (c != at)
                cells << m_cells[r * oldCols + c];
    m_cells = cells;
    m_colWidths.removeAt(at);
    invalidate();
}

void TableObject::distributeRows(int from, int to)
{
    from = std::clamp(from, 0, rows() - 1);
    to = std::clamp(to, from, rows() - 1);
    double total = 0;
    for (int r = from; r <= to; ++r)
        total += m_rowHeights[r];
    for (int r = from; r <= to; ++r)
        m_rowHeights[r] = total / (to - from + 1);
    invalidate();
}

void TableObject::distributeColumns(int from, int to)
{
    from = std::clamp(from, 0, columns() - 1);
    to = std::clamp(to, from, columns() - 1);
    double total = 0;
    for (int c = from; c <= to; ++c)
        total += m_colWidths[c];
    for (int c = from; c <= to; ++c)
        m_colWidths[c] = total / (to - from + 1);
    invalidate();
}

void TableObject::merge(const Range& range)
{
    Range rg{std::clamp(std::min(range.row0, range.row1), 0, rows() - 1), std::clamp(std::min(range.col0, range.col1), 0, columns() - 1),
             std::clamp(std::max(range.row0, range.row1), 0, rows() - 1), std::clamp(std::max(range.col0, range.col1), 0, columns() - 1)};
    if (rg.row0 == rg.row1 && rg.col0 == rg.col1)
        return;
    QStringList texts;
    std::optional<Color> fill = m_cells[index(rg.row0, rg.col0)].fill;
    for (int r = rg.row0; r <= rg.row1; ++r)
        for (int c = rg.col0; c <= rg.col1; ++c) {
            Cell& k = m_cells[index(r, c)];
            if (!k.text.isEmpty())
                texts << k.text;
            k = Cell();
        }
    Cell& a = m_cells[index(rg.row0, rg.col0)];
    a.text = texts.join(QLatin1Char(' '));
    a.fill = fill;
    a.rowSpan = rg.row1 - rg.row0 + 1;
    a.colSpan = rg.col1 - rg.col0 + 1;
    invalidate();
}

void TableObject::unmerge(int row, int column)
{
    const auto [ar, ac] = anchorOf(row, column);
    if (ar < 0)
        return;
    Cell& a = m_cells[index(ar, ac)];
    a.rowSpan = 1;
    a.colSpan = 1;
    invalidate();
}

void TableObject::splitIntoRows(int row, int column, int n)
{
    auto [ar, ac] = anchorOf(row, column);
    if (ar < 0 || n < 2)
        return;
    const Cell anchor = m_cells[index(ar, ac)];
    double total = 0;
    for (int r = ar; r < ar + anchor.rowSpan; ++r)
        total += m_rowHeights[r];
    const int extra = std::max(0, n - anchor.rowSpan);
    const int insertAt = ar + anchor.rowSpan;
    for (int i = 0; i < extra; ++i)
        insertRow(insertAt);
    // Nachbarn in den alten Zeilen, die dort enden, über die neuen Zeilen verlängern
    for (int r = ar; r < insertAt; ++r)
        for (int c = 0; c < columns(); ++c) {
            if (c >= ac && c < ac + anchor.colSpan)
                continue;
            Cell& k = m_cells[index(r, c)];
            if ((k.rowSpan > 1 || k.colSpan > 1 || !isCovered(r, c)) && r + k.rowSpan == insertAt)
                k.rowSpan += extra;
        }
    // Die Zelle selbst: n Zeilen mit je einer Zelle (Breite bleibt)
    const int span = anchor.rowSpan + extra;
    for (int i = 0; i < n; ++i) {
        Cell& k = m_cells[index(ar + i, ac)];
        k.rowSpan = (i == n - 1) ? span - (n - 1) : 1;
        k.colSpan = anchor.colSpan;
        if (i > 0) {
            k.text.clear();
            k.fill = anchor.fill;
        }
    }
    for (int r = ar; r < ar + span; ++r)
        m_rowHeights[r] = total / span;
    invalidate();
}

void TableObject::splitIntoColumns(int row, int column, int n)
{
    auto [ar, ac] = anchorOf(row, column);
    if (ar < 0 || n < 2)
        return;
    const Cell anchor = m_cells[index(ar, ac)];
    double total = 0;
    for (int c = ac; c < ac + anchor.colSpan; ++c)
        total += m_colWidths[c];
    const int extra = std::max(0, n - anchor.colSpan);
    const int insertAt = ac + anchor.colSpan;
    for (int i = 0; i < extra; ++i)
        insertColumn(insertAt);
    for (int r = 0; r < rows(); ++r)
        for (int c = ac; c < insertAt; ++c) {
            if (r >= ar && r < ar + anchor.rowSpan)
                continue;
            Cell& k = m_cells[index(r, c)];
            if ((k.rowSpan > 1 || k.colSpan > 1 || !isCovered(r, c)) && c + k.colSpan == insertAt)
                k.colSpan += extra;
        }
    const int span = anchor.colSpan + extra;
    for (int i = 0; i < n; ++i) {
        Cell& k = m_cells[index(ar, ac + i)];
        k.colSpan = (i == n - 1) ? span - (n - 1) : 1;
        k.rowSpan = anchor.rowSpan;
        if (i > 0) {
            k.text.clear();
            k.fill = anchor.fill;
        }
    }
    for (int c = ac; c < ac + span; ++c)
        m_colWidths[c] = total / span;
    invalidate();
}

std::pair<int, int> TableObject::cellAt(const QPointF& local) const
{
    if (local.x() < 0 || local.y() < 0)
        return {-1, -1};
    int col = -1, row = -1;
    double x = 0;
    for (int c = 0; c < columns(); ++c) {
        x += m_colWidths[c];
        if (local.x() <= x) {
            col = c;
            break;
        }
    }
    double y = 0;
    for (int r = 0; r < rows(); ++r) {
        y += m_rowHeights[r];
        if (local.y() <= y) {
            row = r;
            break;
        }
    }
    return col < 0 || row < 0 ? std::pair{-1, -1} : anchorOf(row, col);
}

QPainterPath TableObject::gridPath() const
{
    // Rahmen jeder (Anker-)Zelle; verbundene Zellen haben keine Linien im Innern
    QPainterPath p;
    for (int r = 0; r < rows(); ++r)
        for (int c = 0; c < columns(); ++c)
            if (!isCovered(r, c))
                p.addRect(cellRect(r, c));
    return p;
}

QList<TextObject::Piece> TableObject::textPieces() const
{
    if (!m_cache) {
        QList<TextObject::Piece> all;
        for (int r = 0; r < rows(); ++r) {
            for (int c = 0; c < columns(); ++c) {
                const QString text = m_cells[index(r, c)].text;
                if (text.isEmpty() || isCovered(r, c))
                    continue;
                const QRectF cr = cellRect(r, c).adjusted(kCellMargin, kCellMargin, -kCellMargin, -kCellMargin);
                if (cr.width() <= 0 || cr.height() <= 0)
                    continue;
                // Mengentext im Zellrahmen (bricht um, Überlauf bleibt verborgen)
                TextObject cellText(TextObject::Kind::Paragraph, cr.size());
                cellText.setPlainText(text, m_format);
                for (TextObject::Piece piece : cellText.pieces()) {
                    piece.path.translate(cr.topLeft());
                    bool merged = false;
                    for (TextObject::Piece& existing : all) {
                        if (existing.color == piece.color) {
                            existing.path.addPath(piece.path);
                            merged = true;
                            break;
                        }
                    }
                    if (!merged)
                        all << piece;
                }
            }
        }
        m_cache = std::make_shared<const QList<TextObject::Piece>>(all);
    }
    return *m_cache;
}

QList<PropertyDescriptor> TableObject::propertyDescriptors() const
{
    using K = PropertyDescriptor::Kind;
    PropertyDescriptor rows{QStringLiteral("rows"), QObject::tr("Zeilen"), QStringLiteral("graph-rows"), K::Number};
    rows.minimum = 1;
    rows.maximum = 200;
    rows.stackWithNext = true;
    PropertyDescriptor cols{QStringLiteral("columns"), QObject::tr("Spalten"), QStringLiteral("graph-columns"), K::Number};
    cols.minimum = 1;
    cols.maximum = 200;
    PropertyDescriptor size{QStringLiteral("fontSize"), QObject::tr("Schriftgrösse der Zellen (pt)"), QStringLiteral("font"), K::Number};
    size.minimum = 1;
    size.maximum = 500;
    size.separatorBefore = true;
    return {rows, cols, size};
}

QVariant TableObject::propertyValue(const QString& id) const
{
    if (id == QLatin1String("rows"))
        return rows();
    if (id == QLatin1String("columns"))
        return columns();
    if (id == QLatin1String("fontSize"))
        return m_format.sizePt;
    return {};
}

bool TableObject::setPropertyValue(const QString& id, const QVariant& value)
{
    if (id == QLatin1String("rows"))
        setRows(value.toInt());
    else if (id == QLatin1String("columns"))
        setColumns(value.toInt());
    else if (id == QLatin1String("fontSize")) {
        m_format.sizePt = std::max(1.0, value.toDouble());
        invalidate();
    } else
        return false;
    return true;
}

void TableObject::writeProperties(QJsonObject& out) const
{
    DrawObject::writeProperties(out);
    QJsonArray cols, rowsArr, cells;
    for (double w : m_colWidths)
        cols << w;
    for (double h : m_rowHeights)
        rowsArr << h;
    for (const Cell& c : m_cells) {
        QJsonObject o;
        if (!c.text.isEmpty())
            o[QStringLiteral("text")] = c.text;
        if (c.fill)
            o[QStringLiteral("fill")] = c.fill->toHex();
        if (c.rowSpan > 1)
            o[QStringLiteral("rowSpan")] = c.rowSpan;
        if (c.colSpan > 1)
            o[QStringLiteral("colSpan")] = c.colSpan;
        cells << o;
    }
    out[QStringLiteral("columns")] = cols;
    out[QStringLiteral("rows")] = rowsArr;
    out[QStringLiteral("cells")] = cells;
    out[QStringLiteral("font")] = m_format.family;
    out[QStringLiteral("size")] = m_format.sizePt;
}

void TableObject::readProperties(const QJsonObject& in)
{
    DrawObject::readProperties(in);
    m_colWidths.clear();
    m_rowHeights.clear();
    m_cells.clear();
    for (const QJsonValue& v : in.value(QStringLiteral("columns")).toArray())
        m_colWidths << v.toDouble();
    for (const QJsonValue& v : in.value(QStringLiteral("rows")).toArray())
        m_rowHeights << v.toDouble();
    if (m_colWidths.isEmpty())
        m_colWidths << 10.0;
    if (m_rowHeights.isEmpty())
        m_rowHeights << 10.0;
    const QJsonArray cells = in.value(QStringLiteral("cells")).toArray();
    m_cells = QList<Cell>(rows() * columns());
    for (int i = 0; i < m_cells.size() && i < cells.size(); ++i) {
        const QJsonObject o = cells[i].toObject();
        m_cells[i].text = o.value(QStringLiteral("text")).toString();
        if (o.contains(QStringLiteral("fill")))
            m_cells[i].fill = Color::fromHex(o.value(QStringLiteral("fill")).toString());
        m_cells[i].rowSpan = std::max(1, o.value(QStringLiteral("rowSpan")).toInt(1));
        m_cells[i].colSpan = std::max(1, o.value(QStringLiteral("colSpan")).toInt(1));
    }
    m_format = CharFormat(in.value(QStringLiteral("font")).toString(QStringLiteral("Arial")), in.value(QStringLiteral("size")).toDouble(10.0));
    invalidate();
}

} // namespace vek
