#include "DimensionObject.h"

#include <QFont>
#include <QFontMetricsF>
#include <QJsonArray>
#include <QLocale>
#include <QObject>
#include <QLineF>
#include <QtMath>

#include <cmath>

namespace vek {

namespace {

constexpr double kGap = 1.0;      // mm Abstand der Hilfslinien vom gemessenen Punkt
constexpr double kOvershoot = 1.5; // mm Überstand der Hilfslinien über die Masslinie
constexpr double kArrowLength = 2.5;
constexpr double kArrowHalf = 0.8;
constexpr double kTextGap = 1.0;

QJsonArray point(const QPointF& p)
{
    return QJsonArray{p.x(), p.y()};
}

QPointF point(const QJsonValue& v)
{
    const QJsonArray a = v.toArray();
    return a.size() == 2 ? QPointF(a[0].toDouble(), a[1].toDouble()) : QPointF();
}

double length(const QPointF& v)
{
    return std::hypot(v.x(), v.y());
}

void arrow(QPainterPath& path, const QPointF& tip, const QPointF& towards)
{
    const QPointF d = towards - tip;
    const double len = length(d);
    if (len < 1e-9)
        return;
    const QPointF u = d / len;
    const QPointF n(-u.y(), u.x());
    const QPointF base = tip + u * kArrowLength;
    path.addPolygon(QPolygonF({tip, base + n * kArrowHalf, base - n * kArrowHalf, tip}));
    path.closeSubpath();
}

// Text als Pfad, mittig über dem Punkt at, in Richtung angle (Grad, Bildschirm, im Uhrzeigersinn)
void addText(QPainterPath& path, const QString& text, const QString& family, double sizePt, const QPointF& at,
             double angle, bool below)
{
    QFont font(family);
    font.setPixelSize(std::max(1, int(std::lround(sizePt * 25.4 / 72.0 * 100.0))));
    QPainterPath glyphs;
    glyphs.addText(QPointF(0, 0), font, text);
    const QRectF b = glyphs.boundingRect();
    QTransform t;
    t.translate(at.x(), at.y());
    t.rotate(angle);
    t.scale(0.01, 0.01);
    // Unterkante bzw. Oberkante des Textes an die Masslinie, waagrecht zentriert
    t.translate(-b.center().x(), below ? -b.top() + kTextGap * 100.0 : -b.bottom() - kTextGap * 100.0);
    path.addPath(t.map(glyphs));
}

QString formatNumber(double v, int precision, bool fraction)
{
    if (!fraction)
        return QLocale().toString(v, 'f', precision);
    // Bruch: ganze Zahl und nächster Sechzehntel
    const double whole = std::floor(std::abs(v));
    int sixteenth = int(std::lround((std::abs(v) - whole) * 16.0));
    double w = whole;
    if (sixteenth == 16) {
        w += 1;
        sixteenth = 0;
    }
    QString s = v < 0 ? QStringLiteral("-") : QString();
    int num = sixteenth, den = 16;
    while (num > 0 && num % 2 == 0) {
        num /= 2;
        den /= 2;
    }
    if (w > 0 || num == 0)
        s += QString::number(int(w));
    if (num > 0)
        s += (w > 0 ? QStringLiteral(" ") : QString()) + QStringLiteral("%1/%2").arg(num).arg(den);
    return s;
}

} // namespace

DimensionObject::DimensionObject(Kind kind, const QPointF& a, const QPointF& b, const QPointF& c, const QPointF& d)
    : m_kind(kind), m_a(a), m_b(b), m_c(c), m_d(d)
{
    Style s;
    s.fill = Fill::none();
    s.stroke = Color(0, 0, 0);
    s.strokeWidth = 0.2;
    setStyle(s);
    if (kind == Kind::Angular)
        m_precision = 0;
}

QStringList DimensionObject::lengthUnits()
{
    return {QStringLiteral("mm"), QStringLiteral("cm"), QStringLiteral("m"), QStringLiteral("in"), QStringLiteral("pt"),
            QStringLiteral("px"), QStringLiteral("pc"), QStringLiteral("km"), QStringLiteral("ft")};
}

double DimensionObject::lengthUnitInMm(int unit)
{
    static const double mm[] = {1.0, 10.0, 1000.0, 25.4, 25.4 / 72.0, 25.4 / 96.0, 25.4 / 6.0, 1000000.0, 304.8};
    return mm[std::clamp(unit, 0, 8)];
}

double DimensionObject::measuredValue() const
{
    switch (m_kind) {
    case Kind::Horizontal: return std::abs(m_b.x() - m_a.x()) * m_scale / lengthUnitInMm(m_units);
    case Kind::Vertical: return std::abs(m_b.y() - m_a.y()) * m_scale / lengthUnitInMm(m_units);
    case Kind::Parallel: return length(m_b - m_a) * m_scale / lengthUnitInMm(m_units);
    case Kind::Angular: {
        double deg = std::abs(QLineF(m_a, m_b).angleTo(QLineF(m_a, m_c)));
        if (deg > 180.0)
            deg = 360.0 - deg;
        if (m_units == 1)
            return qDegreesToRadians(deg);
        if (m_units == 2)
            return deg / 0.9;
        return deg;
    }
    }
    return 0.0;
}

QString DimensionObject::text() const
{
    QString unit;
    if (m_kind == Kind::Angular)
        unit = m_units == 1 ? QStringLiteral(" rad") : m_units == 2 ? QStringLiteral(" gon") : QStringLiteral("°");
    else
        unit = QLatin1Char(' ') + lengthUnits().value(m_units);
    return m_prefix + formatNumber(measuredValue(), m_precision, m_fraction && m_kind != Kind::Angular) + unit + m_suffix;
}

DimensionObject::Geometry DimensionObject::build() const
{
    Geometry g;
    if (m_kind == Kind::Angular) {
        const double radius = length(m_d - m_a);
        double a1 = QLineF(m_a, m_b).angle(); // gegen den Uhrzeigersinn, 0 = rechts
        double a2 = QLineF(m_a, m_c).angle();
        double span = a2 - a1;
        while (span > 180.0)
            span -= 360.0;
        while (span < -180.0)
            span += 360.0;
        // Schenkel vom Scheitel bis über den Bogen hinaus
        for (const QPointF& arm : {m_b, m_c}) {
            const QPointF u = (arm - m_a) / std::max(1e-9, length(arm - m_a));
            g.lines.moveTo(m_a);
            g.lines.lineTo(m_a + u * std::max(length(arm - m_a), radius + kOvershoot));
        }
        const QRectF circle(m_a.x() - radius, m_a.y() - radius, 2 * radius, 2 * radius);
        g.lines.arcMoveTo(circle, a1);
        g.lines.arcTo(circle, a1, span);
        if (radius > 0.01) {
            auto at = [&](double deg) {
                return m_a + QPointF(std::cos(qDegreesToRadians(deg)), -std::sin(qDegreesToRadians(deg))) * radius;
            };
            const double step = qRadiansToDegrees(kArrowLength / radius) * (span < 0 ? -1.0 : 1.0);
            arrow(g.fills, at(a1), at(a1 + step));
            arrow(g.fills, at(a1 + span), at(a1 + span - step));
            const double mid = a1 + span / 2.0;
            double textAngle = -(mid - 90.0); // tangential
            while (textAngle > 90.0)
                textAngle -= 180.0;
            while (textAngle <= -90.0)
                textAngle += 180.0;
            addText(g.fills, text(), m_font, m_sizePt, at(mid), textAngle, false);
        }
        return g;
    }
    // Masslinie: Richtung und Lage
    QPointF dir;
    QPointF s1;
    QPointF s2;
    if (m_kind == Kind::Horizontal) {
        dir = QPointF(1, 0);
        s1 = QPointF(m_a.x(), m_c.y());
        s2 = QPointF(m_b.x(), m_c.y());
    } else if (m_kind == Kind::Vertical) {
        dir = QPointF(0, 1);
        s1 = QPointF(m_c.x(), m_a.y());
        s2 = QPointF(m_c.x(), m_b.y());
    } else {
        const double len = length(m_b - m_a);
        dir = len > 0 ? (m_b - m_a) / len : QPointF(1, 0);
        const QPointF n(-dir.y(), dir.x());
        const double off = QPointF::dotProduct(m_c - m_a, n);
        s1 = m_a + n * off;
        s2 = m_b + n * off;
    }
    // Hilfslinien von den Punkten bis über die Masslinie
    for (const auto& [from, to] : {std::pair{m_a, s1}, std::pair{m_b, s2}}) {
        const QPointF d = to - from;
        const double len = length(d);
        if (len < 1e-6)
            continue;
        const QPointF u = d / len;
        g.lines.moveTo(from + u * std::min(kGap, len));
        g.lines.lineTo(to + u * kOvershoot);
    }
    g.lines.moveTo(s1);
    g.lines.lineTo(s2);
    arrow(g.fills, s1, s2);
    arrow(g.fills, s2, s1);
    // Masszahl über der Masslinie, lesbar gedreht
    double angle = qRadiansToDegrees(std::atan2(s2.y() - s1.y(), s2.x() - s1.x()));
    if (angle > 90.0)
        angle -= 180.0;
    else if (angle <= -90.0)
        angle += 180.0;
    // Auf der Seite des Lagepunkts? Bei waagrechter Masslinie unter den Punkten: Text darunter
    const QPointF mid = (s1 + s2) / 2.0;
    const QPointF n(-std::sin(qDegreesToRadians(angle)), std::cos(qDegreesToRadians(angle))); // "unten"
    const bool below = QPointF::dotProduct(mid - (m_a + m_b) / 2.0, n) > 0.0;
    addText(g.fills, text(), m_font, m_sizePt, mid, angle, below);
    return g;
}

QPainterPath DimensionObject::linePath() const
{
    return transform().map(build().lines);
}

QPainterPath DimensionObject::fillPath() const
{
    return transform().map(build().fills);
}

QPainterPath DimensionObject::localPath() const
{
    const Geometry g = build();
    QPainterPath all = g.lines;
    all.addPath(g.fills);
    return all;
}

void DimensionObject::applyTransform(const QTransform& m)
{
    // Punkte mitbewegen, damit neu gemessen wird; die Matrix bleibt die Einheitsmatrix
    m_a = m.map(m_a);
    m_b = m.map(m_b);
    m_c = m.map(m_c);
    m_d = m.map(m_d);
}

QList<PropertyDescriptor> DimensionObject::propertyDescriptors() const
{
    using K = PropertyDescriptor::Kind;
    PropertyDescriptor precision{QStringLiteral("precision"), QObject::tr("Präzision (Dezimalstellen)"), QString(), K::Number};
    precision.minimum = 0;
    precision.maximum = 6;
    PropertyDescriptor units{QStringLiteral("units"), QObject::tr("Einheiten"), QString(), K::List};
    units.choiceLabels = m_kind == Kind::Angular ? QStringList{QStringLiteral("°"), QObject::tr("Radiant"), QObject::tr("Gon")}
                                                 : lengthUnits();
    units.separatorBefore = true;
    PropertyDescriptor prefix{QStringLiteral("prefix"), QObject::tr("Präfix"), QString(), K::Text};
    prefix.separatorBefore = true;
    PropertyDescriptor suffix{QStringLiteral("suffix"), QObject::tr("Suffix"), QString(), K::Text};
    QList<PropertyDescriptor> list{precision, units, prefix, suffix};
    if (m_kind != Kind::Angular) {
        PropertyDescriptor style{QStringLiteral("style"), QObject::tr("Stil"), QString(), K::List};
        style.choiceLabels = {QObject::tr("Dezimal"), QObject::tr("Bruch")};
        style.separatorBefore = true;
        list.prepend(style);
    }
    return list;
}

QVariant DimensionObject::propertyValue(const QString& id) const
{
    if (id == QLatin1String("precision"))
        return m_precision;
    if (id == QLatin1String("units"))
        return m_units;
    if (id == QLatin1String("prefix"))
        return m_prefix;
    if (id == QLatin1String("suffix"))
        return m_suffix;
    if (id == QLatin1String("style"))
        return m_fraction ? 1 : 0;
    return {};
}

bool DimensionObject::setPropertyValue(const QString& id, const QVariant& value)
{
    if (id == QLatin1String("precision"))
        setPrecision(value.toInt());
    else if (id == QLatin1String("units"))
        m_units = value.toInt();
    else if (id == QLatin1String("prefix"))
        m_prefix = value.toString();
    else if (id == QLatin1String("suffix"))
        m_suffix = value.toString();
    else if (id == QLatin1String("style"))
        m_fraction = value.toInt() == 1;
    else
        return false;
    return true;
}

void DimensionObject::writeProperties(QJsonObject& out) const
{
    DrawObject::writeProperties(out);
    static const char* kinds[] = {"parallel", "horizontal", "vertical", "angular"};
    out[QStringLiteral("kind")] = QLatin1String(kinds[int(m_kind)]);
    out[QStringLiteral("a")] = point(m_a);
    out[QStringLiteral("b")] = point(m_b);
    out[QStringLiteral("c")] = point(m_c);
    if (m_kind == Kind::Angular)
        out[QStringLiteral("d")] = point(m_d);
    out[QStringLiteral("precision")] = m_precision;
    out[QStringLiteral("units")] = m_units;
    if (m_fraction)
        out[QStringLiteral("fraction")] = true;
    if (!m_prefix.isEmpty())
        out[QStringLiteral("prefix")] = m_prefix;
    if (!m_suffix.isEmpty())
        out[QStringLiteral("suffix")] = m_suffix;
    if (m_scale != 1.0)
        out[QStringLiteral("scale")] = m_scale;
    out[QStringLiteral("font")] = m_font;
    out[QStringLiteral("size")] = m_sizePt;
}

void DimensionObject::readProperties(const QJsonObject& in)
{
    DrawObject::readProperties(in);
    const QString kind = in.value(QStringLiteral("kind")).toString();
    m_kind = kind == QLatin1String("horizontal") ? Kind::Horizontal
             : kind == QLatin1String("vertical") ? Kind::Vertical
             : kind == QLatin1String("angular")  ? Kind::Angular
                                                 : Kind::Parallel;
    m_a = point(in.value(QStringLiteral("a")));
    m_b = point(in.value(QStringLiteral("b")));
    m_c = point(in.value(QStringLiteral("c")));
    m_d = point(in.value(QStringLiteral("d")));
    m_precision = in.value(QStringLiteral("precision")).toInt(2);
    m_units = in.value(QStringLiteral("units")).toInt();
    m_fraction = in.value(QStringLiteral("fraction")).toBool();
    m_prefix = in.value(QStringLiteral("prefix")).toString();
    m_suffix = in.value(QStringLiteral("suffix")).toString();
    m_scale = in.value(QStringLiteral("scale")).toDouble(1.0);
    m_font = in.value(QStringLiteral("font")).toString(QStringLiteral("Arial"));
    m_sizePt = in.value(QStringLiteral("size")).toDouble(10.0);
}

} // namespace vek
