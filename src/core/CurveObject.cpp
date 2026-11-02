#include "CurveObject.h"

#include "PathOps.h"

#include <QJsonArray>
#include <QLineF>

#include <algorithm>
#include <cmath>
#include <limits>

namespace vek {

namespace {

QPointF lerp(const QPointF& a, const QPointF& b, double t)
{
    return a + (b - a) * t;
}

QPointF cubic(const QPointF& p0, const QPointF& c1, const QPointF& c2, const QPointF& p3, double t)
{
    const double u = 1.0 - t;
    return p0 * (u * u * u) + c1 * (3 * u * u * t) + c2 * (3 * u * t * t) + p3 * (t * t * t);
}

double length(const QPointF& v)
{
    return std::hypot(v.x(), v.y());
}

bool samePoint(const QPointF& a, const QPointF& b)
{
    return length(a - b) < 1e-9;
}

} // namespace

std::unique_ptr<CurveObject> CurveObject::fromPath(const QPainterPath& path)
{
    QList<SubPath> paths;
    auto finish = [&paths] {
        if (paths.isEmpty())
            return;
        SubPath& s = paths.last();
        // Endet ein Linienzug auf seinem Anfang, ist er geschlossen: das letzte Segment wird
        // zum Eingangssegment des ersten Knotens
        if (s.nodes.size() > 2 && samePoint(s.nodes.first().point, s.nodes.last().point)) {
            const CurveNode last = s.nodes.takeLast();
            CurveNode& first = s.nodes.first();
            first.curveIn = last.curveIn;
            first.c1 = last.c1;
            first.c2 = last.c2;
            s.closed = true;
        }
        if (s.nodes.size() < 2)
            paths.removeLast();
    };
    for (int i = 0; i < path.elementCount(); ++i) {
        const QPainterPath::Element e = path.elementAt(i);
        switch (e.type) {
        case QPainterPath::MoveToElement:
            finish();
            paths.append(SubPath{{CurveNode{QPointF(e.x, e.y)}}, false});
            break;
        case QPainterPath::LineToElement:
            paths.last().nodes.append(CurveNode{QPointF(e.x, e.y)});
            break;
        case QPainterPath::CurveToElement: {
            const QPainterPath::Element c2 = path.elementAt(i + 1);
            const QPainterPath::Element end = path.elementAt(i + 2);
            CurveNode n{QPointF(end.x, end.y), true, QPointF(e.x, e.y), QPointF(c2.x, c2.y)};
            paths.last().nodes.append(n);
            i += 2;
            break;
        }
        case QPainterPath::CurveToDataElement:
            break;
        }
    }
    finish();
    return std::make_unique<CurveObject>(paths);
}

QPainterPath CurveObject::localPath() const
{
    if (!m_media.isActive())
        return spinePath();
    QPainterPath result;
    result.setFillRule(Qt::WindingFill);
    for (const SubPath& s : m_paths)
        result.addPath(pathops::mediaOutline(CurveObject(QList<SubPath>{s}).spinePath(), m_media));
    return result;
}

QList<PropertyDescriptor> CurveObject::propertyDescriptors() const
{
    QList<PropertyDescriptor> list;
    if (!m_media.isActive())
        return list;
    PropertyDescriptor width;
    width.id = QStringLiteral("mediaWidth");
    width.label = QObject::tr("Strichbreite der künstlerischen Medien");
    width.icon = QStringLiteral("media-width");
    width.minimum = 0.05;
    width.maximum = 1000.0;
    width.separatorBefore = true;
    list.append(width);
    if (m_media.kind == ArtisticMedia::Kind::Calligraphy) {
        PropertyDescriptor angle;
        angle.id = QStringLiteral("mediaAngle");
        angle.label = QObject::tr("Federwinkel");
        angle.icon = QStringLiteral("media-angle");
        angle.kind = PropertyDescriptor::Kind::Angle;
        angle.minimum = -360.0;
        angle.maximum = 360.0;
        list.append(angle);
    }
    return list;
}

QVariant CurveObject::propertyValue(const QString& id) const
{
    if (id == QLatin1String("mediaWidth"))
        return m_media.width;
    if (id == QLatin1String("mediaAngle"))
        return m_media.angle;
    return {};
}

bool CurveObject::setPropertyValue(const QString& id, const QVariant& value)
{
    if (id == QLatin1String("mediaWidth"))
        m_media.width = std::max(0.05, value.toDouble());
    else if (id == QLatin1String("mediaAngle"))
        m_media.angle = value.toDouble();
    else
        return false;
    return true;
}

QPainterPath CurveObject::toPainterPath(const QList<SubPath>& paths)
{
    QPainterPath p;
    for (const SubPath& s : paths) {
        if (s.nodes.isEmpty())
            continue;
        p.moveTo(s.nodes.first().point);
        auto segment = [&p](const CurveNode& n) {
            if (n.curveIn)
                p.cubicTo(n.c1, n.c2, n.point);
            else
                p.lineTo(n.point);
        };
        for (int i = 1; i < s.nodes.size(); ++i)
            segment(s.nodes[i]);
        if (s.closed) {
            segment(s.nodes.first());
            p.closeSubpath();
        }
    }
    return p;
}

QPainterPath CurveObject::spinePath() const
{
    return toPainterPath(m_paths);
}

int CurveObject::nodeCount() const
{
    int n = 0;
    for (const SubPath& s : m_paths)
        n += s.nodes.size();
    return n;
}

bool CurveObject::isValid(NodeRef r) const
{
    return r.path >= 0 && r.path < m_paths.size() && r.node >= 0 && r.node < m_paths[r.path].nodes.size();
}

bool CurveObject::hasSegmentInto(NodeRef r) const
{
    if (!isValid(r))
        return false;
    const SubPath& s = m_paths[r.path];
    return s.nodes.size() >= 2 && (r.node > 0 || s.closed);
}

NodeRef CurveObject::previous(NodeRef r) const
{
    if (!hasSegmentInto(r))
        return {};
    const int n = m_paths[r.path].nodes.size();
    return {r.path, (r.node + n - 1) % n};
}

NodeRef CurveObject::next(NodeRef r) const
{
    if (!isValid(r))
        return {};
    const SubPath& s = m_paths[r.path];
    if (r.node + 1 < s.nodes.size())
        return {r.path, r.node + 1};
    return s.closed && s.nodes.size() >= 2 ? NodeRef{r.path, 0} : NodeRef{};
}

bool CurveObject::isEndNode(NodeRef r) const
{
    return isValid(r) && !m_paths[r.path].closed && (r.node == 0 || r.node == m_paths[r.path].nodes.size() - 1);
}

QPointF CurveObject::pointOnSegment(NodeRef r, double t) const
{
    const QPointF p0 = node(previous(r)).point;
    const CurveNode& n = node(r);
    return n.curveIn ? cubic(p0, n.c1, n.c2, n.point, t) : lerp(p0, n.point, t);
}

double CurveObject::nearestOnSegment(NodeRef r, const QPointF& p, double* distance) const
{
    // Grob abtasten, dann um das beste Stück herum verfeinern
    const int steps = 64;
    double bestT = 0.0;
    double best = std::numeric_limits<double>::max();
    for (int i = 0; i <= steps; ++i) {
        const double t = double(i) / steps;
        const double d = length(pointOnSegment(r, t) - p);
        if (d < best) {
            best = d;
            bestT = t;
        }
    }
    double lo = std::max(0.0, bestT - 1.0 / steps);
    double hi = std::min(1.0, bestT + 1.0 / steps);
    for (int i = 0; i < 40; ++i) {
        const double a = lo + (hi - lo) / 3;
        const double b = hi - (hi - lo) / 3;
        if (length(pointOnSegment(r, a) - p) < length(pointOnSegment(r, b) - p))
            hi = b;
        else
            lo = a;
    }
    bestT = (lo + hi) / 2;
    if (distance)
        *distance = length(pointOnSegment(r, bestT) - p);
    return bestT;
}

QPointF CurveObject::controlOut(NodeRef r) const
{
    const NodeRef n = next(r);
    return n.isValid() && node(n).curveIn ? node(n).c1 : node(r).point;
}

void CurveObject::moveNodes(const QList<NodeRef>& nodes, const QPointF& delta)
{
    for (NodeRef r : nodes) {
        if (!isValid(r))
            continue;
        CurveNode& n = m_paths[r.path].nodes[r.node];
        n.point += delta;
        if (n.curveIn && hasSegmentInto(r))
            n.c2 += delta;
        const NodeRef nx = next(r);
        if (nx.isValid() && node(nx).curveIn)
            m_paths[nx.path].nodes[nx.node].c1 += delta;
    }
}

void CurveObject::moveControl(NodeRef r, bool incoming, const QPointF& pos)
{
    if (incoming) {
        if (!hasSegmentInto(r) || !node(r).curveIn)
            return;
        m_paths[r.path].nodes[r.node].c2 = pos;
    } else {
        const NodeRef nx = next(r);
        if (!nx.isValid() || !node(nx).curveIn)
            return;
        m_paths[nx.path].nodes[nx.node].c1 = pos;
    }
    alignOpposite(r, incoming);
}

void CurveObject::alignOpposite(NodeRef r, bool movedIncoming)
{
    const CurveNode& n = node(r);
    if (n.kind == NodeKind::Cusp)
        return;
    const bool hasIn = hasSegmentInto(r) && n.curveIn;
    const NodeRef nx = next(r);
    const bool hasOut = nx.isValid() && node(nx).curveIn;
    if (!hasIn || !hasOut)
        return;
    const QPointF moved = (movedIncoming ? n.c2 : node(nx).c1) - n.point;
    const QPointF opposite = (movedIncoming ? node(nx).c1 : n.c2) - n.point;
    const double movedLen = length(moved);
    if (movedLen < 1e-12)
        return;
    const double len = n.kind == NodeKind::Symmetric ? movedLen : length(opposite);
    const QPointF target = n.point - moved / movedLen * len;
    if (movedIncoming)
        m_paths[nx.path].nodes[nx.node].c1 = target;
    else
        m_paths[r.path].nodes[r.node].c2 = target;
}

void CurveObject::setSegmentCurve(NodeRef r, bool curve)
{
    if (!hasSegmentInto(r))
        return;
    const QPointF p0 = node(previous(r)).point;
    CurveNode& n = m_paths[r.path].nodes[r.node];
    if (curve && !n.curveIn) {
        n.c1 = lerp(p0, n.point, 1.0 / 3.0);
        n.c2 = lerp(p0, n.point, 2.0 / 3.0);
    }
    n.curveIn = curve;
}

void CurveObject::setKind(NodeRef r, NodeKind kind)
{
    if (!isValid(r))
        return;
    m_paths[r.path].nodes[r.node].kind = kind;
    if (kind == NodeKind::Cusp)
        return;
    const CurveNode& n = node(r);
    const bool hasIn = hasSegmentInto(r) && n.curveIn;
    const NodeRef nx = next(r);
    const bool hasOut = nx.isValid() && node(nx).curveIn;
    const QPointF p = n.point;
    if (hasIn && hasOut) {
        // Gemeinsame Richtung aus beiden Steuerpunkten, Längen bleiben (symmetrisch: Mittelwert)
        QPointF dir = node(nx).c1 - n.c2;
        const double l = length(dir);
        if (l < 1e-12)
            return;
        dir /= l;
        double lin = length(n.c2 - p);
        double lout = length(node(nx).c1 - p);
        if (kind == NodeKind::Symmetric)
            lin = lout = (lin + lout) / 2;
        m_paths[r.path].nodes[r.node].c2 = p - dir * lin;
        m_paths[nx.path].nodes[nx.node].c1 = p + dir * lout;
    } else if (hasIn && nx.isValid()) {
        // Kurve trifft auf Linie: Steuerpunkt in Verlängerung der Linie
        QPointF dir = node(nx).point - p;
        const double l = length(dir);
        if (l > 1e-12)
            m_paths[r.path].nodes[r.node].c2 = p - dir / l * length(n.c2 - p);
    } else if (hasOut && hasSegmentInto(r)) {
        QPointF dir = p - node(previous(r)).point;
        const double l = length(dir);
        if (l > 1e-12)
            m_paths[nx.path].nodes[nx.node].c1 = p + dir / l * length(node(nx).c1 - p);
    }
}

NodeRef CurveObject::insertNode(NodeRef r, double t)
{
    if (!hasSegmentInto(r))
        return {};
    const QPointF p0 = node(previous(r)).point;
    CurveNode& n = m_paths[r.path].nodes[r.node];
    CurveNode added;
    if (n.curveIn) {
        // de Casteljau: beide Hälften zusammen ergeben genau die alte Kurve
        const QPointF p01 = lerp(p0, n.c1, t);
        const QPointF p12 = lerp(n.c1, n.c2, t);
        const QPointF p23 = lerp(n.c2, n.point, t);
        const QPointF p012 = lerp(p01, p12, t);
        const QPointF p123 = lerp(p12, p23, t);
        added = CurveNode{lerp(p012, p123, t), true, p01, p012, NodeKind::Smooth};
        n.c1 = p123;
        n.c2 = p23;
    } else {
        added = CurveNode{lerp(p0, n.point, t)};
    }
    QList<CurveNode>& nodes = m_paths[r.path].nodes;
    // Segment in den ersten Knoten ist das Schliesssegment: neuer Knoten kommt ans Ende
    const int index = r.node == 0 ? nodes.size() : r.node;
    nodes.insert(index, added);
    return {r.path, index};
}

void CurveObject::deleteNodes(QList<NodeRef> refs)
{
    // Von hinten nach vorn, damit die übrigen Adressen gültig bleiben
    std::sort(refs.begin(), refs.end(), [](NodeRef a, NodeRef b) {
        return a.path != b.path ? a.path > b.path : a.node > b.node;
    });
    for (NodeRef r : refs) {
        if (!isValid(r))
            continue;
        const NodeRef prev = previous(r);
        const NodeRef nx = next(r);
        if (prev.isValid() && nx.isValid()) {
            // Zwei Segmente prev -> r -> nx werden zu einem prev -> nx
            const CurveNode d = node(r);
            const QPointF a = node(prev).point;
            CurveNode& n = m_paths[nx.path].nodes[nx.node];
            if (d.curveIn || n.curveIn) {
                const QPointF c1 = d.curveIn ? d.c1 : lerp(a, d.point, 1.0 / 3.0);
                const QPointF c2 = n.curveIn ? n.c2 : lerp(d.point, n.point, 2.0 / 3.0);
                n.curveIn = true;
                n.c1 = c1;
                n.c2 = c2;
            }
        } else if (nx.isValid()) {
            // Erster Knoten eines offenen Linienzugs: der nächste beginnt ihn, ohne Segment davor
            m_paths[nx.path].nodes[nx.node].curveIn = false;
        }
        SubPath& s = m_paths[r.path];
        s.nodes.removeAt(r.node);
        // Beim geschlossenen Linienzug führt das Schliesssegment jetzt in den neuen ersten Knoten;
        // der hat die zusammengelegten Daten oben schon bekommen
        if (s.nodes.size() < 2)
            m_paths.removeAt(r.path);
    }
}

void CurveObject::bendSegment(NodeRef r, double t, const QPointF& delta)
{
    if (!hasSegmentInto(r) || !node(r).curveIn)
        return;
    t = std::clamp(t, 0.01, 0.99);
    // Gewichtung wie in Inkscape: nahe einem Ende bewegt sich vor allem dessen Steuerpunkt
    double weight;
    if (t <= 1.0 / 6.0)
        weight = 0.0;
    else if (t <= 0.5)
        weight = std::pow((6 * t - 1) / 2.0, 3) / 2;
    else if (t <= 5.0 / 6.0)
        weight = (1 - std::pow((6 * (1 - t) - 1) / 2.0, 3)) / 2 + 0.5;
    else
        weight = 1.0;
    CurveNode& n = m_paths[r.path].nodes[r.node];
    n.c1 += delta * ((1 - weight) / (3 * t * (1 - t) * (1 - t)));
    n.c2 += delta * (weight / (3 * t * t * (1 - t)));
    // Glatte Nachbarn bleiben glatt
    alignOpposite(r, true);
    if (const NodeRef prev = previous(r); prev.isValid())
        alignOpposite(prev, false);
}

void CurveObject::setClosed(int path, bool closed)
{
    if (path < 0 || path >= m_paths.size())
        return;
    SubPath& s = m_paths[path];
    if (s.closed == closed || s.nodes.size() < 2)
        return;
    s.closed = closed;
    s.nodes.first().curveIn = false; // neues Schliesssegment ist eine Linie
}

void CurveObject::reverse(int path)
{
    if (path < 0 || path >= m_paths.size())
        return;
    SubPath& s = m_paths[path];
    const int n = s.nodes.size();
    // Das Segment in Knoten k (vom Vorgänger her) führt danach in den Vorgänger, mit
    // vertauschten Steuerpunkten
    QList<CurveNode> out = s.nodes;
    for (CurveNode& c : out)
        c.curveIn = false;
    for (int k = 0; k < n; ++k) {
        const NodeRef r{path, k};
        if (!hasSegmentInto(r))
            continue;
        const CurveNode& from = s.nodes[k];
        CurveNode& into = out[previous(r).node];
        into.curveIn = from.curveIn;
        into.c1 = from.c2;
        into.c2 = from.c1;
    }
    std::reverse(out.begin(), out.end());
    if (s.closed) {
        // Reihenfolge so drehen, dass der bisherige erste Knoten erster bleibt
        std::rotate(out.begin(), out.end() - 1, out.end());
    }
    s.nodes = out;
}

NodeRef CurveObject::appendNode(int path, bool atStart, const QPointF& point)
{
    if (path < 0 || path >= m_paths.size() || m_paths[path].closed)
        return {};
    QList<CurveNode>& nodes = m_paths[path].nodes;
    if (atStart) {
        nodes.first().curveIn = false;
        nodes.prepend(CurveNode{point});
        return {path, 0};
    }
    nodes.append(CurveNode{point});
    return {path, int(nodes.size()) - 1};
}

void CurveObject::writeProperties(QJsonObject& out) const
{
    DrawObject::writeProperties(out);
    out[QStringLiteral("paths")] = pathsToJson(m_paths);
    if (m_media.isActive()) {
        static const char* kinds[] = {"none", "preset", "calligraphy", "pressure", "sprayer"};
        QJsonObject m{
            {QStringLiteral("kind"), QLatin1String(kinds[int(m_media.kind)])},
            {QStringLiteral("preset"), m_media.preset},
            {QStringLiteral("width"), m_media.width},
            {QStringLiteral("angle"), m_media.angle},
        };
        if (m_media.kind == ArtisticMedia::Kind::Pressure) {
            QJsonArray p;
            for (double v : m_media.pressure)
                p << v;
            m[QStringLiteral("pressure")] = p;
        }
        if (m_media.kind == ArtisticMedia::Kind::Sprayer) {
            m[QStringLiteral("spray")] = m_media.sprayList;
            m[QStringLiteral("spacing")] = m_media.spacing;
            m[QStringLiteral("randomRotate")] = m_media.randomRotate;
            m[QStringLiteral("randomOrder")] = m_media.randomOrder;
            m[QStringLiteral("seed")] = qint64(m_media.seed);
        }
        out[QStringLiteral("media")] = m;
    }
}

void CurveObject::readProperties(const QJsonObject& in)
{
    DrawObject::readProperties(in);
    m_media = ArtisticMedia();
    if (in.contains(QStringLiteral("media"))) {
        const QJsonObject m = in.value(QStringLiteral("media")).toObject();
        const QString kind = m.value(QStringLiteral("kind")).toString();
        m_media.kind = kind == QLatin1String("calligraphy") ? ArtisticMedia::Kind::Calligraphy
                       : kind == QLatin1String("pressure")  ? ArtisticMedia::Kind::Pressure
                       : kind == QLatin1String("sprayer")   ? ArtisticMedia::Kind::Sprayer
                                                            : ArtisticMedia::Kind::Preset;
        for (const QJsonValue& v : m.value(QStringLiteral("pressure")).toArray())
            m_media.pressure << std::clamp(v.toDouble(), 0.0, 1.0);
        m_media.sprayList = std::clamp(m.value(QStringLiteral("spray")).toInt(), 0, pathops::sprayListCount() - 1);
        m_media.spacing = std::max(0.2, m.value(QStringLiteral("spacing")).toDouble(8.0));
        m_media.randomRotate = m.value(QStringLiteral("randomRotate")).toBool(true);
        m_media.randomOrder = m.value(QStringLiteral("randomOrder")).toBool(true);
        m_media.seed = quint32(m.value(QStringLiteral("seed")).toInteger(1));
        m_media.preset = std::clamp(m.value(QStringLiteral("preset")).toInt(), 0, pathops::mediaPresetCount() - 1);
        m_media.width = std::max(0.05, m.value(QStringLiteral("width")).toDouble(10.0));
        m_media.angle = m.value(QStringLiteral("angle")).toDouble();
    }
    m_paths = pathsFromJson(in.value(QStringLiteral("paths")).toArray());
}

QJsonArray CurveObject::pathsToJson(const QList<SubPath>& list)
{
    QJsonArray paths;
    for (const SubPath& s : list) {
        QJsonArray nodes;
        for (const CurveNode& n : s.nodes) {
            QJsonArray a{n.point.x(), n.point.y(), int(n.kind)};
            if (n.curveIn) {
                a.append(n.c1.x());
                a.append(n.c1.y());
                a.append(n.c2.x());
                a.append(n.c2.y());
            }
            nodes.append(a);
        }
        paths.append(QJsonObject{{QStringLiteral("closed"), s.closed}, {QStringLiteral("nodes"), nodes}});
    }
    return paths;
}

QList<SubPath> CurveObject::pathsFromJson(const QJsonArray& json)
{
    QList<SubPath> list;
    for (const QJsonValue& pv : json) {
        const QJsonObject po = pv.toObject();
        SubPath s;
        s.closed = po.value(QStringLiteral("closed")).toBool();
        for (const QJsonValue& nv : po.value(QStringLiteral("nodes")).toArray()) {
            const QJsonArray a = nv.toArray();
            if (a.size() < 3)
                continue;
            CurveNode n;
            n.point = QPointF(a[0].toDouble(), a[1].toDouble());
            n.kind = static_cast<NodeKind>(std::clamp(a[2].toInt(), 0, 2));
            if (a.size() >= 7) {
                n.curveIn = true;
                n.c1 = QPointF(a[3].toDouble(), a[4].toDouble());
                n.c2 = QPointF(a[5].toDouble(), a[6].toDouble());
            }
            s.nodes.append(n);
        }
        if (s.nodes.size() >= 2)
            list.append(s);
    }
    return list;
}

} // namespace vek
