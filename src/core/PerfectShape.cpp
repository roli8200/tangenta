#include "PerfectShape.h"

#include <QJsonObject>
#include <QObject>
#include <QLineF>
#include <QPolygonF>
#include <QTransform>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace vek {

namespace perfect {

namespace {

using C = Category;

const QList<Info>& catalog()
{
    static const QList<Info> list{
        // Grundformen
        {C::Basic, QObject::tr("Parallelogramm"), {0.25, 0.0}, true},
        {C::Basic, QObject::tr("Trapez"), {0.25, 0.0}, true},
        {C::Basic, QObject::tr("Dreieck"), {0.5, 0.0}, true},
        {C::Basic, QObject::tr("Rechtwinkliges Dreieck"), {0, 0}, false},
        {C::Basic, QObject::tr("Raute"), {0, 0}, false},
        {C::Basic, QObject::tr("Sechseck"), {0.25, 0.0}, true},
        {C::Basic, QObject::tr("Achteck"), {0.29, 0.0}, true},
        {C::Basic, QObject::tr("Kreuz"), {0.33, 0.33}, true},
        {C::Basic, QObject::tr("Ring"), {0.75, 0.5}, true},
        {C::Basic, QObject::tr("Zylinder"), {0.5, 0.15}, true},
        {C::Basic, QObject::tr("Würfel"), {0.25, 0.25}, true},
        {C::Basic, QObject::tr("Herz"), {0, 0}, false},
        {C::Basic, QObject::tr("Blitz"), {0, 0}, false},
        {C::Basic, QObject::tr("Rahmen"), {0.15, 0.15}, true},
        // Pfeile: Griff = Beginn der Spitze (x) und Oberkante des Schafts (y)
        {C::Arrow, QObject::tr("Pfeil nach rechts"), {0.6, 0.3}, true},
        {C::Arrow, QObject::tr("Pfeil nach links"), {0.4, 0.3}, true},
        {C::Arrow, QObject::tr("Pfeil nach oben"), {0.3, 0.4}, true},
        {C::Arrow, QObject::tr("Pfeil nach unten"), {0.3, 0.6}, true},
        {C::Arrow, QObject::tr("Doppelpfeil waagrecht"), {0.25, 0.3}, true},
        {C::Arrow, QObject::tr("Doppelpfeil senkrecht"), {0.3, 0.25}, true},
        {C::Arrow, QObject::tr("Vierfachpfeil"), {0.2, 0.4}, true},
        // Flussdiagramm
        {C::Flowchart, QObject::tr("Prozess"), {0, 0}, false},
        {C::Flowchart, QObject::tr("Alternativer Prozess"), {0, 0}, false},
        {C::Flowchart, QObject::tr("Entscheidung"), {0, 0}, false},
        {C::Flowchart, QObject::tr("Daten"), {0, 0}, false},
        {C::Flowchart, QObject::tr("Vordefinierter Prozess"), {0, 0}, false},
        {C::Flowchart, QObject::tr("Dokument"), {0, 0}, false},
        {C::Flowchart, QObject::tr("Grenzstelle"), {0, 0}, false},
        {C::Flowchart, QObject::tr("Vorbereitung"), {0, 0}, false},
        {C::Flowchart, QObject::tr("Manuelle Eingabe"), {0, 0}, false},
        {C::Flowchart, QObject::tr("Verbindungsstelle"), {0, 0}, false},
        {C::Flowchart, QObject::tr("Datenbank"), {0, 0}, false},
        {C::Flowchart, QObject::tr("Seitenverbinder"), {0, 0}, false},
        // Banner und Sterne
        {C::Banner, QObject::tr("Band"), {0.2, 0.25}, true},
        {C::Banner, QObject::tr("Wellenband"), {0.5, 0.15}, true},
        {C::Banner, QObject::tr("Stern (5 Zacken)"), {0.69, 0.5}, true},
        {C::Banner, QObject::tr("Stern (8 Zacken)"), {0.78, 0.5}, true},
        {C::Banner, QObject::tr("Explosion"), {0.82, 0.5}, true},
        {C::Banner, QObject::tr("Siegel"), {0.93, 0.5}, true},
        // Legenden: Griff = Spitze des Zeigers (auch ausserhalb)
        {C::Callout, QObject::tr("Rechteckige Legende"), {0.2, 1.4}, true},
        {C::Callout, QObject::tr("Abgerundete Legende"), {0.2, 1.4}, true},
        {C::Callout, QObject::tr("Ovale Legende"), {0.2, 1.4}, true},
        {C::Callout, QObject::tr("Gedankenblase"), {0.15, 1.45}, true},
    };
    return list;
}

QPainterPath polygon(const QList<QPointF>& pts)
{
    QPainterPath p;
    p.addPolygon(QPolygonF(pts));
    p.closeSubpath();
    return p;
}

QPainterPath star(int points, double inner, double jitter = 0.0)
{
    QList<QPointF> pts;
    for (int i = 0; i < 2 * points; ++i) {
        const double a = -std::numbers::pi / 2 + i * std::numbers::pi / points;
        double r = i % 2 ? inner : 1.0;
        if (jitter > 0 && i % 2 == 0)
            r *= 1.0 - jitter * ((i * 7) % 5) / 5.0;
        pts << QPointF(0.5 + 0.5 * r * std::cos(a), 0.5 + 0.5 * r * std::sin(a));
    }
    return polygon(pts);
}

QPainterPath rightArrow(double a, double b)
{
    a = std::clamp(a, 0.05, 0.95);
    b = std::clamp(b, 0.0, 0.49);
    return polygon({{0, b}, {a, b}, {a, 0}, {1, 0.5}, {a, 1}, {a, 1 - b}, {0, 1 - b}});
}

QPainterPath doubleArrow(double a, double b)
{
    a = std::clamp(a, 0.05, 0.49);
    b = std::clamp(b, 0.0, 0.49);
    return polygon({{0, 0.5}, {a, 0}, {a, b}, {1 - a, b}, {1 - a, 0}, {1, 0.5}, {1 - a, 1}, {1 - a, 1 - b}, {a, 1 - b}, {a, 1}});
}

// Legenden in 100-facher Grösse vereinigen (Qt rechnet Kurven in Einheiten fein genug), dann zurück
QPainterPath unitedUnit(const QPainterPath& a, const QPainterPath& b)
{
    const QTransform up = QTransform::fromScale(100, 100);
    return QTransform::fromScale(0.01, 0.01).map(up.map(a).united(up.map(b)).simplified());
}

QPainterPath pointer(const QPointF& tip, double baseY, double from, double to)
{
    return polygon({{from, baseY}, tip, {to, baseY}});
}

} // namespace

int count()
{
    return int(catalog().size());
}

const Info& info(int kind)
{
    return catalog().at(std::clamp(kind, 0, count() - 1));
}

QList<int> kinds(Category category)
{
    QList<int> out;
    for (int i = 0; i < count(); ++i)
        if (catalog()[i].category == category)
            out << i;
    return out;
}

QString categoryName(Category category)
{
    switch (category) {
    case C::Basic: return QObject::tr("Grundformen");
    case C::Arrow: return QObject::tr("Pfeilformen");
    case C::Flowchart: return QObject::tr("Flussdiagrammformen");
    case C::Banner: return QObject::tr("Banner- und Sternformen");
    case C::Callout: return QObject::tr("Legendenformen");
    }
    return {};
}

QPainterPath unitPath(int kind, const QPointF& g)
{
    const double gx = std::clamp(g.x(), 0.0, 1.0), gy = std::clamp(g.y(), 0.0, 1.0);
    QPainterPath p;
    switch (kind) {
    case 0: return polygon({{gx, 0}, {1, 0}, {1 - gx, 1}, {0, 1}});
    case 1: {
        const double t = std::min(gx, 0.49);
        return polygon({{t, 0}, {1 - t, 0}, {1, 1}, {0, 1}});
    }
    case 2: return polygon({{gx, 0}, {1, 1}, {0, 1}});
    case 3: return polygon({{0, 0}, {1, 1}, {0, 1}});
    case 4: return polygon({{0.5, 0}, {1, 0.5}, {0.5, 1}, {0, 0.5}});
    case 5: {
        const double t = std::min(gx, 0.49);
        return polygon({{t, 0}, {1 - t, 0}, {1, 0.5}, {1 - t, 1}, {t, 1}, {0, 0.5}});
    }
    case 6: {
        const double c = std::min(gx, 0.49);
        return polygon({{c, 0}, {1 - c, 0}, {1, c}, {1, 1 - c}, {1 - c, 1}, {c, 1}, {0, 1 - c}, {0, c}});
    }
    case 7: {
        const double t = std::clamp(gx, 0.01, 0.49);
        return polygon({{t, 0}, {1 - t, 0}, {1 - t, t}, {1, t}, {1, 1 - t}, {1 - t, 1 - t}, {1 - t, 1}, {t, 1}, {t, 1 - t}, {0, 1 - t}, {0, t}, {t, t}});
    }
    case 8: {
        const double r = std::clamp(std::abs(gx - 0.5) * 2.0, 0.05, 0.95);
        p.addEllipse(QRectF(0, 0, 1, 1));
        p.addEllipse(QRectF(0.5 - r / 2, 0.5 - r / 2, r, r));
        p.setFillRule(Qt::OddEvenFill);
        return p;
    }
    case 9: {
        const double e = std::clamp(gy, 0.02, 0.45);
        p.moveTo(0, e);
        p.lineTo(0, 1 - e);
        p.arcTo(QRectF(0, 1 - 2 * e, 1, 2 * e), 180, 180);
        p.lineTo(1, e);
        p.arcTo(QRectF(0, 0, 1, 2 * e), 0, 180);
        p.closeSubpath();
        p.addEllipse(QRectF(0, 0, 1, 2 * e));
        return p;
    }
    case 10: {
        const double d = std::clamp(gx, 0.05, 0.5);
        p.addPath(polygon({{0, d}, {1 - d, d}, {1 - d, 1}, {0, 1}}));
        p.addPath(polygon({{0, d}, {d, 0}, {1, 0}, {1 - d, d}}));
        p.addPath(polygon({{1 - d, d}, {1, 0}, {1, 1 - d}, {1 - d, 1}}));
        return p;
    }
    case 11:
        p.moveTo(0.5, 0.25);
        p.cubicTo(0.35, -0.05, 0, 0.0, 0.0, 0.3);
        p.cubicTo(0.0, 0.6, 0.35, 0.75, 0.5, 1.0);
        p.cubicTo(0.65, 0.75, 1.0, 0.6, 1.0, 0.3);
        p.cubicTo(1.0, 0.0, 0.65, -0.05, 0.5, 0.25);
        p.closeSubpath();
        return p;
    case 12: return polygon({{0.55, 0}, {0.2, 0.55}, {0.45, 0.55}, {0.3, 1}, {0.85, 0.4}, {0.58, 0.4}, {0.78, 0}});
    case 13: {
        const double t = std::clamp(gx, 0.01, 0.49);
        p.addRect(QRectF(0, 0, 1, 1));
        p.addRect(QRectF(t, t, 1 - 2 * t, 1 - 2 * t));
        p.setFillRule(Qt::OddEvenFill);
        return p;
    }
    // Pfeile
    case 14: return rightArrow(gx, gy);
    case 15: return QTransform(-1, 0, 0, 1, 1, 0).map(rightArrow(1 - gx, gy));
    case 16: return QTransform(0, -1, 1, 0, 0, 1).map(rightArrow(1 - gy, gx)); // (x,y) -> (y, 1-x)
    case 17: return QTransform(0, 1, 1, 0, 0, 0).map(rightArrow(gy, gx));      // (x,y) -> (y, x)
    case 18: return doubleArrow(gx, gy);
    case 19: return QTransform(0, 1, 1, 0, 0, 0).map(doubleArrow(gy, gx));
    case 20: {
        const double h = std::clamp(gx, 0.05, 0.4), b = std::clamp(gy, 0.26, 0.48);
        return polygon({{0.5, 0},  {0.75, h},  {1 - b, h},  {1 - b, b}, {1 - h, b},     {1 - h, 0.25}, {1, 0.5},     {1 - h, 0.75},
                        {1 - h, 1 - b}, {1 - b, 1 - b}, {1 - b, 1 - h}, {0.75, 1 - h}, {0.5, 1}, {0.25, 1 - h}, {b, 1 - h}, {b, 1 - b},
                        {h, 1 - b}, {h, 0.75}, {0, 0.5}, {h, 0.25}, {h, b}, {b, b}, {b, h}, {0.25, h}});
    }
    // Flussdiagramm
    case 21: p.addRect(QRectF(0, 0, 1, 1)); return p;
    case 22: p.addRoundedRect(QRectF(0, 0, 1, 1), 0.15, 0.15); return p;
    case 23: return polygon({{0.5, 0}, {1, 0.5}, {0.5, 1}, {0, 0.5}});
    case 24: return polygon({{0.2, 0}, {1, 0}, {0.8, 1}, {0, 1}});
    case 25:
        p.addRect(QRectF(0, 0, 1, 1));
        p.moveTo(0.1, 0);
        p.lineTo(0.1, 1);
        p.moveTo(0.9, 0);
        p.lineTo(0.9, 1);
        return p;
    case 26:
        p.moveTo(0, 0);
        p.lineTo(1, 0);
        p.lineTo(1, 0.85);
        p.cubicTo(0.75, 0.7, 0.5, 1.05, 0.25, 0.95);
        p.cubicTo(0.12, 0.9, 0.05, 0.85, 0, 0.85);
        p.closeSubpath();
        return p;
    case 27: p.addRoundedRect(QRectF(0, 0, 1, 1), 0.2, 0.5); return p;
    case 28: return polygon({{0.2, 0}, {0.8, 0}, {1, 0.5}, {0.8, 1}, {0.2, 1}, {0, 0.5}});
    case 29: return polygon({{0, 0.25}, {1, 0}, {1, 1}, {0, 1}});
    case 30: p.addEllipse(QRectF(0, 0, 1, 1)); return p;
    case 31: return unitPath(9, QPointF(0.5, 0.15));
    case 32: return polygon({{0, 0}, {1, 0}, {1, 0.7}, {0.5, 1}, {0, 0.7}});
    // Banner und Sterne
    case 33: {
        const double a = std::clamp(gx, 0.05, 0.4), b = std::clamp(gy, 0.05, 0.5);
        p.addPath(polygon({{0, b}, {a + 0.1, b}, {a + 0.1, 1}, {0, 1}, {a * 0.5, (1 + b) / 2}}));
        p.addPath(polygon({{1, b}, {1 - a - 0.1, b}, {1 - a - 0.1, 1}, {1, 1}, {1 - a * 0.5, (1 + b) / 2}}));
        p.addPath(polygon({{a, 0}, {1 - a, 0}, {1 - a, 1 - b}, {a, 1 - b}}));
        return p;
    }
    case 34: {
        const double a = std::clamp(gy, 0.0, 0.3);
        p.moveTo(0, a);
        p.cubicTo(0.33, -a, 0.66, 3 * a, 1, a);
        p.lineTo(1, 1 - a);
        p.cubicTo(0.66, 1 + a, 0.33, 1 - 3 * a, 0, 1 - a);
        p.closeSubpath();
        return p;
    }
    case 35: return star(5, std::clamp(std::abs(gx - 0.5) * 2, 0.05, 0.99));
    case 36: return star(8, std::clamp(std::abs(gx - 0.5) * 2, 0.05, 0.99));
    case 37: return star(12, std::clamp(std::abs(gx - 0.5) * 2, 0.05, 0.99), 0.25);
    case 38: return star(24, std::clamp(std::abs(gx - 0.5) * 2, 0.05, 0.99));
    // Legenden: Zeiger zur Griffstelle
    case 39:
    case 40: {
        const QPointF tip(std::clamp(g.x(), -2.0, 3.0), std::clamp(g.y(), -2.0, 3.0));
        QPainterPath body;
        if (kind == 39)
            body.addRect(QRectF(0, 0, 1, 1));
        else
            body.addRoundedRect(QRectF(0, 0, 1, 1), 0.15, 0.2);
        // Zeiger an der Kante, die der Spitze am nächsten liegt
        if (tip.y() > 1.0)
            return unitedUnit(body, pointer(tip, 0.95, std::clamp(tip.x(), 0.1, 0.75), std::clamp(tip.x(), 0.1, 0.75) + 0.15));
        if (tip.y() < 0.0)
            return unitedUnit(body, pointer(tip, 0.05, std::clamp(tip.x(), 0.1, 0.75), std::clamp(tip.x(), 0.1, 0.75) + 0.15));
        const double x = tip.x() > 0.5 ? 0.95 : 0.05;
        const double y = std::clamp(tip.y(), 0.1, 0.75);
        return unitedUnit(body, polygon({{x, y}, tip, {x, y + 0.15}}));
    }
    case 41:
    case 42: {
        const QPointF tip(std::clamp(g.x(), -2.0, 3.0), std::clamp(g.y(), -2.0, 3.0));
        QPainterPath body;
        if (kind == 41) {
            body.addEllipse(QRectF(0, 0, 1, 1));
        } else {
            // Wolke aus Kreisen
            for (const QRectF& r : {QRectF(0.0, 0.25, 0.4, 0.5), QRectF(0.2, 0.0, 0.4, 0.5), QRectF(0.45, 0.05, 0.4, 0.45),
                                    QRectF(0.6, 0.3, 0.4, 0.45), QRectF(0.3, 0.45, 0.45, 0.55), QRectF(0.1, 0.5, 0.35, 0.45)}) {
                QPainterPath c;
                c.addEllipse(r);
                body = body.isEmpty() ? c : unitedUnit(body, c);
            }
        }
        const QPointF center(0.5, 0.5);
        QPointF dir = tip - center;
        const double len = std::hypot(dir.x(), dir.y());
        if (len < 1e-6)
            return body;
        dir /= len;
        if (kind == 41) {
            const QPointF perp(-dir.y(), dir.x());
            const QPointF base = center + dir * 0.4;
            return unitedUnit(body, polygon({base + perp * 0.1, tip, base - perp * 0.1}));
        }
        // Gedankenblase: zwei kleine Kreise zur Spitze
        QPainterPath dots;
        const QPointF edge = center + dir * 0.5;
        for (const double t : {0.35, 0.8}) {
            const QPointF c = edge + (tip - edge) * t;
            const double r = t < 0.5 ? 0.07 : 0.04;
            dots.addEllipse(c, r, r);
        }
        body.addPath(dots);
        return body;
    }
    default: break;
    }
    p.addRect(QRectF(0, 0, 1, 1));
    return p;
}

} // namespace perfect

PerfectShapeObject::PerfectShapeObject(int kind, const QSizeF& size)
    : m_kind(std::clamp(kind, 0, perfect::count() - 1)), m_size(size), m_glyph(perfect::info(m_kind).glyph)
{
}

QPainterPath PerfectShapeObject::localPath() const
{
    const QPainterPath unit = perfect::unitPath(m_kind, m_glyph);
    QPainterPath p = QTransform::fromScale(m_size.width(), m_size.height()).map(unit);
    p.setFillRule(unit.fillRule());
    return p;
}

void PerfectShapeObject::applyTransform(const QTransform& m)
{
    DrawObject::applyTransform(m);
    absorbScale(m_size);
}

void PerfectShapeObject::setKind(int kind)
{
    kind = std::clamp(kind, 0, perfect::count() - 1);
    if (kind != m_kind) {
        m_kind = kind;
        m_glyph = perfect::info(kind).glyph;
    }
}

void PerfectShapeObject::setGlyph(const QPointF& unit)
{
    const bool callout = perfect::info(m_kind).category == perfect::Category::Callout;
    const double lo = callout ? -2.0 : 0.0, hi = callout ? 3.0 : 1.0;
    m_glyph = QPointF(std::clamp(unit.x(), lo, hi), std::clamp(unit.y(), lo, hi));
}

QList<PropertyDescriptor> PerfectShapeObject::propertyDescriptors() const
{
    PropertyDescriptor shape;
    shape.id = QStringLiteral("shape");
    shape.label = perfect::categoryName(perfect::info(m_kind).category);
    shape.kind = PropertyDescriptor::Kind::List;
    for (int k : perfect::kinds(perfect::info(m_kind).category))
        shape.choiceLabels << perfect::info(k).name;
    shape.separatorBefore = true;
    return {shape};
}

QVariant PerfectShapeObject::propertyValue(const QString& id) const
{
    if (id == QLatin1String("shape"))
        return int(perfect::kinds(perfect::info(m_kind).category).indexOf(m_kind));
    return {};
}

bool PerfectShapeObject::setPropertyValue(const QString& id, const QVariant& value)
{
    if (id != QLatin1String("shape"))
        return false;
    const QList<int> list = perfect::kinds(perfect::info(m_kind).category);
    const int i = value.toInt();
    if (i >= 0 && i < list.size())
        setKind(list[i]);
    return true;
}

void PerfectShapeObject::writeProperties(QJsonObject& out) const
{
    DrawObject::writeProperties(out);
    out[QStringLiteral("kind")] = m_kind;
    out[QStringLiteral("width")] = m_size.width();
    out[QStringLiteral("height")] = m_size.height();
    out[QStringLiteral("gx")] = m_glyph.x();
    out[QStringLiteral("gy")] = m_glyph.y();
}

void PerfectShapeObject::readProperties(const QJsonObject& in)
{
    DrawObject::readProperties(in);
    m_kind = std::clamp(in.value(QStringLiteral("kind")).toInt(), 0, perfect::count() - 1);
    m_size = QSizeF(in.value(QStringLiteral("width")).toDouble(10), in.value(QStringLiteral("height")).toDouble(10));
    m_glyph = QPointF(in.value(QStringLiteral("gx")).toDouble(perfect::info(m_kind).glyph.x()),
                      in.value(QStringLiteral("gy")).toDouble(perfect::info(m_kind).glyph.y()));
}

} // namespace vek
