#include "BlurBake.h"

#include "CurveObject.h"
#include "GroupObject.h"
#include "PathOps.h"

#include <QPainterPathStroker>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace vek::bake {

namespace {


// Fläche um d nach aussen (d > 0) bzw. innen (d < 0) versetzt
QPainterPath offset(const QPainterPath& area, double d)
{
    if (std::abs(d) < 1e-6)
        return area;
    QPainterPathStroker stroker;
    stroker.setWidth(2.0 * std::abs(d));
    stroker.setJoinStyle(Qt::RoundJoin);
    stroker.setCapStyle(Qt::RoundCap);
    const QPainterPath ring = stroker.createStroke(area);
    return d > 0.0 ? pathops::united(area, ring) : pathops::subtracted(area, ring);
}

// Band k eines Verlaufs als Fläche in Seitenkoordinaten
QPainterPath band(const Ramp& r, int k, int kBands, const QTransform& toPage, const QRectF& b)
{
    const QPointF s(b.left() + r.start.x() * b.width(), b.top() + r.start.y() * b.height());
    const QPointF e(b.left() + r.end.x() * b.width(), b.top() + r.end.y() * b.height());
    const QPointF axis = e - s;
    // "Unendlich" für Streifen und Sektoren: weit genug, aber klein genug für genaue Schnitte
    const double kFar = 4.0 * (b.width() + b.height() + std::hypot(axis.x(), axis.y())) + 50.0;
    const double t0 = double(k) / kBands;
    const double t1 = double(k + 1) / kBands;
    QPainterPath local;
    switch (r.type) {
    case Ramp::Type::Linear: {
        // Streifen quer zur Achse; erstes und letztes Band reichen bis ins Unendliche
        const double len = std::hypot(axis.x(), axis.y());
        if (len <= 0.0)
            return QPainterPath();
        const QPointF dir = axis / len;
        const QPointF n(-dir.y(), dir.x());
        const double a = k == 0 ? -kFar : t0 * len;
        const double c = k == kBands - 1 ? kFar : t1 * len;
        local.addPolygon(QPolygonF({s + dir * a + n * kFar, s + dir * c + n * kFar, s + dir * c - n * kFar,
                                    s + dir * a - n * kFar}));
        local.closeSubpath();
        break;
    }
    case Ramp::Type::Radial: {
        const double radius = std::hypot(axis.x(), axis.y());
        const double outer = k == kBands - 1 ? kFar : t1 * radius;
        local.addEllipse(s, outer, outer);
        if (k > 0) {
            QPainterPath inner;
            inner.addEllipse(s, t0 * radius, t0 * radius);
            local = local.subtracted(inner);
        }
        break;
    }
    case Ramp::Type::Conical: {
        const double base = std::atan2(axis.y(), axis.x());
        QPolygonF wedge{s};
        for (int i = 0; i <= 8; ++i) {
            const double a = base + 2.0 * std::numbers::pi * (t0 + (t1 - t0) * i / 8.0);
            wedge << s + QPointF(std::cos(a), std::sin(a)) * kFar;
        }
        local.addPolygon(wedge);
        local.closeSubpath();
        break;
    }
    default:
        return QPainterPath();
    }
    return toPage.map(local);
}

// Stufenkonturen einer Fläche mit Unschärfe sigma (mm), wahlweise auf clip beschnitten
void addContours(GroupObject& group, const Part& part, double sigma, const QPainterPath* clip, int kSteps)
{
    auto add = [&](const QPainterPath& area, double transparency) {
        const QPainterPath cut = clip ? pathops::intersected(area, *clip) : area;
        if (cut.isEmpty())
            return;
        // Angepasste Kurven haben viel weniger Knoten; bei sehr schmalen Stücken schwingt die
        // Anpassung aber über, dann gilt der ungeglättete Umriss
        const double tolerance = std::clamp(sigma / 25.0, 0.005, 0.05);
        auto curve = std::make_unique<CurveObject>(pathops::fitRegion(cut, tolerance));
        const QRectF want = cut.boundingRect().adjusted(-4 * tolerance, -4 * tolerance, 4 * tolerance, 4 * tolerance);
        if (!want.contains(curve->boundingRect()))
            curve = CurveObject::fromPath(cut);
        Style s;
        s.fill = part.fill;
        s.stroke.reset();
        curve->setStyle(s);
        if (transparency > 0.0)
            curve->setEffect(Effect::Transparency, Ramp::uniform(transparency));
        attachDetachedChild(group, std::move(curve));
    };
    if (sigma < 0.01) { // praktisch scharf
        add(part.area, 0.0);
        return;
    }
    // Versatz von +2.5 sigma (aussen, fast durchsichtig) bis -2.5 sigma (innen, deckend). Jede
    // Kontur liegt über den grösseren; ihre Deckkraft ist so gewählt, dass die übereinander
    // liegenden Schichten in jedem Ring die Deckkraft der Gausskante in dessen Mitte ergeben.
    const double reach = 2.5 * sigma;
    const double step = 2.0 * reach / (kSteps - 1);
    double covered = 0.0; // bisherige Gesamtdeckkraft
    for (int k = 0; k < kSteps; ++k) {
        const double d = reach - k * step;
        const double target = k == kSteps - 1 ? 1.0 : edgeAlpha(d - step / 2.0, sigma);
        const double alpha = covered >= 1.0 ? 0.0 : std::clamp(1.0 - (1.0 - target) / (1.0 - covered), 0.0, 1.0);
        covered = target;
        if (alpha <= 0.0)
            continue;
        const QPainterPath area = offset(part.area, d);
        if (area.isEmpty())
            break; // dünner als der Versatz: weiter innen kommt nichts mehr
        add(area, (1.0 - alpha) * 100.0);
    }
}

} // namespace

double edgeAlpha(double distance, double sigma)
{
    return 0.5 * std::erfc(distance / (sigma * std::numbers::sqrt2));
}

std::unique_ptr<GroupObject> blurToContours(const QList<Part>& parts, const Ramp& blur, const QTransform& toPage,
                                            const QRectF& localBounds, int steps, int bands)
{
    steps = std::max(2, steps);
    bands = std::max(1, bands);
    auto group = std::make_unique<GroupObject>();
    for (const Part& part : parts) {
        if (part.area.isEmpty())
            continue;
        if (!blur.isGradient()) {
            addContours(*group, part, blur.maximum(), nullptr, steps);
            continue;
        }
        for (int k = 0; k < bands; ++k) {
            const QPainterPath clip = band(blur, k, bands, toPage, localBounds);
            if (clip.isEmpty())
                continue;
            // Unschärfe in der Mitte des Bandes
            const double sigma = blur.from + (blur.to - blur.from) * (k + 0.5) / bands;
            addContours(*group, part, sigma, &clip, steps);
        }
    }
    if (group->childCount() == 0)
        return nullptr;
    return group;
}

} // namespace vek::bake
