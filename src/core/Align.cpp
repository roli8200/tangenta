#include "Align.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace vek::align {

QPointF offset(const QRectF& r, const QRectF& ref, H h, V v)
{
    QPointF d;
    switch (h) {
    case H::Left: d.setX(ref.left() - r.left()); break;
    case H::Center: d.setX(ref.center().x() - r.center().x()); break;
    case H::Right: d.setX(ref.right() - r.right()); break;
    case H::None: break;
    }
    switch (v) {
    case V::Top: d.setY(ref.top() - r.top()); break;
    case V::Center: d.setY(ref.center().y() - r.center().y()); break;
    case V::Bottom: d.setY(ref.bottom() - r.bottom()); break;
    case V::None: break;
    }
    return d;
}

QPointF toGrid(const QRectF& r, const QSizeF& grid, H h, V v)
{
    auto snap = [](double value, double step) { return step > 0 ? std::round(value / step) * step - value : 0.0; };
    QPointF d;
    switch (h) {
    case H::Left: d.setX(snap(r.left(), grid.width())); break;
    case H::Center: d.setX(snap(r.center().x(), grid.width())); break;
    case H::Right: d.setX(snap(r.right(), grid.width())); break;
    case H::None: break;
    }
    switch (v) {
    case V::Top: d.setY(snap(r.top(), grid.height())); break;
    case V::Center: d.setY(snap(r.center().y(), grid.height())); break;
    case V::Bottom: d.setY(snap(r.bottom(), grid.height())); break;
    case V::None: break;
    }
    return d;
}

namespace {

// Eine Richtung: start/size je Rechteck, Ergebnis = Verschiebung je Rechteck
QList<double> spread(const QList<double>& start, const QList<double>& size, double from, double to, Spread mode)
{
    const int n = int(start.size());
    QList<double> result(n, 0.0);
    if (mode == Spread::None || n < 2)
        return result;
    // Reihenfolge nach Bezugspunkt (bei Abstand nach der Mitte)
    QList<int> order(n);
    std::iota(order.begin(), order.end(), 0);
    auto anchor = [&](int i) {
        switch (mode) {
        case Spread::Start: return start[i];
        case Spread::End: return start[i] + size[i];
        default: return start[i] + size[i] / 2.0;
        }
    };
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return anchor(a) < anchor(b); });
    const int first = order.first(), last = order.last();
    if (mode == Spread::Spacing) {
        double total = 0.0;
        for (double s : size)
            total += s;
        const double gap = (to - from - total) / (n - 1);
        double pos = from;
        for (int i : order) {
            result[i] = pos - start[i];
            pos += size[i] + gap;
        }
        return result;
    }
    // Bezugspunkte des ersten und letzten Objekts an den Enden des Umfangs
    double a0 = 0.0, a1 = 0.0;
    switch (mode) {
    case Spread::Start:
        a0 = from;
        a1 = to - size[last];
        break;
    case Spread::Center:
        a0 = from + size[first] / 2.0;
        a1 = to - size[last] / 2.0;
        break;
    case Spread::End:
        a0 = from + size[first];
        a1 = to;
        break;
    default: break;
    }
    for (int k = 0; k < n; ++k) {
        const int i = order[k];
        const double target = a0 + (a1 - a0) * k / (n - 1);
        result[i] = target - anchor(i);
    }
    return result;
}

} // namespace

QList<QPointF> distribute(const QList<QRectF>& rects, const QRectF& extent, Spread h, Spread v)
{
    QList<double> xs, ws, ys, hs;
    for (const QRectF& r : rects) {
        xs << r.left();
        ws << r.width();
        ys << r.top();
        hs << r.height();
    }
    const QList<double> dx = spread(xs, ws, extent.left(), extent.right(), h);
    const QList<double> dy = spread(ys, hs, extent.top(), extent.bottom(), v);
    QList<QPointF> result;
    for (int i = 0; i < rects.size(); ++i)
        result << QPointF(dx[i], dy[i]);
    return result;
}

} // namespace vek::align
