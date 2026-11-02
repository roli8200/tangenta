#include "CutTools.h"

#include "Canvas.h"
#include "Options.h"
#include "PathOps.h"

#include <QPainterPathStroker>

#include <cmath>

namespace vek {

namespace {

constexpr double kHandleRadius = 5.0;  // px
constexpr double kDragThreshold = 3.0; // px
constexpr double kFitTolerance = 0.02; // mm: Abweichung der neu angepassten Kurven

double distance(const QPointF& a, const QPointF& b)
{
    return std::hypot(a.x() - b.x(), a.y() - b.y());
}

// Wie QRectF::intersects, aber auch für Linien mit Breite oder Höhe 0
bool overlaps(const QRectF& a, const QRectF& b)
{
    return a.left() <= b.right() && b.left() <= a.right() && a.top() <= b.bottom() && b.top() <= a.bottom();
}

bool encloses(const QRectF& outer, const QRectF& inner)
{
    return outer.left() <= inner.left() && inner.right() <= outer.right() && outer.top() <= inner.top()
        && inner.bottom() <= outer.bottom();
}

bool polylinesCross(const QList<QPolygonF>& lines, const QPolygonF& cut)
{
    for (const QPolygonF& line : lines) {
        for (int i = 1; i < line.size(); ++i) {
            for (int k = 1; k < cut.size(); ++k) {
                if (QLineF(line[i - 1], line[i]).intersects(QLineF(cut[k - 1], cut[k])) == QLineF::BoundedIntersection)
                    return true;
            }
        }
    }
    return false;
}

void addPiece(std::vector<std::unique_ptr<DrawObject>>& out, const DrawObject& original, const pathops::Geometry& g,
              double tolerance = kFitTolerance)
{
    if (std::unique_ptr<CurveObject> piece = pathops::rebuild(original, g, tolerance))
        out.push_back(std::move(piece));
}

} // namespace

// --- Beschneiden -------------------------------------------------------------------------

void CropTool::activate()
{
    m_canvas.setCursor(Qt::CrossCursor);
}

void CropTool::deactivate()
{
    m_drag = Drag::None;
    m_frame.reset();
    m_snap = SnapResult();
    m_canvas.notifyToolState();
}

void CropTool::setFrame(const std::optional<QRectF>& frame)
{
    m_frame = frame;
    m_canvas.notifyToolState();
    m_canvas.update();
}

QPointF CropTool::handlePoint(int handle, const QRectF& r) const
{
    switch (handle) {
    case 0: return r.topLeft();
    case 1: return QPointF(r.center().x(), r.top());
    case 2: return r.topRight();
    case 3: return QPointF(r.right(), r.center().y());
    case 4: return r.bottomRight();
    case 5: return QPointF(r.center().x(), r.bottom());
    case 6: return r.bottomLeft();
    default: return QPointF(r.left(), r.center().y());
    }
}

int CropTool::handleAt(const QPointF& screen) const
{
    if (!m_frame)
        return -1;
    for (int h = 0; h < 8; ++h) {
        if (distance(m_canvas.toScreen(handlePoint(h, *m_frame)), screen) <= kHandleRadius)
            return h;
    }
    return -1;
}

QPointF CropTool::snapped(const QPointF& doc)
{
    m_snap = SnapEngine(m_canvas).snap(doc);
    return m_snap.snapped() ? m_snap.point : doc;
}

void CropTool::mousePress(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() != Qt::LeftButton)
        return;
    m_handle = handleAt(event->position());
    if (m_handle >= 0) {
        m_drag = Drag::Handle;
    } else if (m_frame && m_frame->contains(doc)) {
        m_drag = Drag::Move;
    } else {
        m_drag = Drag::New;
        m_frame.reset();
    }
    m_start = m_drag == Drag::New ? snapped(doc) : doc;
    m_startFrame = m_frame.value_or(QRectF());
}

void CropTool::mouseMove(QMouseEvent* event, const QPointF& doc)
{
    Q_UNUSED(event);
    if (m_drag == Drag::None) {
        const int h = handleAt(event->position());
        m_canvas.setCursor(h == 0 || h == 4 ? Qt::SizeFDiagCursor
                           : h == 2 || h == 6 ? Qt::SizeBDiagCursor
                           : h == 1 || h == 5 ? Qt::SizeVerCursor
                           : h == 3 || h == 7 ? Qt::SizeHorCursor
                           : m_frame && m_frame->contains(doc) ? Qt::SizeAllCursor
                                                              : Qt::CrossCursor);
        return;
    }
    if (m_drag == Drag::New) {
        m_frame = QRectF(m_start, snapped(doc)).normalized();
    } else if (m_drag == Drag::Move) {
        m_frame = m_startFrame.translated(doc - m_start);
    } else {
        // Griff ziehen: nur die betroffenen Kanten folgen der Maus
        const QPointF p = snapped(doc);
        QRectF r = m_startFrame;
        if (m_handle == 0 || m_handle == 6 || m_handle == 7)
            r.setLeft(p.x());
        if (m_handle == 2 || m_handle == 3 || m_handle == 4)
            r.setRight(p.x());
        if (m_handle == 0 || m_handle == 1 || m_handle == 2)
            r.setTop(p.y());
        if (m_handle == 4 || m_handle == 5 || m_handle == 6)
            r.setBottom(p.y());
        m_frame = r.normalized();
    }
    m_canvas.notifyToolState();
    m_canvas.update();
}

void CropTool::mouseRelease(QMouseEvent* event, const QPointF& doc)
{
    Q_UNUSED(doc);
    if (event->button() != Qt::LeftButton)
        return;
    // Ein blosser Klick ins Leere ergibt keinen Rahmen
    if (m_drag == Drag::New && m_frame
        && (m_frame->width() * m_canvas.zoom() < kDragThreshold || m_frame->height() * m_canvas.zoom() < kDragThreshold))
        m_frame.reset();
    m_drag = Drag::None;
    m_snap = SnapResult();
    m_canvas.notifyToolState();
    m_canvas.update();
}

void CropTool::mouseDoubleClick(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() == Qt::LeftButton && m_frame && m_frame->contains(doc))
        apply();
}

bool CropTool::keyPress(QKeyEvent* event)
{
    if (!m_frame)
        return false;
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        apply();
        return true;
    }
    if (event->key() == Qt::Key_Escape) {
        setFrame(std::nullopt);
        return true;
    }
    return false;
}

bool CropTool::apply()
{
    if (!m_frame)
        return false;
    const QRectF frame = *m_frame;
    QPainterPath clip;
    clip.addRect(frame);
    Editor& ed = m_canvas.editor();
    const bool changed = ed.replaceObjects(ed.cutTargets(true), [&](const DrawObject& o) -> Editor::Replacement {
        const QRectF bounds = o.boundingRect();
        if (encloses(frame, bounds))
            return std::nullopt;
        std::vector<std::unique_ptr<DrawObject>> out;
        if (!overlaps(frame, bounds))
            return out; // ganz ausserhalb: entfällt
        const pathops::Geometry g = pathops::geometryOf(o);
        addPiece(out, o, {pathops::intersected(g.area, clip), pathops::clipLines(g.lines, clip, true)});
        return out;
    }, QObject::tr("Beschneiden"));
    setFrame(std::nullopt);
    return changed;
}

void CropTool::paintOverlay(QPainter& p)
{
    if (m_frame) {
        const QRectF r = m_canvas.viewTransform().mapRect(*m_frame);
        // Ausserhalb des Rahmens abgedunkelt
        QPainterPath outside;
        outside.addRect(QRectF(p.viewport()).adjusted(-1, -1, 1, 1));
        QPainterPath inside;
        inside.addRect(r);
        p.save();
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(90, 90, 90, 70));
        p.drawPath(outside.subtracted(inside));
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(Qt::black, 1.0));
        p.drawRect(r);
        p.setBrush(Qt::white);
        for (int h = 0; h < 8; ++h) {
            const QPointF c = handlePoint(h, r);
            p.drawRect(QRectF(c - QPointF(3, 3), QSizeF(6, 6)));
        }
        p.restore();
    }
    m_canvas.paintSnapHint(p, m_snap);
}

// --- Messer ------------------------------------------------------------------------------

void KnifeTool::activate()
{
    m_canvas.setCursor(Qt::CrossCursor);
}

void KnifeTool::deactivate()
{
    m_pressed = m_dragging = m_pending = false;
    m_line.clear();
    m_snap = SnapResult();
}

QPointF KnifeTool::snapped(const QPointF& doc)
{
    m_snap = SnapEngine(m_canvas).snap(doc);
    return m_snap.snapped() ? m_snap.point : doc;
}

void KnifeTool::mousePress(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() != Qt::LeftButton)
        return;
    m_pressed = true;
    m_dragging = false;
    m_pressScreen = event->position();
    if (!m_pending)
        m_line = {snapped(doc)};
    m_current = snapped(doc);
}

void KnifeTool::mouseMove(QMouseEvent* event, const QPointF& doc)
{
    m_current = snapped(doc);
    if (m_pressed && !m_pending) {
        if (!m_dragging && distance(event->position(), m_pressScreen) >= kDragThreshold)
            m_dragging = true;
        if (m_dragging) {
            m_snap = SnapResult();
            m_line << doc;
        }
    }
    m_canvas.update();
}

void KnifeTool::mouseRelease(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() != Qt::LeftButton || !m_pressed)
        return;
    m_pressed = false;
    if (m_dragging) {
        m_dragging = false;
        m_line << doc;
        cut(m_line);
        m_line.clear();
        return;
    }
    if (!m_pending) {
        m_pending = true; // erster Klick: Linie folgt der Maus bis zum zweiten Klick
        return;
    }
    const QPointF end = snapped(doc);
    if (distance(m_canvas.toScreen(end), m_canvas.toScreen(m_line.first())) < 1.0)
        return;
    m_pending = false;
    m_line << end;
    cut(m_line);
    m_line.clear();
}

bool KnifeTool::keyPress(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape && (m_pending || m_pressed)) {
        deactivate();
        m_canvas.update();
        return true;
    }
    return false;
}

bool KnifeTool::cut(const QPolygonF& line)
{
    m_snap = SnapResult();
    if (line.size() < 2)
        return false;
    Editor& ed = m_canvas.editor();
    QList<NodeId> targets = ed.cutTargets(false);
    if (targets.isEmpty())
        targets = ed.cutTargets(true);
    const bool changed = ed.replaceObjects(targets, [&](const DrawObject& o) -> Editor::Replacement {
        if (!overlaps(o.boundingRect(), line.boundingRect()))
            return std::nullopt;
        const pathops::Geometry g = pathops::geometryOf(o);
        // Die Schnittlinie selbst muss das Objekt treffen, nicht erst ihre Verlängerung
        if (pathops::clipLines({line}, g.area, true).isEmpty() && !polylinesCross(g.lines, line))
            return std::nullopt;
        const QPainterPath side = pathops::halfPlane(line, o.boundingRect());
        const pathops::Geometry a{pathops::intersected(g.area, side), pathops::clipLines(g.lines, side, true)};
        const pathops::Geometry b{pathops::subtracted(g.area, side), pathops::clipLines(g.lines, side, false)};
        if (a.isEmpty() || b.isEmpty())
            return std::nullopt;
        std::vector<std::unique_ptr<DrawObject>> out;
        const Options& opt = Options::instance();
        auto finish = [&line](const pathops::Geometry& g) {
            // "Objekt automatisch schliessen" aus: Flächen bleiben offen, die Schnittkante fehlt
            pathops::Geometry open;
            open.lines = g.lines;
            for (const QPolygonF& poly : g.area.toSubpathPolygons()) {
                QPolygonF run;
                auto onCut = [&line](const QPointF& p) {
                    for (int i = 0; i + 1 < line.size(); ++i) {
                        const QPointF a = line[i], b = line[i + 1];
                        const QPointF ab = b - a;
                        const double len2 = QPointF::dotProduct(ab, ab);
                        const double t = len2 > 0 ? std::clamp(QPointF::dotProduct(p - a, ab) / len2, 0.0, 1.0) : 0.0;
                        const QPointF q = a + ab * t;
                        if (std::hypot(p.x() - q.x(), p.y() - q.y()) < 0.02)
                            return true;
                    }
                    return false;
                };
                for (int i = 0; i < poly.size(); ++i) {
                    const QPointF p = poly[i];
                    const QPointF next = poly[(i + 1) % poly.size()];
                    run << p;
                    if (onCut(p) && onCut(next) && run.size() > 1) {
                        open.lines << run;
                        run.clear();
                    }
                }
                if (run.size() > 1)
                    open.lines << run;
            }
            return open;
        };
        const bool autoClose = opt.flag(QStringLiteral("ws/tools/knife/autoClose"));
        const pathops::Geometry pa = autoClose ? a : finish(a);
        const pathops::Geometry pb = autoClose ? b : finish(b);
        if (opt.flag(QStringLiteral("ws/tools/knife/keepOne"))) {
            // "Als ein Objekt belassen": beide Teile in einer Kurve
            pathops::Geometry both = pa;
            both.area.addPath(pb.area);
            both.lines << pb.lines;
            addPiece(out, o, both);
        } else {
            addPiece(out, o, pa);
            addPiece(out, o, pb);
        }
        return out;
    }, QObject::tr("Messer"));
    m_canvas.update();
    return changed;
}

void KnifeTool::paintOverlay(QPainter& p)
{
    p.save();
    p.setPen(QPen(Canvas::previewColor(), 1.0));
    p.setBrush(Qt::NoBrush);
    const QTransform view = m_canvas.viewTransform();
    if (m_dragging && m_line.size() > 1)
        p.drawPolyline(view.map(m_line));
    else if (m_pending && !m_line.isEmpty())
        p.drawLine(view.map(m_line.first()), view.map(m_current));
    m_canvas.paintSnapHint(p, m_snap);
    p.restore();
}

// --- Radierer ----------------------------------------------------------------------------

void EraserTool::activate()
{
    m_canvas.setCursor(Qt::BlankCursor); // die Spitze selbst ist der Mauszeiger
}

void EraserTool::deactivate()
{
    m_pressed = false;
    m_track.clear();
    m_mouse.reset();
    m_canvas.update();
}

void EraserTool::mousePress(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() != Qt::LeftButton)
        return;
    m_pressed = true;
    m_track = {doc};
    m_mouse = doc;
    m_canvas.update();
}

void EraserTool::mouseMove(QMouseEvent* event, const QPointF& doc)
{
    Q_UNUSED(event);
    m_mouse = doc;
    if (m_pressed)
        m_track << doc;
    m_canvas.update();
}

void EraserTool::mouseRelease(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() != Qt::LeftButton || !m_pressed)
        return;
    m_pressed = false;
    if (m_track.last() != doc)
        m_track << doc;
    const QPolygonF track = m_track;
    m_track.clear();
    erase(track);
    m_canvas.update();
}

QPainterPath EraserTool::eraseArea(const QPolygonF& track) const
{
    QPainterPath area;
    if (track.isEmpty())
        return area;
    const double half = m_width / 2.0;
    if (track.size() == 1 || track.boundingRect().width() + track.boundingRect().height() < 1e-9) {
        if (m_round)
            area.addEllipse(track.first(), half, half);
        else
            area.addRect(QRectF(track.first() - QPointF(half, half), QSizeF(m_width, m_width)));
        return area;
    }
    QPainterPath line;
    line.addPolygon(track);
    QPainterPathStroker stroker;
    stroker.setWidth(m_width);
    stroker.setCapStyle(m_round ? Qt::RoundCap : Qt::SquareCap);
    stroker.setJoinStyle(m_round ? Qt::RoundJoin : Qt::MiterJoin);
    area = stroker.createStroke(line);
    area.setFillRule(Qt::WindingFill);
    return area;
}

bool EraserTool::erase(const QPolygonF& track)
{
    const QPainterPath area = eraseArea(track);
    if (area.isEmpty())
        return false;
    const QRectF areaBounds = area.boundingRect();
    Editor& ed = m_canvas.editor();
    QList<NodeId> targets = ed.cutTargets(false);
    if (targets.isEmpty())
        targets = ed.cutTargets(true);
    return ed.replaceObjects(targets, [&](const DrawObject& o) -> Editor::Replacement {
        if (!overlaps(o.boundingRect(), areaBounds))
            return std::nullopt;
        const pathops::Geometry g = pathops::geometryOf(o);
        if (pathops::intersected(g.area, area).isEmpty() && pathops::clipLines(g.lines, area, true).isEmpty())
            return std::nullopt;
        std::vector<std::unique_ptr<DrawObject>> out;
        // "Knoten automatisch reduzieren": gröbere Anpassung, weniger Knoten
        const double tolerance = Options::instance().flag(QStringLiteral("ws/tools/eraser/reduceNodes")) ? kFitTolerance * 3.0
                                                                                                        : kFitTolerance / 4.0;
        addPiece(out, o, {pathops::subtracted(g.area, area), pathops::clipLines(g.lines, area, false)}, tolerance);
        return out;
    }, QObject::tr("Radieren"));
}

void EraserTool::paintOverlay(QPainter& p)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    const QTransform view = m_canvas.viewTransform();
    const double w = m_width * m_canvas.zoom();
    if (m_pressed && !m_track.isEmpty()) {
        // Spur weiss übermalen, bis beim Loslassen radiert wird
        QPen pen(Qt::white, w, Qt::SolidLine, m_round ? Qt::RoundCap : Qt::SquareCap,
                 m_round ? Qt::RoundJoin : Qt::MiterJoin);
        p.setPen(pen);
        if (m_track.size() == 1)
            p.drawPoint(view.map(m_track.first()));
        else
            p.drawPolyline(view.map(m_track));
    }
    if (m_mouse) {
        const QPointF c = view.map(*m_mouse);
        const double r = std::max(w / 2.0, 2.0);
        p.setPen(QPen(Qt::black, 1.0));
        p.setBrush(Qt::NoBrush);
        if (m_round)
            p.drawEllipse(c, r, r);
        else
            p.drawRect(QRectF(c - QPointF(r, r), QSizeF(2 * r, 2 * r)));
    }
    p.restore();
}

} // namespace vek
