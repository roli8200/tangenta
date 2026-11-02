#include "DrawingTools.h"

#include "Canvas.h"
#include "Editor.h"
#include "EllipseObject.h"
#include "Options.h"
#include "RectangleObject.h"
#include "GroupObject.h"
#include "ImageObject.h"
#include "PowerClipObject.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>

#include <QtMath>

#include <cmath>

namespace vek {

namespace {

constexpr double kCloseDistance = 6.0; // px: Klick auf den ersten Punkt schliesst

double distanceToPolyline(const QPolygonF& poly, const QPointF& p)
{
    double best = 1e300;
    for (int i = 0; i + 1 < poly.size(); ++i) {
        const QPointF a = poly[i], b = poly[i + 1];
        const QPointF ab = b - a;
        const double len2 = QPointF::dotProduct(ab, ab);
        const double t = len2 > 0 ? std::clamp(QPointF::dotProduct(p - a, ab) / len2, 0.0, 1.0) : 0.0;
        best = std::min(best, QLineF(p, a + ab * t).length());
    }
    return best;
}

} // namespace

// --- B-Spline ----------------------------------------------------------------------------------

void BSplineTool::activate()
{
    m_points.clear();
    m_hasMouse = false;
    m_canvas.setCursor(Qt::CrossCursor);
}

void BSplineTool::deactivate()
{
    // Werkzeugwechsel beendet die Kurve wie beim Bézierwerkzeug
    if (m_points.size() >= 2)
        finish(false);
    m_points.clear();
}

void BSplineTool::mousePress(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() != Qt::LeftButton)
        return;
    if (m_points.size() >= 3) {
        const QPointF first = m_canvas.toScreen(m_points.first());
        if (QLineF(first, event->position()).length() <= kCloseDistance) {
            finish(true);
            return;
        }
    }
    m_points << doc;
    m_canvas.update();
}

void BSplineTool::mouseMove(QMouseEvent*, const QPointF& doc)
{
    m_mouse = doc;
    m_hasMouse = true;
    if (!m_points.isEmpty())
        m_canvas.update();
}

void BSplineTool::mouseRelease(QMouseEvent*, const QPointF&) {}

void BSplineTool::mouseDoubleClick(QMouseEvent* event, const QPointF&)
{
    // Der erste Klick des Doppelklicks hat den letzten Punkt schon gesetzt
    if (event->button() == Qt::LeftButton && m_points.size() >= 2)
        finish(false);
}

bool BSplineTool::keyPress(QKeyEvent* event)
{
    if (m_points.isEmpty())
        return false;
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        finish(false);
        return true;
    }
    if (event->key() == Qt::Key_Escape) {
        m_points.clear();
        m_canvas.update();
        return true;
    }
    if (event->key() == Qt::Key_Backspace) {
        m_points.removeLast();
        m_canvas.update();
        return true;
    }
    return false;
}

void BSplineTool::finish(bool closed)
{
    QList<QPointF> pts;
    for (const QPointF& p : std::as_const(m_points))
        if (pts.isEmpty() || QLineF(pts.last(), p).length() > 1e-9)
            pts << p;
    m_points.clear();
    m_canvas.update();
    if (pts.size() < 2)
        return;
    const SubPath sp = pathops::bspline(pts, closed);
    if (sp.nodes.size() < 2)
        return;
    Editor& ed = m_canvas.editor();
    auto curve = std::make_unique<CurveObject>(QList<SubPath>{sp});
    Style style = ed.defaultStyle();
    if (!sp.closed)
        style.fill = Fill::none();
    curve->setStyle(style);
    ed.addObject(std::move(curve), QObject::tr("B-Spline zeichnen"));
}

void BSplineTool::paintOverlay(QPainter& p)
{
    if (m_points.isEmpty())
        return;
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    const QTransform view = m_canvas.viewTransform();
    QList<QPointF> pts = m_points;
    if (m_hasMouse)
        pts << m_mouse;
    // Steuerpolygon gestrichelt, Kurve blau
    p.setPen(QPen(QColor(0x80, 0x80, 0x80), 1.0, Qt::DashLine));
    p.drawPolyline(view.map(QPolygonF(pts)));
    if (pts.size() >= 2) {
        p.setPen(QPen(Canvas::previewColor(), 1.5));
        p.drawPath(view.map(CurveObject::toPainterPath({pathops::bspline(pts, false)})));
    }
    p.setPen(QPen(Qt::black, 1.0));
    p.setBrush(Qt::white);
    for (const QPointF& pt : std::as_const(m_points))
        p.drawEllipse(view.map(pt), 3.0, 3.0);
    p.restore();
}

// --- Polylinie --------------------------------------------------------------------------------

void PolylineTool::activate()
{
    m_points.clear();
    m_pressed = m_hasMouse = false;
    m_canvas.setCursor(Qt::CrossCursor);
}

void PolylineTool::deactivate()
{
    if (m_points.size() >= 2)
        finish(false);
    m_points.clear();
}

void PolylineTool::mousePress(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() != Qt::LeftButton)
        return;
    if (m_points.size() >= 3 && QLineF(m_canvas.toScreen(m_points.first()), event->position()).length() <= kCloseDistance) {
        finish(true);
        return;
    }
    m_pressScreen = event->position();
    m_pressed = true;
    if (m_points.isEmpty() || QLineF(m_points.last(), doc).length() > 1e-9)
        m_points << doc;
    m_canvas.update();
}

void PolylineTool::mouseMove(QMouseEvent* event, const QPointF& doc)
{
    m_mouse = doc;
    m_hasMouse = true;
    // Bei gedrückter Taste frei zeichnen: Punkte im Abstand von etwa 2 px aufnehmen
    if (m_pressed && QLineF(event->position(), m_pressScreen).length() > 3.0
        && QLineF(m_canvas.toScreen(m_points.last()), event->position()).length() >= 2.0)
        m_points << doc;
    if (!m_points.isEmpty())
        m_canvas.update();
}

void PolylineTool::mouseRelease(QMouseEvent* event, const QPointF&)
{
    if (event->button() == Qt::LeftButton)
        m_pressed = false;
}

void PolylineTool::mouseDoubleClick(QMouseEvent* event, const QPointF&)
{
    if (event->button() == Qt::LeftButton && m_points.size() >= 2)
        finish(false);
}

bool PolylineTool::keyPress(QKeyEvent* event)
{
    if (m_points.isEmpty())
        return false;
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        finish(false);
        return true;
    }
    if (event->key() == Qt::Key_Escape) {
        m_points.clear();
        m_canvas.update();
        return true;
    }
    return false;
}

void PolylineTool::finish(bool closed)
{
    QList<QPointF> pts = m_points;
    m_points.clear();
    m_pressed = false;
    m_canvas.update();
    if (pts.size() < 2)
        return;
    SubPath sp;
    sp.closed = closed && pts.size() >= 3;
    for (const QPointF& p : std::as_const(pts))
        sp.nodes << CurveNode{p};
    Editor& ed = m_canvas.editor();
    auto curve = std::make_unique<CurveObject>(QList<SubPath>{sp});
    Style style = ed.defaultStyle();
    if (!sp.closed)
        style.fill = Fill::none();
    curve->setStyle(style);
    ed.addObject(std::move(curve), QObject::tr("Polylinie zeichnen"));
}

void PolylineTool::paintOverlay(QPainter& p)
{
    if (m_points.isEmpty())
        return;
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    const QTransform view = m_canvas.viewTransform();
    QPolygonF poly(m_points);
    if (m_hasMouse && !m_pressed)
        poly << m_mouse;
    p.setPen(QPen(Canvas::previewColor(), 1.0));
    p.drawPolyline(view.map(poly));
    p.restore();
}

// --- 3-Punkt-Werkzeuge -------------------------------------------------------------------------

void ThreePointTool::activate()
{
    m_phase = 0;
    m_canvas.setCursor(Qt::CrossCursor);
}

void ThreePointTool::deactivate()
{
    m_phase = 0;
}

std::unique_ptr<DrawObject> ThreePointTool::build(Shape shape, const QPointF& a, const QPointF& b, const QPointF& c)
{
    const double len = QLineF(a, b).length();
    if (len < 1e-6)
        return nullptr;
    const QPointF u = (b - a) / len;
    const QPointF n(-u.y(), u.x());               // senkrecht dazu
    const double d = QPointF::dotProduct(c - a, n); // Abstand des dritten Punkts (mit Vorzeichen)
    if (shape == Shape::Curve) {
        // Kurve von a nach b, die durch c geht (quadratisch, als kubische Kurve)
        const QPointF ctrl = 2.0 * c - (a + b) / 2.0;
        SubPath sp;
        sp.nodes << CurveNode{a} << CurveNode{b, true, a + (ctrl - a) * (2.0 / 3.0), b + (ctrl - b) * (2.0 / 3.0)};
        return std::make_unique<CurveObject>(QList<SubPath>{sp});
    }
    if (std::abs(d) < 1e-6)
        return nullptr;
    std::unique_ptr<DrawObject> o;
    QPointF origin;
    double h = 0;
    if (shape == Shape::Rectangle) {
        // a–b ist eine Seite; die Höhe geht zum dritten Punkt
        h = std::abs(d);
        origin = d >= 0 ? a : a + n * d;
        o = std::make_unique<RectangleObject>(QSizeF(len, h));
    } else {
        // a–b ist eine Achse durch die Mitte; der dritte Punkt bestimmt die halbe andere Achse
        h = 2.0 * std::abs(d);
        origin = a - n * std::abs(d);
        o = std::make_unique<EllipseObject>(QSizeF(len, h));
    }
    o->setTransform(QTransform(u.x(), u.y(), n.x(), n.y(), origin.x(), origin.y()));
    return o;
}

void ThreePointTool::mousePress(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() != Qt::LeftButton)
        return;
    if (m_phase == 2) {
        // Dritter Punkt: fertig
        m_phase = 0;
        std::unique_ptr<DrawObject> o = build(m_shape, m_a, m_b, doc);
        m_canvas.update();
        if (!o)
            return;
        Editor& ed = m_canvas.editor();
        Style style = ed.defaultStyle();
        if (m_shape == Shape::Curve)
            style.fill = Fill::none();
        o->setStyle(style);
        const QString label = m_shape == Shape::Rectangle ? QObject::tr("3-Punkt-Rechteck")
                              : m_shape == Shape::Ellipse ? QObject::tr("3-Punkt-Ellipse")
                                                          : QObject::tr("3-Punkt-Kurve");
        ed.addObject(std::move(o), label);
        return;
    }
    m_a = m_b = m_c = doc;
    m_phase = 1;
}

void ThreePointTool::mouseMove(QMouseEvent* event, const QPointF& doc)
{
    if (m_phase == 1) {
        m_b = doc;
        if (event->modifiers() & Qt::ControlModifier) {
            // Strg: Grundlinie in 15°-Schritten
            const QPointF v = doc - m_a;
            const double step = qDegreesToRadians(15.0);
            const double ang = std::round(std::atan2(v.y(), v.x()) / step) * step;
            m_b = m_a + QPointF(std::cos(ang), std::sin(ang)) * std::hypot(v.x(), v.y());
        }
    } else if (m_phase == 2) {
        m_c = doc;
    }
    m_canvas.update();
}

void ThreePointTool::mouseRelease(QMouseEvent* event, const QPointF&)
{
    if (event->button() != Qt::LeftButton || m_phase != 1)
        return;
    if (QLineF(m_canvas.toScreen(m_a), m_canvas.toScreen(m_b)).length() < 3.0) {
        m_phase = 0; // nur geklickt
        return;
    }
    m_c = m_b;
    m_phase = 2;
}

bool ThreePointTool::keyPress(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape && m_phase != 0) {
        m_phase = 0;
        m_canvas.update();
        return true;
    }
    return false;
}

void ThreePointTool::paintOverlay(QPainter& p)
{
    if (m_phase == 0)
        return;
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    const QTransform view = m_canvas.viewTransform();
    p.setPen(QPen(Canvas::previewColor(), 1.0));
    p.setBrush(Qt::NoBrush);
    if (m_phase == 1) {
        p.drawLine(view.map(m_a), view.map(m_b));
    } else if (const std::unique_ptr<DrawObject> o = build(m_shape, m_a, m_b, m_c)) {
        p.drawPath(view.map(o->path()));
    } else {
        p.drawLine(view.map(m_a), view.map(m_b));
    }
    p.restore();
}

// --- Smart-Füllung ---------------------------------------------------------------------------

void SmartFillTool::activate()
{
    m_hover = QPainterPath();
    m_canvas.setCursor(Qt::PointingHandCursor);
}

QPainterPath SmartFillTool::regionAt(const QPointF& doc) const
{
    QPainterPath inside;
    bool any = false;
    QList<QPainterPath> outside;
    for (const DrawObject* o : m_canvas.editor().pickableObjects()) {
        if (node_cast<ImageObject>(o))
            continue;
        const QPainterPath area = pathops::geometryOf(*o).area;
        if (area.isEmpty())
            continue;
        if (area.contains(doc)) {
            inside = any ? pathops::intersected(inside, area) : area;
            any = true;
        } else if (area.boundingRect().intersects(inside.boundingRect()) || !any) {
            outside << area;
        }
    }
    if (!any)
        return {};
    for (const QPainterPath& o : std::as_const(outside))
        if (o.boundingRect().intersects(inside.boundingRect()))
            inside = pathops::subtracted(inside, o);
    // Nur der Teil, der den Klick enthält (Schnitt kann mehrere Stücke ergeben)
    for (const QPolygonF& poly : inside.toFillPolygons()) {
        QPainterPath piece;
        piece.addPolygon(poly);
        if (piece.contains(doc))
            return inside.intersected(piece).simplified();
    }
    return inside;
}

NodeId SmartFillTool::fillAt(const QPointF& doc)
{
    const QPainterPath region = regionAt(doc);
    if (region.isEmpty())
        return 0;
    QList<SubPath> paths = pathops::fitRegion(region, 0.01);
    if (paths.isEmpty())
        return 0;
    Editor& ed = m_canvas.editor();
    auto curve = std::make_unique<CurveObject>(paths);
    const Options& opt = Options::instance();
    Style s = ed.defaultStyle();
    s.fill = Fill::solid(Color::fromHex(opt.text(QStringLiteral("ws/tools/smartFill/fill"))).value_or(Color(255, 210, 77)));
    if (!opt.flag(QStringLiteral("ws/tools/smartFill/outline")))
        s.stroke.reset();
    curve->setStyle(s);
    return ed.addObject(std::move(curve), QObject::tr("Smart-Füllung"));
}

void SmartFillTool::mousePress(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() != Qt::LeftButton)
        return;
    fillAt(doc);
    m_hover = QPainterPath();
    m_canvas.update();
}

void SmartFillTool::mouseMove(QMouseEvent*, const QPointF& doc)
{
    // Fläche unter der Maus andeuten (nicht bei jedem Pixel neu: nur wenn sie den Punkt nicht enthält)
    if (!m_hover.isEmpty() && m_hover.contains(doc))
        return;
    m_hover = regionAt(doc);
    m_canvas.update();
}

void SmartFillTool::paintOverlay(QPainter& p)
{
    if (m_hover.isEmpty())
        return;
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(Canvas::previewColor(), 1.5, Qt::DashLine));
    p.setBrush(QColor(0x1e, 0x88, 0xe5, 40));
    p.drawPath(m_canvas.viewTransform().map(m_hover));
    p.restore();
}

// --- Virtuelles Segment löschen ------------------------------------------------------------------

void VirtualDeleteTool::activate()
{
    m_pressed = m_dragging = false;
    m_hoverPiece.clear();
    m_canvas.setCursor(Qt::CrossCursor);
}

bool VirtualDeleteTool::prepare(const DrawObject& o, QList<SubPath>* paths, QList<QPolygonF>* others) const
{
    // Nur Linien und Flächen aus Knoten (keine Gruppen, Bilder, Text)
    if (node_cast<GroupObject>(&o) || node_cast<PowerClipObject>(&o) || node_cast<ImageObject>(&o) || node_cast<TextObject>(&o))
        return false;
    *paths = CurveObject::fromPath(o.path())->subPaths();
    if (paths->isEmpty())
        return false;
    const double step = std::max(0.02, std::max(o.boundingRect().width(), o.boundingRect().height()) / 300.0);
    others->clear();
    for (const DrawObject* other : m_canvas.editor().pickableObjects())
        // Linien haben ein Rechteck ohne Breite oder Höhe: beide etwas aufweiten
        if (other != &o && other->boundingRect().adjusted(-0.1, -0.1, 0.1, 0.1).intersects(o.boundingRect().adjusted(-0.1, -0.1, 0.1, 0.1)))
            *others << pathops::flattenFine(other->path(), step);
    return true;
}

bool VirtualDeleteTool::replace(const DrawObject& o, const QList<SubPath>& paths, const QString& label)
{
    Editor& ed = m_canvas.editor();
    return ed.replaceObjects({o.id()}, [&paths](const DrawObject& original) -> Editor::Replacement {
        std::vector<std::unique_ptr<DrawObject>> out;
        if (!paths.isEmpty()) {
            auto curve = std::make_unique<CurveObject>(paths);
            curve->setStyle(original.style());
            curve->setEffect(Effect::Blur, original.blurRamp());
            curve->setEffect(Effect::Transparency, original.transparency());
            curve->setName(original.name());
            out.push_back(std::move(curve));
        }
        return out;
    }, label);
}

bool VirtualDeleteTool::deleteAt(const QPointF& doc)
{
    Editor& ed = m_canvas.editor();
    const double tol = m_canvas.pixelsToDoc(5.0);
    // Oberstes Objekt, dessen Linie getroffen ist (nicht die Fläche)
    const QList<const DrawObject*> objects = ed.pickableObjects();
    for (auto it = objects.rbegin(); it != objects.rend(); ++it) {
        const DrawObject* o = *it;
        if (ed.isLocked(o->id()) || !o->boundingRect().adjusted(-tol, -tol, tol, tol).contains(doc))
            continue;
        QList<SubPath> paths;
        QList<QPolygonF> others;
        if (!prepare(*o, &paths, &others))
            continue;
        const QList<pathops::CurvePiece> pieces = pathops::piecesBetweenIntersections(paths, others);
        int best = -1;
        double bestD = tol;
        for (int i = 0; i < pieces.size(); ++i)
            if (const double d = distanceToPolyline(pieces[i].poly, doc); d <= bestD) {
                bestD = d;
                best = i;
            }
        if (best < 0)
            continue;
        return replace(*o, pathops::withoutPieces(paths, pieces, {best}), QObject::tr("Virtuelles Segment löschen"));
    }
    return false;
}

bool VirtualDeleteTool::deleteInside(const QRectF& area)
{
    Editor& ed = m_canvas.editor();
    bool any = false;
    // Erst alle Ergebnisse bestimmen, dann ersetzen (die Schnittpunkte gelten für den Zustand davor)
    QList<std::pair<NodeId, QList<SubPath>>> changes;
    for (const DrawObject* o : ed.pickableObjects()) {
        if (ed.isLocked(o->id()) || !o->boundingRect().adjusted(-0.01, -0.01, 0.01, 0.01).intersects(area))
            continue;
        QList<SubPath> paths;
        QList<QPolygonF> others;
        if (!prepare(*o, &paths, &others))
            continue;
        const QList<pathops::CurvePiece> pieces = pathops::piecesBetweenIntersections(paths, others);
        QList<int> removed;
        for (int i = 0; i < pieces.size(); ++i) {
            const QPolygonF& poly = pieces[i].poly;
            if (!poly.isEmpty() && std::all_of(poly.begin(), poly.end(), [&area](const QPointF& p) { return area.contains(p); }))
                removed << i;
        }
        if (!removed.isEmpty())
            changes << std::pair{o->id(), pathops::withoutPieces(paths, pieces, removed)};
    }
    for (const auto& [id, paths] : std::as_const(changes))
        if (const DrawObject* o = ed.document().findAs<DrawObject>(id))
            any = replace(*o, paths, QObject::tr("Virtuelles Segment löschen")) || any;
    return any;
}

void VirtualDeleteTool::mousePress(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() != Qt::LeftButton)
        return;
    m_press = m_current = doc;
    m_pressed = true;
    m_dragging = false;
}

void VirtualDeleteTool::mouseMove(QMouseEvent* event, const QPointF& doc)
{
    m_current = doc;
    if (m_pressed && !m_dragging && QLineF(m_canvas.toScreen(m_press), event->position()).length() > 3.0)
        m_dragging = true;
    if (!m_pressed) {
        // Stück unter der Maus hervorheben
        m_hoverPiece.clear();
        const double tol = m_canvas.pixelsToDoc(5.0);
        const QList<const DrawObject*> objects = m_canvas.editor().pickableObjects();
        for (auto it = objects.rbegin(); it != objects.rend() && m_hoverPiece.isEmpty(); ++it) {
            if (!(*it)->boundingRect().adjusted(-tol, -tol, tol, tol).contains(doc))
                continue;
            QList<SubPath> paths;
            QList<QPolygonF> others;
            if (!prepare(**it, &paths, &others))
                continue;
            for (const pathops::CurvePiece& piece : pathops::piecesBetweenIntersections(paths, others))
                if (distanceToPolyline(piece.poly, doc) <= tol) {
                    m_hoverPiece = piece.poly;
                    break;
                }
        }
    }
    m_canvas.update();
}

void VirtualDeleteTool::mouseRelease(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() != Qt::LeftButton || !m_pressed)
        return;
    m_pressed = false;
    m_hoverPiece.clear();
    if (m_dragging)
        deleteInside(QRectF(m_press, doc).normalized());
    else
        deleteAt(doc);
    m_dragging = false;
    m_canvas.update();
}

void VirtualDeleteTool::paintOverlay(QPainter& p)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    const QTransform view = m_canvas.viewTransform();
    if (m_dragging) {
        p.setPen(QPen(QColor(80, 80, 80), 1.0, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawRect(QRectF(view.map(m_press), view.map(m_current)).normalized());
    } else if (!m_hoverPiece.isEmpty()) {
        p.setPen(QPen(QColor(0xe0, 0x20, 0x20), 2.0));
        p.drawPolyline(view.map(m_hoverPiece));
    }
    p.restore();
}

} // namespace vek
