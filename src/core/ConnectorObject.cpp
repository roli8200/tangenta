#include "ConnectorObject.h"

#include <QJsonArray>
#include <QObject>

#include <cmath>

namespace vek {

namespace {

QJsonArray point(const QPointF& p)
{
    return QJsonArray{p.x(), p.y()};
}

QPointF point(const QJsonValue& v, const QPointF& fallback = {})
{
    const QJsonArray a = v.toArray();
    return a.size() == 2 ? QPointF(a[0].toDouble(), a[1].toDouble()) : fallback;
}

QJsonObject endToJson(const ConnectorObject::End& e)
{
    QJsonObject o{{QStringLiteral("point"), point(e.point)}};
    if (e.target) {
        o[QStringLiteral("target")] = QString::number(e.target);
        o[QStringLiteral("anchor")] = point(e.anchor);
    }
    return o;
}

ConnectorObject::End endFromJson(const QJsonObject& o)
{
    ConnectorObject::End e;
    e.point = point(o.value(QStringLiteral("point")));
    e.target = o.value(QStringLiteral("target")).toString().toULongLong();
    e.anchor = point(o.value(QStringLiteral("anchor")), QPointF(0.5, 0.5));
    return e;
}

// Austrittsrichtung aus der Grundform: zur nächsten Seite des Rechtecks (Mitte = keine)
QPointF exitDirection(const QPointF& anchor)
{
    const double dl = anchor.x(), dr = 1.0 - anchor.x(), dt = anchor.y(), db = 1.0 - anchor.y();
    const double m = std::min({dl, dr, dt, db});
    if (m > 0.3)
        return {};
    if (m == dl)
        return {-1, 0};
    if (m == dr)
        return {1, 0};
    if (m == dt)
        return {0, -1};
    return {0, 1};
}

} // namespace

ConnectorObject::ConnectorObject(Kind kind, const End& a, const End& b) : m_kind(kind), m_a(a), m_b(b)
{
    Style s;
    s.fill = Fill::none();
    s.stroke = Color(0, 0, 0);
    s.strokeWidth = 0.2;
    setStyle(s);
}

void ConnectorObject::setEnds(const End& a, const End& b)
{
    m_a = a;
    m_b = b;
    m_resolved = false;
}

QPointF ConnectorObject::anchorPoint(const DrawObject& target, const QPointF& anchor)
{
    const QRectF b = target.localPath().boundingRect();
    return target.transform().map(QPointF(b.left() + anchor.x() * b.width(), b.top() + anchor.y() * b.height()));
}

QPointF ConnectorObject::anchorFor(const DrawObject& target, const QPointF& page)
{
    const QRectF b = target.localPath().boundingRect();
    const QPointF local = target.transform().inverted().map(page);
    return QPointF(b.width() > 0 ? std::clamp((local.x() - b.left()) / b.width(), 0.0, 1.0) : 0.5,
                   b.height() > 0 ? std::clamp((local.y() - b.top()) / b.height(), 0.0, 1.0) : 0.5);
}

void ConnectorObject::resolve(const std::function<const DrawObject*(NodeId)>& lookup) const
{
    auto one = [&](const End& e, QPointF& at, QPointF& dir) {
        const DrawObject* t = e.target ? lookup(e.target) : nullptr;
        if (!t) {
            at = e.point;
            dir = QPointF();
            return;
        }
        at = anchorPoint(*t, e.anchor);
        // Richtung auf der Seite: lokale Austrittsrichtung durch die Objektmatrix
        const QPointF d = exitDirection(e.anchor);
        if (d.isNull()) {
            dir = QPointF();
        } else {
            const QPointF mapped = t->transform().map(QPointF(0, 0) + d) - t->transform().map(QPointF(0, 0));
            const double len = std::hypot(mapped.x(), mapped.y());
            dir = len > 0 ? mapped / len : QPointF();
        }
    };
    one(m_a, m_resolvedA, m_dirA);
    one(m_b, m_resolvedB, m_dirB);
    m_resolved = true;
}

QPainterPath ConnectorObject::localPath() const
{
    const QPointF a = m_resolved ? m_resolvedA : m_a.point;
    const QPointF b = m_resolved ? m_resolvedB : m_b.point;
    QPainterPath path(a);
    if (m_kind == Kind::Straight) {
        path.lineTo(b);
        return path;
    }
    // Rechtwinklig: erst ein Stück senkrecht aus dem Objekt heraus, dann mit einem Knick hinüber
    QPointF da = m_resolved ? m_dirA : QPointF();
    QPointF db = m_resolved ? m_dirB : QPointF();
    if (da.isNull())
        da = std::abs(b.x() - a.x()) >= std::abs(b.y() - a.y()) ? QPointF(b.x() > a.x() ? 1 : -1, 0) : QPointF(0, b.y() > a.y() ? 1 : -1);
    if (db.isNull())
        db = std::abs(da.x()) > 0 ? QPointF(-da.x(), 0) : QPointF(0, -da.y());
    const QPointF a1 = a + da * m_clearance;
    const QPointF b1 = b + db * m_clearance;
    QPolygonF pts{a, a1};
    if (std::abs(da.x()) > 0) { // waagrecht hinaus
        if (std::abs(db.x()) > 0) {
            const double mx = (a1.x() + b1.x()) / 2.0;
            pts << QPointF(mx, a1.y()) << QPointF(mx, b1.y());
        } else {
            pts << QPointF(b1.x(), a1.y());
        }
    } else { // senkrecht hinaus
        if (std::abs(db.y()) > 0) {
            const double my = (a1.y() + b1.y()) / 2.0;
            pts << QPointF(a1.x(), my) << QPointF(b1.x(), my);
        } else {
            pts << QPointF(a1.x(), b1.y());
        }
    }
    pts << b1 << b;
    // Doppelte und gerade fortlaufende Punkte entfernen
    QPolygonF clean{pts.first()};
    for (int i = 1; i < pts.size(); ++i)
        if (QLineF(clean.last(), pts[i]).length() > 1e-6)
            clean << pts[i];
    if (m_kind == Kind::RightAngle || clean.size() < 3) {
        path = QPainterPath(clean.first());
        for (int i = 1; i < clean.size(); ++i)
            path.lineTo(clean[i]);
        return path;
    }
    // Abgerundete Ecken
    const double r = std::min(3.0, m_clearance / 2.0);
    path = QPainterPath(clean.first());
    for (int i = 1; i + 1 < clean.size(); ++i) {
        const QPointF p = clean[i];
        const QPointF in = clean[i - 1] - p;
        const QPointF out = clean[i + 1] - p;
        const double li = std::hypot(in.x(), in.y()), lo = std::hypot(out.x(), out.y());
        const double rr = std::min({r, li / 2.0, lo / 2.0});
        path.lineTo(p + in / li * rr);
        path.quadTo(p, p + out / lo * rr);
    }
    path.lineTo(clean.last());
    return path;
}

void ConnectorObject::applyTransform(const QTransform& m)
{
    // Freie Enden bewegen sich mit; angehängte folgen ihren Objekten
    if (!m_a.target)
        m_a.point = m.map(m_a.point);
    if (!m_b.target)
        m_b.point = m.map(m_b.point);
    if (m_resolved) {
        if (!m_a.target)
            m_resolvedA = m_a.point;
        if (!m_b.target)
            m_resolvedB = m_b.point;
    }
}

QList<PropertyDescriptor> ConnectorObject::propertyDescriptors() const
{
    using K = PropertyDescriptor::Kind;
    PropertyDescriptor kind{QStringLiteral("connectorKind"), QObject::tr("Art der Verbindung"), QString(), K::Choice};
    kind.choiceIcons = {QStringLiteral("tool-connector"), QStringLiteral("tool-connector-angle"),
                        QStringLiteral("tool-connector-round")};
    kind.choiceLabels = {QObject::tr("Gerade"), QObject::tr("Rechtwinklig"), QObject::tr("Rechtwinklig abgerundet")};
    PropertyDescriptor clearance{QStringLiteral("clearance"), QObject::tr("Abstand der Linie zu den Objekten"),
                                 QStringLiteral("connector-clearance"), K::Length};
    clearance.maximum = 1000;
    clearance.separatorBefore = true;
    return {kind, clearance};
}

QVariant ConnectorObject::propertyValue(const QString& id) const
{
    if (id == QLatin1String("connectorKind"))
        return int(m_kind);
    if (id == QLatin1String("clearance"))
        return m_clearance;
    return {};
}

bool ConnectorObject::setPropertyValue(const QString& id, const QVariant& value)
{
    if (id == QLatin1String("connectorKind"))
        m_kind = Kind(std::clamp(value.toInt(), 0, 2));
    else if (id == QLatin1String("clearance"))
        m_clearance = std::max(0.0, value.toDouble());
    else
        return false;
    return true;
}

void ConnectorObject::writeProperties(QJsonObject& out) const
{
    DrawObject::writeProperties(out);
    static const char* kinds[] = {"straight", "rightAngle", "rounded"};
    out[QStringLiteral("kind")] = QLatin1String(kinds[int(m_kind)]);
    // Lage der Enden mitschreiben, damit auch ohne Ziel alles am richtigen Ort bleibt
    End a = m_a;
    End b = m_b;
    if (m_resolved) {
        a.point = m_resolvedA;
        b.point = m_resolvedB;
    }
    out[QStringLiteral("start")] = endToJson(a);
    out[QStringLiteral("end")] = endToJson(b);
    out[QStringLiteral("clearance")] = m_clearance;
}

void ConnectorObject::readProperties(const QJsonObject& in)
{
    DrawObject::readProperties(in);
    const QString kind = in.value(QStringLiteral("kind")).toString();
    m_kind = kind == QLatin1String("rightAngle") ? Kind::RightAngle : kind == QLatin1String("rounded") ? Kind::RoundedRightAngle
                                                                                                         : Kind::Straight;
    m_a = endFromJson(in.value(QStringLiteral("start")).toObject());
    m_b = endFromJson(in.value(QStringLiteral("end")).toObject());
    m_clearance = in.value(QStringLiteral("clearance")).toDouble(8.0);
    m_resolved = false;
}

} // namespace vek
