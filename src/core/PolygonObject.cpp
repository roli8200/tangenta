#include "PolygonObject.h"

#include <QJsonArray>
#include <QJsonObject>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <numeric>

namespace vek {

namespace {

const QString kPoints = QStringLiteral("points");
const QString kSharpness = QStringLiteral("sharpness");
const QString kNodeShape = QStringLiteral("nodeShape");
constexpr int kMaxPoints = 500;

} // namespace

PolygonObject::PolygonObject(const QSizeF& size, Kind kind, int points, int sharpness)
    : m_size(size), m_kind(kind), m_points(std::clamp(points, minPoints(kind), kMaxPoints))
{
    m_sharpness = clampSharpness(kind, m_points, sharpness);
}

int PolygonObject::maxSharpness(Kind kind, int points)
{
    if (kind == Kind::Star)
        return 99;
    if (kind == Kind::ComplexStar)
        return std::max(1, (points - 1) / 2 - 1); // Schritt bis knapp unter die Hälfte
    return 0;
}

int PolygonObject::clampSharpness(Kind kind, int points, int sharpness)
{
    return kind == Kind::Polygon ? 0 : std::clamp(sharpness, 1, maxSharpness(kind, points));
}

QPainterPath PolygonObject::localPath() const
{
    using std::numbers::pi;
    const QPointF c(m_size.width() / 2.0, m_size.height() / 2.0);
    // Punkt auf der eingeschriebenen Ellipse; Ecke 0 oben, im Uhrzeigersinn
    auto vertex = [&](double index, int count, double radius) {
        const double a = -pi / 2.0 + 2.0 * pi * index / count;
        return QPointF(c.x() + std::cos(a) * c.x() * radius, c.y() + std::sin(a) * c.y() * radius);
    };
    QPainterPath p;
    const int n = m_points;
    if (m_kind == Kind::ComplexStar) {
        // Sternvieleck {n/k}; haben n und k gemeinsame Teiler, sind es mehrere Linienzüge
        const int step = m_sharpness + 1;
        const int loops = std::gcd(n, step);
        for (int loop = 0; loop < loops; ++loop) {
            QPolygonF poly;
            for (int i = 0; i < n / loops; ++i)
                poly << vertex(loop + i * step, n, 1.0);
            p.addPolygon(poly);
            p.closeSubpath();
        }
        return p;
    }
    const QList<QPointF> ring = shapeNodes();
    if (m_kind == Kind::Polygon && m_nodeShape.innerMoved()) {
        // Geschwungenes Polygon: glatt durch die Seitenmitten, spitz an den Ecken
        p.moveTo(ring[0]);
        for (int i = 0; i < n; ++i) {
            const QPointF v0 = ring[2 * i], m = ring[2 * i + 1], v1 = ring[(2 * i + 2) % (2 * n)];
            const QPointF side = (v1 - v0) / 6.0;
            // Steuerpunkte parallel zur ursprünglichen Seite: keine Wendepunkte
            p.cubicTo(v0 + side, m - side, m);
            p.cubicTo(m + side, v1 - side, v1);
        }
        p.closeSubpath();
        return p;
    }
    QPolygonF poly;
    if (m_kind == Kind::Star || !m_nodeShape.isDefault()) {
        for (const QPointF& q : ring)
            poly << q;
    } else {
        for (int i = 0; i < n; ++i)
            poly << vertex(i, n, 1.0);
    }
    p.addPolygon(poly);
    p.closeSubpath();
    return p;
}

double PolygonObject::baseRadius(bool inner) const
{
    using std::numbers::pi;
    if (!inner)
        return 1.0;
    const double mid = std::cos(pi / m_points);
    return m_kind == Kind::Star ? mid * (1.0 - m_sharpness / 100.0) : mid;
}

QList<QPointF> PolygonObject::shapeNodes() const
{
    using std::numbers::pi;
    QList<QPointF> out;
    if (m_kind == Kind::ComplexStar)
        return out;
    const QPointF c(m_size.width() / 2.0, m_size.height() / 2.0);
    const int count = 2 * m_points;
    for (int j = 0; j < count; ++j) {
        const bool inner = j % 2 == 1;
        const double twist = (inner ? m_nodeShape.innerTwist : m_nodeShape.outerTwist) * pi / 180.0;
        const double radius = baseRadius(inner) * (inner ? m_nodeShape.innerScale : m_nodeShape.outerScale);
        const double a = -pi / 2.0 + 2.0 * pi * j / count + twist;
        out << QPointF(c.x() + std::cos(a) * c.x() * radius, c.y() + std::sin(a) * c.y() * radius);
    }
    return out;
}

void PolygonObject::applyTransform(const QTransform& m)
{
    DrawObject::applyTransform(m);
    absorbScale(m_size);
}

QList<PropertyDescriptor> PolygonObject::propertyDescriptors() const
{
    using K = PropertyDescriptor::Kind;
    QList<PropertyDescriptor> list;
    PropertyDescriptor points;
    points.id = kPoints;
    points.kind = K::Number;
    points.label = m_kind == Kind::Polygon ? QObject::tr("Punkte oder Seiten des Polygons")
                                           : QObject::tr("Spitzen des Sterns");
    points.icon = m_kind == Kind::Polygon ? QStringLiteral("polygon-points")
                  : m_kind == Kind::Star  ? QStringLiteral("star-points")
                                          : QStringLiteral("complex-star-points");
    points.minimum = minPoints(m_kind);
    points.maximum = kMaxPoints;
    points.separatorBefore = true;
    list << points;
    if (m_kind != Kind::Polygon) {
        PropertyDescriptor sharp;
        sharp.id = kSharpness;
        sharp.kind = K::Number;
        sharp.label = QObject::tr("Schärfe der Spitzen");
        sharp.icon = QStringLiteral("star-sharpness");
        sharp.minimum = 1;
        sharp.maximum = maxSharpness(m_kind, m_points);
        list << sharp;
    }
    return list;
}

QVariant PolygonObject::propertyValue(const QString& id) const
{
    if (id == kPoints)
        return m_points;
    if (id == kSharpness)
        return m_sharpness;
    return {};
}

bool PolygonObject::setPropertyValue(const QString& id, const QVariant& value)
{
    if (id == kPoints)
        m_points = std::clamp(int(std::lround(value.toDouble())), minPoints(m_kind), kMaxPoints);
    else if (id == kSharpness)
        m_sharpness = int(std::lround(value.toDouble()));
    else
        return false;
    m_sharpness = clampSharpness(m_kind, m_points, m_sharpness);
    return true;
}

void PolygonObject::writeProperties(QJsonObject& out) const
{
    DrawObject::writeProperties(out);
    out[QStringLiteral("width")] = m_size.width();
    out[QStringLiteral("height")] = m_size.height();
    out[QStringLiteral("polygonKind")] = m_kind == Kind::Star          ? QStringLiteral("star")
                                         : m_kind == Kind::ComplexStar ? QStringLiteral("complexStar")
                                                                       : QStringLiteral("polygon");
    out[kPoints] = m_points;
    if (m_kind != Kind::Polygon)
        out[kSharpness] = m_sharpness;
    if (!m_nodeShape.isDefault())
        out[kNodeShape] = QJsonArray{m_nodeShape.outerScale, m_nodeShape.outerTwist, m_nodeShape.innerScale,
                                     m_nodeShape.innerTwist};
}

void PolygonObject::readProperties(const QJsonObject& in)
{
    DrawObject::readProperties(in);
    m_size = QSizeF(in.value(QStringLiteral("width")).toDouble(), in.value(QStringLiteral("height")).toDouble());
    const QString kind = in.value(QStringLiteral("polygonKind")).toString();
    m_kind = kind == QLatin1String("star")          ? Kind::Star
             : kind == QLatin1String("complexStar") ? Kind::ComplexStar
                                                    : Kind::Polygon;
    m_points = std::clamp(in.value(kPoints).toInt(5), minPoints(m_kind), kMaxPoints);
    m_sharpness = clampSharpness(m_kind, m_points, in.value(kSharpness).toInt(53));
    const QJsonArray shape = in.value(kNodeShape).toArray();
    m_nodeShape = shape.size() == 4 ? NodeShape{shape[0].toDouble(1.0), shape[1].toDouble(), shape[2].toDouble(1.0),
                                                shape[3].toDouble()}
                                    : NodeShape{};
}

} // namespace vek
