#include "SnapEngine.h"

#include "Canvas.h"
#include "Options.h"

#include <cmath>
#include <QtMath>
#include <functional>
#include <limits>

namespace vek {

namespace {

double distance(const QPointF& a, const QPointF& b)
{
    return std::hypot(a.x() - b.x(), a.y() - b.y());
}

QPointF nearestOnSegment(const QPointF& p, const QPointF& a, const QPointF& b)
{
    const QPointF ab = b - a;
    const double len2 = QPointF::dotProduct(ab, ab);
    if (len2 == 0.0)
        return a;
    const double t = std::clamp(QPointF::dotProduct(p - a, ab) / len2, 0.0, 1.0);
    return a + ab * t;
}

// Knoten eines Pfads: Anfangs- und Endpunkte der Segmente, ohne Kontrollpunkte
QList<QPointF> pathNodes(const QPainterPath& path)
{
    QList<QPointF> nodes;
    for (int i = 0; i < path.elementCount(); ++i) {
        const QPainterPath::Element e = path.elementAt(i);
        if (e.isMoveTo() || e.isLineTo()) {
            nodes.append(e);
        } else if (e.isCurveTo() && i + 2 < path.elementCount()) {
            nodes.append(path.elementAt(i + 2));
            i += 2;
        }
    }
    return nodes;
}

} // namespace

SnapEngine::SnapEngine(const Canvas& canvas, const QList<NodeId>& exclude)
    : m_threshold(canvas.pixelsToDoc(Options::instance().number(QStringLiteral("ws/snap/radius"))))
{
    const Editor& ed = canvas.editor();
    const SnapSettings& s = ed.snapSettings();
    const Options& o = Options::instance();
    const QStringList modes = o.text(QStringLiteral("ws/snap/modes")).split(QLatin1Char(','));
    const bool nodes = modes.contains(QLatin1String("node"));
    const bool centers = modes.contains(QLatin1String("center"));
    const bool edges = modes.contains(QLatin1String("edge"));
    // Pixelgitter (96 dpi) ab 800 % Vergrösserung
    if (o.flag(QStringLiteral("doc/grid/pixel/snap")) && canvas.zoomPercent() >= 800.0)
        m_pixel = 25.4 / 96.0;
    // Basisliniengitter: waagrechte Linien im Zeilenabstand (pt) ab "von oben beginnen"
    if (o.flag(QStringLiteral("doc/grid/baseline/snap"))) {
        const double spacing = o.number(QStringLiteral("doc/grid/baseline/spacing")) * 25.4 / 72.0;
        const double top = o.number(QStringLiteral("doc/grid/baseline/top"));
        const double height = ed.currentPage().size().height();
        for (double y = top; spacing > 0.1 && y <= height; y += spacing)
            m_yLines.append({y, QObject::tr("Basisliniengitter")});
    }
    // Dynamische Hilfslinien: aus Knoten und Mitten der Objekte unter festen Winkeln
    if (o.flag(QStringLiteral("ws/dynguides/enabled"))) {
        for (const QString& a : o.text(QStringLiteral("ws/dynguides/angles")).split(QLatin1Char(','), Qt::SkipEmptyParts))
            m_angles << a.toDouble();
        if (o.flag(QStringLiteral("ws/dynguides/snapToMarks")))
            m_markDistance = o.number(QStringLiteral("ws/dynguides/distance"));
        m_angleTip = o.flag(QStringLiteral("ws/dynguides/angleTip"));
        m_distanceTip = o.flag(QStringLiteral("ws/dynguides/distanceTip"));
        for (const DrawObject* obj : ed.pickableObjects()) {
            if (exclude.contains(obj->id()))
                continue;
            m_anchors << obj->boundingRect().center();
            for (const QPointF& p : pathNodes(obj->snapPath())) {
                if (m_anchors.size() > 400)
                    break;
                m_anchors << p;
            }
        }
    }
    // Ausrichtungshilfslinien: Kanten und Mitten der übrigen Objekte (Gruppe als Ganzes oder einzeln)
    if (o.flag(QStringLiteral("ws/alignguides/enabled"))) {
        m_alignColor = o.color(QStringLiteral("ws/alignguides/color"));
        const bool centers = o.flag(QStringLiteral("ws/alignguides/centerEdges"));
        const bool margins = o.flag(QStringLiteral("ws/alignguides/useMargins"));
        const bool marginsOnly = margins && o.integer(QStringLiteral("ws/alignguides/marginScope")) == 0;
        const double margin = o.number(QStringLiteral("ws/alignguides/margin"));
        const bool single = o.integer(QStringLiteral("ws/alignguides/scope")) == 1;
        QList<const DrawObject*> objects;
        std::function<void(const DrawObject*)> add = [&](const DrawObject* d) {
            if (single && d->childCount() > 0) {
                for (int i = 0; i < d->childCount(); ++i)
                    if (const auto* c = dynamic_cast<const DrawObject*>(d->child(i)))
                        add(c);
            } else {
                objects << d;
            }
        };
        for (const DrawObject* obj : ed.pickableObjects())
            if (!exclude.contains(obj->id()))
                add(obj);
        const QString align = QObject::tr("Ausrichtung");
        for (const DrawObject* obj : objects) {
            const QRectF b = obj->boundingRect();
            QList<double> xs;
            QList<double> ys;
            if (!marginsOnly) {
                xs << b.left() << b.right();
                ys << b.top() << b.bottom();
                if (centers) {
                    xs << b.center().x();
                    ys << b.center().y();
                }
            }
            if (margins) {
                xs << b.left() - margin << b.right() + margin;
                ys << b.top() - margin << b.bottom() + margin;
            }
            for (double x : xs)
                m_xLines.append({x, align, b.top(), b.bottom(), true});
            for (double y : ys)
                m_yLines.append({y, align, b.left(), b.right(), true});
        }
    }
    if (o.flag(QStringLiteral("doc/grid/snap"))) {
        const bool spacing = o.integer(QStringLiteral("doc/grid/mode")) == 1;
        const double h = o.number(QStringLiteral("doc/grid/h"));
        const double v = o.number(QStringLiteral("doc/grid/v"));
        if (h > 0 && v > 0)
            m_grid = spacing ? QSizeF(h, v) : QSizeF(1.0 / h, 1.0 / v);
    }
    const QString node = QObject::tr("Knoten");
    const QString center = QObject::tr("Mittelpunkt");
    const QString guide = QObject::tr("Hilfslinie");
    const QString page = QObject::tr("Seite");

    if (s.objects) {
        for (const DrawObject* o : ed.pickableObjects()) {
            if (exclude.contains(o->id()))
                continue;
            const QPainterPath path = o->snapPath();
            if (nodes) {
                for (const QPointF& p : pathNodes(path))
                    m_points.append({p, node});
            }
            if (centers)
                m_points.append({o->boundingRect().center(), center});
            if (edges)
                m_outlines.append(path.toSubpathPolygons());
        }
    }
    if (s.guides && canvas.guidesVisible()) {
        for (const Guide& g : ed.guides()) {
            if (g.isVertical())
                m_xLines.append({g.anchor.x(), guide});
            else if (g.isHorizontal())
                m_yLines.append({g.anchor.y(), guide});
            else
                m_slanted.append(g);
        }
    }
    if (s.page) {
        const QSizeF size = ed.currentPage().size();
        for (double x : {0.0, size.width() / 2.0, size.width()})
            m_xLines.append({x, page});
        for (double y : {0.0, size.height() / 2.0, size.height()})
            m_yLines.append({y, page});
    }
}

SnapResult SnapEngine::snapToPoints(const QPointF& p) const
{
    SnapResult best;
    double bestDist = m_threshold;
    for (const Target& t : m_points) {
        const double d = distance(p, t.point);
        if (d <= bestDist) {
            bestDist = d;
            best = {t.point, t.hint};
        }
    }
    if (best.snapped())
        return best;

    for (const QPolygonF& poly : m_outlines) {
        for (int i = 0; i + 1 < poly.size(); ++i) {
            const QPointF q = nearestOnSegment(p, poly[i], poly[i + 1]);
            const double d = distance(p, q);
            if (d <= bestDist) {
                bestDist = d;
                best = {q, QObject::tr("Rand")};
            }
        }
    }
    return best;
}

SnapResult SnapEngine::snapToLines(const QPointF& p) const
{
    SnapResult result{p, QString()};
    auto nearest = [this](const QList<Line>& lines, double v, const Line** hit) {
        double bestDist = m_threshold;
        for (const Line& l : lines) {
            const double d = std::abs(l.value - v);
            if (d <= bestDist) {
                bestDist = d;
                *hit = &l;
            }
        }
    };
    const Line* xHit = nullptr;
    const Line* yHit = nullptr;
    nearest(m_xLines, p.x(), &xHit);
    nearest(m_yLines, p.y(), &yHit);
    if (xHit) {
        result.point.setX(xHit->value);
        result.hint = xHit->hint;
    }
    if (yHit) {
        result.point.setY(yHit->value);
        if (!xHit || yHit->hint == QObject::tr("Hilfslinie"))
            result.hint = yHit->hint;
    }
    // Ausrichtungshilfslinien zeigen die Linie vom Objekt bis zur Maus
    if (xHit && xHit->alignment) {
        result.lines << QLineF(QPointF(xHit->value, std::min(xHit->from, p.y())), QPointF(xHit->value, std::max(xHit->to, p.y())));
        result.lineColor = m_alignColor;
    }
    if (yHit && yHit->alignment) {
        result.lines << QLineF(QPointF(std::min(yHit->from, p.x()), yHit->value), QPointF(std::max(yHit->to, p.x()), yHit->value));
        result.lineColor = m_alignColor;
    }
    // Pixelgitter
    if (m_pixel > 0 && !result.snapped()) {
        result.point = QPointF(std::round(p.x() / m_pixel) * m_pixel, std::round(p.y() / m_pixel) * m_pixel);
        result.hint = QObject::tr("Pixel");
    }
    // Dokumentgitter: nächste Gitterlinie je Richtung
    if (!m_grid.isEmpty()) {
        const QString grid = QObject::tr("Gitter");
        if (!xHit) {
            const double gx = std::round(p.x() / m_grid.width()) * m_grid.width();
            if (std::abs(gx - p.x()) <= m_threshold) {
                result.point.setX(gx);
                result.hint = grid;
            }
        }
        if (!yHit) {
            const double gy = std::round(p.y() / m_grid.height()) * m_grid.height();
            if (std::abs(gy - p.y()) <= m_threshold) {
                result.point.setY(gy);
                if (result.hint.isEmpty())
                    result.hint = grid;
            }
        }
    }
    if (!result.snapped()) {
        double bestDist = m_threshold;
        for (const Guide& g : m_slanted) {
            const double d = std::abs(g.distance(p));
            if (d <= bestDist) {
                bestDist = d;
                result = {g.project(p), QObject::tr("Hilfslinie")};
            }
        }
    }
    return result;
}

SnapResult SnapEngine::snapToDynamicGuides(const QPointF& p) const
{
    SnapResult best;
    double bestDist = m_threshold;
    for (const QPointF& a : m_anchors) {
        for (double angle : m_angles) {
            const double rad = angle * M_PI / 180.0;
            const QPointF dir(std::cos(rad), -std::sin(rad)); // gegen den Uhrzeigersinn, y nach unten
            const QPointF d = p - a;
            double along = QPointF::dotProduct(d, dir);
            const double across = std::abs(d.x() * dir.y() - d.y() * dir.x());
            if (across > bestDist || std::abs(along) < m_threshold)
                continue;
            if (m_markDistance > 0)
                along = std::round(along / m_markDistance) * m_markDistance;
            bestDist = across;
            const QPointF q = a + dir * along;
            QString hint = QObject::tr("Dynamische Hilfslinie");
            if (m_angleTip)
                hint += QStringLiteral(" %1°").arg(std::fmod(angle + (along < 0 ? 180.0 : 0.0), 360.0), 0, 'f', 0);
            if (m_distanceTip)
                hint += QStringLiteral(", %1 mm").arg(std::abs(along), 0, 'f', 2);
            best = {q, hint, {QLineF(a - dir * 1000.0, a + dir * 1000.0)}, QColor(0x2b, 0x9a, 0x3c)};
        }
    }
    return best;
}

SnapResult SnapEngine::snap(const QPointF& point) const
{
    SnapResult r = snapToPoints(point);
    if (r.snapped())
        return r;
    r = snapToDynamicGuides(point);
    if (r.snapped())
        return r;
    return snapToLines(point);
}

QPointF SnapEngine::snapDelta(const QList<QPointF>& sources, const QPointF& delta, SnapResult* result) const
{
    // Zuerst Punkt auf Punkt/Rand: der Quellpunkt mit der kleinsten Korrektur gewinnt
    double bestDist = std::numeric_limits<double>::max();
    SnapResult bestPoint;
    QPointF bestCorrection;
    for (const QPointF& s : sources) {
        const QPointF q = s + delta;
        const SnapResult r = snapToPoints(q);
        if (r.snapped() && distance(q, r.point) < bestDist) {
            bestDist = distance(q, r.point);
            bestPoint = r;
            bestCorrection = r.point - q;
        }
    }
    if (bestPoint.snapped()) {
        if (result)
            *result = bestPoint;
        return delta + bestCorrection;
    }
    // Dann dynamische Hilfslinien
    for (const QPointF& s : sources) {
        const QPointF q = s + delta;
        const SnapResult r = snapToDynamicGuides(q);
        if (r.snapped() && distance(q, r.point) < bestDist) {
            bestDist = distance(q, r.point);
            bestPoint = r;
            bestCorrection = r.point - q;
        }
    }
    if (bestPoint.snapped()) {
        if (result)
            *result = bestPoint;
        return delta + bestCorrection;
    }

    // Sonst Linien, waagrecht und senkrecht getrennt
    double bestDx = std::numeric_limits<double>::max();
    double bestDy = std::numeric_limits<double>::max();
    SnapResult lineHit;
    QPointF correction;
    for (const QPointF& s : sources) {
        const QPointF q = s + delta;
        const SnapResult r = snapToLines(q);
        if (!r.snapped())
            continue;
        const QPointF c = r.point - q;
        if (c.x() != 0.0 && std::abs(c.x()) < bestDx) {
            bestDx = std::abs(c.x());
            correction.setX(c.x());
            lineHit = r;
        }
        if (c.y() != 0.0 && std::abs(c.y()) < bestDy) {
            bestDy = std::abs(c.y());
            correction.setY(c.y());
            lineHit = r;
        }
        if (c.isNull()) { // liegt exakt auf einer Linie
            lineHit = r;
        }
    }
    if (result)
        *result = lineHit;
    return delta + correction;
}

} // namespace vek
