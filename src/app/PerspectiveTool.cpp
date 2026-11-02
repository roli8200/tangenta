#include "PerspectiveTool.h"

#include "Canvas.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>

namespace vek {

namespace {

constexpr double kHandle = 5.0;

} // namespace

void PerspectiveTool::activate()
{
    m_target = 0;
    m_drag = -1;
    Editor& ed = m_canvas.editor();
    QList<const DrawObject*> objects = ed.selectedObjects();
    if (objects.size() != 1)
        return;
    if (!node_cast<CurveObject>(objects.first())) {
        ed.convertSelectionToCurves();
        objects = ed.selectedObjects();
    }
    const auto* curve = objects.isEmpty() ? nullptr : node_cast<CurveObject>(objects.first());
    if (!curve)
        return;
    m_target = curve->id();
    // Knoten in Seitenkoordinaten (die Kurve trägt ihre Transformation)
    m_original = CurveObject::fromPath(curve->path())->subPaths();
    m_box = curve->path().boundingRect();
    m_quad = QPolygonF({m_box.topLeft(), m_box.topRight(), m_box.bottomRight(), m_box.bottomLeft()});
    m_canvas.setCursor(Qt::CrossCursor);
    m_canvas.update();
}

void PerspectiveTool::deactivate()
{
    m_target = 0;
    m_drag = -1;
}

QList<SubPath> PerspectiveTool::mapped(const QPolygonF& quad) const
{
    QTransform t;
    if (!QTransform::quadToQuad(QPolygonF({m_box.topLeft(), m_box.topRight(), m_box.bottomRight(), m_box.bottomLeft()}), quad, t))
        return m_original;
    QList<SubPath> out = m_original;
    for (SubPath& sp : out) {
        for (CurveNode& n : sp.nodes) {
            n.point = t.map(n.point);
            n.c1 = t.map(n.c1);
            n.c2 = t.map(n.c2);
        }
    }
    return out;
}

void PerspectiveTool::moveCorner(int corner, const QPointF& doc)
{
    if (!m_target || corner < 0 || corner > 3)
        return;
    m_quad[corner] = doc;
    const QList<SubPath> paths = mapped(m_quad);
    m_canvas.editor().modifyCurve(m_target, [&paths](CurveObject& c) {
        c.setTransform(QTransform());
        c.setSubPaths(paths);
    }, QObject::tr("Perspektive"));
    m_canvas.update();
}

void PerspectiveTool::mousePress(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() != Qt::LeftButton || !m_target)
        return;
    const QPointF s = m_canvas.toScreen(doc);
    for (int i = 0; i < 4; ++i) {
        const QPointF h = m_canvas.toScreen(m_quad[i]);
        if (std::abs(h.x() - s.x()) <= kHandle + 2 && std::abs(h.y() - s.y()) <= kHandle + 2) {
            m_drag = i;
            return;
        }
    }
}

void PerspectiveTool::mouseMove(QMouseEvent*, const QPointF& doc)
{
    if (m_drag < 0)
        return;
    m_quad[m_drag] = doc;
    m_canvas.update();
}

void PerspectiveTool::mouseRelease(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() != Qt::LeftButton || m_drag < 0)
        return;
    const int corner = m_drag;
    m_drag = -1;
    moveCorner(corner, doc);
}

bool PerspectiveTool::keyPress(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        m_canvas.setTool(ToolId::Pick);
        return true;
    }
    return false;
}

void PerspectiveTool::paintOverlay(QPainter& p)
{
    if (!m_target)
        return;
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    const QTransform view = m_canvas.viewTransform();
    if (m_drag >= 0) {
        p.setPen(QPen(Canvas::previewColor(), 1.0));
        p.setBrush(Qt::NoBrush);
        p.drawPath(view.map(CurveObject::toPainterPath(mapped(m_quad))));
    }
    // Perspektivisches Raster: gestricheltes Viereck mit Teilungen
    QTransform t;
    QTransform::quadToQuad(QPolygonF({m_box.topLeft(), m_box.topRight(), m_box.bottomRight(), m_box.bottomLeft()}), m_quad, t);
    p.setPen(QPen(QColor(0xd0, 0x30, 0x30), 1.0, Qt::DashLine));
    for (int i = 0; i <= 4; ++i) {
        const double f = i / 4.0;
        p.drawLine(view.map(t.map(QPointF(m_box.left() + m_box.width() * f, m_box.top()))),
                   view.map(t.map(QPointF(m_box.left() + m_box.width() * f, m_box.bottom()))));
        p.drawLine(view.map(t.map(QPointF(m_box.left(), m_box.top() + m_box.height() * f))),
                   view.map(t.map(QPointF(m_box.right(), m_box.top() + m_box.height() * f))));
    }
    p.setPen(QPen(Qt::black, 1.0));
    p.setBrush(Qt::white);
    for (const QPointF& c : m_quad) {
        const QPointF s = view.map(c);
        p.drawRect(QRectF(s.x() - kHandle, s.y() - kHandle, 2 * kHandle, 2 * kHandle));
    }
    p.restore();
}

} // namespace vek
