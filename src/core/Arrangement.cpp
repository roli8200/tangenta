// Planares Netz für die Smart-Füllung: alle Linienzüge an ihren Schnittpunkten teilen,
// lose Enden abschneiden, Maschen umlaufen und die Masche um einen Punkt finden
#include "Arrangement.h"

#include "PathOps.h"

#include <QLineF>
#include <QPolygonF>
#include <QRectF>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace vek::pathops {

namespace {

constexpr double kFlatTolerance = 0.002; // mm, Abweichung der Abtastung von der Kurve
constexpr double kMergeEps = 1e-3;       // mm, so nahe Schnittpunkte sind ein Knoten

struct Cubic {
    QPointF p0, c1, c2, p3;
    bool line = true;
};

int segCount(const SubPath& sp)
{
    const int n = int(sp.nodes.size());
    return sp.closed ? n : std::max(0, n - 1);
}

Cubic cubicAt(const SubPath& sp, int k)
{
    const int n = int(sp.nodes.size());
    const CurveNode& a = sp.nodes[k % n];
    const CurveNode& b = sp.nodes[(k + 1) % n];
    if (b.curveIn)
        return {a.point, b.c1, b.c2, b.point, false};
    return {a.point, a.point, b.point, b.point, true};
}

QPointF evalCubic(const Cubic& c, double t)
{
    if (c.line)
        return c.p0 + (c.p3 - c.p0) * t;
    const double u = 1 - t;
    return u * u * u * c.p0 + 3 * u * u * t * c.c1 + 3 * u * t * t * c.c2 + t * t * t * c.p3;
}

double cross(const QPointF& a, const QPointF& b)
{
    return a.x() * b.y() - a.y() * b.x();
}

double dot(const QPointF& a, const QPointF& b)
{
    return a.x() * b.x() + a.y() * b.y();
}

double dist(const QPointF& a, const QPointF& b)
{
    return std::hypot(a.x() - b.x(), a.y() - b.y());
}

// Abtastung eines Linienzugs; params = Segmentnummer + t
struct Poly {
    QList<QPointF> pts;
    QList<double> params;
};

Poly flattenPath(const SubPath& sp)
{
    Poly p;
    const int m = segCount(sp);
    if (m == 0)
        return p;
    p.pts << sp.nodes.first().point;
    p.params << 0.0;
    for (int k = 0; k < m; ++k) {
        const Cubic c = cubicAt(sp, k);
        int n = 1;
        if (!c.line) {
            // Fehler der Sehne <= h^2 * 3/4 * max|zweite Differenz|
            const double dd = std::max(dist(c.p0 - c.c1, c.c1 - c.c2), dist(c.c1 - c.c2, c.c2 - c.p3));
            n = std::clamp(int(std::ceil(std::sqrt(0.75 * dd / kFlatTolerance))), 2, 400);
        }
        for (int i = 1; i <= n; ++i) {
            const double t = double(i) / n;
            p.pts << evalCubic(c, t);
            p.params << k + t;
        }
    }
    return p;
}

// Punkt der Abtastung beim Parameter (linear zwischen den Abtastpunkten)
QPointF pointAt(const Poly& p, double param)
{
    if (p.pts.isEmpty())
        return {};
    auto it = std::upper_bound(p.params.begin(), p.params.end(), param);
    if (it == p.params.begin())
        return p.pts.first();
    if (it == p.params.end())
        return p.pts.last();
    const int j = int(it - p.params.begin());
    const double a = p.params[j - 1], b = p.params[j];
    const double f = b > a ? (param - a) / (b - a) : 0.0;
    return p.pts[j - 1] + (p.pts[j] - p.pts[j - 1]) * f;
}

double polyLength(const QPolygonF& poly)
{
    double l = 0;
    for (int i = 1; i < poly.size(); ++i)
        l += dist(poly[i - 1], poly[i]);
    return l;
}

// Punkt nach Bogenlänge s vom Anfang
QPointF alongPoly(const QPolygonF& poly, double s)
{
    for (int i = 1; i < poly.size(); ++i) {
        const double l = dist(poly[i - 1], poly[i]);
        if (s <= l && l > 0)
            return poly[i - 1] + (poly[i] - poly[i - 1]) * (s / l);
        s -= l;
    }
    return poly.isEmpty() ? QPointF() : poly.last();
}

double shoelace(const QPolygonF& poly)
{
    double a = 0;
    for (int i = 0; i < poly.size(); ++i)
        a += cross(poly[i], poly[(i + 1) % poly.size()]);
    return a / 2.0;
}

SubPath reversedOpen(const SubPath& sp)
{
    SubPath out;
    const int n = int(sp.nodes.size());
    for (int i = n - 1; i >= 0; --i) {
        CurveNode node{sp.nodes[i].point};
        if (i + 1 < n && sp.nodes[i + 1].curveIn) {
            node.curveIn = true;
            node.c1 = sp.nodes[i + 1].c2;
            node.c2 = sp.nodes[i + 1].c1;
        }
        out.nodes << node;
    }
    return out;
}

struct UnionFind {
    std::vector<int> parent;
    explicit UnionFind(int n) : parent(std::size_t(n)) { std::iota(parent.begin(), parent.end(), 0); }
    int find(int x)
    {
        while (parent[std::size_t(x)] != x)
            x = parent[std::size_t(x)] = parent[std::size_t(parent[std::size_t(x)])];
        return x;
    }
    void unite(int a, int b) { parent[std::size_t(find(a))] = find(b); }
};

// Gitter für die Suche nach nahen Strecken
struct Grid {
    double x0 = 0, y0 = 0, cell = 1;
    std::unordered_map<std::int64_t, std::vector<int>> cells;
    std::int64_t key(int ix, int iy) const { return (std::int64_t(ix) << 32) ^ std::uint32_t(iy); }
    int ix(double x) const { return int(std::floor((x - x0) / cell)); }
    int iy(double y) const { return int(std::floor((y - y0) / cell)); }
    void insert(int id, const QRectF& r, std::vector<const std::vector<int>*>* touched = nullptr)
    {
        for (int i = ix(r.left()); i <= ix(r.right()); ++i)
            for (int j = iy(r.top()); j <= iy(r.bottom()); ++j) {
                std::vector<int>& list = cells[key(i, j)];
                if (list.empty() || list.back() != id) {
                    list.push_back(id);
                    if (touched)
                        touched->push_back(&list);
                }
            }
    }
    // Strecke in Stücken einer Zellgrösse eintragen (lange schräge Linien nicht über ihr ganzes Rechteck)
    void insertSegment(int id, const QPointF& a, const QPointF& b, double pad, std::vector<const std::vector<int>*>* touched = nullptr)
    {
        const int n = std::max(1, int(std::ceil(std::hypot(b.x() - a.x(), b.y() - a.y()) / cell)));
        for (int k = 0; k < n; ++k)
            insert(id, QRectF(a + (b - a) * (double(k) / n), a + (b - a) * (double(k + 1) / n)).normalized().adjusted(-pad, -pad, pad, pad),
                   touched);
    }
    template <typename F>
    void query(const QRectF& r, F&& f) const
    {
        for (int i = ix(r.left()); i <= ix(r.right()); ++i)
            for (int j = iy(r.top()); j <= iy(r.bottom()); ++j) {
                auto it = cells.find(key(i, j));
                if (it != cells.end())
                    for (int id : it->second)
                        f(id);
            }
    }
};

struct Seg {
    int path;
    int index; // Strecke Nummer index in der Abtastung des Linienzugs
    QPointF a, b;
    double pa, pb;
    QRectF box() const { return QRectF(a, b).normalized(); }
};

struct Cut {
    int path;
    double param;
    QPointF point;
};

struct Edge {
    int path;
    double from, to; // Parameter im Linienzug (geschlossen: to kann über das Ende gehen)
    int a, b;        // Knoten
    QPolygonF poly;  // von a nach b
    bool alive = true;
};

struct Face {
    QList<int> halfEdges;
    QPolygonF poly;
    double area = 0;
    QRectF box;
    int comp = 0;
};

} // namespace

struct Arrangement::Data {
    QList<SubPath> paths;
    QList<QPointF> nodes;
    QList<Edge> edges;
    QList<Face> faces;
    double gap = 0.1;

    void build(const QList<SubPath>& input);
    QPolygonF halfPoly(int he) const
    {
        QPolygonF p = edges[he >> 1].poly;
        if (he & 1)
            std::reverse(p.begin(), p.end());
        return p;
    }
    int origin(int he) const { return (he & 1) ? edges[he >> 1].b : edges[he >> 1].a; }
    int dest(int he) const { return (he & 1) ? edges[he >> 1].a : edges[he >> 1].b; }
    QList<QList<int>> loopsOf(const QList<int>& cycle) const;
    SubPath loopPath(const QList<int>& loop) const;
};

void Arrangement::Data::build(const QList<SubPath>& input)
{
    for (const SubPath& sp : input)
        if (segCount(sp) > 0)
            paths << sp;
    QList<Poly> polys;
    for (const SubPath& sp : std::as_const(paths))
        polys << flattenPath(sp);

    // Strecken aller Abtastungen ins Gitter
    QList<Seg> segs;
    QRectF bounds;
    double total = 0;
    for (int i = 0; i < polys.size(); ++i) {
        const Poly& p = polys[i];
        for (int j = 0; j + 1 < p.pts.size(); ++j) {
            Seg s{i, j, p.pts[j], p.pts[j + 1], p.params[j], p.params[j + 1]};
            if (dist(s.a, s.b) < 1e-12)
                continue;
            total += dist(s.a, s.b);
            bounds = bounds.isNull() ? s.box() : bounds.united(s.box());
            segs << s;
        }
    }
    if (segs.isEmpty())
        return;
    const double pad = std::max(gap, kMergeEps);
    Grid grid;
    grid.x0 = bounds.left() - pad;
    grid.y0 = bounds.top() - pad;
    grid.cell = std::max({total / segs.size() * 3.0, std::max(bounds.width(), bounds.height()) / 2048.0, 0.01});
    std::vector<std::vector<const std::vector<int>*>> segCells(std::size_t(segs.size()));
    for (int i = 0; i < segs.size(); ++i)
        grid.insertSegment(i, segs[i].a, segs[i].b, kMergeEps, &segCells[std::size_t(i)]);

    QList<Cut> cuts;
    auto addCut = [&](const Seg& s, double u, const QPointF& pt) { cuts << Cut{s.path, s.pa + (s.pb - s.pa) * u, pt}; };
    auto adjacent = [&](const Seg& s, const Seg& t) {
        if (s.path != t.path)
            return false;
        const int n = int(polys[s.path].pts.size()) - 1;
        const int d = std::abs(s.index - t.index);
        return d <= 1 || (paths[s.path].closed && d == n - 1);
    };

    // Schnittpunkte (auch Berührungen und überlappende gerade Stücke)
    // Jedes Paar nur einmal: Stempel je Strecke statt einer Menge aller Paare
    std::vector<int> stamp(std::size_t(segs.size()), -1);
    auto testPair = [&](int i, int j) {
        const Seg& s = segs[i];
        const Seg& t = segs[j];
        // Rahmen vergleichen (QRectF::intersects taugt nicht für waagrechte/senkrechte Strecken)
        if (adjacent(s, t) || std::max(s.a.x(), s.b.x()) + kMergeEps < std::min(t.a.x(), t.b.x())
            || std::max(t.a.x(), t.b.x()) + kMergeEps < std::min(s.a.x(), s.b.x())
            || std::max(s.a.y(), s.b.y()) + kMergeEps < std::min(t.a.y(), t.b.y())
            || std::max(t.a.y(), t.b.y()) + kMergeEps < std::min(s.a.y(), s.b.y()))
            return;
        const QPointF r = s.b - s.a, q = t.b - t.a;
        const double den = cross(r, q);
        const double lr = std::hypot(r.x(), r.y()), lq = std::hypot(q.x(), q.y());
        if (std::abs(den) > 1e-9 * lr * lq) {
            const QPointF ac = t.a - s.a;
            const double u = cross(ac, q) / den, v = cross(ac, r) / den;
            const double eu = 1e-9, ev = 1e-9;
            if (u < -eu || u > 1 + eu || v < -ev || v > 1 + ev)
                return;
            const QPointF pt = s.a + r * std::clamp(u, 0.0, 1.0);
            addCut(s, std::clamp(u, 0.0, 1.0), pt);
            addCut(t, std::clamp(v, 0.0, 1.0), pt);
        } else if (std::abs(cross(t.a - s.a, r)) / lr < kMergeEps * 0.1) {
            // Auf einer Geraden: Enden, die auf der anderen Strecke liegen, teilen sie
            for (int e = 0; e < 2; ++e) {
                const QPointF pt = e ? t.b : t.a;
                const double u = dot(pt - s.a, r) / (lr * lr);
                if (u > 0 && u < 1) {
                    addCut(s, u, pt);
                    addCut(t, e, pt);
                }
                const QPointF ps = e ? s.b : s.a;
                const double v = dot(ps - t.a, q) / (lq * lq);
                if (v > 0 && v < 1) {
                    addCut(t, v, ps);
                    addCut(s, e, ps);
                }
            }
        }
    };
    for (int i = 0; i < segs.size(); ++i)
        for (const std::vector<int>* list : segCells[std::size_t(i)])
            for (int j : *list) {
                if (j <= i || stamp[std::size_t(j)] == i)
                    continue;
                stamp[std::size_t(j)] = i;
                testPair(i, j);
            }

    // Kleine Lücken: ein offenes Ende, das eine andere Linie knapp verfehlt, mit ihr verbinden
    const int inputCount = int(paths.size());
    for (int i = 0; i < inputCount; ++i) {
        if (paths[i].closed)
            continue;
        const int m = segCount(paths[i]);
        for (int end = 0; end < 2; ++end) {
            const QPointF e = end ? polys[i].pts.last() : polys[i].pts.first();
            // Schon verbunden (Schnitt am Ende oder knapp dahinter)?
            const bool connected = std::any_of(cuts.begin(), cuts.end(), [&](const Cut& c) {
                return c.path == i && dist(c.point, e) <= std::max(2.0 * gap, kMergeEps);
            });
            if (connected)
                continue;
            double best = std::max(gap, kMergeEps);
            int bestSeg = -1;
            double bestU = 0;
            grid.query(QRectF(e - QPointF(best, best), e + QPointF(best, best)), [&](int id) {
                const Seg& s = segs[id];
                if (s.path == i)
                    return;
                const QPointF r = s.b - s.a;
                const double u = std::clamp(dot(e - s.a, r) / dot(r, r), 0.0, 1.0);
                const double d = dist(e, s.a + r * u);
                if (d <= best) {
                    best = d;
                    bestSeg = id;
                    bestU = u;
                }
            });
            if (bestSeg < 0)
                continue;
            const Seg& s = segs[bestSeg];
            const QPointF pt = s.a + (s.b - s.a) * bestU;
            addCut(s, bestU, pt);
            if (best > kMergeEps * 0.5) {
                paths << SubPath{{CurveNode{e}, CurveNode{pt}}, false};
                polys << flattenPath(paths.last());
            } else {
                cuts << Cut{i, end ? double(m) : 0.0, e};
            }
        }
    }
    // Enden offener Linienzüge sind Knoten; geschlossene ohne Schnitt beginnen irgendwo
    for (int i = 0; i < paths.size(); ++i) {
        const int m = segCount(paths[i]);
        if (!paths[i].closed) {
            cuts << Cut{i, 0.0, polys[i].pts.first()} << Cut{i, double(m), polys[i].pts.last()};
        } else if (std::none_of(cuts.begin(), cuts.end(), [i](const Cut& c) { return c.path == i; })) {
            cuts << Cut{i, 0.0, polys[i].pts.first()};
        }
    }

    // Nahe Schnittpunkte zu Knoten zusammenfassen
    UnionFind uf(int(cuts.size()));
    {
        // Raster der Weite kMergeEps: alle Schnitte einer Zelle sind ein Knoten, Nachbarzellen
        // nur über ihren ersten Schnitt vergleichen (bleibt linear, auch wenn viele Linien durch
        // einen Punkt laufen)
        const double x0 = bounds.left() - pad - 1, y0 = bounds.top() - pad - 1;
        std::unordered_map<std::int64_t, int> rep;
        auto cellKey = [](std::int64_t i, std::int64_t j) { return (i << 32) ^ std::int64_t(std::uint32_t(j)); };
        for (int c = 0; c < cuts.size(); ++c) {
            const QPointF p = cuts[c].point;
            const std::int64_t i = std::int64_t(std::floor((p.x() - x0) / kMergeEps));
            const std::int64_t j = std::int64_t(std::floor((p.y() - y0) / kMergeEps));
            auto [it, fresh] = rep.try_emplace(cellKey(i, j), c);
            if (!fresh) {
                uf.unite(c, it->second);
                continue;
            }
            for (int di = -1; di <= 1; ++di)
                for (int dj = -1; dj <= 1; ++dj) {
                    if (di == 0 && dj == 0)
                        continue;
                    auto n = rep.find(cellKey(i + di, j + dj));
                    if (n != rep.end() && dist(cuts[n->second].point, p) <= kMergeEps)
                        uf.unite(c, n->second);
                }
        }
    }
    std::unordered_map<int, int> nodeOf;
    QList<QPointF> sum;
    QList<int> count;
    QList<int> cutNode(cuts.size());
    for (int c = 0; c < cuts.size(); ++c) {
        const int root = uf.find(c);
        auto [it, fresh] = nodeOf.try_emplace(root, int(sum.size()));
        if (fresh) {
            sum << QPointF();
            count << 0;
        }
        cutNode[c] = it->second;
        sum[it->second] += cuts[c].point;
        ++count[it->second];
    }
    for (int n = 0; n < sum.size(); ++n)
        nodes << sum[n] / count[n];

    // Kanten zwischen aufeinanderfolgenden Schnitten jedes Linienzugs
    QList<QList<std::pair<double, int>>> perPath(paths.size());
    for (int c = 0; c < cuts.size(); ++c)
        perPath[cuts[c].path] << std::pair{cuts[c].param, cutNode[c]};
    std::unordered_map<std::uint64_t, QList<int>> byEnds;
    auto slice = [&](int i, double from, double to) {
        const Poly& p = polys[i];
        const double m = segCount(paths[i]);
        QPolygonF out;
        out << pointAt(p, from);
        for (int lap = 0; lap < (paths[i].closed ? 2 : 1); ++lap)
            for (int j = 0; j < p.params.size(); ++j) {
                const double q = p.params[j] + lap * m;
                if (q > from + 1e-12 && q < to - 1e-12)
                    out << p.pts[j];
            }
        out << pointAt(p, to > m ? to - m : to);
        return out;
    };
    auto addEdge = [&](int i, double from, double to, int a, int b) {
        Edge e{i, from, to, a, b, slice(i, from, to)};
        e.poly.first() = nodes[a];
        e.poly.last() = nodes[b];
        const double len = polyLength(e.poly);
        if (len < 4 * kMergeEps && (a == b || len < kMergeEps))
            return;
        // Doppelte Kanten (übereinanderliegende Linien) nur einmal
        const std::uint64_t key = (std::uint64_t(std::min(a, b)) << 32) | std::uint32_t(std::max(a, b));
        QList<int>& same = byEnds[key];
        for (int o : std::as_const(same)) {
            const Edge& f = edges[o];
            if (std::abs(polyLength(f.poly) - len) > 10 * kMergeEps)
                continue;
            const double lf = polyLength(f.poly);
            const bool flip = f.a != a;
            bool dup = true;
            for (double frac : {0.25, 0.5, 0.75}) {
                if (dist(alongPoly(e.poly, frac * len), alongPoly(f.poly, (flip ? 1 - frac : frac) * lf)) > 10 * kMergeEps) {
                    dup = false;
                    break;
                }
            }
            if (!dup && a == b) {
                // Geschlossene Schleife: auch die andere Richtung prüfen
                dup = true;
                for (double frac : {0.25, 0.5, 0.75})
                    if (dist(alongPoly(e.poly, frac * len), alongPoly(f.poly, (1 - frac) * lf)) > 10 * kMergeEps)
                        dup = false;
            }
            if (dup)
                return;
        }
        same << int(edges.size());
        edges << e;
    };
    for (int i = 0; i < paths.size(); ++i) {
        QList<std::pair<double, int>> list = perPath[i];
        std::sort(list.begin(), list.end());
        QList<std::pair<double, int>> unique;
        for (const auto& c : list)
            if (unique.isEmpty() || c.first - unique.last().first > 1e-9)
                unique << c;
        if (unique.isEmpty())
            continue;
        const int m = segCount(paths[i]);
        if (!paths[i].closed) {
            for (int k = 0; k + 1 < unique.size(); ++k)
                addEdge(i, unique[k].first, unique[k + 1].first, unique[k].second, unique[k + 1].second);
        } else {
            if (unique.size() >= 2 && unique.first().first + m - unique.last().first < 1e-9)
                unique.removeLast();
            for (int k = 0; k + 1 < unique.size(); ++k)
                addEdge(i, unique[k].first, unique[k + 1].first, unique[k].second, unique[k + 1].second);
            addEdge(i, unique.last().first, unique.first().first + m, unique.last().second, unique.first().second);
        }
    }

    // Lose Enden (Kanten mit einem Knoten vom Grad 1) wiederholt abschneiden
    QList<int> degree(nodes.size(), 0);
    QList<QList<int>> incident(nodes.size());
    for (int e = 0; e < edges.size(); ++e) {
        ++degree[edges[e].a];
        ++degree[edges[e].b];
        incident[edges[e].a] << e;
        if (edges[e].b != edges[e].a)
            incident[edges[e].b] << e;
    }
    QList<int> queue;
    for (int n = 0; n < nodes.size(); ++n)
        if (degree[n] == 1)
            queue << n;
    while (!queue.isEmpty()) {
        const int n = queue.takeLast();
        if (degree[n] != 1)
            continue;
        for (int e : std::as_const(incident[n])) {
            if (!edges[e].alive)
                continue;
            edges[e].alive = false;
            const int other = edges[e].a == n ? edges[e].b : edges[e].a;
            --degree[n];
            --degree[other];
            if (degree[other] == 1)
                queue << other;
            break;
        }
    }

    // Halbkanten um jeden Knoten nach Winkel ordnen
    const int heCount = int(edges.size()) * 2;
    QList<QList<int>> out(nodes.size());
    QList<double> angle(heCount, 0.0);
    for (int e = 0; e < edges.size(); ++e) {
        if (!edges[e].alive)
            continue;
        for (int dir = 0; dir < 2; ++dir) {
            const int he = 2 * e + dir;
            const QPolygonF p = halfPoly(he);
            const double len = polyLength(p);
            const QPointF d = alongPoly(p, std::min(0.05, 0.25 * len)) - p.first();
            angle[he] = std::atan2(d.y(), d.x());
            out[origin(he)] << he;
        }
    }
    QList<int> pos(heCount, -1);
    for (QList<int>& list : out) {
        std::sort(list.begin(), list.end(), [&angle](int x, int y) { return angle[x] < angle[y]; });
        for (int k = 0; k < list.size(); ++k)
            pos[list[k]] = k;
    }
    auto next = [&](int he) {
        const QList<int>& list = out[dest(he)];
        const int idx = pos[he ^ 1];
        return list[(idx - 1 + int(list.size())) % int(list.size())];
    };

    // Zusammenhangskomponenten
    UnionFind comp(int(nodes.size()));
    for (const Edge& e : std::as_const(edges))
        if (e.alive)
            comp.unite(e.a, e.b);

    // Maschen umlaufen: links abbiegen; umschlossene Maschen haben positive Fläche
    QList<bool> visited(heCount, false);
    for (int start = 0; start < heCount; ++start) {
        if (!edges[start >> 1].alive || visited[start])
            continue;
        Face f;
        int he = start;
        for (int guard = 0; guard <= heCount && !visited[he]; ++guard) {
            visited[he] = true;
            f.halfEdges << he;
            const QPolygonF p = halfPoly(he);
            for (int k = f.poly.isEmpty() ? 0 : 1; k < p.size(); ++k)
                f.poly << p[k];
            he = next(he);
        }
        if (f.poly.size() > 1 && f.poly.first() == f.poly.last())
            f.poly.removeLast();
        f.area = shoelace(f.poly);
        f.box = f.poly.boundingRect();
        f.comp = comp.find(origin(start));
        faces << f;
    }
}

QList<QList<int>> Arrangement::Data::loopsOf(const QList<int>& cycle) const
{
    // Kanten, die die Masche auf beiden Seiten hat (Stege zu Inseln), fallen weg; der Rest
    // zerfällt in einzelne Umläufe
    std::unordered_set<int> in(cycle.begin(), cycle.end());
    QList<QList<int>> loops;
    QList<int> stack;
    std::unordered_map<int, int> where;
    for (int he : cycle) {
        if (!in.count(he ^ 1)) {
            stack << he;
            continue;
        }
        auto it = where.find(he ^ 1);
        if (it != where.end()) {
            const int p = it->second;
            QList<int> loop = stack.mid(p + 1);
            for (int k = p; k < stack.size(); ++k)
                where.erase(stack[k]);
            stack.resize(p);
            if (!loop.isEmpty())
                loops << loop;
        } else {
            where[he] = int(stack.size());
            stack << he;
        }
    }
    QList<int> rest;
    for (int he : std::as_const(stack))
        if (!in.count(he ^ 1))
            rest << he;
    if (!rest.isEmpty())
        loops << rest;
    return loops;
}

SubPath Arrangement::Data::loopPath(const QList<int>& loop) const
{
    SubPath result;
    result.closed = true;
    for (int he : loop) {
        const Edge& e = edges[he >> 1];
        SubPath piece = subPath(paths[e.path], e.from, e.to);
        if (piece.nodes.size() < 2)
            continue;
        if (he & 1)
            piece = reversedOpen(piece);
        piece.nodes.first().point = nodes[origin(he)];
        piece.nodes.last().point = nodes[dest(he)];
        if (result.nodes.isEmpty())
            result.nodes << piece.nodes.first();
        result.nodes.append(piece.nodes.mid(1));
    }
    if (result.nodes.size() >= 2) {
        // Letzter Knoten = erster: sein Eingangssegment schliesst den Linienzug
        const CurveNode last = result.nodes.takeLast();
        result.nodes.first().curveIn = last.curveIn;
        result.nodes.first().c1 = last.c1;
        result.nodes.first().c2 = last.c2;
    }
    return result;
}

Arrangement::Arrangement(const QList<SubPath>& paths, double gap) : d(std::make_unique<Data>())
{
    d->gap = std::max(0.0, gap);
    d->build(paths);
}

Arrangement::~Arrangement() = default;
Arrangement::Arrangement(Arrangement&&) noexcept = default;
Arrangement& Arrangement::operator=(Arrangement&&) noexcept = default;

int Arrangement::faceCount() const
{
    return int(std::count_if(d->faces.begin(), d->faces.end(), [](const Face& f) { return f.area > 1e-9; }));
}

int Arrangement::edgeCount() const
{
    return int(std::count_if(d->edges.begin(), d->edges.end(), [](const Edge& e) { return e.alive; }));
}

QList<SubPath> Arrangement::faceAt(const QPointF& point) const
{
    const Face* best = nullptr;
    for (const Face& f : std::as_const(d->faces)) {
        if (f.area <= 1e-9 || !f.box.contains(point) || (best && f.area >= best->area))
            continue;
        if (f.poly.containsPoint(point, Qt::OddEvenFill))
            best = &f;
    }
    if (!best)
        return {};
    // Inseln: Aussenränder anderer Komponenten, die in der Masche liegen (nicht in einer anderen Insel)
    QList<const Face*> holes;
    for (const Face& f : std::as_const(d->faces)) {
        if (f.area >= -1e-9 || f.comp == best->comp || -f.area >= best->area || !best->box.contains(f.box))
            continue;
        if (best->poly.containsPoint(f.poly.first(), Qt::OddEvenFill))
            holes << &f;
    }
    QList<const Face*> direct;
    for (const Face* h : std::as_const(holes)) {
        const bool nested = std::any_of(holes.begin(), holes.end(), [h](const Face* o) {
            return o != h && -o->area > -h->area && o->box.contains(h->box) && o->poly.containsPoint(h->poly.first(), Qt::OddEvenFill);
        });
        if (!nested)
            direct << h;
    }
    QList<SubPath> result;
    for (const QList<int>& loop : d->loopsOf(best->halfEdges)) {
        const SubPath sp = d->loopPath(loop);
        if (sp.nodes.size() >= 2)
            result << sp;
    }
    for (const Face* h : std::as_const(direct))
        for (const QList<int>& loop : d->loopsOf(h->halfEdges)) {
            const SubPath sp = d->loopPath(loop);
            if (sp.nodes.size() >= 2)
                result << sp;
        }
    return result;
}

QPainterPath Arrangement::facePathAt(const QPointF& point) const
{
    return CurveObject::toPainterPath(faceAt(point));
}

} // namespace vek::pathops
