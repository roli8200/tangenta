#include "RectangleObject.h"

#include <QJsonArray>

#include <cmath>

namespace vek {

namespace {

const QString kCornerType = QStringLiteral("cornerType");
const QString kCornersLinked = QStringLiteral("cornersLinked");
const QString kRadius[4] = {QStringLiteral("radiusTopLeft"), QStringLiteral("radiusTopRight"),
                            QStringLiteral("radiusBottomRight"), QStringLiteral("radiusBottomLeft")};

const char* cornerTypeName(RectangleObject::CornerType t)
{
    switch (t) {
    case RectangleObject::CornerType::Scallop: return "scallop";
    case RectangleObject::CornerType::Chamfer: return "chamfer";
    default: return "round";
    }
}

} // namespace

void RectangleObject::setCornerRadius(Corner c, double radius)
{
    m_radii[c] = std::clamp(radius, 0.0, maxRadius());
}

QPainterPath RectangleObject::localPath() const
{
    const double w = m_size.width();
    const double h = m_size.height();
    const double limit = maxRadius();
    double r[4];
    for (int i = 0; i < 4; ++i)
        r[i] = std::min(m_radii[i], limit);

    QPainterPath p;
    if (r[0] == 0.0 && r[1] == 0.0 && r[2] == 0.0 && r[3] == 0.0) {
        p.addRect(QRectF(0, 0, w, h));
        return p;
    }

    // Ecke von "from" nach "to"; Winkel nach Qt-Konvention (0 = 3 Uhr, positiv = gegen den Uhrzeiger)
    auto corner = [&](double radius, const QPointF& to, const QRectF& roundBox, double roundStart,
                      const QRectF& scallopBox, double scallopStart) {
        if (radius <= 0.0) {
            p.lineTo(to);
            return;
        }
        switch (m_cornerType) {
        case CornerType::Round: p.arcTo(roundBox, roundStart, -90); break;
        case CornerType::Scallop: p.arcTo(scallopBox, scallopStart, 90); break;
        case CornerType::Chamfer: p.lineTo(to); break;
        }
    };

    p.moveTo(r[TopLeft], 0);
    p.lineTo(w - r[TopRight], 0);
    corner(r[TopRight], QPointF(w, r[TopRight]), QRectF(w - 2 * r[TopRight], 0, 2 * r[TopRight], 2 * r[TopRight]), 90,
           QRectF(w - r[TopRight], -r[TopRight], 2 * r[TopRight], 2 * r[TopRight]), 180);
    p.lineTo(w, h - r[BottomRight]);
    corner(r[BottomRight], QPointF(w - r[BottomRight], h),
           QRectF(w - 2 * r[BottomRight], h - 2 * r[BottomRight], 2 * r[BottomRight], 2 * r[BottomRight]), 0,
           QRectF(w - r[BottomRight], h - r[BottomRight], 2 * r[BottomRight], 2 * r[BottomRight]), 90);
    p.lineTo(r[BottomLeft], h);
    corner(r[BottomLeft], QPointF(0, h - r[BottomLeft]),
           QRectF(0, h - 2 * r[BottomLeft], 2 * r[BottomLeft], 2 * r[BottomLeft]), 270,
           QRectF(-r[BottomLeft], h - r[BottomLeft], 2 * r[BottomLeft], 2 * r[BottomLeft]), 0);
    p.lineTo(0, r[TopLeft]);
    corner(r[TopLeft], QPointF(r[TopLeft], 0), QRectF(0, 0, 2 * r[TopLeft], 2 * r[TopLeft]), 180,
           QRectF(-r[TopLeft], -r[TopLeft], 2 * r[TopLeft], 2 * r[TopLeft]), 270);
    p.closeSubpath();
    return p;
}

namespace {
bool g_scaleCorners = true;
} // namespace

bool RectangleObject::scaleCorners()
{
    return g_scaleCorners;
}

void RectangleObject::setScaleCorners(bool on)
{
    g_scaleCorners = on;
}

void RectangleObject::applyTransform(const QTransform& m)
{
    DrawObject::applyTransform(m);
    const QSizeF before = m_size;
    absorbScale(m_size);
    if (g_scaleCorners && before.width() != 0 && before.height() != 0 && m_size != before) {
        // Ecken wachsen mit der kleineren Streckung, damit sie rund bleiben
        const double f = std::min(std::abs(m_size.width() / before.width()), std::abs(m_size.height() / before.height()));
        for (double& r : m_radii)
            r *= f;
    }
}

QList<PropertyDescriptor> RectangleObject::propertyDescriptors() const
{
    using K = PropertyDescriptor::Kind;
    QList<PropertyDescriptor> list;

    PropertyDescriptor type;
    type.id = kCornerType;
    type.label = QObject::tr("Eckentyp");
    type.kind = K::Choice;
    type.choiceIcons = {QStringLiteral("corner-round"), QStringLiteral("corner-scallop"),
                        QStringLiteral("corner-chamfer")};
    type.choiceLabels = {QObject::tr("Abgerundete Ecke"), QObject::tr("Ausgebuchtete Ecke"),
                         QObject::tr("Abgeschrägte Ecke")};
    type.separatorBefore = true;
    list << type;

    // Anordnung: oben links/unten links, Kopplung, oben rechts/unten rechts
    auto radius = [](const QString& id, const QString& label, const QString& icon, bool stack) {
        PropertyDescriptor d;
        d.id = id;
        d.label = label;
        d.icon = icon;
        d.kind = K::Length;
        d.stackWithNext = stack;
        return d;
    };
    list << radius(kRadius[TopLeft], QObject::tr("Eckenradius oben links"), QStringLiteral("radius-tl"), true);
    list << radius(kRadius[BottomLeft], QObject::tr("Eckenradius unten links"), QStringLiteral("radius-bl"), false);

    PropertyDescriptor link;
    link.id = kCornersLinked;
    link.label = QObject::tr("Alle Ecken gleich");
    link.icon = QStringLiteral("lock");
    link.kind = K::Toggle;
    list << link;

    list << radius(kRadius[TopRight], QObject::tr("Eckenradius oben rechts"), QStringLiteral("radius-tr"), true);
    list << radius(kRadius[BottomRight], QObject::tr("Eckenradius unten rechts"), QStringLiteral("radius-br"), false);
    return list;
}

QVariant RectangleObject::propertyValue(const QString& id) const
{
    if (id == kCornerType)
        return static_cast<int>(m_cornerType);
    if (id == kCornersLinked)
        return m_cornersLinked;
    for (int i = 0; i < 4; ++i) {
        if (id == kRadius[i])
            return m_radii[i];
    }
    return {};
}

bool RectangleObject::setPropertyValue(const QString& id, const QVariant& value)
{
    if (id == kCornerType) {
        m_cornerType = static_cast<CornerType>(std::clamp(value.toInt(), 0, 2));
        return true;
    }
    if (id == kCornersLinked) {
        m_cornersLinked = value.toBool();
        if (m_cornersLinked)
            m_radii.fill(m_radii[TopLeft]);
        return true;
    }
    for (int i = 0; i < 4; ++i) {
        if (id != kRadius[i])
            continue;
        if (m_cornersLinked) {
            for (int c = 0; c < 4; ++c)
                setCornerRadius(static_cast<Corner>(c), value.toDouble());
        } else {
            setCornerRadius(static_cast<Corner>(i), value.toDouble());
        }
        return true;
    }
    return false;
}

void RectangleObject::writeProperties(QJsonObject& out) const
{
    DrawObject::writeProperties(out);
    out[QStringLiteral("width")] = m_size.width();
    out[QStringLiteral("height")] = m_size.height();
    out[kCornerType] = QLatin1String(cornerTypeName(m_cornerType));
    out[kCornersLinked] = m_cornersLinked;
    out[QStringLiteral("radii")] = QJsonArray{m_radii[0], m_radii[1], m_radii[2], m_radii[3]};
}

void RectangleObject::readProperties(const QJsonObject& in)
{
    DrawObject::readProperties(in);
    m_size = QSizeF(in.value(QStringLiteral("width")).toDouble(), in.value(QStringLiteral("height")).toDouble());
    const QString type = in.value(kCornerType).toString();
    m_cornerType = type == QLatin1String("scallop")   ? CornerType::Scallop
                   : type == QLatin1String("chamfer") ? CornerType::Chamfer
                                                      : CornerType::Round;
    m_cornersLinked = in.value(kCornersLinked).toBool(true);
    const QJsonArray radii = in.value(QStringLiteral("radii")).toArray();
    for (int i = 0; i < 4; ++i)
        m_radii[static_cast<std::size_t>(i)] = radii.size() == 4 ? radii[i].toDouble() : 0.0;
}

} // namespace vek
