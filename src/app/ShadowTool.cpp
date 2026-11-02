#include "ShadowTool.h"

#include "Canvas.h"
#include "GradientSlider.h"
#include "Icons.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QtMath>

#include <cmath>

namespace vek {

namespace {

constexpr double kHandle = 5.0;

} // namespace

void ShadowTool::activate()
{
    m_canvas.setCursor(QCursor(icon(QStringLiteral("tool-shadow")).pixmap(24), 3, 3));
}

void ShadowTool::deactivate()
{
    m_dragging = false;
    m_preview.reset();
}

const DrawObject* ShadowTool::target() const
{
    const QList<const DrawObject*> objects = m_canvas.editor().selectedObjects();
    return objects.size() == 1 ? objects.first() : nullptr;
}

DropShadow ShadowTool::shadowFor(const DrawObject& o, const QPointF& press, const QPointF& to, const DropShadow& base)
{
    DropShadow s = base;
    s.active = true;
    const QRectF b = o.boundingRect();
    const double edge = std::max(1.0, b.height() * 0.15);
    const bool bottom = std::abs(press.y() - b.bottom()) <= edge;
    const bool top = !bottom && std::abs(press.y() - b.top()) <= edge;
    if (bottom || top) {
        // Perspektivisch: Kantenmitte -> Zugende bestimmt Neigung und Länge
        s.perspective = true;
        s.fromTop = top;
        const QPointF base0(b.center().x(), top ? b.top() : b.bottom());
        const QPointF d = to - base0;
        const double h = std::max(1e-6, b.height());
        const double up = top ? d.y() : -d.y(); // Länge vom Objekt weg
        s.stretch = std::clamp(std::abs(up) / h * 100.0, 5.0, 400.0);
        s.angle = std::clamp(qRadiansToDegrees(std::atan2(-d.x() * (top ? -1 : 1), std::max(1e-6, std::abs(up)))), -85.0, 85.0);
    } else {
        s.perspective = false;
        s.offset = (to - press) / kGearing;
    }
    return s;
}

std::pair<QPointF, QPointF> ShadowTool::handles(const DrawObject& o, const DropShadow& s)
{
    const QRectF b = o.boundingRect();
    if (!s.perspective)
        return {b.center(), b.center() + s.offset * kGearing};
    const QPointF base0(b.center().x(), s.fromTop ? b.top() : b.bottom());
    const QTransform t = Canvas::shadowTransform(b, s);
    return {base0, t.map(QPointF(b.center().x(), s.fromTop ? b.bottom() : b.top()))};
}

void ShadowTool::mousePress(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() != Qt::LeftButton)
        return;
    Editor& ed = m_canvas.editor();
    const DrawObject* o = target();
    // Endgriff eines bestehenden Schattens?
    if (o && o->shadow().active) {
        const auto [start, end] = handles(*o, o->shadow());
        const QPointF s = m_canvas.toScreen(end), m = m_canvas.toScreen(doc);
        if (std::abs(s.x() - m.x()) <= kHandle + 2 && std::abs(s.y() - m.y()) <= kHandle + 2) {
            m_target = o->id();
            m_press = start;
            m_dragging = true;
            m_dragEnd = true;
            m_dragOpacity = false;
            return;
        }
        // Deckungsregler: seine Lage auf der Linie ist die Deckung
        if (slider::hit(m, m_canvas.toScreen(start), s, std::clamp(o->shadow().opacity / 100.0, 0.0, 1.0))) {
            m_target = o->id();
            m_press = doc;
            m_dragging = true;
            m_dragEnd = false;
            m_dragOpacity = true;
            m_preview = o->shadow();
            return;
        }
    }
    const DrawObject* hit = ed.objectAt(doc, m_canvas.pixelsToDoc(4.0));
    if (!hit) {
        ed.clearSelection();
        return;
    }
    if (!ed.isSelected(hit->id()))
        ed.setSelection({hit->id()});
    m_target = hit->id();
    m_press = doc;
    m_dragging = true;
    m_dragEnd = false;
    m_dragOpacity = false;
    m_preview.reset();
}

void ShadowTool::mouseMove(QMouseEvent*, const QPointF& doc)
{
    if (!m_dragging)
        return;
    const auto* o = m_canvas.editor().document().findAs<DrawObject>(m_target);
    if (!o)
        return;
    DropShadow base = o->shadow();
    if (!base.active)
        base = DropShadow();
    if (m_dragOpacity) {
        const auto [start, end] = handles(*o, base);
        m_preview = base;
        m_preview->opacity = std::round(slider::fraction(m_canvas.toScreen(doc), m_canvas.toScreen(start), m_canvas.toScreen(end), 0.0, 1.0) * 100.0);
    } else if (m_dragEnd) {
        // Endgriff: gleiche Art, neues Ende
        if (base.perspective) {
            const QRectF b = o->boundingRect();
            m_preview = shadowFor(*o, QPointF(b.center().x(), base.fromTop ? b.top() : b.bottom()), doc, base);
        } else {
            m_preview = base;
            m_preview->offset = (doc - o->boundingRect().center()) / kGearing;
        }
    } else {
        m_preview = shadowFor(*o, m_press, doc, base);
    }
    m_canvas.update();
}

void ShadowTool::mouseRelease(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() != Qt::LeftButton || !m_dragging)
        return;
    mouseMove(event, doc);
    m_dragging = false;
    if (m_preview && m_dragOpacity)
        m_canvas.editor().setObjectShadow(m_target, *m_preview, QObject::tr("Schattendeckung"));
    else if (m_preview && std::hypot(doc.x() - m_press.x(), doc.y() - m_press.y()) > m_canvas.pixelsToDoc(2.0))
        m_canvas.editor().setObjectShadow(m_target, *m_preview, QObject::tr("Hinterlegter Schatten"));
    m_dragOpacity = false;
    m_preview.reset();
    m_canvas.update();
    m_canvas.notifyToolState();
}

bool ShadowTool::keyPress(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape && m_dragging) {
        m_dragging = false;
        m_preview.reset();
        m_canvas.update();
        return true;
    }
    return false;
}

void ShadowTool::paintOverlay(QPainter& p)
{
    const DrawObject* o = m_dragging ? m_canvas.editor().document().findAs<DrawObject>(m_target) : target();
    if (!o)
        return;
    p.save();
    if (m_preview) {
        // Vorschau: Schatten, darüber das Objekt
        m_canvas.paintShadow(p, *o, *m_preview);
        m_canvas.paintObjectDirect(p, *o);
    }
    const DropShadow s = m_preview ? *m_preview : o->shadow();
    if (s.active) {
        const auto [start, end] = handles(*o, s);
        const QPointF a = m_canvas.toScreen(start), e = m_canvas.toScreen(end);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(QColor(0x30, 0x30, 0x30), 1.0, Qt::DashLine));
        p.drawLine(a, e);
        p.setPen(QPen(Qt::black, 1.0));
        p.setBrush(Qt::white);
        p.drawRect(QRectF(a.x() - kHandle, a.y() - kHandle, 2 * kHandle, 2 * kHandle));
        p.setBrush(s.color.toQColor());
        p.drawRect(QRectF(e.x() - kHandle, e.y() - kHandle, 2 * kHandle, 2 * kHandle));
        // Deckungsregler auf der Linie (Lage = Deckung)
        slider::paint(p, a, e, std::clamp(s.opacity / 100.0, 0.0, 1.0),
                      m_dragOpacity ? QObject::tr("Deckung %1 %").arg(std::lround(s.opacity)) : QString());
    }
    p.restore();
}

} // namespace vek
