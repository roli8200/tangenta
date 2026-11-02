#include "EllipseObject.h"

#include <QJsonObject>

#include <cmath>

namespace vek {

namespace {

const QString kKind = QStringLiteral("ellipseKind");
const QString kStart = QStringLiteral("startAngle");
const QString kEnd = QStringLiteral("endAngle");
const QString kClockwise = QStringLiteral("clockwise");

double normalized(double degrees)
{
    double a = std::fmod(degrees, 360.0);
    return a < 0 ? a + 360.0 : a;
}

} // namespace

QPainterPath EllipseObject::localPath() const
{
    QPainterPath p;
    const QRectF r(QPointF(0, 0), m_size);
    if (m_kind == Kind::Ellipse) {
        p.addEllipse(r);
        return p;
    }
    // Überstrichener Winkel von Anfang zu Ende in der gewählten Richtung
    double sweep = normalized(m_end - m_start);
    if (sweep < 1e-9)
        sweep = 360.0;
    if (m_clockwise)
        sweep -= 360.0;
    if (m_kind == Kind::Pie) {
        p.moveTo(r.center());
        p.arcTo(r, m_start, sweep);
        p.closeSubpath();
    } else {
        p.arcMoveTo(r, m_start);
        p.arcTo(r, m_start, sweep);
    }
    return p;
}

void EllipseObject::applyTransform(const QTransform& m)
{
    DrawObject::applyTransform(m);
    absorbScale(m_size);
}

QList<PropertyDescriptor> EllipseObject::propertyDescriptors() const
{
    using K = PropertyDescriptor::Kind;
    QList<PropertyDescriptor> list;
    PropertyDescriptor kind;
    kind.id = kKind;
    kind.label = QObject::tr("Ellipsenart");
    kind.kind = K::Choice;
    kind.choiceIcons = {QStringLiteral("ellipse-full"), QStringLiteral("ellipse-pie"), QStringLiteral("ellipse-arc")};
    kind.choiceLabels = {QObject::tr("Ellipse"), QObject::tr("Kreissegment"), QObject::tr("Bogen")};
    kind.separatorBefore = true;
    list << kind;

    PropertyDescriptor start;
    start.id = kStart;
    start.label = QObject::tr("Anfangswinkel");
    start.icon = QStringLiteral("angle-start");
    start.kind = K::Angle;
    start.minimum = -360.0;
    start.maximum = 360.0;
    start.stackWithNext = true;
    PropertyDescriptor end = start;
    end.id = kEnd;
    end.label = QObject::tr("Endwinkel");
    end.icon = QStringLiteral("angle-end");
    end.stackWithNext = false;
    list << start << end;

    PropertyDescriptor direction;
    direction.id = kClockwise;
    direction.label = QObject::tr("Richtung ändern (im Uhrzeigersinn)");
    direction.icon = QStringLiteral("arc-direction");
    direction.kind = K::Toggle;
    list << direction;
    return list;
}

QVariant EllipseObject::propertyValue(const QString& id) const
{
    if (id == kKind)
        return int(m_kind);
    if (id == kStart)
        return m_start;
    if (id == kEnd)
        return m_end;
    if (id == kClockwise)
        return m_clockwise;
    return {};
}

bool EllipseObject::setPropertyValue(const QString& id, const QVariant& value)
{
    if (id == kKind)
        m_kind = static_cast<Kind>(std::clamp(value.toInt(), 0, 2));
    else if (id == kStart)
        m_start = value.toDouble();
    else if (id == kEnd)
        m_end = value.toDouble();
    else if (id == kClockwise)
        m_clockwise = value.toBool();
    else
        return false;
    return true;
}

void EllipseObject::writeProperties(QJsonObject& out) const
{
    DrawObject::writeProperties(out);
    out[QStringLiteral("width")] = m_size.width();
    out[QStringLiteral("height")] = m_size.height();
    if (m_kind != Kind::Ellipse) {
        out[kKind] = m_kind == Kind::Pie ? QStringLiteral("pie") : QStringLiteral("arc");
        out[kStart] = m_start;
        out[kEnd] = m_end;
        out[kClockwise] = m_clockwise;
    }
}

void EllipseObject::readProperties(const QJsonObject& in)
{
    DrawObject::readProperties(in);
    m_size = QSizeF(in.value(QStringLiteral("width")).toDouble(), in.value(QStringLiteral("height")).toDouble());
    const QString kind = in.value(kKind).toString();
    m_kind = kind == QLatin1String("pie") ? Kind::Pie : kind == QLatin1String("arc") ? Kind::Arc : Kind::Ellipse;
    m_start = in.value(kStart).toDouble(0.0);
    m_end = in.value(kEnd).toDouble(270.0);
    m_clockwise = in.value(kClockwise).toBool(false);
}

} // namespace vek
