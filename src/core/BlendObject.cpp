#include "BlendObject.h"

#include <QObject>

#include <cmath>

namespace vek {

namespace {

double signedArea(const QPolygonF& p)
{
    double a = 0;
    for (int i = 0; i < p.size(); ++i) {
        const QPointF& u = p[i];
        const QPointF& v = p[(i + 1) % p.size()];
        a += u.x() * v.y() - v.x() * u.y();
    }
    return a / 2.0;
}

// Linienzug gleichmässig nach Bogenlänge in n Punkte
QPolygonF resample(const QPolygonF& in, int n, bool closed)
{
    QPolygonF poly = in;
    if (closed && poly.size() > 1 && poly.first() == poly.last())
        poly.removeLast();
    if (poly.size() < 2)
        return QPolygonF(n, poly.value(0));
    QList<double> cum{0.0};
    const int segs = closed ? int(poly.size()) : int(poly.size()) - 1;
    for (int i = 0; i < segs; ++i)
        cum << cum.last() + QLineF(poly[i], poly[(i + 1) % poly.size()]).length();
    const double total = std::max(1e-9, cum.last());
    QPolygonF out;
    int seg = 0;
    for (int k = 0; k < n; ++k) {
        const double d = total * k / (closed ? n : std::max(1, n - 1));
        while (seg < segs - 1 && cum[seg + 1] < d)
            ++seg;
        const double len = std::max(1e-12, cum[seg + 1] - cum[seg]);
        const double f = std::clamp((d - cum[seg]) / len, 0.0, 1.0);
        out << poly[seg] + (poly[(seg + 1) % poly.size()] - poly[seg]) * f;
    }
    return out;
}

Color mix(const Color& a, const Color& b, double t)
{
    auto ch = [t](int x, int y) { return std::uint8_t(std::lround(x + (y - x) * t)); };
    return Color(ch(a.r, b.r), ch(a.g, b.g), ch(a.b, b.b), ch(a.a, b.a));
}

} // namespace

const DrawObject* BlendObject::start() const
{
    return childCount() > 0 ? node_cast<DrawObject>(child(0)) : nullptr;
}

const DrawObject* BlendObject::end() const
{
    return childCount() > 1 ? node_cast<DrawObject>(child(1)) : nullptr;
}

QList<QPolygonF> BlendObject::interpolate(const QPainterPath& a, const QPainterPath& b, double t, int samples)
{
    const QList<QPolygonF> pa = a.toSubpathPolygons(), pb = b.toSubpathPolygons();
    QList<QPolygonF> out;
    const int n = int(std::min(pa.size(), pb.size()));
    for (int s = 0; s < n; ++s) {
        const bool closed = pa[s].isClosed() || pb[s].isClosed();
        const QPolygonF ra = resample(pa[s], samples, closed);
        QPolygonF rb = resample(pb[s], samples, closed);
        if (closed) {
            // Gleicher Umlaufsinn und günstigster Anfangspunkt
            if ((signedArea(ra) < 0) != (signedArea(rb) < 0))
                std::reverse(rb.begin(), rb.end());
            int best = 0;
            double bestDist = 1e300;
            for (int k = 0; k < rb.size(); ++k) {
                double d = 0;
                for (int i = 0; i < ra.size(); i += std::max(1, int(ra.size()) / 16))
                    d += QLineF(ra[i], rb[(i + k) % rb.size()]).length();
                if (d < bestDist) {
                    bestDist = d;
                    best = k;
                }
            }
            std::rotate(rb.begin(), rb.begin() + best, rb.end());
        }
        QPolygonF mixed;
        for (int i = 0; i < ra.size(); ++i)
            mixed << ra[i] + (rb[i] - ra[i]) * t;
        if (closed)
            mixed << mixed.first();
        out << mixed;
    }
    return out;
}

QList<std::shared_ptr<CurveObject>> BlendObject::intermediates() const
{
    QList<std::shared_ptr<CurveObject>> list;
    const DrawObject* a = start();
    const DrawObject* b = end();
    if (!a || !b)
        return list;
    const QPainterPath pa = a->path(), pb = b->path();
    const Style sa = a->style(), sb = b->style();
    const QPointF ca = a->boundingRect().center(), cb = b->boundingRect().center();
    for (int i = 1; i <= m_steps; ++i) {
        const double t = double(i) / (m_steps + 1);
        QList<SubPath> subs;
        for (const QPolygonF& poly : interpolate(pa, pb, t)) {
            SubPath sp;
            sp.closed = poly.isClosed();
            for (int k = 0; k < poly.size() - (sp.closed ? 1 : 0); ++k)
                sp.nodes << CurveNode{poly[k]};
            subs << sp;
        }
        auto c = std::make_shared<CurveObject>(subs);
        if (m_rotation != 0.0) {
            const QPointF m = ca + (cb - ca) * t;
            c->applyTransform(QTransform::fromTranslate(-m.x(), -m.y()) * QTransform().rotate(-m_rotation * t) * QTransform::fromTranslate(m.x(), m.y()));
        }
        Style s = sa;
        const std::optional<Color> fa = sa.fill.representativeColor(), fb = sb.fill.representativeColor();
        if (fa && fb)
            s.fill = Fill::solid(mix(*fa, *fb, t));
        else if (sa.fill.isNone() && sb.fill.isNone())
            s.fill = Fill::none();
        if (sa.stroke && sb.stroke)
            s.stroke = mix(*sa.stroke, *sb.stroke, t);
        s.strokeWidth = sa.strokeWidth + (sb.strokeWidth - sa.strokeWidth) * t;
        c->setStyle(s);
        list << c;
    }
    return list;
}

QPainterPath BlendObject::path() const
{
    QPainterPath p;
    if (const DrawObject* a = start())
        p.addPath(a->path());
    for (const auto& c : intermediates())
        p.addPath(c->path());
    if (const DrawObject* b = end())
        p.addPath(b->path());
    return p;
}

QRectF BlendObject::boundingRect() const
{
    QRectF r;
    for (int i = 0; i < childCount(); ++i)
        if (const auto* o = node_cast<DrawObject>(child(i)))
            r = r.isNull() ? o->boundingRect() : r.united(o->boundingRect());
    for (const auto& c : intermediates())
        r = r.united(c->boundingRect());
    return r;
}

bool BlendObject::hitTest(const QPointF& point, double tolerance, bool treatAsFilled) const
{
    for (int i = 0; i < childCount(); ++i)
        if (const auto* o = node_cast<DrawObject>(child(i)); o && o->hitTest(point, tolerance, treatAsFilled))
            return true;
    for (const auto& c : intermediates())
        if (c->hitTest(point, tolerance, true))
            return true;
    return false;
}

void BlendObject::applyTransform(const QTransform& m)
{
    for (int i = 0; i < childCount(); ++i)
        if (auto* o = node_cast<DrawObject>(child(i)))
            o->applyTransform(m);
}

QList<PropertyDescriptor> BlendObject::propertyDescriptors() const
{
    using K = PropertyDescriptor::Kind;
    PropertyDescriptor steps{QStringLiteral("steps"), QObject::tr("Überblendungsschritte"), QStringLiteral("blend-steps"), K::Number};
    steps.minimum = 1;
    steps.maximum = 999;
    steps.separatorBefore = true;
    PropertyDescriptor rot{QStringLiteral("rotation"), QObject::tr("Überblendungsrichtung (Drehung)"), QStringLiteral("rotate"), K::Number};
    rot.minimum = -3600;
    rot.maximum = 3600;
    return {steps, rot};
}

QVariant BlendObject::propertyValue(const QString& id) const
{
    if (id == QLatin1String("steps"))
        return m_steps;
    if (id == QLatin1String("rotation"))
        return m_rotation;
    return {};
}

bool BlendObject::setPropertyValue(const QString& id, const QVariant& value)
{
    if (id == QLatin1String("steps"))
        setSteps(value.toInt());
    else if (id == QLatin1String("rotation"))
        m_rotation = value.toDouble();
    else
        return false;
    return true;
}

void BlendObject::writeProperties(QJsonObject& out) const
{
    DrawObject::writeProperties(out);
    out[QStringLiteral("steps")] = m_steps;
    if (m_rotation != 0.0)
        out[QStringLiteral("rotation")] = m_rotation;
}

void BlendObject::readProperties(const QJsonObject& in)
{
    DrawObject::readProperties(in);
    m_steps = std::clamp(in.value(QStringLiteral("steps")).toInt(20), 1, 999);
    m_rotation = in.value(QStringLiteral("rotation")).toDouble();
}

} // namespace vek
