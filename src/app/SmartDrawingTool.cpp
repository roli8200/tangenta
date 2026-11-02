#include "SmartDrawingTool.h"

#include "Canvas.h"
#include "CurveObject.h"
#include "EllipseObject.h"
#include "Options.h"
#include "PathOps.h"
#include "RectangleObject.h"

#include <QMouseEvent>
#include <QPainter>
#include <QtMath>

#include <cmath>

namespace vek {

namespace {

double dist(const QPointF& a, const QPointF& b)
{
    return std::hypot(a.x() - b.x(), a.y() - b.y());
}

double pointToSegment(const QPointF& p, const QPointF& a, const QPointF& b)
{
    const QPointF ab = b - a;
    const double len2 = QPointF::dotProduct(ab, ab);
    const double t = len2 > 0 ? std::clamp(QPointF::dotProduct(p - a, ab) / len2, 0.0, 1.0) : 0.0;
    return dist(p, a + ab * t);
}

// Ramer-Douglas-Peucker
void simplify(const QList<QPointF>& in, int first, int last, double tolerance, QList<QPointF>& out)
{
    double worst = 0.0;
    int index = -1;
    for (int i = first + 1; i < last; ++i) {
        const double d = pointToSegment(in[i], in[first], in[last]);
        if (d > worst) {
            worst = d;
            index = i;
        }
    }
    if (index >= 0 && worst > tolerance) {
        simplify(in, first, index, tolerance, out);
        simplify(in, index, last, tolerance, out);
    } else {
        out.append(in[last]);
    }
}

std::unique_ptr<DrawObject> straight(const QList<QPointF>& points, bool closed)
{
    SubPath path;
    for (const QPointF& p : points)
        path.nodes.append(CurveNode{p});
    path.closed = closed;
    return std::make_unique<CurveObject>(QList<SubPath>{path});
}

} // namespace

SmartDrawingTool::SmartDrawingTool(Canvas& canvas) : Tool(canvas)
{
    m_timer.setSingleShot(true);
    QObject::connect(&m_timer, &QTimer::timeout, &m_canvas, [this] { finishNow(); });
}

void SmartDrawingTool::activate()
{
    m_canvas.setCursor(Qt::CrossCursor);
}

void SmartDrawingTool::deactivate()
{
    finishNow();
}

std::unique_ptr<DrawObject> SmartDrawingTool::recognize(const QList<QPointF>& stroke, double px)
{
    if (stroke.size() < 2)
        return nullptr;
    QRectF box = QPolygonF(stroke.begin(), stroke.end()).boundingRect();
    const double diag = std::hypot(box.width(), box.height());
    if (diag < 2 * px)
        return nullptr;
    const bool closed = dist(stroke.first(), stroke.last()) < std::max(15.0 * px, diag * 0.12);

    if (!closed) {
        // Gerade? Grösste Abweichung von der Sehne klein gegenüber der Länge
        double worst = 0.0;
        for (const QPointF& p : stroke)
            worst = std::max(worst, pointToSegment(p, stroke.first(), stroke.last()));
        if (worst < std::max(3.0 * px, dist(stroke.first(), stroke.last()) * 0.04))
            return straight({stroke.first(), stroke.last()}, false);
        // Sonst geglättete Kurve
        return std::make_unique<CurveObject>(QList<SubPath>{pathops::fitPolyline(stroke, false, 2.0 * px)});
    }

    // Geschlossen: Ellipse über die Hauptachsen (Kovarianz der Punkte)
    QPointF c;
    for (const QPointF& p : stroke)
        c += p;
    c /= stroke.size();
    double sxx = 0, syy = 0, sxy = 0;
    for (const QPointF& p : stroke) {
        const QPointF d = p - c;
        sxx += d.x() * d.x();
        syy += d.y() * d.y();
        sxy += d.x() * d.y();
    }
    const double angle = 0.5 * std::atan2(2.0 * sxy, sxx - syy); // Hauptachse, Bogenmass
    const QTransform toAxes = QTransform().rotateRadians(-angle).translate(-c.x(), -c.y());
    QList<QPointF> local;
    for (const QPointF& p : stroke)
        local << toAxes.map(p);
    const QRectF lb = QPolygonF(local.begin(), local.end()).boundingRect();
    const double rx = lb.width() / 2.0, ry = lb.height() / 2.0;
    const QPointF lc = lb.center();
    double err = 0.0;
    for (const QPointF& p : local) {
        const double x = (p.x() - lc.x()) / std::max(1e-9, rx), y = (p.y() - lc.y()) / std::max(1e-9, ry);
        err += std::abs(std::sqrt(x * x + y * y) - 1.0);
    }
    err /= local.size();

    // Ecken über Vereinfachung
    QList<QPointF> ring = stroke;
    ring << stroke.first();
    QList<QPointF> corners{ring.first()};
    simplify(ring, 0, int(ring.size()) - 1, diag * 0.06, corners);
    corners.removeLast(); // Anfang doppelt
    // Ecke am Anfangspunkt nur, wenn dort wirklich ein Knick ist
    if (corners.size() >= 3) {
        const QPointF a = corners.last(), b = corners.first(), n = corners[1];
        const double cosAngle = QPointF::dotProduct(b - a, n - b) / std::max(1e-9, dist(a, b) * dist(b, n));
        if (cosAngle > std::cos(qDegreesToRadians(30.0)))
            corners.removeFirst();
    }
    const int n = int(corners.size());

    if (n >= 5 || n < 3) {
        if (err < 0.12) {
            // Ellipse in den Hauptachsen, um den Winkel gedreht
            auto e = std::make_unique<EllipseObject>(QSizeF(lb.width(), lb.height()));
            const QTransform back = toAxes.inverted();
            e->setTransform(QTransform::fromTranslate(lb.left(), lb.top()) * back);
            return e;
        }
        return std::make_unique<CurveObject>(QList<SubPath>{pathops::fitPolyline(stroke, true, 2.0 * px)});
    }
    if (n == 3)
        return straight(corners, true);
    // Viereck: annähernd rechte Winkel = Rechteck in Richtung der ersten Kante
    bool right = true;
    for (int i = 0; i < 4; ++i) {
        const QPointF a = corners[(i + 3) % 4], b = corners[i], d = corners[(i + 1) % 4];
        const double cosAngle = QPointF::dotProduct(a - b, d - b) / std::max(1e-9, dist(a, b) * dist(d, b));
        right = right && std::abs(cosAngle) < std::cos(qDegreesToRadians(70.0));
    }
    if (!right)
        return straight(corners, true);
    const QPointF e1 = corners[1] - corners[0];
    double theta = std::atan2(e1.y(), e1.x());
    // Nahe an waagrecht/senkrecht: gerade ausrichten
    const double snap = std::round(theta / (M_PI / 2.0)) * (M_PI / 2.0);
    if (std::abs(theta - snap) < qDegreesToRadians(8.0))
        theta = snap;
    const QTransform rot = QTransform().rotateRadians(-theta);
    QList<QPointF> rc;
    for (const QPointF& p : corners)
        rc << rot.map(p);
    const QRectF r = QPolygonF(rc.begin(), rc.end()).boundingRect();
    auto rect = std::make_unique<RectangleObject>(r.size());
    rect->setTransform(QTransform::fromTranslate(r.left(), r.top()) * rot.inverted());
    return rect;
}

void SmartDrawingTool::finishNow()
{
    m_timer.stop();
    if (m_pending.isEmpty())
        return;
    const QList<QList<QPointF>> strokes = m_pending;
    m_pending.clear();
    Editor& ed = m_canvas.editor();
    for (const QList<QPointF>& s : strokes) {
        std::unique_ptr<DrawObject> o = recognize(s, m_canvas.pixelsToDoc(1.0));
        if (!o)
            continue;
        Style st = ed.defaultStyle();
        if (const auto* c = node_cast<CurveObject>(o.get()); c && !c->subPaths().first().closed)
            st.fill = Fill::none();
        o->setStyle(st);
        ed.addObject(std::move(o), QObject::tr("Smart-Zeichnen"));
    }
    m_canvas.update();
}

void SmartDrawingTool::mousePress(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() != Qt::LeftButton)
        return;
    m_timer.stop(); // weiterzeichnen: noch nicht umwandeln
    m_drawing = true;
    m_stroke = {doc};
}

void SmartDrawingTool::mouseMove(QMouseEvent*, const QPointF& doc)
{
    if (!m_drawing)
        return;
    if (m_stroke.isEmpty() || dist(m_stroke.last(), doc) > m_canvas.pixelsToDoc(1.0))
        m_stroke << doc;
    m_canvas.update();
}

void SmartDrawingTool::mouseRelease(QMouseEvent* event, const QPointF& doc)
{
    if (event->button() != Qt::LeftButton || !m_drawing)
        return;
    m_drawing = false;
    m_stroke << doc;
    if (m_stroke.size() > 2)
        m_pending << m_stroke;
    m_stroke.clear();
    const int ms = int(std::lround(Options::instance().number(QStringLiteral("ws/tools/shapeRecognition/delay")) * 1000.0));
    m_timer.start(std::max(0, ms));
    m_canvas.update();
}

void SmartDrawingTool::paintOverlay(QPainter& p)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor(0x30, 0x30, 0x30), 1.2));
    const QTransform view = m_canvas.viewTransform();
    auto draw = [&](const QList<QPointF>& s) {
        QPolygonF poly;
        for (const QPointF& q : s)
            poly << view.map(q);
        p.drawPolyline(poly);
    };
    for (const QList<QPointF>& s : m_pending)
        draw(s);
    draw(m_stroke);
    p.restore();
}

} // namespace vek
