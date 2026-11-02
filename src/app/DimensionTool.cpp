#include "DimensionTool.h"

#include "Canvas.h"
#include "Options.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>

#include <cmath>

namespace vek {

void DimensionTool::activate()
{
    m_canvas.setCursor(Qt::CrossCursor);
}

void DimensionTool::deactivate()
{
    m_points.clear();
    m_snap = SnapResult();
    m_canvas.update();
}

std::unique_ptr<DimensionObject> DimensionTool::make(const QList<QPointF>& pts) const
{
    std::unique_ptr<DimensionObject> d;
    const Options& o = Options::instance();
    if (m_mode == Mode::Angular) {
        d = std::make_unique<DimensionObject>(DimensionObject::Kind::Angular, pts.value(0), pts.value(1), pts.value(2), pts.value(3));
        d->setPrecision(o.integer(QStringLiteral("ws/tools/angle/precision")));
        d->setUnits(o.integer(QStringLiteral("ws/tools/angle/units")));
        d->setPrefix(o.text(QStringLiteral("ws/tools/angle/prefix")));
        d->setSuffix(o.text(QStringLiteral("ws/tools/angle/suffix")));
    } else {
        DimensionObject::Kind kind = DimensionObject::Kind::Parallel;
        if (m_mode == Mode::HorizontalVertical) {
            // Wohin man die Masslinie zieht, entscheidet über waagrecht oder senkrecht
            const QPointF p = pts.value(2);
            const QRectF box = QRectF(pts.value(0), pts.value(1)).normalized();
            const bool beside = p.x() < box.left() || p.x() > box.right();
            const bool aboveBelow = p.y() < box.top() || p.y() > box.bottom();
            kind = beside && !aboveBelow ? DimensionObject::Kind::Vertical : DimensionObject::Kind::Horizontal;
        }
        d = std::make_unique<DimensionObject>(kind, pts.value(0), pts.value(1), pts.value(2));
        d->setPrecision(o.integer(QStringLiteral("ws/tools/dimension/precision")));
        d->setUnits(o.integer(QStringLiteral("ws/tools/dimension/units")));
        d->setFraction(o.integer(QStringLiteral("ws/tools/dimension/style")) == 1);
        d->setPrefix(o.text(QStringLiteral("ws/tools/dimension/prefix")));
        d->setSuffix(o.text(QStringLiteral("ws/tools/dimension/suffix")));
        d->setScale(o.number(QStringLiteral("doc/rulers/scale")));
    }
    // Schrift wie die Vorgabe für Grafiktext (Hinweis im Optionen-Dialog)
    const ShapeDefaults& sd = m_canvas.editor().shapeDefaults();
    d->setFont(sd.artisticText.family, std::min(sd.artisticText.sizePt, 10.0));
    Style s = d->style();
    if (const std::optional<Color> stroke = m_canvas.editor().defaultStyle().stroke)
        s.stroke = stroke;
    d->setStyle(s);
    return d;
}

void DimensionTool::mousePress(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() == Qt::RightButton) {
        deactivate();
        return;
    }
    if (event->button() != Qt::LeftButton)
        return;
    m_snap = SnapEngine(m_canvas).snap(doc);
    const QPointF p = m_snap.snapped() ? m_snap.point : doc;
    m_points << p;
    m_current = p;
    if (m_points.size() < pointsNeeded()) {
        m_canvas.update();
        return;
    }
    const QList<QPointF> points = m_points;
    m_points.clear();
    m_snap = SnapResult();
    Editor& ed = m_canvas.editor();
    ed.addObject(make(points), m_mode == Mode::Angular ? QObject::tr("Winkelbemassung") : QObject::tr("Bemassung"));
    m_canvas.update();
}

void DimensionTool::mouseMove(QMouseEvent*, const QPointF& doc)
{
    m_snap = SnapEngine(m_canvas).snap(doc);
    m_current = m_snap.snapped() ? m_snap.point : doc;
    m_canvas.update();
}

bool DimensionTool::keyPress(QKeyEvent* event)
{
    if (event->key() != Qt::Key_Escape || m_points.isEmpty())
        return false;
    deactivate();
    return true;
}

void DimensionTool::paintOverlay(QPainter& p)
{
    p.save();
    const QTransform view = m_canvas.viewTransform();
    p.setPen(QPen(Canvas::previewColor(), 1));
    p.setBrush(Qt::NoBrush);
    if (m_points.size() == pointsNeeded() - 1) {
        // Vorschau der fertigen Bemassung
        QList<QPointF> points = m_points;
        points << m_current;
        const std::unique_ptr<DimensionObject> preview = make(points);
        p.drawPath(view.map(preview->linePath()));
        p.setBrush(Canvas::previewColor());
        p.drawPath(view.map(preview->fillPath()));
    } else if (!m_points.isEmpty()) {
        QPolygonF line;
        for (const QPointF& pt : m_points)
            line << view.map(pt);
        line << view.map(m_current);
        p.drawPolyline(line);
    }
    m_canvas.paintSnapHint(p, m_snap);
    p.restore();
}

} // namespace vek
