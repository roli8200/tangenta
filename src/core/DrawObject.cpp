#include "DrawObject.h"

#include <QJsonArray>
#include <QPainterPathStroker>
#include <QtMath>

#include <cmath>

namespace vek {

namespace xform {

QTransform translation(const QPointF& delta)
{
    return QTransform::fromTranslate(delta.x(), delta.y());
}

// Qt wendet Matrizen von links nach rechts auf Zeilenvektoren an: p * A * B = erst A, dann B.
QTransform scaling(double sx, double sy, const QPointF& anchor)
{
    return QTransform::fromTranslate(-anchor.x(), -anchor.y()) * QTransform::fromScale(sx, sy)
           * QTransform::fromTranslate(anchor.x(), anchor.y());
}

QTransform rotation(double degrees, const QPointF& center)
{
    QTransform r;
    r.rotate(degrees);
    return QTransform::fromTranslate(-center.x(), -center.y()) * r
           * QTransform::fromTranslate(center.x(), center.y());
}

QTransform skewing(double degreesX, double degreesY, const QPointF& anchor)
{
    // x' = x + tan(ax) * y,  y' = y + tan(ay) * x
    const QTransform shear(1.0, qTan(qDegreesToRadians(degreesY)), qTan(qDegreesToRadians(degreesX)), 1.0, 0.0, 0.0);
    return QTransform::fromTranslate(-anchor.x(), -anchor.y()) * shear
           * QTransform::fromTranslate(anchor.x(), anchor.y());
}

QTransform rectToRect(const QRectF& from, const QRectF& to)
{
    const double sx = from.width() != 0.0 ? to.width() / from.width() : 1.0;
    const double sy = from.height() != 0.0 ? to.height() / from.height() : 1.0;
    return QTransform::fromTranslate(-from.left(), -from.top()) * QTransform::fromScale(sx, sy)
           * QTransform::fromTranslate(to.left(), to.top());
}

} // namespace xform

QPainterPath DrawObject::enveloped(const QPainterPath& local) const
{
    if (m_envelopes.isEmpty())
        return local;
    EnvelopeCache& c = m_envelopeCache;
    if (c.envelopes != m_envelopes) {
        c.envelopes = m_envelopes;
        c.paths.clear();
    }
    // Gleiche Pfade teilen sich meist ihre Daten; dann ist der Vergleich billig
    for (const auto& [from, to] : std::as_const(c.paths))
        if (from == local)
            return to;
    QPainterPath result = local;
    for (const Envelope& e : m_envelopes)
        result = e.map(result);
    if (c.paths.size() >= 256)
        c.paths.removeFirst();
    c.paths.append({local, result});
    return result;
}

bool DrawObject::hitTest(const QPointF& point, double tolerance, bool treatAsFilled) const
{
    const QPainterPath p = path();
    if (!p.controlPointRect().adjusted(-tolerance, -tolerance, tolerance, tolerance).contains(point))
        return false;
    if ((treatAsFilled || !m_style.fill.isNone()) && p.contains(point))
        return true;
    QPainterPathStroker stroker;
    stroker.setWidth(std::max(2.0 * tolerance, m_style.stroke ? m_style.strokeWidth : 0.0));
    return stroker.createStroke(p).contains(point);
}

void DrawObject::move(const QPointF& delta)
{
    applyTransform(xform::translation(delta));
}

void DrawObject::scale(double sx, double sy, const QPointF& anchor)
{
    applyTransform(xform::scaling(sx, sy, anchor));
}

void DrawObject::resizeTo(const QRectF& target)
{
    applyTransform(xform::rectToRect(boundingRect(), target));
}

void DrawObject::rotate(double degrees, const QPointF& center)
{
    applyTransform(xform::rotation(degrees, center));
}

void DrawObject::skew(double degreesX, double degreesY, const QPointF& anchor)
{
    applyTransform(xform::skewing(degreesX, degreesY, anchor));
}

double DrawObject::rotationAngle() const
{
    return qRadiansToDegrees(std::atan2(m_transform.m12(), m_transform.m11()));
}

void DrawObject::absorbScale(QSizeF& size)
{
    const QTransform t = m_transform;
    const QPointF ax(t.m11(), t.m12()); // Bild der lokalen x-Achse
    const QPointF ay(t.m21(), t.m22()); // Bild der lokalen y-Achse
    const double lx = std::hypot(ax.x(), ax.y());
    const double ly = std::hypot(ay.x(), ay.y());
    if (lx < 1e-12 || ly < 1e-12)
        return;
    // Geneigt (Achsen nicht rechtwinklig): Skalierung bleibt in der Matrix
    if (std::abs(QPointF::dotProduct(ax, ay)) > 1e-9 * lx * ly)
        return;
    size = QSizeF(size.width() * lx, size.height() * ly);
    // Die Grundform wächst mit: Hüllen in lokalen Koordinaten mitziehen
    for (Envelope& e : m_envelopes)
        e.transform(QTransform::fromScale(lx, ly));
    m_transform = QTransform(ax.x() / lx, ax.y() / lx, ay.x() / ly, ay.y() / ly, t.dx(), t.dy());
}

void DrawObject::writeProperties(QJsonObject& out) const
{
    Node::writeProperties(out);
    const QTransform& t = m_transform;
    out[QStringLiteral("transform")] = QJsonArray{t.m11(), t.m12(), t.m21(), t.m22(), t.dx(), t.dy()};
    out[QStringLiteral("style")] = m_style.toJson();
    // Gleichmässige Unschärfe als Zahl wie bisher, Verläufe als eigenes Objekt
    if (m_blur.isGradient())
        out[QStringLiteral("blurRamp")] = m_blur.toJson();
    else if (m_blur.isActive())
        out[QStringLiteral("blur")] = m_blur.from;
    if (m_transparency.type != Ramp::Type::None)
        out[QStringLiteral("transparency")] = m_transparency.toJson();
    if (m_locked)
        out[QStringLiteral("locked")] = true;
    if (m_lens.isActive())
        out[QStringLiteral("lens")] = m_lens.toJson();
    if (m_bevel.isActive())
        out[QStringLiteral("bevel")] = m_bevel.toJson();
    if (m_shadow.active)
        out[QStringLiteral("shadow")] = m_shadow.toJson();
    if (m_contour.active)
        out[QStringLiteral("contour")] = m_contour.toJson();
    if (m_extrusion.active)
        out[QStringLiteral("extrusion")] = m_extrusion.toJson();
    if (!m_envelopes.isEmpty()) {
        QJsonArray list;
        for (const Envelope& e : m_envelopes)
            list.append(e.toJson());
        out[QStringLiteral("envelopes")] = list;
    }
}

void DrawObject::readProperties(const QJsonObject& in)
{
    Node::readProperties(in);
    const QJsonArray m = in.value(QStringLiteral("transform")).toArray();
    if (m.size() == 6)
        m_transform = QTransform(m[0].toDouble(), m[1].toDouble(), m[2].toDouble(), m[3].toDouble(), m[4].toDouble(),
                                 m[5].toDouble());
    else
        m_transform = QTransform();
    m_style = Style::fromJson(in.value(QStringLiteral("style")).toObject());
    if (in.contains(QStringLiteral("blurRamp")))
        m_blur = Ramp::fromJson(in.value(QStringLiteral("blurRamp")));
    else
        setBlur(in.value(QStringLiteral("blur")).toDouble(0.0));
    m_transparency = in.contains(QStringLiteral("transparency")) ? Ramp::fromJson(in.value(QStringLiteral("transparency")))
                                                                  : Ramp();
    m_locked = in.value(QStringLiteral("locked")).toBool(false);
    m_lens = in.contains(QStringLiteral("lens")) ? Lens::fromJson(in.value(QStringLiteral("lens")).toObject()) : Lens();
    m_bevel = in.contains(QStringLiteral("bevel")) ? Bevel::fromJson(in.value(QStringLiteral("bevel")).toObject()) : Bevel();
    m_shadow = in.contains(QStringLiteral("shadow")) ? DropShadow::fromJson(in.value(QStringLiteral("shadow")).toObject()) : DropShadow();
    m_contour = in.contains(QStringLiteral("contour")) ? ContourEffect::fromJson(in.value(QStringLiteral("contour")).toObject()) : ContourEffect();
    m_extrusion = in.contains(QStringLiteral("extrusion")) ? Extrusion::fromJson(in.value(QStringLiteral("extrusion")).toObject()) : Extrusion();
    m_envelopes.clear();
    for (const QJsonValue& v : in.value(QStringLiteral("envelopes")).toArray())
        if (const Envelope e = Envelope::fromJson(v.toObject()); e.isActive())
            m_envelopes.append(e);
}

} // namespace vek
