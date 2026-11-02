#include "BezierTool.h"

#include "Canvas.h"
#include "Editor.h"

#include <cmath>

namespace vek {

namespace {

constexpr double kNodeRadius = 6.0;    // px
constexpr double kDragThreshold = 3.0; // px

double distance(const QPointF& a, const QPointF& b)
{
    return std::hypot(a.x() - b.x(), a.y() - b.y());
}

} // namespace

void BezierTool::activate()
{
    m_canvas.setCursor(Qt::CrossCursor);
}

void BezierTool::deactivate()
{
    // Werkzeugwechsel beendet die Kurve
    finish(false);
}

void BezierTool::reset()
{
    m_nodes.clear();
    m_continue = CurveEnd();
    m_pressed = false;
    m_dragging = false;
    m_snap = SnapResult();
    m_canvas.update();
}

void BezierTool::mousePress(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() != Qt::LeftButton)
        return;
    m_pressScreen = event->position();
    if (m_nodes.isEmpty()) {
        m_continue = curveEndAt(m_canvas, event->position());
        m_snap = m_continue.object != InvalidNodeId ? SnapResult() : SnapEngine(m_canvas).snap(doc);
    } else if (m_nodes.size() >= 2 && m_continue.object == InvalidNodeId
               && distance(m_canvas.viewTransform().map(m_nodes.first().point), event->position()) <= kNodeRadius) {
        finish(true); // Klick auf den ersten Knoten schliesst die Kurve
        return;
    } else {
        m_snap = SnapEngine(m_canvas).snap(doc);
    }
    QPointF point = m_snap.snapped() ? m_snap.point : doc;
    if (m_nodes.isEmpty() && m_continue.object != InvalidNodeId)
        point = m_continue.doc;
    m_nodes.append(Placed{point, {}, {}});
    m_pressed = true;
    m_dragging = false;
    m_mouse = point;
    m_canvas.update();
}

void BezierTool::mouseMove(QMouseEvent* event, const QPointF& doc)
{
    if (m_pressed) {
        if (!m_dragging && distance(event->position(), m_pressScreen) >= kDragThreshold)
            m_dragging = true;
        if (m_dragging) {
            // Ziehen formt einen symmetrischen Knoten: Ausgang folgt der Maus, Eingang gespiegelt
            Placed& n = m_nodes.last();
            n.out = doc;
            n.in = 2.0 * n.point - doc;
        }
        m_snap = SnapResult();
        m_mouse = doc;
    } else {
        m_snap = SnapEngine(m_canvas).snap(doc);
        m_mouse = m_snap.snapped() ? m_snap.point : doc;
    }
    m_canvas.update();
}

void BezierTool::mouseRelease(QMouseEvent* event, const QPointF& doc)
{
    Q_UNUSED(doc);
    if (event->button() != Qt::LeftButton)
        return;
    m_pressed = false;
    m_dragging = false;
    m_canvas.update();
}

void BezierTool::mouseDoubleClick(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() != Qt::LeftButton)
        return;
    // Normalerweise hat der erste Klick des Doppelklicks den letzten Knoten schon gesetzt
    if (!m_nodes.isEmpty()
        && distance(m_canvas.viewTransform().map(m_nodes.last().point), event->position()) > kNodeRadius)
        m_nodes.append(Placed{doc, {}, {}});
    finish(false);
}

bool BezierTool::keyPress(QKeyEvent* event)
{
    if (m_nodes.isEmpty())
        return false;
    switch (event->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        finish(false);
        return true;
    case Qt::Key_Escape:
        reset();
        return true;
    case Qt::Key_Space:
        finish(false);
        return false; // Leertaste wechselt danach wie gewohnt zum Auswahlwerkzeug
    default:
        return false;
    }
}

CurveNode BezierTool::segmentTo(const Placed& from, const Placed& to)
{
    CurveNode node{to.point};
    if (from.out || to.in) {
        node.curveIn = true;
        node.c1 = from.out.value_or(from.point);
        node.c2 = to.in.value_or(to.point);
    }
    if (to.in && to.out)
        node.kind = NodeKind::Symmetric;
    return node;
}

void BezierTool::finish(bool closed)
{
    const QList<Placed> nodes = m_nodes;
    const CurveEnd from = m_continue;
    reset();
    if (nodes.size() < 2)
        return;
    Editor& ed = m_canvas.editor();
    const auto* existing = from.object != InvalidNodeId ? ed.document().findAs<CurveObject>(from.object) : nullptr;
    if (existing) {
        const QTransform toLocal = existing->transform().inverted();
        ed.modifyCurve(existing->id(), [&](CurveObject& c) {
            if (from.atStart)
                c.reverse(from.path); // am Anfang fortsetzen = umgekehrt am Ende anhängen
            QList<SubPath> paths = c.subPaths();
            QList<CurveNode>& target = paths[from.path].nodes;
            for (int i = 1; i < nodes.size(); ++i) {
                CurveNode n = segmentTo(nodes[i - 1], nodes[i]);
                n.point = toLocal.map(n.point);
                n.c1 = toLocal.map(n.c1);
                n.c2 = toLocal.map(n.c2);
                target.append(n);
            }
            c.setSubPaths(paths);
        }, QObject::tr("Kurve fortsetzen"));
        return;
    }
    SubPath path;
    path.closed = closed;
    for (int i = 0; i < nodes.size(); ++i)
        path.nodes.append(i == 0 ? CurveNode{nodes[0].point} : segmentTo(nodes[i - 1], nodes[i]));
    if (closed) {
        const CurveNode closing = segmentTo(nodes.last(), nodes.first());
        path.nodes.first() = closing;
    } else if (nodes.first().in && nodes.first().out) {
        path.nodes.first().kind = NodeKind::Symmetric;
    }
    auto curve = std::make_unique<CurveObject>(QList<SubPath>{path});
    Style style = ed.defaultStyle();
    style.fill = Fill::none();
    curve->setStyle(style);
    ed.addObject(std::move(curve), QObject::tr("Kurve zeichnen"));
}

void BezierTool::paintOverlay(QPainter& p)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    const QTransform view = m_canvas.viewTransform();
    if (!m_nodes.isEmpty()) {
        QPainterPath path(view.map(m_nodes.first().point));
        auto segment = [&](const Placed& a, const Placed& b) {
            const CurveNode n = segmentTo(a, b);
            if (n.curveIn)
                path.cubicTo(view.map(n.c1), view.map(n.c2), view.map(n.point));
            else
                path.lineTo(view.map(n.point));
        };
        for (int i = 1; i < m_nodes.size(); ++i)
            segment(m_nodes[i - 1], m_nodes[i]);
        if (!m_pressed)
            segment(m_nodes.last(), Placed{m_mouse, {}, {}}); // Gummiband zur Maus
        p.setBrush(Qt::NoBrush);
        if (m_pen) {
            // Stift: die Linie so, wie sie entsteht (Farbe und Breite der Vorgabe)
            const Style s = m_canvas.editor().defaultStyle();
            p.setPen(QPen(s.stroke.value_or(Color(0, 0, 0)).toQColor(), std::max(1.0, s.strokeWidth * m_canvas.zoom())));
            p.drawPath(path);
        }
        p.setPen(QPen(Canvas::previewColor(), 1.0));
        p.drawPath(path);

        // Knoten als Quadrate, am letzten Knoten die Steuerpunkte
        p.setPen(QPen(Qt::black, 1.0));
        p.setBrush(Qt::white);
        for (const Placed& n : std::as_const(m_nodes)) {
            const QPointF s = view.map(n.point);
            p.drawRect(QRectF(s - QPointF(3, 3), QSizeF(6, 6)));
        }
        const Placed& last = m_nodes.last();
        p.setPen(QPen(Canvas::previewColor(), 1.0, Qt::DashLine));
        p.setBrush(Canvas::previewColor());
        for (const auto& handle : {last.in, last.out}) {
            if (!handle)
                continue;
            p.drawLine(view.map(last.point), view.map(*handle));
            p.drawEllipse(view.map(*handle), 3.0, 3.0);
        }
    }
    m_canvas.paintSnapHint(p, m_snap);
    p.restore();
}

} // namespace vek
