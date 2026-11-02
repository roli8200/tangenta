#include "Ramp.h"

#include <QJsonArray>
#include <QJsonObject>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace vek {

namespace {

const char* typeName(Ramp::Type t)
{
    switch (t) {
    case Ramp::Type::Uniform: return "uniform";
    case Ramp::Type::Linear: return "linear";
    case Ramp::Type::Radial: return "radial";
    case Ramp::Type::Conical: return "conical";
    case Ramp::Type::Pattern: return "pattern";
    default: return "none";
    }
}

QJsonArray point(const QPointF& p)
{
    return QJsonArray{p.x(), p.y()};
}

QPointF point(const QJsonValue& v, const QPointF& fallback)
{
    const QJsonArray a = v.toArray();
    return a.size() == 2 ? QPointF(a[0].toDouble(), a[1].toDouble()) : fallback;
}

} // namespace

Ramp Ramp::uniform(double value)
{
    Ramp r;
    if (value > 0.0) {
        r.type = Type::Uniform;
        r.from = value;
    }
    return r;
}

double Ramp::minimum() const
{
    if (type == Type::None)
        return 0.0;
    return type == Type::Uniform ? from : std::min(from, to);
}

double Ramp::maximum() const
{
    if (type == Type::None)
        return 0.0;
    return type == Type::Uniform ? from : std::max(from, to);
}

double Ramp::parameterAt(const QPointF& local, const QRectF& b) const
{
    if (!isGradient())
        return 0.0;
    const QPointF s(b.left() + start.x() * b.width(), b.top() + start.y() * b.height());
    const QPointF e(b.left() + end.x() * b.width(), b.top() + end.y() * b.height());
    const QPointF axis = e - s;
    const QPointF d = local - s;
    double t = 0.0;
    switch (type) {
    case Type::Linear: {
        const double len2 = QPointF::dotProduct(axis, axis);
        t = len2 > 0.0 ? QPointF::dotProduct(d, axis) / len2 : 0.0;
        break;
    }
    case Type::Radial: {
        const double r = std::hypot(axis.x(), axis.y());
        t = r > 0.0 ? std::hypot(d.x(), d.y()) / r : 0.0;
        break;
    }
    case Type::Conical: {
        // Einmal im Uhrzeigersinn herum, beginnend in Richtung end
        const double twoPi = 2.0 * std::numbers::pi;
        const double a = std::atan2(d.y(), d.x()) - std::atan2(axis.y(), axis.x());
        t = std::fmod(a + 2.0 * twoPi, twoPi) / twoPi;
        break;
    }
    default:
        break;
    }
    return std::clamp(t, 0.0, 1.0);
}

double Ramp::valueAt(const QPointF& local, const QRectF& localBounds) const
{
    switch (type) {
    case Type::None: return 0.0;
    case Type::Uniform: return from;
    case Type::Pattern: return from + (to - from) * pattern.lightness(local - localBounds.topLeft());
    default: {
        const double t = parameterAt(local, localBounds);
        const double m = std::clamp(midpoint, 0.01, 0.99);
        const double k = t < m ? 0.5 * t / m : 0.5 + 0.5 * (t - m) / (1.0 - m);
        return from + (to - from) * k;
    }
    }
}

QJsonValue Ramp::toJson() const
{
    QJsonObject o;
    o[QStringLiteral("type")] = QLatin1String(typeName(type));
    o[QStringLiteral("from")] = from;
    if (isGradient()) {
        o[QStringLiteral("to")] = to;
        o[QStringLiteral("start")] = point(start);
        o[QStringLiteral("end")] = point(end);
        if (std::abs(midpoint - 0.5) > 1e-9)
            o[QStringLiteral("mid")] = midpoint;
    }
    if (type == Type::Pattern) {
        o[QStringLiteral("to")] = to;
        o[QStringLiteral("pattern")] = pattern.toJson();
    }
    return o;
}

Ramp Ramp::fromJson(const QJsonValue& json)
{
    const QJsonObject o = json.toObject();
    Ramp r;
    const QString type = o.value(QStringLiteral("type")).toString();
    for (Type t : {Type::Uniform, Type::Linear, Type::Radial, Type::Conical, Type::Pattern}) {
        if (type == QLatin1String(typeName(t)))
            r.type = t;
    }
    r.from = o.value(QStringLiteral("from")).toDouble();
    r.to = o.value(QStringLiteral("to")).toDouble();
    r.start = point(o.value(QStringLiteral("start")), r.start);
    r.end = point(o.value(QStringLiteral("end")), r.end);
    r.midpoint = std::clamp(o.value(QStringLiteral("mid")).toDouble(0.5), 0.01, 0.99);
    if (r.type == Type::Pattern)
        r.pattern = PatternFill::fromJson(o.value(QStringLiteral("pattern")).toObject());
    return r;
}

} // namespace vek
