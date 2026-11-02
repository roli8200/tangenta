#include "ConnectorTool.h"

#include "Canvas.h"
#include "Options.h"

#include <QMouseEvent>
#include <QPainter>

#include <cmath>

namespace vek {

namespace {

constexpr double kAnchorRadius = 10.0; // px

} // namespace

void ConnectorTool::activate()
{
    m_canvas.setCursor(Qt::CrossCursor);
}

void ConnectorTool::deactivate()
{
    m_start.reset();
    m_hover = {};
    m_canvas.update();
}

ConnectorObject::End ConnectorTool::endAt(const QPointF& doc, NodeId exclude) const
{
    ConnectorObject::End end;
    end.point = doc;
    const Editor& ed = m_canvas.editor();
    const DrawObject* hit = ed.objectAt(doc, m_canvas.pixelsToDoc(kAnchorRadius));
    if (!hit || hit->id() == exclude || node_cast<ConnectorObject>(hit))
        return end;
    // Ecken, Seitenmitten und Mitte der Grundform; mit geometrischen Ankern auch die Knoten
    QList<QPointF> anchors{{0, 0}, {0.5, 0}, {1, 0}, {1, 0.5}, {1, 1}, {0.5, 1}, {0, 1}, {0, 0.5}, {0.5, 0.5}};
    if (Options::instance().flag(QStringLiteral("ws/tools/connector/anchors"))) {
        const QPainterPath path = hit->localPath();
        const QRectF b = path.boundingRect();
        for (int i = 0; i < path.elementCount() && anchors.size() < 200; ++i) {
            const QPainterPath::Element e = path.elementAt(i);
            if ((e.isMoveTo() || e.isLineTo()) && b.width() > 0 && b.height() > 0)
                anchors << QPointF((e.x - b.left()) / b.width(), (e.y - b.top()) / b.height());
        }
    }
    double best = std::numeric_limits<double>::max();
    for (const QPointF& a : anchors) {
        const QPointF p = ConnectorObject::anchorPoint(*hit, a);
        const double d = std::hypot(p.x() - doc.x(), p.y() - doc.y());
        if (d < best) {
            best = d;
            end.anchor = a;
            end.point = p;
        }
    }
    end.target = hit->id();
    return end;
}

void ConnectorTool::mousePress(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() != Qt::LeftButton)
        return;
    m_start = endAt(doc, 0);
    m_hover = *m_start;
}

void ConnectorTool::mouseMove(QMouseEvent*, const QPointF& doc)
{
    m_hover = endAt(doc, m_start ? m_start->target : 0);
    m_canvas.update();
}

void ConnectorTool::mouseRelease(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() != Qt::LeftButton || !m_start)
        return;
    const ConnectorObject::End a = *m_start;
    const ConnectorObject::End b = endAt(doc, a.target);
    m_start.reset();
    m_canvas.update();
    if (std::hypot(a.point.x() - b.point.x(), a.point.y() - b.point.y()) < m_canvas.pixelsToDoc(3.0))
        return;
    auto connector = std::make_unique<ConnectorObject>(m_kind, a, b);
    connector->setClearance(Options::instance().number(QStringLiteral("ws/tools/connector/distance")));
    if (const std::optional<Color> stroke = m_canvas.editor().defaultStyle().stroke) {
        Style s = connector->style();
        s.stroke = stroke;
        s.strokeWidth = m_canvas.editor().defaultStyle().strokeWidth;
        connector->setStyle(s);
    }
    m_canvas.editor().addObject(std::move(connector), QObject::tr("Verbindung"));
}

void ConnectorTool::paintOverlay(QPainter& p)
{
    p.save();
    const QTransform view = m_canvas.viewTransform();
    // Anker des Objekts unter der Maus
    auto mark = [&](const ConnectorObject::End& e, bool active) {
        if (!e.target)
            return;
        const QPointF s = view.map(e.point);
        p.setPen(QPen(Canvas::previewColor(), 1));
        p.setBrush(active ? Canvas::previewColor() : QColor(Qt::white));
        p.drawRect(QRectF(s.x() - 4, s.y() - 4, 8, 8));
    };
    if (m_start) {
        ConnectorObject preview(m_kind, *m_start, m_hover);
        preview.setClearance(Options::instance().number(QStringLiteral("ws/tools/connector/distance")));
        preview.resolve([this](NodeId id) { return m_canvas.editor().document().findAs<DrawObject>(id); });
        p.setPen(QPen(Canvas::previewColor(), 1));
        p.setBrush(Qt::NoBrush);
        p.drawPath(view.map(preview.path()));
        mark(*m_start, true);
    }
    mark(m_hover, false);
    p.restore();
}

} // namespace vek
