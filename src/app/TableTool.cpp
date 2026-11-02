#include "TableTool.h"

#include "Canvas.h"
#include "Options.h"
#include "Serialization.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>

namespace vek {

TableTool::TableTool(Canvas& canvas) : Tool(canvas) {}

TableTool::~TableTool() = default;

void TableTool::activate()
{
    m_canvas.setCursor(Qt::CrossCursor);
}

void TableTool::deactivate()
{
    finishEditing();
    m_dragging = false;
    m_snap = SnapResult();
}

QPointF TableTool::snapped(const QPointF& doc)
{
    m_snap = SnapEngine(m_canvas).snap(doc);
    return m_snap.snapped() ? m_snap.point : doc;
}

void TableTool::beginEditing(NodeId table, int row, int column)
{
    const auto* original = m_canvas.editor().document().findAs<TableObject>(table);
    if (!original || m_canvas.editor().isLocked(table))
        return;
    if (m_target != table) {
        finishEditing();
        std::unique_ptr<Node> copy = serial::fromJson(serial::toJson(*original), serial::Ids::Keep);
        m_proto.reset(static_cast<TableObject*>(copy.release()));
        m_target = table;
        m_canvas.setHiddenObject(table);
    }
    m_row = std::clamp(row, 0, m_proto->rows() - 1);
    m_col = std::clamp(column, 0, m_proto->columns() - 1);
    m_anchorRow = m_row;
    m_anchorCol = m_col;
    m_canvas.update();
}

TableObject::Range TableTool::selection() const
{
    if (!m_proto)
        return {};
    TableObject::Range r{std::min(m_row, m_anchorRow), std::min(m_col, m_anchorCol), std::max(m_row, m_anchorRow),
                         std::max(m_col, m_anchorCol)};
    // Verbundene Zellen ganz einschliessen
    bool grown = true;
    while (grown) {
        grown = false;
        for (int row = r.row0; row <= r.row1; ++row)
            for (int col = r.col0; col <= r.col1; ++col) {
                const auto [ar, ac] = m_proto->anchorOf(row, col);
                const TableObject::Cell a = m_proto->cell(ar, ac);
                const TableObject::Range span{ar, ac, ar + a.rowSpan - 1, ac + a.colSpan - 1};
                if (span.row0 < r.row0 || span.col0 < r.col0 || span.row1 > r.row1 || span.col1 > r.col1) {
                    r = {std::min(r.row0, span.row0), std::min(r.col0, span.col0), std::max(r.row1, span.row1), std::max(r.col1, span.col1)};
                    grown = true;
                }
            }
    }
    return r;
}

void TableTool::selectRange(const TableObject::Range& range)
{
    if (!m_proto)
        return;
    m_anchorRow = std::clamp(range.row0, 0, m_proto->rows() - 1);
    m_anchorCol = std::clamp(range.col0, 0, m_proto->columns() - 1);
    m_row = std::clamp(range.row1, 0, m_proto->rows() - 1);
    m_col = std::clamp(range.col1, 0, m_proto->columns() - 1);
    m_canvas.update();
}

void TableTool::finishEditing()
{
    if (!m_proto)
        return;
    Editor& ed = m_canvas.editor();
    const std::unique_ptr<TableObject> result = std::move(m_proto);
    const NodeId target = m_target;
    m_target = 0;
    m_canvas.setHiddenObject(0);
    const auto* original = ed.document().findAs<TableObject>(target);
    if (!original)
        return;
    bool changed = original->rows() != result->rows() || original->columns() != result->columns();
    for (int r = 0; !changed && r < result->rows(); ++r)
        for (int c = 0; !changed && c < result->columns(); ++c)
            changed = !(original->cell(r, c) == result->cell(r, c));
    if (changed) {
        ed.modifyTable(target, [&result](TableObject& t) {
            for (int r = 0; r < t.rows(); ++r) {
                for (int c = 0; c < t.columns(); ++c) {
                    const TableObject::Cell cell = result->cell(r, c);
                    t.setCellText(r, c, cell.text);
                    t.setCellFill(r, c, cell.fill);
                }
            }
        }, QObject::tr("Tabelle bearbeiten"));
    }
    ed.setSelection({target});
    m_canvas.update();
}

void TableTool::step(int direction)
{
    if (!m_proto)
        return;
    const int cols = m_proto->columns();
    const int count = m_proto->rows() * cols;
    // Tabulatorreihenfolge: 0 = links nach rechts, 1 = rechts nach links (je von oben nach unten)
    const bool rightToLeft = Options::instance().integer(QStringLiteral("ws/tools/table/tabOrder")) == 1;
    int index = m_row * cols + (rightToLeft ? cols - 1 - m_col : m_col);
    // Verdeckte Zellen verbundener Bereiche überspringen
    for (int guard = 0; guard < count; ++guard) {
        index = ((index + direction) % count + count) % count;
        m_row = index / cols;
        m_col = rightToLeft ? cols - 1 - index % cols : index % cols;
        if (!m_proto->isCovered(m_row, m_col))
            break;
    }
    m_anchorRow = m_row;
    m_anchorCol = m_col;
    m_canvas.update();
}

void TableTool::mousePress(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() != Qt::LeftButton)
        return;
    Editor& ed = m_canvas.editor();
    const double tolerance = m_canvas.pixelsToDoc(3.0);
    // In der bearbeiteten Tabelle: nur die Zelle wechseln
    if (m_proto) {
        const auto [r, c] = m_proto->cellAt(m_proto->transform().inverted().map(doc));
        if (r >= 0) {
            m_row = r;
            m_col = c;
            if (!(event->modifiers() & Qt::ShiftModifier)) {
                m_anchorRow = r;
                m_anchorCol = c;
            }
            m_selecting = true;
            m_canvas.update();
            return;
        }
    }
    if (const auto* table = node_cast<TableObject>(ed.objectAt(doc, tolerance))) {
        const auto [r, c] = table->cellAt(table->transform().inverted().map(doc));
        if (r >= 0) {
            beginEditing(table->id(), r, c);
            m_selecting = true; // gleich weiterziehen markiert einen Bereich
            return;
        }
    }
    finishEditing();
    m_dragging = true;
    m_start = m_current = snapped(doc);
}

void TableTool::mouseMove(QMouseEvent*, const QPointF& doc)
{
    if (m_selecting && m_proto) {
        // Ziehen über Zellen markiert einen Bereich
        const auto [r, c] = m_proto->cellAt(m_proto->transform().inverted().map(doc));
        if (r >= 0 && (r != m_row || c != m_col)) {
            m_row = r;
            m_col = c;
            m_canvas.update();
        }
        return;
    }
    m_current = snapped(doc);
    m_canvas.update();
}

void TableTool::mouseRelease(QMouseEvent* event, const QPointF& doc)
{
    m_selecting = false;
    if (event->button() != Qt::LeftButton || !m_dragging)
        return;
    m_dragging = false;
    m_current = snapped(doc);
    m_snap = SnapResult();
    const QRectF r = QRectF(m_start, m_current).normalized();
    m_canvas.update();
    const double minSize = m_canvas.pixelsToDoc(2.0);
    if (r.width() < minSize || r.height() < minSize)
        return;
    const Options& o = Options::instance();
    auto table = std::make_unique<TableObject>(r.size(), std::max(1, o.integer(QStringLiteral("ws/tools/table/rows"))),
                                               std::max(1, o.integer(QStringLiteral("ws/tools/table/cols"))));
    table->setTransform(QTransform::fromTranslate(r.left(), r.top()));
    Editor& ed = m_canvas.editor();
    ed.addObject(std::move(table), QObject::tr("Tabelle erstellen"));
    // Werkzeug bleibt aktiv: ein Klick in eine Zelle beginnt das Schreiben
}

bool TableTool::keyPress(QKeyEvent* e)
{
    if (!m_proto)
        return false;
    QString text = m_proto->cell(m_row, m_col).text;
    switch (e->key()) {
    case Qt::Key_Escape:
        finishEditing();
        return true;
    case Qt::Key_Tab:
    case Qt::Key_Backtab:
        if (e->key() == Qt::Key_Backtab || (e->modifiers() & Qt::ShiftModifier)) {
            step(-1);
        } else if (Options::instance().integer(QStringLiteral("ws/tools/table/tabMode")) == 1) {
            step(1);
        } else {
            m_proto->setCellText(m_row, m_col, text + QLatin1Char('\t'));
            m_canvas.update();
        }
        return true;
    case Qt::Key_Left:
        step(-1);
        return true;
    case Qt::Key_Right:
        step(1);
        return true;
    case Qt::Key_Up:
        m_row = m_proto->anchorOf((m_row + m_proto->rows() - 1) % m_proto->rows(), m_col).first;
        m_anchorRow = m_row;
        m_anchorCol = m_col;
        m_canvas.update();
        return true;
    case Qt::Key_Down: {
        const TableObject::Cell here = m_proto->cell(m_row, m_col);
        m_row = m_proto->anchorOf((m_row + std::max(1, here.rowSpan)) % m_proto->rows(), m_col).first;
        m_anchorRow = m_row;
        m_anchorCol = m_col;
        m_canvas.update();
        return true;
    }
    case Qt::Key_Backspace:
        if (!text.isEmpty()) {
            text.chop(1);
            m_proto->setCellText(m_row, m_col, text);
            m_canvas.update();
        }
        return true;
    case Qt::Key_Delete:
        m_proto->setCellText(m_row, m_col, QString());
        m_canvas.update();
        return true;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        m_proto->setCellText(m_row, m_col, text + QLatin1Char('\n'));
        m_canvas.update();
        return true;
    default:
        break;
    }
    const QString typed = e->text();
    if (typed.isEmpty() || !typed.at(0).isPrint() || (e->modifiers() & Qt::ControlModifier))
        return false;
    m_proto->setCellText(m_row, m_col, text + typed);
    m_canvas.update();
    return true;
}

bool TableTool::paletteColor(const std::optional<Color>& color, bool fill)
{
    if (!m_proto || !fill)
        return false;
    const TableObject::Range sel = selection();
    for (int r = sel.row0; r <= sel.row1; ++r)
        for (int c = sel.col0; c <= sel.col1; ++c)
            m_proto->setCellFill(r, c, color);
    m_canvas.update();
    return true;
}

void TableTool::paintOverlay(QPainter& p)
{
    p.save();
    const QTransform view = m_canvas.viewTransform();
    if (m_proto) {
        // Arbeitskopie an Stelle der verborgenen Tabelle, aktuelle Zelle hervorgehoben
        m_canvas.paintObjectDirect(p, *m_proto);
        const TableObject::Range sel = selection();
        const QRectF r = m_proto->cellRect(sel.row0, sel.col0).united(m_proto->cellRect(sel.row1, sel.col1));
        const QPolygonF cell = (m_proto->transform() * view).map(QPolygonF(r));
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(Canvas::previewColor(), 2.0));
        p.setBrush(QColor(0x40, 0x80, 0xff, 0x28));
        p.drawPolygon(cell);
    }
    if (m_dragging) {
        const QRectF r = QRectF(m_start, m_current).normalized();
        if (r.width() > 0 && r.height() > 0) {
            const Options& o = Options::instance();
            TableObject preview(r.size(), std::max(1, o.integer(QStringLiteral("ws/tools/table/rows"))),
                                std::max(1, o.integer(QStringLiteral("ws/tools/table/cols"))));
            p.setPen(QPen(Canvas::previewColor(), 1.0));
            p.setBrush(Qt::NoBrush);
            p.drawPath((QTransform::fromTranslate(r.left(), r.top()) * view).map(preview.gridPath()));
        }
    }
    m_canvas.paintSnapHint(p, m_snap);
    p.restore();
}

} // namespace vek
