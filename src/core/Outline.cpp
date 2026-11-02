#include "Outline.h"

#include <QPainterPathStroker>
#include <QTransform>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace vek::outline {

namespace {

struct Arrow {
    const char* name;
    double retract; // so weit wird die Linie unter die Spitze zurückgenommen (Einheiten)
};

const Arrow kArrows[] = {
    {QT_TRANSLATE_NOOP("outline", "Keine"), 0.0},
    {QT_TRANSLATE_NOOP("outline", "Pfeil offen"), 0.6},
    {QT_TRANSLATE_NOOP("outline", "Dreieck"), 4.5},
    {QT_TRANSLATE_NOOP("outline", "Schlankes Dreieck"), 6.5},
    {QT_TRANSLATE_NOOP("outline", "Widerhaken"), 3.5},
    {QT_TRANSLATE_NOOP("outline", "Kreis"), 2.0},
    {QT_TRANSLATE_NOOP("outline", "Ring"), 4.0},
    {QT_TRANSLATE_NOOP("outline", "Quadrat"), 2.0},
    {QT_TRANSLATE_NOOP("outline", "Raute"), 2.5},
    {QT_TRANSLATE_NOOP("outline", "Querstrich"), 0.0},
    {QT_TRANSLATE_NOOP("outline", "Doppelpfeil"), 7.5},
    {QT_TRANSLATE_NOOP("outline", "Halbpfeil"), 0.5},
    {QT_TRANSLATE_NOOP("outline", "Punkt"), 0.0},
};

double length(const QPointF& v)
{
    return std::hypot(v.x(), v.y());
}

QPainterPath polygon(std::initializer_list<QPointF> points)
{
    QPainterPath p;
    p.addPolygon(QPolygonF(points));
    p.closeSubpath();
    return p;
}

} // namespace

int dashCount()
{
    return 9;
}

QList<double> dashPattern(int index)
{
    switch (index) {
    case 1: return {4, 2};
    case 2: return {1, 2};
    case 3: return {8, 3};
    case 4: return {6, 2, 1, 2};
    case 5: return {6, 2, 1, 2, 1, 2};
    case 6: return {2, 2};
    case 7: return {12, 4};
    case 8: return {1, 4};
    default: return {};
    }
}

int arrowCount()
{
    return int(std::size(kArrows));
}

QString arrowName(int index)
{
    return QString::fromUtf8(kArrows[std::clamp(index, 0, arrowCount() - 1)].name);
}

QPainterPath arrowShape(int index)
{
    switch (index) {
    case 1: {
        QPainterPath v;
        v.moveTo(-4, -2.2);
        v.lineTo(0, 0);
        v.lineTo(-4, 2.2);
        QPainterPathStroker stroker;
        stroker.setWidth(1.0);
        stroker.setJoinStyle(Qt::MiterJoin);
        stroker.setCapStyle(Qt::FlatCap);
        return stroker.createStroke(v).simplified();
    }
    case 2: return polygon({{0, 0}, {-5, -2.5}, {-5, 2.5}});
    case 3: return polygon({{0, 0}, {-7, -1.8}, {-7, 1.8}});
    case 4: return polygon({{0, 0}, {-5.5, -2.8}, {-4, 0}, {-5.5, 2.8}});
    case 5: {
        QPainterPath p;
        p.addEllipse(QPointF(-2, 0), 2, 2);
        return p;
    }
    case 6: {
        QPainterPath p;
        p.addEllipse(QPointF(-2, 0), 2, 2);
        p.addEllipse(QPointF(-2, 0), 1.2, 1.2);
        return p;
    }
    case 7: {
        QPainterPath p;
        p.addRect(QRectF(-4, -2, 4, 4));
        return p;
    }
    case 8: return polygon({{0, 0}, {-2.5, -2.5}, {-5, 0}, {-2.5, 2.5}});
    case 9: {
        QPainterPath p;
        p.addRect(QRectF(-0.6, -3, 1.2, 6));
        return p;
    }
    case 10: {
        QPainterPath p = polygon({{0, 0}, {-4, -2.2}, {-4, 2.2}});
        p.addPath(polygon({{-4.5, 0}, {-8.5, -2.2}, {-8.5, 2.2}}));
        return p;
    }
    case 11: return polygon({{0, 0}, {-5, -2.8}, {-5, 0.5}, {0, 0.5}});
    case 12: {
        QPainterPath p;
        p.addEllipse(QPointF(0, 0), 1.3, 1.3);
        return p;
    }
    default: return {};
    }
}

double arrowUnit(const Style& style)
{
    return std::max(style.strokeWidth, 0.25);
}

Stroke withArrows(const QPainterPath& path, const Style& style)
{
    Stroke result{path, {}};
    if (style.startArrow <= 0 && style.endArrow <= 0)
        return result;
    const double unit = arrowUnit(style);
    QPainterPath& line = result.line;

    // Teilpfade als Bereiche [first, last] der Pfadelemente
    QList<std::pair<int, int>> ranges;
    for (int i = 0; i < line.elementCount(); ++i) {
        if (line.elementAt(i).type == QPainterPath::MoveToElement)
            ranges.append({i, i});
        else if (!ranges.isEmpty())
            ranges.last().second = i;
    }

    auto at = [&line](int i) { return QPointF(line.elementAt(i).x, line.elementAt(i).y); };
    auto place = [&](int index, const QPointF& tip, const QPointF& direction) {
        QTransform t;
        t.translate(tip.x(), tip.y());
        t.rotateRadians(std::atan2(direction.y(), direction.x()));
        t.scale(unit, unit);
        result.heads.addPath(t.map(arrowShape(index)));
    };
    // Ende el (mit Nachbar neighbor) unter die Spitze zurücknehmen; ein Kontrollpunkt einer
    // Kurve am selben Ende wandert mit
    auto retract = [&](int el, int control, const QPointF& direction, double amount, double available) {
        const QPointF shift = -direction * std::min(amount, available * 0.9);
        line.setElementPositionAt(el, at(el).x() + shift.x(), at(el).y() + shift.y());
        if (control >= 0)
            line.setElementPositionAt(control, at(control).x() + shift.x(), at(control).y() + shift.y());
    };

    for (const auto& [first, last] : ranges) {
        if (last <= first || length(at(first) - at(last)) < 1e-9)
            continue; // einzelner Punkt oder geschlossen
        if (style.endArrow > 0) {
            int prev = last - 1;
            while (prev > first && length(at(last) - at(prev)) < 1e-9)
                --prev;
            const QPointF tip = at(last);
            const QPointF dir = (tip - at(prev)) / std::max(length(tip - at(prev)), 1e-12);
            place(style.endArrow, tip, dir);
            const bool curve = line.elementAt(last).type == QPainterPath::CurveToDataElement;
            retract(last, curve ? last - 1 : -1, dir, kArrows[style.endArrow].retract * unit,
                    length(tip - at(prev)));
        }
        if (style.startArrow > 0) {
            int next = first + 1;
            while (next < last && length(at(next) - at(first)) < 1e-9)
                ++next;
            const QPointF tip = at(first);
            const QPointF dir = (tip - at(next)) / std::max(length(tip - at(next)), 1e-12);
            place(style.startArrow, tip, dir);
            const bool curve = line.elementAt(first + 1).type == QPainterPath::CurveToElement;
            retract(first, curve ? first + 1 : -1, dir, kArrows[style.startArrow].retract * unit,
                    length(tip - at(next)));
        }
    }
    return result;
}

} // namespace vek::outline
