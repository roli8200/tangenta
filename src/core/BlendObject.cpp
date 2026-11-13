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

// Farbe eines Verlaufs an der Stelle pos (0..1)
Color colorAt(const QList<GradientStop>& stops, double pos)
{
    if (pos <= stops.first().position)
        return stops.first().color;
    for (int i = 1; i < stops.size(); ++i)
        if (pos <= stops[i].position) {
            const double span = stops[i].position - stops[i - 1].position;
            return mix(stops[i - 1].color, stops[i].color, span > 1e-9 ? (pos - stops[i - 1].position) / span : 1.0);
        }
    return stops.last().color;
}

// Lage auf dem Pfad (0..1 nach Länge), die dem Punkt am nächsten liegt
double nearestPercent(const QPainterPath& guide, const QPointF& point)
{
    double best = 0.0, bestDist = 1e300;
    constexpr int n = 600;
    for (int i = 0; i <= n; ++i) {
        const QPointF d = guide.pointAtPercent(double(i) / n) - point;
        const double dist = d.x() * d.x() + d.y() * d.y();
        if (dist < bestDist) {
            bestDist = dist;
            best = double(i) / n;
        }
    }
    return best;
}

// Richtung des Pfads an der Stelle pct in Grad (wie QTransform::rotate)
double directionAt(const QPainterPath& guide, double pct)
{
    const double a = std::clamp(pct - 0.002, 0.0, 1.0), b = std::clamp(pct + 0.002, 0.0, 1.0);
    const QPointF d = guide.pointAtPercent(b) - guide.pointAtPercent(a);
    return std::atan2(d.y(), d.x()) * 180.0 / M_PI;
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

const DrawObject* BlendObject::pathObject() const
{
    return childCount() > 2 ? node_cast<DrawObject>(child(2)) : nullptr;
}

void BlendObject::setFullPath(bool on)
{
    m_fullPath = on;
    const DrawObject* guideObject = pathObject();
    if (!on || !guideObject)
        return;
    // Start und Ende an die Enden des Pfads setzen
    const QPainterPath guide = guideObject->path();
    for (int i = 0; i < 2 && i < childCount(); ++i)
        if (auto* o = node_cast<DrawObject>(child(i))) {
            const QPointF d = guide.pointAtPercent(i == 0 ? 0.0 : 1.0) - o->boundingRect().center();
            o->applyTransform(QTransform::fromTranslate(d.x(), d.y()));
        }
}

QList<const DrawObject*> BlendObject::paintOrder() const
{
    QList<const DrawObject*> list;
    if (const DrawObject* a = start())
        list << a;
    for (const auto& step : cache()->intermediates)
        list << step.get();
    if (const DrawObject* b = end())
        list << b;
    if (m_reversed)
        std::reverse(list.begin(), list.end());
    return list;
}

Fill BlendObject::mixFill(const Fill& a, const Fill& b, double t)
{
    if (a.isGradient() && b.isGradient() && a.type == b.type && !a.stops.isEmpty() && !b.stops.isEmpty()) {
        // Verlauf bleibt Verlauf: Achse und Farben an allen Stopps beider Verläufe überblenden
        Fill f = a;
        f.start = a.start + (b.start - a.start) * t;
        f.end = a.end + (b.end - a.end) * t;
        QList<double> positions;
        for (const GradientStop& s : a.stops + b.stops)
            positions << s.position;
        std::sort(positions.begin(), positions.end());
        positions.erase(std::unique(positions.begin(), positions.end(), [](double x, double y) { return std::abs(x - y) < 1e-6; }),
                        positions.end());
        f.stops.clear();
        for (double p : std::as_const(positions))
            f.stops << GradientStop{p, mix(colorAt(a.stops, p), colorAt(b.stops, p), t)};
        return f;
    }
    // Verlauf und gleichmässige Farbe: die Farbe wie einen einfarbigen Verlauf behandeln
    if (a.isGradient() && !a.stops.isEmpty() && b.type == Fill::Type::Solid) {
        Fill flat = a;
        for (GradientStop& s : flat.stops)
            s.color = b.color;
        return mixFill(a, flat, t);
    }
    if (b.isGradient() && !b.stops.isEmpty() && a.type == Fill::Type::Solid) {
        Fill flat = b;
        for (GradientStop& s : flat.stops)
            s.color = a.color;
        return mixFill(flat, b, t);
    }
    const std::optional<Color> fa = a.representativeColor(), fb = b.representativeColor();
    if (fa && fb)
        return Fill::solid(mix(*fa, *fb, t));
    if (a.isNone() && b.isNone())
        return Fill::none();
    return t < 0.5 ? a : b;
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

std::shared_ptr<const BlendObject::Cache> BlendObject::cache() const
{
    const DrawObject* a = start();
    const DrawObject* b = end();
    const QPainterPath pa = a ? a->path() : QPainterPath();
    const QPainterPath pb = b ? b->path() : QPainterPath();
    const Style sa = a ? a->style() : Style();
    const Style sb = b ? b->style() : Style();
    const DrawObject* g = pathObject();
    const QPainterPath guide = g ? g->path() : QPainterPath();
    if (m_cache && m_cache->steps == m_steps && m_cache->rotation == m_rotation && m_cache->startStyle == sa
        && m_cache->endStyle == sb && m_cache->startPath == pa && m_cache->endPath == pb && m_cache->guide == guide
        && m_cache->fullPath == m_fullPath && m_cache->rotateAll == m_rotateAll)
        return m_cache;
    // Neuer Eintrag statt Ändern: Kopien des Objekts (Rückgängig) teilen den alten
    auto c = std::make_shared<Cache>();
    c->startPath = pa;
    c->endPath = pb;
    c->startStyle = sa;
    c->endStyle = sb;
    c->steps = m_steps;
    c->rotation = m_rotation;
    c->guide = guide;
    c->fullPath = m_fullPath;
    c->rotateAll = m_rotateAll;
    c->intermediates = computeIntermediates();
    c->path.addPath(pa);
    for (const auto& step : std::as_const(c->intermediates))
        c->path.addPath(step->path());
    c->path.addPath(pb);
    c->path.addPath(guide);
    for (int i = 0; i < childCount(); ++i)
        if (const auto* o = node_cast<DrawObject>(child(i)))
            c->bounds = c->bounds.isNull() ? o->boundingRect() : c->bounds.united(o->boundingRect());
    for (const auto& step : std::as_const(c->intermediates))
        c->bounds = c->bounds.united(step->boundingRect());
    m_cache = c;
    return m_cache;
}

QList<std::shared_ptr<CurveObject>> BlendObject::intermediates() const
{
    return cache()->intermediates;
}

QList<std::shared_ptr<CurveObject>> BlendObject::computeIntermediates() const
{
    QList<std::shared_ptr<CurveObject>> list;
    const DrawObject* a = start();
    const DrawObject* b = end();
    if (!a || !b)
        return list;
    const QPainterPath pa = a->path(), pb = b->path();
    const Style sa = a->style(), sb = b->style();
    const QPointF ca = a->boundingRect().center(), cb = b->boundingRect().center();
    // An Pfad: Stufen laufen zwischen den Pfadstellen von Start und Ende (bzw. über den ganzen Pfad)
    const DrawObject* g = pathObject();
    const QPainterPath guide = g ? g->path() : QPainterPath();
    const bool onPath = !guide.isEmpty() && guide.length() > 1e-6;
    const double p0 = onPath && !m_fullPath ? nearestPercent(guide, ca) : 0.0;
    const double p1 = onPath && !m_fullPath ? nearestPercent(guide, cb) : 1.0;
    const double dir0 = onPath ? directionAt(guide, p0) : 0.0;
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
        const QPointF m = ca + (cb - ca) * t;
        if (onPath) {
            const double pct = p0 + (p1 - p0) * t;
            const QPointF at = guide.pointAtPercent(pct);
            const double turn = m_rotateAll ? directionAt(guide, pct) - dir0 : 0.0;
            c->applyTransform(QTransform::fromTranslate(-m.x(), -m.y()) * QTransform().rotate(turn - m_rotation * t)
                              * QTransform::fromTranslate(at.x(), at.y()));
        } else if (m_rotation != 0.0) {
            c->applyTransform(QTransform::fromTranslate(-m.x(), -m.y()) * QTransform().rotate(-m_rotation * t) * QTransform::fromTranslate(m.x(), m.y()));
        }
        Style s = sa;
        s.fill = mixFill(sa.fill, sb.fill, t);
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
    return cache()->path;
}

QRectF BlendObject::boundingRect() const
{
    return cache()->bounds;
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
    QList<PropertyDescriptor> list{steps, rot};
    PropertyDescriptor newPath{QStringLiteral("blendPath"), QObject::tr("Neuer Pfad: Überblendung an einen Pfad hängen"), QStringLiteral("blend-path"), K::Action};
    newPath.separatorBefore = true;
    list << newPath;
    if (pathObject()) {
        list << PropertyDescriptor{QStringLiteral("blendDetach"), QObject::tr("Vom Pfad lösen"), QStringLiteral("blend-detach"), K::Action};
        PropertyDescriptor full{QStringLiteral("fullPath"), QObject::tr("Über den ganzen Pfad überblenden"), QStringLiteral("blend-fullpath"), K::Toggle};
        PropertyDescriptor turn{QStringLiteral("rotateAll"), QObject::tr("Alle Objekte drehen (der Pfadrichtung nach)"), QStringLiteral("blend-rotate"), K::Toggle};
        list << full << turn;
    }
    PropertyDescriptor reverse{QStringLiteral("reversed"), QObject::tr("Reihenfolge umkehren (Ende zuunterst)"), QStringLiteral("blend-reverse"), K::Toggle};
    reverse.separatorBefore = true;
    list << reverse;
    return list;
}

QVariant BlendObject::propertyValue(const QString& id) const
{
    if (id == QLatin1String("steps"))
        return m_steps;
    if (id == QLatin1String("rotation"))
        return m_rotation;
    if (id == QLatin1String("fullPath"))
        return m_fullPath;
    if (id == QLatin1String("rotateAll"))
        return m_rotateAll;
    if (id == QLatin1String("reversed"))
        return m_reversed;
    return {};
}

bool BlendObject::setPropertyValue(const QString& id, const QVariant& value)
{
    if (id == QLatin1String("steps"))
        setSteps(value.toInt());
    else if (id == QLatin1String("rotation"))
        m_rotation = value.toDouble();
    else if (id == QLatin1String("fullPath"))
        setFullPath(value.toBool());
    else if (id == QLatin1String("rotateAll"))
        m_rotateAll = value.toBool();
    else if (id == QLatin1String("reversed"))
        m_reversed = value.toBool();
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
    if (m_fullPath)
        out[QStringLiteral("fullPath")] = true;
    if (m_rotateAll)
        out[QStringLiteral("rotateAll")] = true;
    if (m_reversed)
        out[QStringLiteral("reversed")] = true;
}

void BlendObject::readProperties(const QJsonObject& in)
{
    DrawObject::readProperties(in);
    m_steps = std::clamp(in.value(QStringLiteral("steps")).toInt(20), 1, 999);
    m_rotation = in.value(QStringLiteral("rotation")).toDouble();
    m_fullPath = in.value(QStringLiteral("fullPath")).toBool();
    m_rotateAll = in.value(QStringLiteral("rotateAll")).toBool();
    m_reversed = in.value(QStringLiteral("reversed")).toBool();
}

} // namespace vek
