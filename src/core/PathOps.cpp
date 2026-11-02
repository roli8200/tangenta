#include "PathOps.h"

#include <QLineF>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <vector>

namespace vek::pathops {

namespace {

// Interne Auflösung für Qts Flächenoperationen: Qt unterteilt Kurven mit fester Toleranz
// in Pfadeinheiten, in Millimetern wäre das viel zu grob
constexpr double kOpScale = 100.0;

using Bezier = std::array<QPointF, 4>;

double dot(const QPointF& a, const QPointF& b)
{
    return a.x() * b.x() + a.y() * b.y();
}

double length(const QPointF& v)
{
    return std::hypot(v.x(), v.y());
}

QPointF unit(const QPointF& v)
{
    const double l = length(v);
    return l < 1e-12 ? QPointF() : v / l;
}

QPointF evaluate(const Bezier& b, double t)
{
    const double u = 1.0 - t;
    return b[0] * (u * u * u) + b[1] * (3 * u * u * t) + b[2] * (3 * u * t * t) + b[3] * (t * t * t);
}

// Abstand von p zur Strecke a-b
double distanceToSegment(const QPointF& p, const QPointF& a, const QPointF& b)
{
    const QPointF d = b - a;
    const double len2 = dot(d, d);
    const double t = len2 < 1e-18 ? 0.0 : std::clamp(dot(p - a, d) / len2, 0.0, 1.0);
    return length(p - (a + d * t));
}

// --- Kurvenanpassung nach Philip J. Schneider (Graphics Gems, 1990) ---------------------

std::vector<double> chordParameters(const QList<QPointF>& pts, int first, int last)
{
    std::vector<double> u(std::size_t(last - first + 1), 0.0);
    for (int i = first + 1; i <= last; ++i)
        u[std::size_t(i - first)] = u[std::size_t(i - first - 1)] + length(pts[i] - pts[i - 1]);
    const double total = u.back();
    for (double& v : u)
        v = total > 0 ? v / total : 0.0;
    return u;
}

Bezier generateBezier(const QList<QPointF>& pts, int first, int last, const std::vector<double>& u,
                      const QPointF& t1, const QPointF& t2)
{
    const QPointF p0 = pts[first];
    const QPointF p3 = pts[last];
    double c00 = 0, c01 = 0, c11 = 0, x0 = 0, x1 = 0;
    for (int i = 0; i <= last - first; ++i) {
        const double t = u[std::size_t(i)];
        const double s = 1.0 - t;
        const double b0 = s * s * s, b1 = 3 * t * s * s, b2 = 3 * t * t * s, b3 = t * t * t;
        const QPointF a1 = t1 * b1;
        const QPointF a2 = t2 * b2;
        c00 += dot(a1, a1);
        c01 += dot(a1, a2);
        c11 += dot(a2, a2);
        const QPointF tmp = pts[first + i] - (p0 * (b0 + b1) + p3 * (b2 + b3));
        x0 += dot(a1, tmp);
        x1 += dot(a2, tmp);
    }
    const double det = c00 * c11 - c01 * c01;
    double alphaL = 0, alphaR = 0;
    if (std::abs(det) > 1e-12) {
        alphaL = (x0 * c11 - c01 * x1) / det;
        alphaR = (c00 * x1 - x0 * c01) / det;
    }
    const double seg = length(p3 - p0);
    if (alphaL < 1e-6 * seg || alphaR < 1e-6 * seg)
        alphaL = alphaR = seg / 3.0;
    return {p0, p0 + t1 * alphaL, p3 + t2 * alphaR, p3};
}

double newtonStep(const Bezier& b, const QPointF& p, double t)
{
    const double u = 1.0 - t;
    const QPointF q = evaluate(b, t);
    const QPointF d1 = (b[1] - b[0]) * (3 * u * u) + (b[2] - b[1]) * (6 * u * t) + (b[3] - b[2]) * (3 * t * t);
    const QPointF d2 = (b[2] - 2 * b[1] + b[0]) * (6 * u) + (b[3] - 2 * b[2] + b[1]) * (6 * t);
    const double numerator = dot(q - p, d1);
    const double denominator = dot(d1, d1) + dot(q - p, d2);
    return std::abs(denominator) < 1e-12 ? t : std::clamp(t - numerator / denominator, 0.0, 1.0);
}

// Grösster quadrierter Abstand und die Stelle, an der er auftritt
double maxError(const QList<QPointF>& pts, int first, int last, const Bezier& b, const std::vector<double>& u,
                int* split)
{
    double worst = 0.0;
    *split = (first + last) / 2;
    for (int i = first + 1; i < last; ++i) {
        const QPointF d = evaluate(b, u[std::size_t(i - first)]) - pts[i];
        const double e = dot(d, d);
        if (e >= worst) {
            worst = e;
            *split = i;
        }
    }
    return worst;
}

void fitCubic(const QList<QPointF>& pts, int first, int last, const QPointF& t1, const QPointF& t2,
              double error2, std::vector<Bezier>& out)
{
    if (last - first == 1) {
        const double d = length(pts[last] - pts[first]) / 3.0;
        out.push_back({pts[first], pts[first] + t1 * d, pts[last] + t2 * d, pts[last]});
        return;
    }
    std::vector<double> u = chordParameters(pts, first, last);
    Bezier b = generateBezier(pts, first, last, u, t1, t2);
    int split = 0;
    double err = maxError(pts, first, last, b, u, &split);
    if (err < error2) {
        out.push_back(b);
        return;
    }
    if (err < error2 * 16.0) {
        for (int iteration = 0; iteration < 20; ++iteration) {
            for (int i = 0; i <= last - first; ++i)
                u[std::size_t(i)] = newtonStep(b, pts[first + i], u[std::size_t(i)]);
            b = generateBezier(pts, first, last, u, t1, t2);
            err = maxError(pts, first, last, b, u, &split);
            if (err < error2) {
                out.push_back(b);
                return;
            }
        }
    }
    const QPointF center = unit(pts[split - 1] - pts[split + 1]);
    fitCubic(pts, first, split, t1, center, error2, out);
    fitCubic(pts, split, last, -center, t2, error2, out);
}

// Ein Stück zwischen zwei Ecken: Linie, wenn es gerade genug ist, sonst Kurven
void fitRun(const QList<QPointF>& pts, int first, int last, double tolerance, std::vector<Bezier>& curves,
            std::vector<bool>& straight)
{
    double worst = 0.0;
    for (int i = first + 1; i < last; ++i)
        worst = std::max(worst, distanceToSegment(pts[i], pts[first], pts[last]));
    if (worst <= tolerance) {
        curves.push_back({pts[first], pts[first], pts[last], pts[last]});
        straight.push_back(true);
        return;
    }
    std::vector<Bezier> fitted;
    fitCubic(pts, first, last, unit(pts[first + 1] - pts[first]), unit(pts[last - 1] - pts[last]),
             tolerance * tolerance, fitted);
    for (const Bezier& b : fitted) {
        curves.push_back(b);
        straight.push_back(false);
    }
}

QPainterPath scaled(const QPainterPath& p, double factor)
{
    return QTransform::fromScale(factor, factor).map(p);
}

// Flache Polygone mit feiner Unterteilung (Seitenkoordinaten)
QList<QPolygonF> flatten(const QPainterPath& p)
{
    QList<QPolygonF> result;
    for (QPolygonF poly : scaled(p, kOpScale).toSubpathPolygons()) {
        for (QPointF& pt : poly)
            pt /= kOpScale;
        result.append(poly);
    }
    return result;
}

double polylineLength(const QPolygonF& line)
{
    double l = 0.0;
    for (int i = 1; i < line.size(); ++i)
        l += length(line[i] - line[i - 1]);
    return l;
}

} // namespace

SubPath fitPolyline(QList<QPointF> points, bool closed, double tolerance, double cornerAngle)
{
    SubPath result;
    result.closed = closed;
    // Doppelte und fast doppelte Punkte stören die Tangenten
    QList<QPointF> pts;
    for (const QPointF& p : points) {
        if (pts.isEmpty() || length(p - pts.last()) > tolerance * 0.05)
            pts.append(p);
    }
    if (closed && pts.size() > 2 && length(pts.first() - pts.last()) <= tolerance * 0.05)
        pts.removeLast();
    if (pts.size() < 2)
        return result;
    if (pts.size() == 2)
        closed = result.closed = false;

    // Ecken: Richtungswechsel über 40°
    const int n = int(pts.size());
    auto isCorner = [&](int i) {
        if (!closed && (i == 0 || i == n - 1))
            return true;
        const QPointF a = pts[(i - 1 + n) % n];
        const QPointF b = pts[i];
        const QPointF c = pts[(i + 1) % n];
        const double cosAngle = dot(unit(b - a), unit(c - b));
        return cosAngle < std::cos(cornerAngle * std::numbers::pi / 180.0);
    };
    QList<int> corners;
    for (int i = 0; i < n; ++i) {
        if (isCorner(i))
            corners.append(i);
    }

    std::vector<Bezier> curves;
    std::vector<bool> straight;
    std::vector<bool> cornerAtEnd; // Knoten am Ende des Segments ist eine Ecke
    auto addRun = [&](const QList<QPointF>& run, bool endIsCorner) {
        const std::size_t before = curves.size();
        fitRun(run, 0, int(run.size()) - 1, tolerance, curves, straight);
        for (std::size_t i = before; i < curves.size(); ++i)
            cornerAtEnd.push_back(i + 1 == curves.size() ? endIsCorner : false);
    };

    if (closed && corners.isEmpty()) {
        // Glatter geschlossener Linienzug: am ersten Punkt aufschneiden, Tangente durchgehend
        QList<QPointF> loop = pts;
        loop.append(pts.first());
        const QPointF center = unit(pts[1] - pts[n - 1]);
        std::vector<Bezier> fitted;
        fitCubic(loop, 0, int(loop.size()) - 1, center, -center, tolerance * tolerance, fitted);
        for (const Bezier& b : fitted) {
            curves.push_back(b);
            straight.push_back(false);
            cornerAtEnd.push_back(false);
        }
    } else if (closed) {
        for (int k = 0; k < corners.size(); ++k) {
            const int from = corners[k];
            const int to = corners[(k + 1) % corners.size()];
            QList<QPointF> run;
            for (int i = from;; i = (i + 1) % n) {
                run.append(pts[i]);
                if (i == to && run.size() > 1)
                    break;
            }
            addRun(run, true);
        }
        // Mit der ersten Ecke beginnen
        std::rotate(pts.begin(), pts.begin() + corners.first(), pts.end());
    } else {
        for (int k = 0; k + 1 < corners.size(); ++k)
            addRun(pts.mid(corners[k], corners[k + 1] - corners[k] + 1), true);
    }

    result.nodes.append(CurveNode{curves.front()[0]});
    for (std::size_t i = 0; i < curves.size(); ++i) {
        CurveNode node{curves[i][3]};
        if (!straight[i]) {
            node.curveIn = true;
            node.c1 = curves[i][1];
            node.c2 = curves[i][2];
        }
        node.kind = cornerAtEnd[i] ? NodeKind::Cusp : NodeKind::Smooth;
        result.nodes.append(node);
    }
    if (closed) {
        // Das letzte Segment endet auf dem Anfang: es wird zum Eingangssegment des ersten Knotens
        const CurveNode last = result.nodes.takeLast();
        CurveNode& first = result.nodes.first();
        first.curveIn = last.curveIn;
        first.c1 = last.c1;
        first.c2 = last.c2;
        first.kind = last.kind;
    }
    return result;
}

QList<SubPath> fitRegion(const QPainterPath& region, double tolerance)
{
    QList<SubPath> result;
    for (const QPolygonF& poly : flatten(region)) {
        SubPath s = fitPolyline(QList<QPointF>(poly.begin(), poly.end()), true, tolerance);
        if (s.nodes.size() >= 2)
            result.append(s);
    }
    return result;
}

SubPath bspline(const QList<QPointF>& cps, bool closed)
{
    SubPath sp;
    sp.closed = closed && cps.size() >= 3;
    if (cps.size() < 2)
        return sp;
    if (cps.size() == 2 && !sp.closed) {
        sp.nodes << CurveNode{cps[0]} << CurveNode{cps[1]};
        return sp;
    }
    // Segment aus vier aufeinanderfolgenden Steuerpunkten (gleichmässiger kubischer B-Spline)
    auto segment = [](const QPointF& q0, const QPointF& q1, const QPointF& q2, const QPointF& q3) {
        return std::array<QPointF, 4>{(q0 + 4 * q1 + q2) / 6.0, (2 * q1 + q2) / 3.0, (q1 + 2 * q2) / 3.0, (q1 + 4 * q2 + q3) / 6.0};
    };
    QList<std::array<QPointF, 4>> segs;
    if (sp.closed) {
        const int n = int(cps.size());
        for (int i = 0; i < n; ++i)
            segs << segment(cps[(i + n - 1) % n], cps[i], cps[(i + 1) % n], cps[(i + 2) % n]);
    } else {
        // Endpunkte dreifach: die Kurve beginnt und endet genau dort
        QList<QPointF> q{cps.first(), cps.first()};
        q << cps;
        q << cps.last() << cps.last();
        for (int i = 0; i + 3 < q.size(); ++i) {
            const auto s = segment(q[i], q[i + 1], q[i + 2], q[i + 3]);
            if (QLineF(s[0], s[3]).length() > 1e-9)
                segs << s;
        }
    }
    if (segs.isEmpty())
        return sp;
    if (sp.closed) {
        const auto& last = segs.last();
        sp.nodes << CurveNode{segs.first()[0], true, last[1], last[2], NodeKind::Smooth};
        for (int i = 0; i + 1 < segs.size(); ++i)
            sp.nodes << CurveNode{segs[i][3], true, segs[i][1], segs[i][2], NodeKind::Smooth};
    } else {
        sp.nodes << CurveNode{segs.first()[0]};
        for (int i = 0; i < segs.size(); ++i)
            sp.nodes << CurveNode{segs[i][3], true, segs[i][1], segs[i][2], i + 1 < segs.size() ? NodeKind::Smooth : NodeKind::Cusp};
    }
    return sp;
}

namespace {

// Ein Segment als kubische Kurve (Linien mit Steuerpunkten auf der Strecke)
struct Cubic {
    QPointF p0, c1, c2, p3;
};

int segmentCount(const SubPath& sp)
{
    const int n = int(sp.nodes.size());
    return sp.closed ? n : std::max(0, n - 1);
}

Cubic cubicOf(const SubPath& sp, int k)
{
    const int n = int(sp.nodes.size());
    const CurveNode& a = sp.nodes[k % n];
    const CurveNode& b = sp.nodes[(k + 1) % n];
    if (b.curveIn)
        return {a.point, b.c1, b.c2, b.point};
    return {a.point, a.point + (b.point - a.point) / 3.0, a.point + (b.point - a.point) * (2.0 / 3.0), b.point};
}

QPointF eval(const Cubic& c, double t)
{
    const double u = 1 - t;
    return u * u * u * c.p0 + 3 * u * u * t * c.c1 + 3 * u * t * t * c.c2 + t * t * t * c.p3;
}

// Teil [t0, t1] einer kubischen Kurve (de Casteljau)
Cubic part(const Cubic& c, double t0, double t1)
{
    auto split = [](const Cubic& k, double t, bool keepLeft) {
        const QPointF a = k.p0 + (k.c1 - k.p0) * t, b = k.c1 + (k.c2 - k.c1) * t, d = k.c2 + (k.p3 - k.c2) * t;
        const QPointF ab = a + (b - a) * t, bd = b + (d - b) * t;
        const QPointF m = ab + (bd - ab) * t;
        return keepLeft ? Cubic{k.p0, a, ab, m} : Cubic{m, bd, d, k.p3};
    };
    Cubic left = t1 < 1.0 ? split(c, t1, true) : c;
    if (t0 <= 0.0)
        return left;
    return split(left, t1 > 1e-12 ? t0 / t1 : 0.0, false);
}

bool isLineSegment(const SubPath& sp, int k)
{
    return !sp.nodes[(k + 1) % sp.nodes.size()].curveIn;
}

// Abtastung eines Linienzugs mit Parametern
struct Sampled {
    QList<QPointF> pts;
    QList<double> params;
};

Sampled sample(const SubPath& sp, int perCurve)
{
    Sampled s;
    const int m = segmentCount(sp);
    if (m == 0)
        return s;
    s.pts << sp.nodes.first().point;
    s.params << 0.0;
    for (int k = 0; k < m; ++k) {
        const Cubic c = cubicOf(sp, k);
        const int steps = isLineSegment(sp, k) ? 1 : perCurve;
        for (int i = 1; i <= steps; ++i) {
            const double t = double(i) / steps;
            s.pts << eval(c, t);
            s.params << k + t;
        }
    }
    return s;
}

// Schnitt zweier Strecken: Anteile u (auf ab) und v (auf cd)
bool crossing(const QPointF& a, const QPointF& b, const QPointF& c, const QPointF& d, double* u, double* v)
{
    const QPointF r = b - a, s = d - c;
    const double den = r.x() * s.y() - r.y() * s.x();
    if (std::abs(den) < 1e-14)
        return false;
    const QPointF ac = c - a;
    const double uu = (ac.x() * s.y() - ac.y() * s.x()) / den;
    const double vv = (ac.x() * r.y() - ac.y() * r.x()) / den;
    if (uu < -1e-9 || uu > 1 + 1e-9 || vv < -1e-9 || vv > 1 + 1e-9)
        return false;
    *u = std::clamp(uu, 0.0, 1.0);
    *v = std::clamp(vv, 0.0, 1.0);
    return true;
}

} // namespace

QList<QPolygonF> flattenFine(const QPainterPath& path, double step)
{
    QList<QPolygonF> out;
    const QPainterPath fine = warpPath(path, [](const QPointF& p) { return p; }, step);
    for (const QPolygonF& poly : fine.toSubpathPolygons())
        if (poly.size() >= 2)
            out << poly;
    return out;
}

SubPath subPath(const SubPath& sp, double a, double b)
{
    SubPath out;
    const int m = segmentCount(sp);
    if (m == 0 || b <= a)
        return out;
    auto cubicAt = [&](int k) { return cubicOf(sp, ((k % m) + m) % m); };
    const int k0 = int(std::floor(a));
    out.nodes << CurveNode{eval(cubicAt(k0), a - k0)};
    for (int k = k0; k < b - 1e-12; ++k) {
        const double t0 = std::max(a, double(k)) - k, t1 = std::min(b, double(k + 1)) - k;
        if (t1 - t0 < 1e-12)
            continue;
        const Cubic c = part(cubicAt(k), t0, t1);
        CurveNode node{c.p3};
        if (!isLineSegment(sp, ((k % m) + m) % m)) {
            node.curveIn = true;
            node.c1 = c.c1;
            node.c2 = c.c2;
        }
        out.nodes << node;
    }
    return out;
}

QList<CurvePiece> piecesBetweenIntersections(const QList<SubPath>& paths, const QList<QPolygonF>& others)
{
    QList<CurvePiece> pieces;
    QList<Sampled> samples;
    for (const SubPath& sp : paths)
        samples << sample(sp, 48);
    QList<QList<double>> cuts(paths.size());
    // Mit anderen Objekten
    for (int i = 0; i < samples.size(); ++i) {
        const Sampled& s = samples[i];
        const QRectF own = QPolygonF(QList<QPointF>(s.pts)).boundingRect().adjusted(-1e-6, -1e-6, 1e-6, 1e-6);
        for (const QPolygonF& o : others) {
            if (!o.boundingRect().adjusted(-1e-6, -1e-6, 1e-6, 1e-6).intersects(own))
                continue;
            for (int e = 0; e + 1 < s.pts.size(); ++e)
                for (int f = 0; f + 1 < o.size(); ++f) {
                    double u, v;
                    if (crossing(s.pts[e], s.pts[e + 1], o[f], o[f + 1], &u, &v))
                        cuts[i] << s.params[e] + (s.params[e + 1] - s.params[e]) * u;
                }
        }
    }
    // Mit sich selbst (auch zwischen Linienzügen desselben Objekts)
    for (int i = 0; i < samples.size(); ++i)
        for (int j = i; j < samples.size(); ++j) {
            const Sampled& a = samples[i];
            const Sampled& b = samples[j];
            const int na = int(a.pts.size()) - 1, nb = int(b.pts.size()) - 1;
            for (int e = 0; e < na; ++e)
                for (int f = (i == j ? e + 2 : 0); f < nb; ++f) {
                    if (i == j && paths[i].closed && e == 0 && f == na - 1)
                        continue; // Nachbarn über den Schluss
                    double u, v;
                    if (crossing(a.pts[e], a.pts[e + 1], b.pts[f], b.pts[f + 1], &u, &v)) {
                        cuts[i] << a.params[e] + (a.params[e + 1] - a.params[e]) * u;
                        cuts[j] << b.params[f] + (b.params[f + 1] - b.params[f]) * v;
                    }
                }
        }
    for (int i = 0; i < paths.size(); ++i) {
        const int m = segmentCount(paths[i]);
        if (m == 0)
            continue;
        QList<double> c = cuts[i];
        std::sort(c.begin(), c.end());
        QList<double> unique;
        for (double x : c)
            if (unique.isEmpty() || x - unique.last() > 1e-6)
                unique << x;
        // Schnitt genau an den Enden eines offenen Linienzugs teilt nichts
        if (!paths[i].closed) {
            while (!unique.isEmpty() && unique.first() < 1e-6)
                unique.removeFirst();
            while (!unique.isEmpty() && unique.last() > m - 1e-6)
                unique.removeLast();
        } else if (unique.size() >= 2 && unique.first() + m - unique.last() < 1e-6) {
            unique.removeLast();
        }
        auto add = [&](double from, double to) {
            CurvePiece piece{i, from, to, {}};
            const SubPath sp = subPath(paths[i], from, to);
            const Sampled s = sample(sp, 24);
            piece.poly = QPolygonF(QList<QPointF>(s.pts));
            pieces << piece;
        };
        if (paths[i].closed) {
            if (unique.isEmpty()) {
                add(0.0, m);
                continue;
            }
            for (int k = 0; k + 1 < unique.size(); ++k)
                add(unique[k], unique[k + 1]);
            add(unique.last(), unique.first() + m); // über den Schluss
        } else {
            double from = 0.0;
            for (double x : unique) {
                add(from, x);
                from = x;
            }
            add(from, m);
        }
    }
    return pieces;
}

QList<SubPath> withoutPieces(const QList<SubPath>& paths, const QList<CurvePiece>& pieces, const QList<int>& removed)
{
    QList<SubPath> out;
    for (int i = 0; i < paths.size(); ++i) {
        QList<int> mine;
        for (int k = 0; k < pieces.size(); ++k)
            if (pieces[k].path == i)
                mine << k;
        const bool touched = std::any_of(mine.begin(), mine.end(), [&removed](int k) { return removed.contains(k); });
        if (!touched) {
            out << paths[i];
            continue;
        }
        // Zusammenhängende behaltene Stücke zu Linienzügen; geschlossene: über den Schluss verbinden
        QList<std::pair<double, double>> runs;
        for (int k : mine) {
            if (removed.contains(k))
                continue;
            if (!runs.isEmpty() && std::abs(runs.last().second - pieces[k].from) < 1e-9)
                runs.last().second = pieces[k].to;
            else
                runs << std::pair{pieces[k].from, pieces[k].to};
        }
        const int m = segmentCount(paths[i]);
        if (paths[i].closed && runs.size() >= 2 && std::abs(runs.last().second - (runs.first().first + m)) < 1e-9) {
            runs.first().first = runs.last().first - m;
            runs.removeLast();
        }
        for (const auto& [a, b] : std::as_const(runs)) {
            // Negative Anfänge (über den Schluss) nach vorn schieben
            const double shift = a < 0 ? m : 0.0;
            SubPath sp = subPath(paths[i], a + shift, b + shift);
            if (sp.nodes.size() >= 2)
                out << sp;
        }
    }
    return out;
}

QList<SubPath> fitPath(const QPainterPath& path, double tolerance)
{
    if (tolerance <= 0.0) {
        const QRectF b = path.boundingRect();
        tolerance = std::clamp(std::max(b.width(), b.height()) / 2000.0, 0.002, 0.05);
    }
    QList<SubPath> result;
    for (QPolygonF poly : path.toSubpathPolygons()) {
        const bool closed = poly.size() > 2 && poly.first() == poly.last();
        if (closed)
            poly.removeLast();
        if (poly.size() < 2)
            continue;
        SubPath s = fitPolyline(QList<QPointF>(poly.begin(), poly.end()), closed, tolerance);
        if (s.nodes.size() >= 2)
            result.append(s);
    }
    return result;
}

Geometry geometryOf(const DrawObject& object)
{
    Geometry g;
    const std::unique_ptr<CurveObject> parts = CurveObject::fromPath(object.path());
    for (const SubPath& s : parts->subPaths()) {
        const QPainterPath p = CurveObject(QList<SubPath>{s}).localPath();
        if (s.closed)
            g.area.addPath(p);
        else
            g.lines.append(flatten(p));
    }
    return g;
}

QPainterPath intersected(const QPainterPath& a, const QPainterPath& b)
{
    return scaled(scaled(a, kOpScale).intersected(scaled(b, kOpScale)), 1.0 / kOpScale);
}

QPainterPath subtracted(const QPainterPath& a, const QPainterPath& b)
{
    return scaled(scaled(a, kOpScale).subtracted(scaled(b, kOpScale)), 1.0 / kOpScale);
}

QPainterPath united(const QPainterPath& a, const QPainterPath& b)
{
    return scaled(scaled(a, kOpScale).united(scaled(b, kOpScale)), 1.0 / kOpScale);
}

QList<QPolygonF> clipLines(const QList<QPolygonF>& lines, const QPainterPath& region, bool keepInside)
{
    QList<QPolygonF> result;
    for (const QPolygonF& line : lines) {
        // Dicht abtasten, damit auch schmale Bereiche zwischen zwei Punkten auffallen
        const double step = std::max(0.05, polylineLength(line) / 20000.0);
        QPolygonF samples;
        for (int i = 1; i < line.size(); ++i) {
            const QPointF a = line[i - 1];
            const QPointF b = line[i];
            const int count = std::max(1, int(std::ceil(length(b - a) / step)));
            for (int k = 0; k < count; ++k)
                samples.append(a + (b - a) * (double(k) / count));
        }
        if (!line.isEmpty())
            samples.append(line.last());

        QPolygonF piece;
        bool previousKept = false;
        for (int i = 0; i < samples.size(); ++i) {
            const bool kept = region.contains(samples[i]) == keepInside;
            if (i > 0 && kept != previousKept) {
                // Grenze zwischen zwei Abtastpunkten genauer suchen
                QPointF lo = samples[i - 1], hi = samples[i];
                for (int k = 0; k < 20; ++k) {
                    const QPointF mid = (lo + hi) / 2.0;
                    if ((region.contains(mid) == keepInside) == previousKept)
                        lo = mid;
                    else
                        hi = mid;
                }
                const QPointF boundary = (lo + hi) / 2.0;
                if (previousKept) {
                    piece.append(boundary);
                    if (piece.size() >= 2 && polylineLength(piece) > 1e-3)
                        result.append(piece);
                    piece.clear();
                } else {
                    piece = {boundary};
                }
            }
            if (kept)
                piece.append(samples[i]);
            previousKept = kept;
        }
        if (piece.size() >= 2 && polylineLength(piece) > 1e-3)
            result.append(piece);
    }
    return result;
}

std::unique_ptr<CurveObject> rebuild(const DrawObject& original, const Geometry& geometry, double tolerance)
{
    QList<SubPath> paths = fitRegion(geometry.area, tolerance);
    for (const QPolygonF& line : geometry.lines) {
        SubPath s = fitPolyline(QList<QPointF>(line.begin(), line.end()), false, tolerance);
        if (s.nodes.size() >= 2)
            paths.append(s);
    }
    if (paths.isEmpty())
        return nullptr;
    auto curve = std::make_unique<CurveObject>(paths);
    curve->setStyle(original.style());
    curve->copyEffects(original);
    curve->setName(original.name());
    return curve;
}

QPainterPath halfPlane(const QPolygonF& cut, const QRectF& bounds)
{
    QPainterPath result;
    if (cut.size() < 2)
        return result;
    const QRectF all = bounds.united(cut.boundingRect());
    const double far = 2.0 * std::hypot(all.width(), all.height()) + 100.0;
    // Richtung an den Enden aus einem Punkt mit etwas Abstand, nicht aus dem letzten Zittern
    auto endDirection = [&](bool atStart) {
        const QPointF end = atStart ? cut.first() : cut.last();
        for (int k = 1; k < cut.size(); ++k) {
            const QPointF p = atStart ? cut[k] : cut[cut.size() - 1 - k];
            if (length(end - p) > 1.0 || k == cut.size() - 1)
                return unit(end - p);
        }
        return QPointF();
    };
    const QPointF e0 = cut.first() + endDirection(true) * far;
    const QPointF e1 = cut.last() + endDirection(false) * far;
    const QPointF d = unit(e1 - e0);
    const QPointF side(d.y(), -d.x());
    QPolygonF poly;
    poly << e0;
    poly << cut;
    poly << e1 << e1 + side * (2 * far) << e0 + side * (2 * far);
    result.addPolygon(poly);
    result.closeSubpath();
    return result;
}

int mediaPresetCount()
{
    return 8;
}

QString mediaPresetName(int preset)
{
    static const char* const names[] = {
        QT_TRANSLATE_NOOP("pathops", "Beidseitig spitz"),   QT_TRANSLATE_NOOP("pathops", "Gleichmässig, runde Enden"),
        QT_TRANSLATE_NOOP("pathops", "Zum Ende spitz"),     QT_TRANSLATE_NOOP("pathops", "Zum Anfang spitz"),
        QT_TRANSLATE_NOOP("pathops", "Tropfen"),            QT_TRANSLATE_NOOP("pathops", "Keil"),
        QT_TRANSLATE_NOOP("pathops", "Pinselstrich"),       QT_TRANSLATE_NOOP("pathops", "Taille"),
    };
    return QString::fromUtf8(names[std::clamp(preset, 0, mediaPresetCount() - 1)]);
}

double mediaProfile(int preset, double t, double length, double width)
{
    using std::numbers::pi;
    t = std::clamp(t, 0.0, 1.0);
    // Runde Kappe: Halbkreis mit dem halben Strich als Radius
    const double r = std::max(width / 2.0, 1e-9);
    auto cap = [&](double distance) {
        if (distance >= r)
            return 1.0;
        const double x = 1.0 - distance / r;
        return std::sqrt(std::max(0.0, 1.0 - x * x));
    };
    const double fromStart = t * length;
    const double fromEnd = (1.0 - t) * length;
    switch (std::clamp(preset, 0, mediaPresetCount() - 1)) {
    case 0:
        return std::pow(std::sin(pi * t), 0.6);
    case 1:
        return cap(fromStart) * cap(fromEnd);
    case 2:
        return cap(fromStart) * std::pow(1.0 - t, 0.8);
    case 3:
        return std::pow(t, 0.8) * cap(fromEnd);
    case 4:
        return t < 0.25 ? std::pow(std::sin(pi / 2.0 * t / 0.25), 0.5) : std::pow((1.0 - t) / 0.75, 0.8);
    case 5:
        return cap(fromStart) * (1.0 - 0.9 * t);
    case 6:
        return (0.25 + 0.75 * std::pow(std::sin(pi * t), 0.4)) * (1.0 - 0.3 * std::pow(std::sin(3.0 * pi * t), 2));
    default:
        return (0.3 + 0.7 * std::abs(std::cos(pi * t))) * cap(fromStart) * cap(fromEnd);
    }
}

namespace {

// Breite aus der Druckliste bei t (0..1)
double pressureAt(const QList<double>& pressure, double t)
{
    if (pressure.isEmpty())
        return 1.0;
    if (pressure.size() == 1)
        return pressure.first();
    const double x = std::clamp(t, 0.0, 1.0) * double(pressure.size() - 1);
    const int i = std::min(int(x), int(pressure.size()) - 2);
    return pressure[i] + (pressure[i + 1] - pressure[i]) * (x - i);
}

// Eintrag einer Sprühliste: Form um den Ursprung (Grösse 1) und Farbe
struct SprayItem {
    QPainterPath shape;
    Color color;
};

QPainterPath unitStar(int points, double inner)
{
    QPolygonF poly;
    for (int i = 0; i < 2 * points; ++i) {
        const double a = -std::numbers::pi / 2 + i * std::numbers::pi / points;
        const double r = i % 2 ? inner * 0.5 : 0.5;
        poly << QPointF(r * std::cos(a), r * std::sin(a));
    }
    QPainterPath p;
    p.addPolygon(poly);
    p.closeSubpath();
    return p;
}

const QList<QList<SprayItem>>& sprayLists()
{
    static const QList<QList<SprayItem>> lists = [] {
        QList<QList<SprayItem>> all;
        QPainterPath dot;
        dot.addEllipse(QPointF(0, 0), 0.5, 0.5);
        all << QList<SprayItem>{{dot, Color(229, 57, 53)}, {dot, Color(30, 136, 229)}, {dot, Color(67, 160, 71)},
                                {dot, Color(253, 216, 53)}, {dot, Color(142, 36, 170)}};
        all << QList<SprayItem>{{unitStar(5, 0.45), Color(255, 193, 7)}, {unitStar(5, 0.45), Color(255, 152, 0)},
                                {unitStar(6, 0.5), Color(255, 235, 59)}};
        QPainterPath leaf;
        leaf.moveTo(-0.5, 0);
        leaf.quadTo(0, -0.45, 0.5, 0);
        leaf.quadTo(0, 0.45, -0.5, 0);
        all << QList<SprayItem>{{leaf, Color(46, 125, 50)}, {leaf, Color(102, 187, 106)}, {leaf, Color(156, 204, 101)}};
        QPainterPath heart;
        heart.moveTo(0, -0.25);
        heart.cubicTo(-0.15, -0.55, -0.5, -0.5, -0.5, -0.2);
        heart.cubicTo(-0.5, 0.1, -0.15, 0.25, 0, 0.5);
        heart.cubicTo(0.15, 0.25, 0.5, 0.1, 0.5, -0.2);
        heart.cubicTo(0.5, -0.5, 0.15, -0.55, 0, -0.25);
        all << QList<SprayItem>{{heart, Color(229, 57, 53)}, {heart, Color(236, 64, 122)}, {heart, Color(244, 143, 177)}};
        QPainterPath bubble;
        bubble.addEllipse(QPointF(0, 0), 0.5, 0.5);
        bubble.addEllipse(QPointF(0, 0), 0.42, 0.42);
        bubble.addEllipse(QPointF(-0.18, -0.18), 0.08, 0.08);
        bubble.setFillRule(Qt::OddEvenFill);
        all << QList<SprayItem>{{bubble, Color(79, 195, 247)}, {bubble, Color(41, 182, 246)}};
        QPainterPath piece;
        piece.addRect(QRectF(-0.5, -0.2, 1.0, 0.4));
        all << QList<SprayItem>{{piece, Color(229, 57, 53)}, {piece, Color(30, 136, 229)}, {piece, Color(67, 160, 71)},
                                {piece, Color(253, 216, 53)}, {piece, Color(255, 112, 67)}, {piece, Color(171, 71, 188)}};
        return all;
    }();
    return lists;
}

} // namespace

int sprayListCount()
{
    return int(sprayLists().size());
}

QString sprayListName(int index)
{
    static const QStringList names{QObject::tr("Punkte"), QObject::tr("Sterne"), QObject::tr("Blätter"),
                                   QObject::tr("Herzen"), QObject::tr("Blasen"), QObject::tr("Konfetti")};
    return names.value(index);
}

QList<SprayPiece> sprayPieces(const QPainterPath& spine, const ArtisticMedia& media)
{
    QList<SprayPiece> out;
    const double total = spine.length();
    const QList<SprayItem>& items = sprayLists().at(std::clamp(media.sprayList, 0, sprayListCount() - 1));
    if (total < 1e-6 || items.isEmpty() || media.width <= 0)
        return out;
    const double step = std::max(0.2, media.spacing);
    quint32 state = media.seed * 2654435761u + 1u;
    auto random = [&state] {
        state = state * 1664525u + 1013904223u;
        return double(state >> 8) / double(1u << 24);
    };
    int i = 0;
    for (double s = 0.0; s <= total + 1e-9; s += step, ++i) {
        const double pct = spine.percentAtLength(std::min(s, total));
        const QPointF at = spine.pointAtPercent(pct);
        const double tangent = spine.angleAtPercent(pct); // Grad gegen den Uhrzeigersinn
        const SprayItem& item = items[media.randomOrder ? int(random() * items.size()) % int(items.size()) : i % int(items.size())];
        const double size = media.width * (0.6 + 0.4 * random());
        const double rot = media.randomRotate ? random() * 360.0 : -tangent;
        const QPointF normal(-std::sin(qDegreesToRadians(-tangent)), std::cos(qDegreesToRadians(-tangent)));
        const QPointF offset = normal * ((random() - 0.5) * media.width * 0.6);
        QTransform m;
        m.translate(at.x() + offset.x(), at.y() + offset.y());
        m.rotate(rot);
        m.scale(size, size);
        out << SprayPiece{m.map(item.shape), item.color};
    }
    return out;
}

QPainterPath mediaOutline(const QPainterPath& spine, const ArtisticMedia& media)
{
    if (media.kind == ArtisticMedia::Kind::Sprayer) {
        QPainterPath all;
        all.setFillRule(Qt::WindingFill);
        for (const SprayPiece& p : sprayPieces(spine, media))
            all.addPath(p.path);
        return all;
    }
    QPainterPath result;
    result.setFillRule(Qt::WindingFill);
    const double total = spine.length();
    if (total < 1e-6 || media.width <= 0.0)
        return result;
    const int count = std::clamp(int(total / 0.2), 16, 4000);
    QList<QPointF> points;
    points.reserve(count + 1);
    for (int i = 0; i <= count; ++i)
        points.append(spine.pointAtPercent(spine.percentAtLength(total * i / count)));

    QPolygonF left, right;
    const double angle = media.angle * std::numbers::pi / 180.0;
    const QPointF nib = QPointF(std::cos(angle), -std::sin(angle)) * (media.width / 2.0);
    for (int i = 0; i <= count; ++i) {
        QPointF offset;
        if (media.kind == ArtisticMedia::Kind::Calligraphy) {
            offset = nib; // feste Feder: Breite hängt von der Strichrichtung ab
        } else {
            const QPointF tangent = unit(points[std::min(i + 1, count)] - points[std::max(i - 1, 0)]);
            const double t = double(i) / count;
            const double half = media.kind == ArtisticMedia::Kind::Pressure
                                    ? media.width / 2.0 * std::max(0.02, pressureAt(media.pressure, t))
                                    : media.width / 2.0 * mediaProfile(media.preset, t, total, media.width);
            offset = QPointF(-tangent.y(), tangent.x()) * half;
        }
        left << points[i] + offset;
        right << points[i] - offset;
    }
    QPolygonF outline = left;
    for (auto it = right.crbegin(); it != right.crend(); ++it)
        outline << *it;
    result.addPolygon(outline);
    result.closeSubpath();
    return result;
}

} // namespace vek::pathops
