#include "Effects.h"

#include "BitmapFilters.h"

#include <QJsonArray>
#include <QPainterPathStroker>
#include <QLineF>
#include <QPolygonF>
#include <QTransform>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <vector>

namespace vek {

QJsonObject Lens::toJson() const
{
    QJsonObject o{{QStringLiteral("type"), int(type)}, {QStringLiteral("amount"), amount}, {QStringLiteral("color"), color.toHex()}};
    if (type == Type::CustomColorMap || type == Type::Wireframe)
        o[QStringLiteral("color2")] = color2.toHex();
    if (type == Type::BitmapEffect)
        o[QStringLiteral("effect")] = bitmapEffect;
    if (viewpoint) {
        o[QStringLiteral("vx")] = viewOffset.x();
        o[QStringLiteral("vy")] = viewOffset.y();
    }
    return o;
}

Lens Lens::fromJson(const QJsonObject& json)
{
    Lens l;
    l.type = Type(std::clamp(json.value(QStringLiteral("type")).toInt(), 0, int(Type::BitmapEffect)));
    l.bitmapEffect = json.value(QStringLiteral("effect")).toString(QStringLiteral("blur"));
    l.amount = json.value(QStringLiteral("amount")).toDouble(50.0);
    l.color = Color::fromHex(json.value(QStringLiteral("color")).toString()).value_or(Color(255, 0, 0));
    l.color2 = Color::fromHex(json.value(QStringLiteral("color2")).toString()).value_or(Color(255, 255, 0));
    l.viewpoint = json.contains(QStringLiteral("vx"));
    l.viewOffset = QPointF(json.value(QStringLiteral("vx")).toDouble(), json.value(QStringLiteral("vy")).toDouble());
    return l;
}

bool Lens::changesColor() const
{
    return type != Type::None && type != Type::Magnify && type != Type::FishEye && type != Type::Wireframe
           && type != Type::BitmapEffect;
}

QList<std::pair<QString, const char*>> Lens::bitmapEffects()
{
    return {{QStringLiteral("blur"), QT_TRANSLATE_NOOP("vek::Lens", "Gaussscher Weichzeichner")},
            {QStringLiteral("pixelate"), QT_TRANSLATE_NOOP("vek::Lens", "Verpixeln")},
            {QStringLiteral("mosaic"), QT_TRANSLATE_NOOP("vek::Lens", "Mosaik")},
            {QStringLiteral("crystallize"), QT_TRANSLATE_NOOP("vek::Lens", "Kristallisieren")},
            {QStringLiteral("emboss"), QT_TRANSLATE_NOOP("vek::Lens", "Relief")},
            {QStringLiteral("edges"), QT_TRANSLATE_NOOP("vek::Lens", "Kanten finden")},
            {QStringLiteral("charcoal"), QT_TRANSLATE_NOOP("vek::Lens", "Kohle")},
            {QStringLiteral("oil"), QT_TRANSLATE_NOOP("vek::Lens", "Ölfarbe")},
            {QStringLiteral("solarize"), QT_TRANSLATE_NOOP("vek::Lens", "Solarisieren")},
            {QStringLiteral("halftone"), QT_TRANSLATE_NOOP("vek::Lens", "Rasterpunkte")},
            {QStringLiteral("swirl"), QT_TRANSLATE_NOOP("vek::Lens", "Wirbel")},
            {QStringLiteral("ripple"), QT_TRANSLATE_NOOP("vek::Lens", "Wellen")},
            {QStringLiteral("noise"), QT_TRANSLATE_NOOP("vek::Lens", "Rauschen")}};
}

QImage Lens::applyBitmapEffect(const QImage& below, double pxPerMm) const
{
    if (below.isNull())
        return below;
    const double k = std::clamp(amount, 0.0, 100.0) / 100.0;
    // Räumliche Grössen in mm, damit die Linse beim Zoomen gleich aussieht
    const double mm = std::max(0.05, pxPerMm);
    auto px = [mm](double v) { return std::max(1, int(std::lround(v * mm))); };
    const QString& f = bitmapEffect;
    QImage out;
    if (f == QLatin1String("pixelate"))
        out = filters::pixelate(below, px(0.5 + 9.5 * k));
    else if (f == QLatin1String("mosaic"))
        out = filters::mosaic(below, px(0.5 + 9.5 * k));
    else if (f == QLatin1String("crystallize"))
        out = filters::crystallize(below, px(0.5 + 9.5 * k));
    else if (f == QLatin1String("emboss"))
        out = filters::emboss(below, 1.0 + 9.0 * k, 135.0);
    else if (f == QLatin1String("edges"))
        out = filters::findEdges(below, 0.2 + 1.8 * k);
    else if (f == QLatin1String("charcoal"))
        out = filters::charcoal(below, std::max(1.0, 0.1 * mm + 4.0 * k), 2.0 + 8.0 * k);
    else if (f == QLatin1String("oil"))
        out = filters::oilPaint(below, std::clamp(px(0.1 + 1.0 * k), 1, 12), 20);
    else if (f == QLatin1String("solarize"))
        out = filters::solarize(below, int(255 * (1.0 - k)));
    else if (f == QLatin1String("halftone"))
        out = filters::halftone(below, std::max(2.0, (0.5 + 3.5 * k) * mm));
    else if (f == QLatin1String("swirl"))
        out = filters::swirl(below, 360.0 * k);
    else if (f == QLatin1String("ripple"))
        out = filters::ripple(below, std::max(1.0, 2.0 * k * mm), std::max(4.0, 10.0 * mm));
    else if (f == QLatin1String("noise"))
        out = filters::addNoise(below, 100.0 * k, false);
    else
        out = filters::gaussian(below, std::max(0.5, 5.0 * k * mm));
    out = out.convertToFormat(below.format());
    out.setDevicePixelRatio(below.devicePixelRatio());
    return out;
}

void Lens::filter(int& r, int& g, int& b, int a) const
{
    const double k = std::clamp(amount / 100.0, 0.0, 1.0);
    const Color& tint = color;
    auto luminance = [&] { return (0.299 * r + 0.587 * g + 0.114 * b) / std::max(1, a); };
    switch (type) {
    case Type::Brighten: // auch negativ: abdunkeln
        if (amount >= 0) {
            r += int((a - r) * k);
            g += int((a - g) * k);
            b += int((a - b) * k);
        } else {
            const double d = std::clamp(-amount / 100.0, 0.0, 1.0);
            r -= int(r * d);
            g -= int(g * d);
            b -= int(b * d);
        }
        break;
    case Type::ColorAdd:
        r = std::min(a, r + int(tint.r * k * a / 255));
        g = std::min(a, g + int(tint.g * k * a / 255));
        b = std::min(a, b + int(tint.b * k * a / 255));
        break;
    case Type::Invert:
        r = a - r;
        g = a - g;
        b = a - b;
        break;
    case Type::Transparency:
        // Linse als durchscheinende Farbfläche
        r = int(r * k + tint.r * (1 - k) * a / 255);
        g = int(g * k + tint.g * (1 - k) * a / 255);
        b = int(b * k + tint.b * (1 - k) * a / 255);
        break;
    case Type::TintedGrayscale: {
        const double lum = luminance();
        r = int(tint.r * lum);
        g = int(tint.g * lum);
        b = int(tint.b * lum);
        break;
    }
    case Type::ColorLimit:
        // Nur die Linsenfarbe (und Schwarz) kommt durch
        r = int(r * (1 - k) + r * tint.r / 255.0 * k);
        g = int(g * (1 - k) + g * tint.g / 255.0 * k);
        b = int(b * (1 - k) + b * tint.b / 255.0 * k);
        break;
    case Type::CustomColorMap: {
        // Helligkeit zwischen zwei Farben abbilden
        const double lum = luminance();
        r = int((color.r + (color2.r - color.r) * lum) * a / 255);
        g = int((color.g + (color2.g - color.g) * lum) * a / 255);
        b = int((color.b + (color2.b - color.b) * lum) * a / 255);
        break;
    }
    case Type::HeatMap: {
        // Kalt (dunkel) bis heiss (hell), um amount gedreht
        struct Stop {
            double t;
            int r, g, b;
        };
        static const Stop stops[] = {{0.0, 0, 0, 128},    {0.2, 0, 64, 255},   {0.4, 0, 224, 255}, {0.55, 64, 255, 64},
                                     {0.7, 255, 255, 0}, {0.85, 255, 128, 0}, {1.0, 255, 0, 0}};
        double t = luminance() + amount / 100.0;
        if (t > 1.0)
            t -= std::floor(t);
        int i = 0;
        while (i + 2 < int(std::size(stops)) && t > stops[i + 1].t)
            ++i;
        const Stop& s0 = stops[i];
        const Stop& s1 = stops[i + 1];
        const double f = std::clamp((t - s0.t) / (s1.t - s0.t), 0.0, 1.0);
        r = int((s0.r + (s1.r - s0.r) * f) * a / 255);
        g = int((s0.g + (s1.g - s0.g) * f) * a / 255);
        b = int((s0.b + (s1.b - s0.b) * f) * a / 255);
        break;
    }
    default: break;
    }
    r = std::clamp(r, 0, a);
    g = std::clamp(g, 0, a);
    b = std::clamp(b, 0, a);
}

Color Lens::mapColor(const Color& c) const
{
    int r = c.r, g = c.g, b = c.b;
    filter(r, g, b, 255);
    return Color(std::uint8_t(r), std::uint8_t(g), std::uint8_t(b), c.a);
}

QPointF Lens::fishEye(const QPointF& p, const QPointF& center, const QSizeF& radius, bool inverse) const
{
    if (radius.width() <= 0 || radius.height() <= 0)
        return p;
    const double nx = (p.x() - center.x()) / radius.width(), ny = (p.y() - center.y()) / radius.height();
    const double r = std::hypot(nx, ny);
    if (r <= 1e-12 || r >= 1.0)
        return p;
    // Rate > 0 wölbt die Mitte nach vorn (das Bild holt die Vorlage zur Mitte), < 0 drückt sie ein
    const double rate = std::clamp(amount, -1000.0, 1000.0) / 100.0;
    double e = rate >= 0 ? 1.0 + rate : 1.0 / (1.0 - rate);
    if (inverse)
        e = 1.0 / e;
    const double f = std::pow(r, e) / r;
    return center + QPointF(nx * f * radius.width(), ny * f * radius.height());
}

QPainterPath warpPath(const QPainterPath& path, const std::function<QPointF(const QPointF&)>& fn, double maxStep)
{
    maxStep = std::max(maxStep, 1e-6);
    QPainterPath out;
    QPointF current;
    QPointF start;
    bool open = false;
    auto closeIfBack = [&] {
        if (open && current == start && out.elementCount() > 1)
            out.closeSubpath();
    };
    for (int i = 0; i < path.elementCount(); ++i) {
        const QPainterPath::Element e = path.elementAt(i);
        if (e.isMoveTo()) {
            closeIfBack();
            current = start = e;
            out.moveTo(fn(current));
            open = true;
        } else if (e.isLineTo()) {
            const QPointF to = e;
            const int parts = std::clamp(int(std::ceil(QLineF(current, to).length() / maxStep)), 1, 4000);
            for (int k = 1; k <= parts; ++k)
                out.lineTo(fn(current + (to - current) * (double(k) / parts)));
            current = to;
        } else if (e.isCurveTo() && i + 2 < path.elementCount()) {
            const QPointF c1 = e, c2 = path.elementAt(i + 1), end = path.elementAt(i + 2);
            const double hull = QLineF(current, c1).length() + QLineF(c1, c2).length() + QLineF(c2, end).length();
            const int parts = std::clamp(int(std::ceil(hull / maxStep)), 2, 4000);
            const QPointF p0 = current;
            for (int k = 1; k <= parts; ++k) {
                const double t = double(k) / parts, u = 1 - t;
                out.lineTo(fn(u * u * u * p0 + 3 * u * u * t * c1 + 3 * u * t * t * c2 + t * t * t * end));
            }
            current = end;
            i += 2;
        }
    }
    closeIfBack();
    out.setFillRule(path.fillRule());
    return out;
}

namespace {

// Punkt auf einem gleichmässig abgetasteten Rand bei t (0..1)
QPointF edgeAt(const QList<QPointF>& edge, double t)
{
    const double x = std::clamp(t, 0.0, 1.0) * double(edge.size() - 1);
    const int i = std::min(int(x), int(edge.size()) - 2);
    const double f = x - i;
    return edge[i] + (edge[i + 1] - edge[i]) * f;
}

QList<QPointF> straight(const QPointF& a, const QPointF& b)
{
    QList<QPointF> pts;
    for (int k = 0; k < Envelope::kSamples; ++k)
        pts << a + (b - a) * (double(k) / (Envelope::kSamples - 1));
    return pts;
}

// Linienzug nach Bogenlänge auf kSamples Punkte bringen
QList<QPointF> resample(const QList<QPointF>& poly)
{
    QList<double> len{0.0};
    for (int i = 1; i < poly.size(); ++i)
        len << len.last() + QLineF(poly[i - 1], poly[i]).length();
    QList<QPointF> out;
    const double total = len.last();
    int j = 1;
    for (int k = 0; k < Envelope::kSamples; ++k) {
        const double s = total * k / (Envelope::kSamples - 1);
        while (j < poly.size() - 1 && len[j] < s)
            ++j;
        const double seg = len[j] - len[j - 1];
        const double f = seg > 1e-12 ? (s - len[j - 1]) / seg : 0.0;
        out << poly[j - 1] + (poly[j] - poly[j - 1]) * std::clamp(f, 0.0, 1.0);
    }
    return out;
}

} // namespace

Envelope Envelope::rectangle(const QRectF& box, Mode mode)
{
    Envelope e;
    e.mode = mode;
    e.box = box;
    e.top = straight(box.topLeft(), box.topRight());
    e.bottom = straight(box.bottomLeft(), box.bottomRight());
    e.left = straight(box.topLeft(), box.bottomLeft());
    e.right = straight(box.topRight(), box.bottomRight());
    return e;
}

Envelope Envelope::fromOutline(const QRectF& box, const QPainterPath& outline, Mode mode)
{
    const QList<QPolygonF> polys = outline.toSubpathPolygons();
    if (polys.isEmpty() || box.isEmpty())
        return rectangle(box, mode);
    // Grösster Teilpfad
    auto areaOf = [](const QPolygonF& p) { return p.boundingRect().width() * p.boundingRect().height(); };
    QPolygonF poly = *std::max_element(polys.begin(), polys.end(),
                                       [&](const QPolygonF& a, const QPolygonF& b) { return areaOf(a) < areaOf(b); });
    if (poly.size() > 1 && poly.first() == poly.last())
        poly.removeLast();
    const QRectF bounds = poly.boundingRect();
    if (poly.size() < 3 || bounds.width() <= 0 || bounds.height() <= 0)
        return rectangle(box, mode);
    // Fein abtasten, damit die Ecken genau gefunden werden
    QList<QPointF> fine;
    const double step = std::max(bounds.width(), bounds.height()) / 400.0;
    for (int i = 0; i < poly.size(); ++i) {
        const QPointF a = poly[i], b = poly[(i + 1) % poly.size()];
        const int parts = std::max(1, int(std::ceil(QLineF(a, b).length() / step)));
        for (int k = 0; k < parts; ++k)
            fine << a + (b - a) * (double(k) / parts);
    }
    // Im Uhrzeigersinn auf dem Bildschirm (y nach unten): oben links -> oben rechts -> unten rechts
    double area = 0;
    for (int i = 0; i < fine.size(); ++i) {
        const QPointF a = fine[i], b = fine[(i + 1) % fine.size()];
        area += a.x() * b.y() - b.x() * a.y();
    }
    if (area < 0)
        std::reverse(fine.begin(), fine.end());
    auto nearest = [&](const QPointF& corner) {
        int best = 0;
        double bestD = 1e300;
        for (int i = 0; i < fine.size(); ++i) {
            const double d = QLineF(fine[i], corner).length();
            if (d < bestD) {
                bestD = d;
                best = i;
            }
        }
        return best;
    };
    const int tl = nearest(bounds.topLeft()), tr = nearest(bounds.topRight()), br = nearest(bounds.bottomRight()),
              bl = nearest(bounds.bottomLeft());
    const int n = int(fine.size());
    auto ahead = [n](int from, int to) { return (to - from + n) % n; };
    // Die vier Ecken müssen verschieden sein und im Uhrzeigersinn aufeinander folgen
    if (!(ahead(tl, tr) > 0 && ahead(tl, tr) < ahead(tl, br) && ahead(tl, br) < ahead(tl, bl)))
        return rectangle(box, mode);
    auto span = [&](int from, int to) {
        QList<QPointF> pts;
        for (int i = from;; i = (i + 1) % n) {
            pts << fine[i];
            if (i == to)
                break;
        }
        return resample(pts);
    };
    // Vorlage in box einpassen
    const QTransform fit = QTransform::fromTranslate(-bounds.left(), -bounds.top())
                           * QTransform::fromScale(box.width() / bounds.width(), box.height() / bounds.height())
                           * QTransform::fromTranslate(box.left(), box.top());
    auto mapped = [&fit](QList<QPointF> pts, bool reversed) {
        for (QPointF& p : pts)
            p = fit.map(p);
        if (reversed)
            std::reverse(pts.begin(), pts.end());
        return pts;
    };
    Envelope e;
    e.mode = mode;
    e.box = box;
    e.top = mapped(span(tl, tr), false);
    e.right = mapped(span(tr, br), false);
    e.bottom = mapped(span(br, bl), true);
    e.left = mapped(span(bl, tl), true);
    return e;
}

QPointF Envelope::map(const QPointF& p) const
{
    if (!isActive())
        return p;
    const double u = (p.x() - box.left()) / box.width(), v = (p.y() - box.top()) / box.height();
    const QPointF t = edgeAt(top, u), b = edgeAt(bottom, u), l = edgeAt(left, v), r = edgeAt(right, v);
    const QPointF c00 = top.first(), c10 = top.last(), c01 = bottom.first(), c11 = bottom.last();
    // Ecken bilinear (gerade Zeilen und Spalten)
    const QPointF corners = (1 - u) * (1 - v) * c00 + u * (1 - v) * c10 + (1 - u) * v * c01 + u * v * c11;
    switch (mapping) {
    case Mapping::Original:
        // Nur obere und untere Kante: senkrecht dazwischen gerade
        return (1 - v) * t + v * b;
    case Mapping::Putty: {
        // Wie Standard, die Ränder wirken aber mit geglätteter Gewichtung (weicher nach innen)
        auto smooth = [](double x) {
            x = std::clamp(x, 0.0, 1.0);
            return x * x * (3.0 - 2.0 * x);
        };
        const double su = smooth(u), sv = smooth(v);
        return (1 - sv) * t + sv * b + (1 - su) * l + su * r
               - ((1 - su) * (1 - sv) * c00 + su * (1 - sv) * c10 + (1 - su) * sv * c01 + su * sv * c11);
    }
    case Mapping::Horizontal: {
        // Waagrecht: x zwischen linker und rechter Kante, Zeilen gerade
        const QPointF row = (1 - u) * l + u * r;
        return QPointF(row.x(), corners.y());
    }
    case Mapping::Vertical: {
        // Senkrecht: y zwischen oberer und unterer Kante, Spalten gerade
        const QPointF column = (1 - v) * t + v * b;
        return QPointF(corners.x(), column.y());
    }
    case Mapping::Standard: break;
    }
    return (1 - v) * t + v * b + (1 - u) * l + u * r - corners;
}

QPainterPath Envelope::map(const QPainterPath& path) const
{
    if (!isActive())
        return path;
    return warpPath(path, [this](const QPointF& p) { return map(p); }, std::max(box.width(), box.height()) / 150.0);
}

QPainterPath Envelope::outline() const
{
    QPainterPath p;
    if (!isActive())
        return p;
    p.moveTo(top.first());
    for (int i = 1; i < top.size(); ++i)
        p.lineTo(top[i]);
    for (int i = 1; i < right.size(); ++i)
        p.lineTo(right[i]);
    for (int i = int(bottom.size()) - 2; i >= 0; --i)
        p.lineTo(bottom[i]);
    for (int i = int(left.size()) - 2; i > 0; --i)
        p.lineTo(left[i]);
    p.closeSubpath();
    return p;
}

QList<QPointF> Envelope::handles() const
{
    if (!isActive())
        return {};
    const int m = kSamples / 2;
    return {top.first(), top[m], top.last(), right[m], bottom.last(), bottom[m], bottom.first(), left[m]};
}

void Envelope::moveHandle(int handle, const QPointF& to)
{
    if (!isActive() || handle < 0 || handle > 7)
        return;
    const QPointF d = to - handles()[handle];
    auto shift = [&d](QList<QPointF>& edge, const std::function<double(double)>& weight) {
        for (int k = 0; k < kSamples; ++k)
            edge[k] += d * weight(double(k) / (kSamples - 1));
    };
    // Ecke: Gewicht 1 an der Ecke, 0 ab der Mitte des Randes (atStart: Ecke am Anfang des Randes)
    auto corner = [this](bool atStart) {
        return [this, atStart](double t) {
            const double s = atStart ? t : 1.0 - t;
            if (s >= 0.5)
                return 0.0;
            return mode == Mode::Straight ? 1.0 - 2.0 * s : 0.5 * (1.0 + std::cos(2.0 * M_PI * s));
        };
    };
    // Mitte: gerade (Dreieck), ein Bogen (Sinus) oder zwei Bögen (Glocke mit Wendepunkten)
    auto middle = [this](double t) {
        switch (mode) {
        case Mode::Straight: return 1.0 - std::abs(2.0 * t - 1.0);
        case Mode::DoubleArc: return 0.5 * (1.0 - std::cos(2.0 * M_PI * t));
        default: return std::sin(M_PI * t);
        }
    };
    switch (handle) {
    case 0:
        shift(top, corner(true));
        shift(left, corner(true));
        break;
    case 2:
        shift(top, corner(false));
        shift(right, corner(true));
        break;
    case 4:
        shift(bottom, corner(false));
        shift(right, corner(false));
        break;
    case 6:
        shift(bottom, corner(true));
        shift(left, corner(false));
        break;
    case 1: shift(top, middle); break;
    case 3: shift(right, middle); break;
    case 5: shift(bottom, middle); break;
    case 7: shift(left, middle); break;
    default: break;
    }
    // Ecken gehören zu zwei Rändern: genau gleich halten
    left.first() = top.first();
    right.first() = top.last();
    left.last() = bottom.first();
    right.last() = bottom.last();
}

void Envelope::transform(const QTransform& m)
{
    if (!isActive())
        return;
    box = m.mapRect(box);
    for (QList<QPointF>* edge : {&top, &right, &bottom, &left})
        for (QPointF& p : *edge)
            p = m.map(p);
}

QJsonObject Envelope::toJson() const
{
    auto pts = [](const QList<QPointF>& list) {
        QJsonArray a;
        for (const QPointF& p : list) {
            a.append(p.x());
            a.append(p.y());
        }
        return a;
    };
    QJsonObject o{{QStringLiteral("mode"), int(mode)},
                  {QStringLiteral("box"), QJsonArray{box.x(), box.y(), box.width(), box.height()}},
                  {QStringLiteral("top"), pts(top)},
                  {QStringLiteral("right"), pts(right)},
                  {QStringLiteral("bottom"), pts(bottom)},
                  {QStringLiteral("left"), pts(left)}};
    if (mapping != Mapping::Standard)
        o[QStringLiteral("mapping")] = int(mapping);
    return o;
}

Envelope Envelope::fromJson(const QJsonObject& json)
{
    auto pts = [](const QJsonValue& v) {
        QList<QPointF> list;
        const QJsonArray a = v.toArray();
        for (int i = 0; i + 1 < a.size(); i += 2)
            list << QPointF(a[i].toDouble(), a[i + 1].toDouble());
        return list;
    };
    Envelope e;
    e.mode = Mode(std::clamp(json.value(QStringLiteral("mode")).toInt(3), 0, 3));
    e.mapping = Mapping(std::clamp(json.value(QStringLiteral("mapping")).toInt(0), 0, int(Mapping::Vertical)));
    const QJsonArray b = json.value(QStringLiteral("box")).toArray();
    if (b.size() == 4)
        e.box = QRectF(b[0].toDouble(), b[1].toDouble(), b[2].toDouble(), b[3].toDouble());
    e.top = pts(json.value(QStringLiteral("top")));
    e.right = pts(json.value(QStringLiteral("right")));
    e.bottom = pts(json.value(QStringLiteral("bottom")));
    e.left = pts(json.value(QStringLiteral("left")));
    return e;
}

QJsonObject Bevel::toJson() const
{
    return {{QStringLiteral("style"), int(style)},       {QStringLiteral("width"), width},
            {QStringLiteral("intensity"), intensity},     {QStringLiteral("direction"), direction},
            {QStringLiteral("altitude"), altitude},       {QStringLiteral("light"), light.toHex()},
            {QStringLiteral("shadow"), shadow.toHex()}, {QStringLiteral("toCenter"), toCenter}};
}

Bevel Bevel::fromJson(const QJsonObject& json)
{
    Bevel b;
    b.style = Style(std::clamp(json.value(QStringLiteral("style")).toInt(), 0, int(Style::Emboss)));
    b.width = json.value(QStringLiteral("width")).toDouble(2.0);
    b.intensity = json.value(QStringLiteral("intensity")).toDouble(60.0);
    b.direction = json.value(QStringLiteral("direction")).toDouble(135.0);
    b.altitude = json.value(QStringLiteral("altitude")).toDouble(45.0);
    b.light = Color::fromHex(json.value(QStringLiteral("light")).toString()).value_or(Color(255, 255, 255));
    b.shadow = Color::fromHex(json.value(QStringLiteral("shadow")).toString()).value_or(Color(0, 0, 0));
    b.toCenter = json.value(QStringLiteral("toCenter")).toBool(false);
    return b;
}

namespace {

// Eindimensionale Abstandstransformation (Felzenszwalb/Huttenlocher): f = quadrierte Abstände
void distance1d(const std::vector<float>& f, std::vector<float>& d, std::vector<int>& v, std::vector<float>& z)
{
    const int n = int(f.size());
    constexpr float inf = 1e20f;
    int k = 0;
    v[0] = 0;
    z[0] = -inf;
    z[1] = inf;
    for (int q = 1; q < n; ++q) {
        float s = ((f[q] + float(q) * q) - (f[v[k]] + float(v[k]) * v[k])) / (2.0f * q - 2.0f * v[k]);
        while (s <= z[k]) {
            --k;
            s = ((f[q] + float(q) * q) - (f[v[k]] + float(v[k]) * v[k])) / (2.0f * q - 2.0f * v[k]);
        }
        ++k;
        v[k] = q;
        z[k] = s;
        z[k + 1] = inf;
    }
    k = 0;
    for (int q = 0; q < n; ++q) {
        while (z[k + 1] < q)
            ++k;
        d[q] = float(q - v[k]) * float(q - v[k]) + f[v[k]];
    }
}

} // namespace

QList<float> distanceToEdge(const QImage& mask, float* maxDistance)
{
    const QImage m = mask.convertToFormat(QImage::Format_Grayscale8);
    const int w = m.width(), h = m.height();
    QList<float> out(qsizetype(w) * h, 0.0f);
    if (maxDistance)
        *maxDistance = 0.0f;
    if (w == 0 || h == 0)
        return out;
    constexpr float inf = 1e20f;
    // Abstand zum nächsten Aussenpunkt; ein Rand von einem Pixel ringsum liegt aussen
    const int W = w + 2, H = h + 2;
    std::vector<float> grid(std::size_t(W) * H, 0.0f);
    for (int y = 0; y < h; ++y) {
        const uchar* line = m.constScanLine(y);
        for (int x = 0; x < w; ++x)
            grid[std::size_t(y + 1) * W + x + 1] = line[x] >= 128 ? inf : 0.0f;
    }
    const int n = std::max(W, H);
    std::vector<float> f, d, z(n + 1);
    std::vector<int> v(n);
    f.resize(H);
    d.resize(H);
    for (int x = 0; x < W; ++x) {
        for (int y = 0; y < H; ++y)
            f[y] = grid[std::size_t(y) * W + x];
        distance1d(f, d, v, z);
        for (int y = 0; y < H; ++y)
            grid[std::size_t(y) * W + x] = d[y];
    }
    f.resize(W);
    d.resize(W);
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x)
            f[x] = grid[std::size_t(y) * W + x];
        distance1d(f, d, v, z);
        for (int x = 0; x < W; ++x)
            grid[std::size_t(y) * W + x] = d[x];
    }
    float best = 0.0f;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const float dist = std::sqrt(grid[std::size_t(y + 1) * W + x + 1]);
            out[qsizetype(y) * w + x] = dist;
            best = std::max(best, dist);
        }
    if (maxDistance)
        *maxDistance = best;
    return out;
}

double BlockShadow::depth() const
{
    return std::hypot(offset.x(), offset.y());
}

double BlockShadow::angle() const
{
    return depth() < 1e-12 ? 0.0 : qRadiansToDegrees(std::atan2(-offset.y(), offset.x()));
}

void BlockShadow::setPolar(double depthMm, double angleDeg)
{
    const double a = qDegreesToRadians(angleDeg);
    offset = QPointF(std::cos(a), -std::sin(a)) * std::max(0.0, depthMm);
}

QJsonObject BlockShadow::toJson() const
{
    QJsonObject o{{QStringLiteral("dx"), offset.x()}, {QStringLiteral("dy"), offset.y()}, {QStringLiteral("color"), color.toHex()}};
    if (expand != 0.0)
        o[QStringLiteral("expand")] = expand;
    if (simplify)
        o[QStringLiteral("simplify")] = true;
    if (removeHoles)
        o[QStringLiteral("removeHoles")] = true;
    if (!useOutline)
        o[QStringLiteral("useOutline")] = false;
    return o;
}

BlockShadow BlockShadow::fromJson(const QJsonObject& json)
{
    BlockShadow s;
    s.active = true;
    s.offset = QPointF(json.value(QStringLiteral("dx")).toDouble(5.0), json.value(QStringLiteral("dy")).toDouble(5.0));
    s.color = Color::fromHex(json.value(QStringLiteral("color")).toString()).value_or(Color(0, 0, 0));
    s.expand = json.value(QStringLiteral("expand")).toDouble(0.0);
    s.simplify = json.value(QStringLiteral("simplify")).toBool(false);
    s.removeHoles = json.value(QStringLiteral("removeHoles")).toBool(false);
    s.useOutline = json.value(QStringLiteral("useOutline")).toBool(true);
    return s;
}

namespace {

double signedArea(const QPolygonF& poly)
{
    double a = 0;
    for (int i = 0; i < poly.size(); ++i) {
        const QPointF p = poly[i], q = poly[(i + 1) % poly.size()];
        a += p.x() * q.y() - q.x() * p.y();
    }
    return a / 2.0;
}

} // namespace

QPainterPath sweepArea(const QPainterPath& area, const QPointF& offset, double flatness)
{
    if (area.isEmpty())
        return area;
    const QPainterPath both = area.united(area.translated(offset));
    if (std::hypot(offset.x(), offset.y()) < 1e-9)
        return both.simplified();
    // Kurven fein in Strecken zerlegen: Qt zerlegt in Einheiten von etwa einem halben Punkt,
    // darum vergrössert zerlegen und zurückrechnen
    const double k = 1.0 / std::max(1e-4, flatness);
    const QTransform up = QTransform::fromScale(k, k), down = QTransform::fromScale(1.0 / k, 1.0 / k);
    QList<QPolygonF> polys = up.map(area).toSubpathPolygons();
    for (QPolygonF& poly : polys)
        poly = down.map(poly);
    // Jede Kante überstreicht ein Parallelogramm; vereinigt paarweise (wie ein Turnier), damit
    // die Pfade der Mengenoperationen klein bleiben
    QList<QPainterPath> parts;
    for (const QPolygonF& poly : std::as_const(polys)) {
        for (int i = 0; i < poly.size(); ++i) {
            const QPointF a = poly[i], b = poly[(i + 1) % poly.size()];
            const QPointF e = b - a;
            if (std::abs(e.x() * offset.y() - e.y() * offset.x()) < 1e-12)
                continue;
            QPainterPath q;
            q.addPolygon(QPolygonF{a, b, b + offset, a + offset});
            q.closeSubpath();
            parts << q;
        }
    }
    parts << both;
    while (parts.size() > 1) {
        QList<QPainterPath> next;
        for (int i = 0; i + 1 < parts.size(); i += 2)
            next << parts[i].united(parts[i + 1]);
        if (parts.size() % 2)
            next << parts.last();
        parts = next;
    }
    return parts.first().simplified();
}

QPainterPath blockShadowPath(const QPainterPath& area, const BlockShadow& s)
{
    QPainterPath base = area;
    if (base.isEmpty())
        return {};
    if (s.expand > 0) {
        QPainterPathStroker st;
        st.setWidth(2.0 * s.expand);
        st.setJoinStyle(Qt::RoundJoin);
        base = base.united(st.createStroke(base)).simplified();
    }
    QPainterPath shadow = sweepArea(base, s.offset);
    if (s.removeHoles) {
        // Alle Teilpfade gleich orientiert: Löcher werden mitgefüllt
        QPainterPath filled;
        filled.setFillRule(Qt::WindingFill);
        for (QPolygonF poly : shadow.toSubpathPolygons()) {
            if (signedArea(poly) < 0)
                std::reverse(poly.begin(), poly.end());
            filled.addPolygon(poly);
            filled.closeSubpath();
        }
        shadow = filled.simplified();
    }
    if (s.simplify)
        shadow = shadow.subtracted(area).simplified();
    return shadow;
}

QJsonObject ContourEffect::toJson() const
{
    return {{QStringLiteral("direction"), int(direction)}, {QStringLiteral("steps"), steps},
            {QStringLiteral("offset"), offset},           {QStringLiteral("endFill"), endFill.toHex()},
            {QStringLiteral("endStroke"), endStroke.toHex()}};
}

ContourEffect ContourEffect::fromJson(const QJsonObject& json)
{
    ContourEffect c;
    c.active = true;
    c.direction = Direction(std::clamp(json.value(QStringLiteral("direction")).toInt(2), 0, 2));
    c.steps = std::clamp(json.value(QStringLiteral("steps")).toInt(4), 1, 999);
    c.offset = json.value(QStringLiteral("offset")).toDouble(2.0);
    c.endFill = Color::fromHex(json.value(QStringLiteral("endFill")).toString()).value_or(Color(255, 255, 255));
    c.endStroke = Color::fromHex(json.value(QStringLiteral("endStroke")).toString()).value_or(Color(0, 0, 0));
    return c;
}

QJsonObject Extrusion::toJson() const
{
    QJsonObject o{{QStringLiteral("vx"), vanish.x()}, {QStringLiteral("vy"), vanish.y()}, {QStringLiteral("depth"), depth},
                  {QStringLiteral("parallel"), parallel}, {QStringLiteral("color"), color.toHex()}, {QStringLiteral("shade"), shade}};
    if (isRotated()) {
        o[QStringLiteral("rx")] = rotX;
        o[QStringLiteral("ry")] = rotY;
        o[QStringLiteral("rz")] = rotZ;
    }
    return o;
}

Extrusion Extrusion::fromJson(const QJsonObject& json)
{
    Extrusion e;
    e.active = true;
    e.vanish = QPointF(json.value(QStringLiteral("vx")).toDouble(20.0), json.value(QStringLiteral("vy")).toDouble(-20.0));
    e.depth = json.value(QStringLiteral("depth")).toDouble(30.0);
    e.parallel = json.value(QStringLiteral("parallel")).toBool();
    e.color = Color::fromHex(json.value(QStringLiteral("color")).toString()).value_or(Color(128, 128, 128));
    e.shade = json.value(QStringLiteral("shade")).toBool(true);
    e.rotX = json.value(QStringLiteral("rx")).toDouble(0.0);
    e.rotY = json.value(QStringLiteral("ry")).toDouble(0.0);
    e.rotZ = json.value(QStringLiteral("rz")).toDouble(0.0);
    return e;
}

double Extrusion::bodyDepth() const
{
    const double f = std::clamp(depth, 0.0, 99.0) / 100.0;
    const double len = std::hypot(vanish.x(), vanish.y());
    if (parallel)
        return std::max(1e-6, len * f);
    const double k = std::max(1e-3, len);
    return f * k / (1.0 - f);
}

void Extrusion::rotation(double r[3][3]) const
{
    // x: Oberkante nach hinten, y: rechte Seite nach hinten, z: im Uhrzeigersinn (y nach unten)
    const double ax = qDegreesToRadians(rotX), ay = qDegreesToRadians(rotY), az = qDegreesToRadians(rotZ);
    const double rx[3][3] = {{1, 0, 0}, {0, std::cos(ax), std::sin(ax)}, {0, -std::sin(ax), std::cos(ax)}};
    const double ry[3][3] = {{std::cos(ay), 0, -std::sin(ay)}, {0, 1, 0}, {std::sin(ay), 0, std::cos(ay)}};
    const double rz[3][3] = {{std::cos(az), -std::sin(az), 0}, {std::sin(az), std::cos(az), 0}, {0, 0, 1}};
    double ryx[3][3];
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            ryx[i][j] = ry[i][0] * rx[0][j] + ry[i][1] * rx[1][j] + ry[i][2] * rx[2][j];
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            r[i][j] = rz[i][0] * ryx[0][j] + rz[i][1] * ryx[1][j] + rz[i][2] * ryx[2][j];
}

QPointF Extrusion::project(const QPointF& center, const QPointF& p, double t, double* depthOut) const
{
    double r[3][3];
    rotation(r);
    const double d = bodyDepth();
    const double v[3] = {p.x() - center.x(), p.y() - center.y(), t * d};
    double q[3];
    for (int i = 0; i < 3; ++i)
        q[i] = r[i][0] * v[0] + r[i][1] * v[1] + r[i][2] * v[2];
    if (depthOut)
        *depthOut = q[2];
    if (parallel) {
        // Schräg: die Tiefe verläuft in Richtung des Fluchtpunkts
        const double f = std::clamp(depth, 0.0, 99.0) / 100.0;
        const QPointF dir = vanish * f / d;
        return center + QPointF(q[0], q[1]) + dir * q[2];
    }
    const double k = std::max(1e-3, std::hypot(vanish.x(), vanish.y()));
    const double z = std::max(q[2], -0.9 * k); // nicht hinter das Auge
    return center + vanish + (QPointF(q[0], q[1]) - vanish) * (k / (k + z));
}

QTransform Extrusion::faceTransform(const QPointF& center) const
{
    if (!isRotated())
        return QTransform();
    double r[3][3];
    rotation(r);
    const double cx = center.x(), cy = center.y();
    QTransform m;
    if (parallel) {
        const double f = std::clamp(depth, 0.0, 99.0) / 100.0;
        const QPointF dir = vanish * f / bodyDepth();
        m = QTransform(r[0][0] + dir.x() * r[2][0], r[1][0] + dir.y() * r[2][0], r[0][1] + dir.x() * r[2][1],
                       r[1][1] + dir.y() * r[2][1], cx, cy);
    } else {
        // w = 1 + z/k mit z = r20 x + r21 y; Bild = (c + V) w - V + (r00 x + r01 y, ...)
        const double k = std::max(1e-3, std::hypot(vanish.x(), vanish.y()));
        const double ex = cx + vanish.x(), ey = cy + vanish.y();
        m = QTransform(r[0][0] + ex * r[2][0] / k, r[1][0] + ey * r[2][0] / k, r[2][0] / k,
                       r[0][1] + ex * r[2][1] / k, r[1][1] + ey * r[2][1] / k, r[2][1] / k,
                       cx, cy, 1.0);
    }
    return QTransform::fromTranslate(-cx, -cy) * m;
}

QJsonObject DropShadow::toJson() const
{
    QJsonObject o{{QStringLiteral("perspective"), perspective}, {QStringLiteral("fromTop"), fromTop},
                  {QStringLiteral("dx"), offset.x()},           {QStringLiteral("dy"), offset.y()},
                  {QStringLiteral("angle"), angle},             {QStringLiteral("stretch"), stretch},
                  {QStringLiteral("fade"), fade},               {QStringLiteral("opacity"), opacity},
                  {QStringLiteral("feather"), feather},         {QStringLiteral("color"), color.toHex()}};
    if (inner)
        o[QStringLiteral("inner")] = true;
    if (blend != BlendMode::Normal)
        o[QStringLiteral("blend")] = blending::id(blend);
    return o;
}

DropShadow DropShadow::fromJson(const QJsonObject& json)
{
    DropShadow s;
    s.active = true;
    s.perspective = json.value(QStringLiteral("perspective")).toBool();
    s.fromTop = json.value(QStringLiteral("fromTop")).toBool();
    s.offset = QPointF(json.value(QStringLiteral("dx")).toDouble(2.0), json.value(QStringLiteral("dy")).toDouble(2.0));
    s.angle = json.value(QStringLiteral("angle")).toDouble(45.0);
    s.stretch = json.value(QStringLiteral("stretch")).toDouble(50.0);
    s.fade = json.value(QStringLiteral("fade")).toDouble(0.0);
    s.opacity = json.value(QStringLiteral("opacity")).toDouble(50.0);
    s.feather = json.value(QStringLiteral("feather")).toDouble(1.5);
    s.color = Color::fromHex(json.value(QStringLiteral("color")).toString()).value_or(Color(0, 0, 0));
    s.inner = json.value(QStringLiteral("inner")).toBool();
    s.blend = blending::fromId(json.value(QStringLiteral("blend")).toString()).value_or(BlendMode::Normal);
    return s;
}

} // namespace vek
