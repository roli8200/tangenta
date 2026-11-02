#include "FreehandTool.h"

#include "Canvas.h"
#include "Options.h"
#include "PathOps.h"

#include <cmath>
#include <numbers>

namespace vek {

namespace {

constexpr double kEndNodeRadius = 6.0; // px
constexpr double kDragThreshold = 3.0; // px

double distance(const QPointF& a, const QPointF& b)
{
    return std::hypot(a.x() - b.x(), a.y() - b.y());
}

// Ramer-Douglas-Peucker: lässt Punkte weg, die weniger als tolerance von der Sehne abweichen
void simplify(const QList<QPointF>& in, int first, int last, double tolerance, QList<QPointF>& out)
{
    double worst = 0.0;
    int index = -1;
    const QPointF a = in[first];
    const QPointF b = in[last];
    const double len = distance(a, b);
    for (int i = first + 1; i < last; ++i) {
        const QPointF p = in[i];
        const double d = len < 1e-12 ? distance(p, a)
                                     : std::abs((b.x() - a.x()) * (a.y() - p.y()) - (a.x() - p.x()) * (b.y() - a.y())) / len;
        if (d > worst) {
            worst = d;
            index = i;
        }
    }
    if (index >= 0 && worst > tolerance) {
        simplify(in, first, index, tolerance, out);
        simplify(in, index, last, tolerance, out);
    } else {
        out.append(b);
    }
}

} // namespace

void FreehandTool::activate()
{
    m_canvas.setCursor(Qt::CrossCursor);
}

void FreehandTool::deactivate()
{
    reset();
    m_lastEnd = EndNode();
}

void FreehandTool::reset()
{
    m_pressed = false;
    m_freehand = false;
    m_pending = false;
    m_stroke.clear();
    m_continue = EndNode();
    m_snap = SnapResult();
    m_canvas.update();
}

QPointF FreehandTool::snapped(const QPointF& doc, Qt::KeyboardModifiers mods)
{
    if (mods & Qt::ControlModifier) {
        // Strg: Richtung vom Startpunkt in 15°-Schritten
        m_snap = SnapResult();
        const QPointF d = doc - m_start;
        const double step = std::numbers::pi / 12.0;
        const double angle = std::round(std::atan2(d.y(), d.x()) / step) * step;
        const double len = std::hypot(d.x(), d.y());
        return m_start + QPointF(std::cos(angle), std::sin(angle)) * len;
    }
    m_snap = SnapEngine(m_canvas).snap(doc);
    return m_snap.snapped() ? m_snap.point : doc;
}

CurveEnd curveEndAt(const Canvas& canvas, const QPointF& screen)
{
    const Editor& ed = canvas.editor();
    for (const DrawObject* o : ed.selectedObjects()) {
        const auto* curve = node_cast<CurveObject>(o);
        if (!curve || ed.isLocked(curve->id()))
            continue;
        const QTransform toScreen = curve->transform() * canvas.viewTransform();
        const QList<SubPath>& paths = curve->subPaths();
        for (int p = 0; p < paths.size(); ++p) {
            if (paths[p].closed)
                continue;
            for (bool atStart : {false, true}) {
                const QPointF local = atStart ? paths[p].nodes.first().point : paths[p].nodes.last().point;
                if (distance(toScreen.map(local), screen) <= kEndNodeRadius)
                    return {curve->id(), p, atStart, curve->transform().map(local)};
            }
        }
    }
    return {};
}

void FreehandTool::mousePress(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() != Qt::LeftButton)
        return;
    m_pressed = true;
    m_freehand = false;
    m_pressScreen = event->position();
    if (!m_pending) {
        // Neuer Anfang: auf einem Endknoten der ausgewählten Kurve wird sie fortgesetzt
        m_continue = curveEndAt(m_canvas, event->position());
        m_start = m_continue.object != InvalidNodeId ? m_continue.doc : snapped(doc);
    }
    m_current = m_pending ? snapped(doc, event->modifiers()) : m_start;
    m_stroke = {m_start};
}

void FreehandTool::mouseMove(QMouseEvent* event, const QPointF& doc)
{
    // Strg wirkt nur, solange eine Linie vom Startpunkt aus entsteht
    m_current = snapped(doc, m_pressed || m_pending ? event->modifiers() : Qt::NoModifier);
    if (m_pressed && !m_pending) {
        if (!m_freehand && distance(event->position(), m_pressScreen) >= kDragThreshold)
            m_freehand = true;
        if (m_freehand && m_mode == Mode::Freehand) {
            m_snap = SnapResult(); // beim freien Zeichnen nichts einrasten
            m_stroke.append(doc);
        }
    }
    m_canvas.update();
}

void FreehandTool::mouseRelease(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() != Qt::LeftButton || !m_pressed)
        return;
    m_pressed = false;
    if (m_freehand && m_mode == Mode::Line) {
        m_freehand = false;
        commit({m_start, snapped(doc, event->modifiers())});
        return;
    }
    if (m_freehand) {
        m_freehand = false;
        m_stroke.append(doc);
        QList<QPointF> points{m_stroke.first()};
        // Freihandglättung 0..100 (50 = 1,5 px Abweichung)
        const double smoothing = Options::instance().number(QStringLiteral("ws/tools/freehand/smoothing"));
        simplify(m_stroke, 0, int(m_stroke.size()) - 1, m_canvas.pixelsToDoc(0.1 + smoothing / 100.0 * 2.8), points);
        commit(points);
        m_stroke.clear();
        return;
    }
    if (!m_pending) {
        // Erster Klick: Startpunkt steht, die Linie folgt der Maus bis zum zweiten Klick
        m_pending = true;
        m_canvas.update();
        return;
    }
    const QPointF end = snapped(doc, event->modifiers());
    if (distance(m_canvas.viewTransform().map(end), m_canvas.viewTransform().map(m_start)) < 1.0)
        return; // Zweiter Klick auf dieselbe Stelle zählt nicht
    m_pending = false;
    commit({m_start, end});
}

void FreehandTool::mouseDoubleClick(QMouseEvent* event, const QPointF& doc)
{
    Q_UNUSED(doc);
    if (event->button() != Qt::LeftButton || m_pending || m_lastEnd.object == InvalidNodeId)
        return;
    // Doppelklick auf dem gerade gesetzten Endpunkt: nächstes Segment beginnt dort
    if (distance(m_canvas.viewTransform().map(m_lastEnd.doc), event->position()) > kEndNodeRadius)
        return;
    m_continue = m_lastEnd;
    m_start = m_current = m_lastEnd.doc;
    m_pending = true;
    m_canvas.update();
}

void FreehandTool::commit(const QList<QPointF>& points)
{
    m_snap = SnapResult();
    if (points.size() < 2)
        return;
    Editor& ed = m_canvas.editor();
    // "Autom. verbinden" in Pixeln
    const double closeTolerance = m_canvas.pixelsToDoc(Options::instance().number(QStringLiteral("ws/tools/freehand/autoJoin")));
    const auto* existing = m_continue.object != InvalidNodeId ? ed.document().findAs<CurveObject>(m_continue.object)
                                                              : nullptr;
    if (existing) {
        const EndNode from = m_continue;
        const QTransform toLocal = existing->transform().inverted();
        const SubPath& path = existing->subPaths().at(from.path);
        const QPointF otherEnd = existing->transform().map(from.atStart ? path.nodes.last().point : path.nodes.first().point);
        // Endet die Fortsetzung auf dem anderen Ende, wird die Kurve geschlossen
        const bool closes = distance(points.last(), otherEnd) <= closeTolerance;
        ed.modifyCurve(existing->id(), [&](CurveObject& c) {
            for (int i = 1; i < points.size() - (closes ? 1 : 0); ++i)
                c.appendNode(from.path, from.atStart, toLocal.map(points[i]));
            if (closes)
                c.setClosed(from.path, true);
        }, QObject::tr("Kurve fortsetzen"));
        m_lastEnd = closes ? EndNode() : EndNode{existing->id(), from.path, from.atStart, points.last()};
    } else {
        SubPath path;
        const bool close = points.size() > 2 && distance(points.first(), points.last()) <= closeTolerance;
        if (points.size() > 2) {
            // Freihandstrich als Kurve: Geradengrenzwert = erlaubte Abweichung,
            // Eckengrenzwert = ab welchem Knick eine Ecke bleibt (kleiner = mehr Ecken)
            const Options& o = Options::instance();
            QList<QPointF> fit = points;
            if (close)
                fit.removeLast();
            path = pathops::fitPolyline(fit, close, m_canvas.pixelsToDoc(0.3 * o.number(QStringLiteral("ws/tools/freehand/straight"))),
                                        10.0 + 6.0 * o.number(QStringLiteral("ws/tools/freehand/corner")));
        } else {
            for (const QPointF& p : points)
                path.nodes.append(CurveNode{p});
        }
        path.closed = close;
        auto curve = std::make_unique<CurveObject>(QList<SubPath>{path});
        Style style = ed.defaultStyle();
        style.fill = Fill::none(); // eine Linie hat keine Füllung
        curve->setStyle(style);
        const NodeId id = ed.addObject(std::move(curve), QObject::tr("Linie zeichnen"));
        m_lastEnd = path.closed ? EndNode() : EndNode{id, 0, false, points.last()};
    }
    m_continue = EndNode();
    m_canvas.update();
}

bool FreehandTool::keyPress(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape && (m_pending || m_pressed)) {
        reset();
        return true;
    }
    return false;
}

void FreehandTool::paintOverlay(QPainter& p)
{
    p.save();
    const QTransform view = m_canvas.viewTransform();
    p.setPen(QPen(Canvas::previewColor(), 1.0));
    p.setBrush(Qt::NoBrush);
    if (m_freehand && m_mode == Mode::Line) {
        p.drawLine(view.map(m_start), view.map(m_current));
    } else if (m_freehand && m_stroke.size() > 1) {
        QPolygonF line;
        for (const QPointF& pt : std::as_const(m_stroke))
            line << view.map(pt);
        p.drawPolyline(line);
    } else if (m_pending) {
        p.drawLine(view.map(m_start), view.map(m_current));
    }
    m_canvas.paintSnapHint(p, m_snap);
    p.restore();
}

} // namespace vek
