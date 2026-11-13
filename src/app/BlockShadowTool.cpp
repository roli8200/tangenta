#include "BlockShadowTool.h"

#include "Canvas.h"
#include "Icons.h"
#include "Options.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>

#include <cmath>

namespace vek {

namespace {

constexpr double kHandle = 5.0;

bool nearPoint(const QPointF& a, const QPointF& b)
{
    return std::abs(a.x() - b.x()) <= kHandle + 2 && std::abs(a.y() - b.y()) <= kHandle + 2;
}

// Vorgaben für einen neuen Schatten (zuletzt benutzte Farbe)
BlockShadow defaults()
{
    BlockShadow s;
    const QString hex = Options::instance().text(QStringLiteral("ws/tools/blockShadow/color"));
    if (const std::optional<Color> c = Color::fromHex(hex))
        s.color = *c;
    return s;
}

} // namespace

void BlockShadowTool::activate()
{
    m_canvas.setCursor(QCursor(icon(QStringLiteral("tool-block-shadow")).pixmap(24), 3, 3));
}

void BlockShadowTool::deactivate()
{
    m_dragging = false;
    m_preview.reset();
}

std::pair<QPointF, QPointF> BlockShadowTool::handles(const DrawObject& o, const BlockShadow& s)
{
    const QPointF c = o.boundingRect().center();
    return {c, c + s.offset};
}

void BlockShadowTool::mousePress(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() != Qt::LeftButton)
        return;
    Editor& ed = m_canvas.editor();
    m_preview.reset();
    m_dragEnd = false;
    // Endgriff eines bestehenden Schattens
    const QList<const DrawObject*> sel = ed.selectedObjects();
    if (sel.size() == 1 && sel.first()->blockShadow().active) {
        const auto [start, end] = handles(*sel.first(), sel.first()->blockShadow());
        if (nearPoint(m_canvas.toScreen(end), m_canvas.toScreen(doc))) {
            m_target = sel.first()->id();
            m_dragging = true;
            m_dragEnd = true;
            m_press = doc;
            m_startOffset = sel.first()->blockShadow().offset;
            return;
        }
    }
    const DrawObject* hit = ed.objectAt(doc, m_canvas.pixelsToDoc(4.0));
    if (hit && !ed.isSelected(hit->id()))
        ed.setSelection({hit->id()});
    m_target = hit && !ed.isLocked(hit->id()) ? hit->id() : 0;
    m_dragging = m_target != 0;
    m_press = doc;
}

void BlockShadowTool::mouseMove(QMouseEvent* event, const QPointF& doc)
{
    if (!m_dragging)
        return;
    const auto* o = m_canvas.editor().document().findAs<DrawObject>(m_target);
    if (!o)
        return;
    BlockShadow s = o->blockShadow().active ? o->blockShadow() : defaults();
    s.active = true;
    QPointF d = (m_dragEnd ? m_startOffset : QPointF()) + (doc - m_press);
    // Strg: in 15°-Schritten
    if (event && (event->modifiers() & Qt::ControlModifier)) {
        const double len = std::hypot(d.x(), d.y());
        const double a = std::round(std::atan2(d.y(), d.x()) / (M_PI / 12.0)) * (M_PI / 12.0);
        d = QPointF(std::cos(a), std::sin(a)) * len;
    }
    s.offset = d;
    m_preview = s;
    m_canvas.update();
}

void BlockShadowTool::mouseRelease(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() != Qt::LeftButton || !m_dragging)
        return;
    mouseMove(event, doc);
    m_dragging = false;
    // Kurzer Klick ohne Zug: nur auswählen
    if (m_preview && std::hypot(m_preview->offset.x(), m_preview->offset.y()) > m_canvas.pixelsToDoc(3.0))
        m_canvas.editor().setObjectBlockShadow(m_target, *m_preview, QObject::tr("Blockschatten"));
    m_preview.reset();
    m_canvas.update();
    m_canvas.notifyToolState();
}

bool BlockShadowTool::keyPress(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape && m_dragging) {
        m_dragging = false;
        m_preview.reset();
        m_canvas.update();
        return true;
    }
    return false;
}

void BlockShadowTool::paintOverlay(QPainter& p)
{
    const QList<const DrawObject*> sel = m_canvas.editor().selectedObjects();
    const DrawObject* o = m_preview ? m_canvas.editor().document().findAs<DrawObject>(m_target) : (sel.size() == 1 ? sel.first() : nullptr);
    if (!o)
        return;
    const BlockShadow s = m_preview ? *m_preview : o->blockShadow();
    if (!s.active)
        return;
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    if (m_preview) {
        // Umriss des künftigen Schattens
        p.setPen(QPen(Canvas::previewColor(), 1.0));
        p.setBrush(Qt::NoBrush);
        p.drawPath(m_canvas.viewTransform().map(Canvas::blockShadowShape(*o, s)));
    }
    const auto [start, end] = handles(*o, s);
    const QPointF a = m_canvas.toScreen(start), b = m_canvas.toScreen(end);
    p.setPen(QPen(QColor(0x30, 0x30, 0x30), 1.0, Qt::DashLine));
    p.drawLine(a, b);
    p.setPen(QPen(Qt::black, 1.0));
    p.setBrush(Qt::white);
    p.drawRect(QRectF(a.x() - kHandle, a.y() - kHandle, 2 * kHandle, 2 * kHandle));
    p.setBrush(s.color.toQColor());
    p.drawRect(QRectF(b.x() - kHandle, b.y() - kHandle, 2 * kHandle, 2 * kHandle));
    p.restore();
}

} // namespace vek
