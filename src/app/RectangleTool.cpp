#include "RectangleTool.h"

#include "Canvas.h"
#include "CurveObject.h"
#include "GroupObject.h"
#include "Options.h"
#include "PathOps.h"
#include "RectangleObject.h"

#include <QtMath>

#include <cmath>

namespace vek {

void RectangleTool::activate()
{
    m_canvas.setCursor(Qt::CrossCursor);
}

void RectangleTool::deactivate()
{
    m_dragging = false;
    m_snap = SnapResult();
}

QRectF RectangleTool::currentRect(Qt::KeyboardModifiers mods) const
{
    QPointF d = m_current - m_start;
    if (mods & Qt::ControlModifier) {
        const double side = std::max(std::abs(d.x()), std::abs(d.y()));
        d = QPointF(std::copysign(side, d.x()), std::copysign(side, d.y()));
    }
    if (mods & Qt::ShiftModifier)
        return QRectF(m_start - d, m_start + d).normalized();
    return QRectF(m_start, m_start + d).normalized();
}

QPointF RectangleTool::snapped(const QPointF& doc)
{
    m_snap = SnapEngine(m_canvas).snap(doc);
    return m_snap.snapped() ? m_snap.point : doc;
}

void RectangleTool::mousePress(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() != Qt::LeftButton)
        return;
    m_dragging = true;
    m_start = m_current = snapped(doc);
    m_mods = event->modifiers();
}

void RectangleTool::mouseMove(QMouseEvent* event, const QPointF& doc)
{
    // Auch ohne gedrückte Taste: zeigt, wo der Startpunkt einrasten würde
    m_current = snapped(doc);
    m_mods = event->modifiers();
    m_canvas.update();
}

void RectangleTool::mouseRelease(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() != Qt::LeftButton || !m_dragging)
        return;
    m_dragging = false;
    m_current = snapped(doc);
    m_snap = SnapResult();
    const QRectF r = currentRect(event->modifiers());
    m_canvas.update();
    // Blosses Klicken erzeugt kein Rechteck
    const double minSize = m_canvas.pixelsToDoc(2.0);
    if (r.width() < minSize || r.height() < minSize)
        return;
    Editor& ed = m_canvas.editor();
    std::unique_ptr<DrawObject> object = create(r);
    object->setStyle(ed.defaultStyle());
    ed.addObject(std::move(object), label());
    // Nach dem Zeichnen zurück zur Auswahl, das neue Objekt ist ausgewählt
    m_canvas.setTool(ToolId::Pick);
}

std::unique_ptr<DrawObject> RectangleTool::create(const QRectF& r) const
{
    const ShapeDefaults& d = m_canvas.editor().shapeDefaults();
    std::unique_ptr<DrawObject> object;
    switch (m_shape) {
    case Shape::Rectangle: {
        // Eckentyp und Radius aus Optionen > Hilfsmittel 'Rechteck'
        auto rect = std::make_unique<RectangleObject>(r.size());
        const Options& o = Options::instance();
        rect->setCornerType(RectangleObject::CornerType(std::clamp(o.integer(QStringLiteral("ws/tools/rect/cornerType")), 0, 2)));
        const double radius = o.number(QStringLiteral("ws/tools/rect/radius"));
        for (auto c : {RectangleObject::TopLeft, RectangleObject::TopRight, RectangleObject::BottomRight, RectangleObject::BottomLeft})
            rect->setCornerRadius(c, radius);
        object = std::move(rect);
        break;
    }
    case Shape::Spiral: {
        const Options& o = Options::instance();
        std::unique_ptr<DrawObject> spiral = makeSpiral(r, o.integer(QStringLiteral("ws/tools/spiral/revolutions")),
                                                        o.integer(QStringLiteral("ws/tools/spiral/type")) == 1,
                                                        o.integer(QStringLiteral("ws/tools/spiral/expansion")));
        return spiral; // Kurve liegt schon in Seitenkoordinaten
    }
    case Shape::GraphPaper: {
        // Gruppe aus Zellen (Rechtecken)
        const Options& o = Options::instance();
        const int cols = std::max(1, o.integer(QStringLiteral("ws/tools/graph/cols")));
        const int rows = std::max(1, o.integer(QStringLiteral("ws/tools/graph/rows")));
        auto group = std::make_unique<GroupObject>();
        const QSizeF cell(r.width() / cols, r.height() / rows);
        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < cols; ++x) {
                auto c = std::make_unique<RectangleObject>(cell);
                c->setTransform(QTransform::fromTranslate(r.left() + x * cell.width(), r.top() + y * cell.height()));
                c->setStyle(m_canvas.editor().defaultStyle());
                attachDetachedChild(*group, std::move(c));
            }
        }
        return group;
    }
    case Shape::Ellipse: {
        auto e = std::make_unique<EllipseObject>(r.size());
        e->setKind(d.ellipseKind);
        e->setAngles(d.startAngle, d.endAngle);
        e->setClockwise(d.clockwise);
        object = std::move(e);
        break;
    }
    case Shape::Polygon:
        object = std::make_unique<PolygonObject>(r.size(), PolygonObject::Kind::Polygon, d.polygonPoints, 0);
        break;
    case Shape::Star:
        object = std::make_unique<PolygonObject>(r.size(), PolygonObject::Kind::Star, d.starPoints, d.starSharpness);
        break;
    case Shape::ComplexStar:
        object = std::make_unique<PolygonObject>(r.size(), PolygonObject::Kind::ComplexStar, d.complexStarPoints,
                                                 d.complexStarSharpness);
        break;
    }
    object->setTransform(QTransform::fromTranslate(r.left(), r.top()));
    return object;
}

std::unique_ptr<DrawObject> RectangleTool::makeSpiral(const QRectF& r, int revolutions, bool logarithmic, int expansion)
{
    revolutions = std::clamp(revolutions, 1, 100);
    const double turns = 2.0 * M_PI * revolutions;
    const double growth = 1.0 + std::clamp(expansion, 1, 100) / 25.0; // je Umdrehung
    QList<QPointF> points;
    const int steps = 64 * revolutions;
    for (int i = 0; i <= steps; ++i) {
        const double t = turns * i / steps;
        double radius = t / turns;
        if (logarithmic)
            radius = (std::pow(growth, t / (2.0 * M_PI)) - 1.0) / (std::pow(growth, revolutions) - 1.0);
        points << QPointF(r.center().x() + radius * std::cos(t) * r.width() / 2.0,
                          r.center().y() + radius * std::sin(t) * r.height() / 2.0);
    }
    // Wie gewohnt füllt die Spirale das aufgezogene Rechteck ganz aus
    const QRectF got = QPolygonF(points.begin(), points.end()).boundingRect();
    if (got.width() > 0 && got.height() > 0) {
        for (QPointF& p : points)
            p = QPointF(r.left() + (p.x() - got.left()) / got.width() * r.width(),
                        r.top() + (p.y() - got.top()) / got.height() * r.height());
    }
    SubPath path = pathops::fitPolyline(points, false, std::max(0.005, std::min(r.width(), r.height()) / 2000.0));
    return std::make_unique<CurveObject>(QList<SubPath>{path});
}

QString RectangleTool::label() const
{
    switch (m_shape) {
    case Shape::Spiral: return QObject::tr("Spirale erstellen");
    case Shape::GraphPaper: return QObject::tr("Millimeterpapier erstellen");
    case Shape::Ellipse: return QObject::tr("Ellipse erstellen");
    case Shape::Polygon: return QObject::tr("Polygon erstellen");
    case Shape::Star: return QObject::tr("Stern erstellen");
    case Shape::ComplexStar: return QObject::tr("Komplexen Stern erstellen");
    default: return QObject::tr("Rechteck erstellen");
    }
}

bool RectangleTool::keyPress(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape && m_dragging) {
        m_dragging = false;
        m_canvas.update();
        return true;
    }
    return false;
}

void RectangleTool::paintOverlay(QPainter& p)
{
    p.save();
    if (m_dragging) {
        p.setPen(QPen(Canvas::previewColor(), 1.0));
        p.setBrush(Qt::NoBrush);
        // Vorschau in der echten Form
        const QRectF r = currentRect(m_mods);
        if (r.width() > 0 && r.height() > 0)
            p.drawPath(m_canvas.viewTransform().map(create(r)->path()));
    }
    m_canvas.paintSnapHint(p, m_snap);
    p.restore();
}

} // namespace vek
