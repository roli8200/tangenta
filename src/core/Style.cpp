#include "Style.h"

#include <QCache>
#include <QConicalGradient>
#include <QJsonDocument>
#include <QJsonArray>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace vek {

namespace {

QJsonValue colorToJson(const std::optional<Color>& c)
{
    return c ? QJsonValue(c->toHex()) : QJsonValue(QJsonValue::Null);
}

std::optional<Color> colorFromJson(const QJsonValue& v)
{
    return v.isString() ? Color::fromHex(v.toString()) : std::nullopt;
}

const char* typeName(Fill::Type t)
{
    switch (t) {
    case Fill::Type::Solid: return "solid";
    case Fill::Type::Linear: return "linear";
    case Fill::Type::Radial: return "radial";
    case Fill::Type::Conical: return "conical";
    case Fill::Type::Mesh: return "mesh";
    case Fill::Type::Pattern: return "pattern";
    default: return "none";
    }
}

Fill::Type typeFromName(const QString& s)
{
    for (Fill::Type t : {Fill::Type::Solid, Fill::Type::Linear, Fill::Type::Radial, Fill::Type::Conical, Fill::Type::Mesh, Fill::Type::Pattern}) {
        if (s == QLatin1String(typeName(t)))
            return t;
    }
    return Fill::Type::None;
}

QSizeF usable(const QSizeF& s)
{
    return QSizeF(std::max(std::abs(s.width()), 1e-6), std::max(std::abs(s.height()), 1e-6));
}

// Abstand vom Punkt c (mm) zur entferntesten Ecke des Rechtecks 0..w x 0..h
double farthestCorner(const QPointF& c, const QSizeF& s)
{
    double best = 0.0;
    for (const QPointF& corner : {QPointF(0, 0), QPointF(s.width(), 0), QPointF(0, s.height()), QPointF(s.width(), s.height())})
        best = std::max(best, std::hypot(corner.x() - c.x(), corner.y() - c.y()));
    return best;
}

double cross(const QPointF& a, const QPointF& b)
{
    return a.x() * b.y() - a.y() * b.x();
}

// Umkehrung der bilinearen Abbildung des Vierecks a b c d (oben links, oben rechts, unten
// rechts, unten links): liefert (u, v) mit p = bilinear(u, v), sonst nichts
std::optional<QPointF> inverseBilinear(const QPointF& p, const QPointF& a, const QPointF& b, const QPointF& c, const QPointF& d)
{
    const QPointF e = b - a, f = d - a, g = a - b + c - d, h = p - a;
    const double k2 = cross(g, f);
    const double k1 = cross(e, f) + cross(h, g);
    const double k0 = cross(h, e);
    const auto solveU = [&](double v) -> std::optional<QPointF> {
        const QPointF axis = e + g * v;
        const double len2 = QPointF::dotProduct(axis, axis);
        if (len2 < 1e-18)
            return std::nullopt;
        const double u = QPointF::dotProduct(h - f * v, axis) / len2;
        constexpr double eps = 1e-3;
        if (u < -eps || u > 1 + eps || v < -eps || v > 1 + eps)
            return std::nullopt;
        return QPointF(std::clamp(u, 0.0, 1.0), std::clamp(v, 0.0, 1.0));
    };
    if (std::abs(k2) < 1e-12) {
        if (std::abs(k1) < 1e-18)
            return std::nullopt;
        return solveU(-k0 / k1);
    }
    double w = k1 * k1 - 4.0 * k0 * k2;
    if (w < 0)
        return std::nullopt;
    w = std::sqrt(w);
    if (auto r = solveU((-k1 - w) / (2.0 * k2)))
        return r;
    return solveU((-k1 + w) / (2.0 * k2));
}

QImage meshImage(const MeshFill& mesh, const QSize& size)
{
    // Kleiner Zwischenspeicher: der Pinsel wird bei jedem Neuzeichnen verlangt
    static QCache<QString, QImage> cache(64 * 1024); // Kosten in KiB
    const QString key = QString::fromUtf8(QJsonDocument(mesh.toJson()).toJson(QJsonDocument::Compact))
                        + QStringLiteral("@%1x%2").arg(size.width()).arg(size.height());
    if (const QImage* hit = cache.object(key))
        return *hit;
    QImage image = mesh.render(size);
    cache.insert(key, new QImage(image), std::max<qsizetype>(1, image.sizeInBytes() / 1024));
    return image;
}

} // namespace

MeshFill MeshFill::grid(int columns, int rows, const Color& color)
{
    MeshFill m;
    m.columns = std::clamp(columns, 1, 50);
    m.rows = std::clamp(rows, 1, 50);
    for (int r = 0; r <= m.rows; ++r) {
        for (int c = 0; c <= m.columns; ++c) {
            m.points << QPointF(double(c) / m.columns, double(r) / m.rows);
            m.colors << color;
        }
    }
    return m;
}

bool MeshFill::isValid() const
{
    return columns >= 1 && rows >= 1 && points.size() == nodeCount() && colors.size() == nodeCount();
}

std::array<QPointF, 4> MeshFill::patch(int row, int column) const
{
    return {points[index(row, column)], points[index(row, column + 1)], points[index(row + 1, column + 1)],
            points[index(row + 1, column)]};
}

QImage MeshFill::render(const QSize& size) const
{
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    if (!isValid() || size.isEmpty())
        return image;
    const int w = size.width(), h = size.height();
    for (int pr = 0; pr < rows; ++pr) {
        for (int pc = 0; pc < columns; ++pc) {
            std::array<QPointF, 4> q = patch(pr, pc);
            for (QPointF& p : q)
                p = QPointF(p.x() * w, p.y() * h);
            const Color c00 = colors[index(pr, pc)], c10 = colors[index(pr, pc + 1)];
            const Color c11 = colors[index(pr + 1, pc + 1)], c01 = colors[index(pr + 1, pc)];
            double x0 = q[0].x(), x1 = x0, y0 = q[0].y(), y1 = y0;
            for (const QPointF& p : q) {
                x0 = std::min(x0, p.x());
                x1 = std::max(x1, p.x());
                y0 = std::min(y0, p.y());
                y1 = std::max(y1, p.y());
            }
            const int px0 = std::max(0, int(std::floor(x0))), px1 = std::min(w - 1, int(std::ceil(x1)));
            const int py0 = std::max(0, int(std::floor(y0))), py1 = std::min(h - 1, int(std::ceil(y1)));
            for (int y = py0; y <= py1; ++y) {
                auto* line = reinterpret_cast<QRgb*>(image.scanLine(y));
                for (int x = px0; x <= px1; ++x) {
                    const std::optional<QPointF> uv = inverseBilinear(QPointF(x + 0.5, y + 0.5), q[0], q[1], q[2], q[3]);
                    if (!uv)
                        continue;
                    const double u = uv->x(), v = uv->y();
                    const double w00 = (1 - u) * (1 - v), w10 = u * (1 - v), w11 = u * v, w01 = (1 - u) * v;
                    const auto mix = [&](auto channel) {
                        return w00 * channel(c00) + w10 * channel(c10) + w11 * channel(c11) + w01 * channel(c01);
                    };
                    const double a = mix([](const Color& c) { return double(c.a); });
                    const double k = a / 255.0;
                    line[x] = qRgba(int(std::lround(mix([](const Color& c) { return double(c.r); }) * k)),
                                    int(std::lround(mix([](const Color& c) { return double(c.g); }) * k)),
                                    int(std::lround(mix([](const Color& c) { return double(c.b); }) * k)), int(std::lround(a)));
                }
            }
        }
    }
    return image;
}

QJsonObject MeshFill::toJson() const
{
    QJsonArray pts, cols;
    for (const QPointF& p : points) {
        pts << p.x();
        pts << p.y();
    }
    for (const Color& c : colors)
        cols << c.toHex();
    return {{QStringLiteral("columns"), columns}, {QStringLiteral("rows"), rows}, {QStringLiteral("points"), pts},
            {QStringLiteral("colors"), cols}};
}

MeshFill MeshFill::fromJson(const QJsonObject& json)
{
    MeshFill m;
    m.columns = std::max(1, json.value(QStringLiteral("columns")).toInt(2));
    m.rows = std::max(1, json.value(QStringLiteral("rows")).toInt(2));
    const QJsonArray pts = json.value(QStringLiteral("points")).toArray();
    for (int i = 0; i + 1 < pts.size(); i += 2)
        m.points << QPointF(pts[i].toDouble(), pts[i + 1].toDouble());
    for (const QJsonValue& v : json.value(QStringLiteral("colors")).toArray())
        m.colors << Color::fromHex(v.toString()).value_or(Color(255, 255, 255));
    if (!m.isValid())
        return grid(m.columns, m.rows, m.colors.value(0, Color(255, 255, 255)));
    return m;
}

Fill Fill::meshFill(const MeshFill& m)
{
    Fill f;
    f.type = Type::Mesh;
    f.mesh = m;
    return f;
}

Fill Fill::solid(const Color& c)
{
    Fill f;
    f.type = Type::Solid;
    f.color = c;
    return f;
}

Fill Fill::gradient(Type type, const Color& from, const Color& to)
{
    Fill f;
    f.type = type;
    f.stops = {GradientStop{0.0, from}, GradientStop{1.0, to}};
    if (type == Type::Linear) {
        f.start = QPointF(0.0, 0.5);
        f.end = QPointF(1.0, 0.5);
    } else {
        f.start = QPointF(0.5, 0.5);
        f.end = QPointF(1.0, 0.0);
    }
    return f;
}

std::optional<Color> Fill::representativeColor() const
{
    if (type == Type::Solid)
        return color;
    if (isGradient() && !stops.isEmpty())
        return stops.first().color;
    if (isMesh() && !mesh.colors.isEmpty())
        return mesh.colors.first();
    if (isPattern())
        return pattern.fg;
    return std::nullopt;
}

void Fill::sortStops()
{
    std::stable_sort(stops.begin(), stops.end(),
                     [](const GradientStop& a, const GradientStop& b) { return a.position < b.position; });
}

Fill Fill::patternFill(const PatternFill& p)
{
    Fill f;
    f.type = Type::Pattern;
    f.pattern = p;
    return f;
}

QBrush Fill::brush(const QRectF& localBounds) const
{
    switch (type) {
    case Type::None: return Qt::NoBrush;
    case Type::Solid: return QBrush(color.toQColor());
    case Type::Pattern: {
        // Kachel tile mm gross, beginnend oben links an der Form
        const QImage img = pattern.image();
        if (img.isNull())
            return QBrush(QColor(0xd0, 0xd0, 0xd0));
        QBrush b(img);
        b.setTransform(QTransform::fromScale(pattern.tile / img.width(), pattern.tile / img.height())
                       * QTransform::fromTranslate(localBounds.left(), localBounds.top()));
        return b;
    }
    case Type::Mesh: {
        if (localBounds.width() <= 0 || localBounds.height() <= 0)
            return Qt::NoBrush;
        // Raster etwa 4 Pixel je mm, höchstens 1024 an der langen Seite
        const double scale = std::min(4.0, 1024.0 / std::max(localBounds.width(), localBounds.height()));
        const QSize size(std::max(8, int(std::ceil(localBounds.width() * scale))),
                         std::max(8, int(std::ceil(localBounds.height() * scale))));
        QBrush b(meshImage(mesh, size));
        b.setTransform(QTransform::fromScale(localBounds.width() / size.width(), localBounds.height() / size.height())
                       * QTransform::fromTranslate(localBounds.left(), localBounds.top()));
        return b;
    }
    default: break;
    }
    const auto toLocal = [&](const QPointF& u) {
        return QPointF(localBounds.left() + u.x() * localBounds.width(), localBounds.top() + u.y() * localBounds.height());
    };
    const QPointF s = toLocal(start);
    const QPointF e = toLocal(end);
    QGradientStops qstops;
    for (const GradientStop& st : stops)
        qstops.append({std::clamp(st.position, 0.0, 1.0), st.color.toQColor()});
    if (qstops.isEmpty())
        qstops = {{0.0, Qt::black}, {1.0, Qt::white}};

    if (type == Type::Linear) {
        QLinearGradient g(s, e);
        g.setStops(qstops);
        return QBrush(g);
    }
    if (type == Type::Radial) {
        QRadialGradient g(s, std::max(1e-6, std::hypot(e.x() - s.x(), e.y() - s.y())));
        g.setStops(qstops);
        return QBrush(g);
    }
    // Qt: Winkel gegen den Uhrzeigersinn am Bildschirm, 0 = 3 Uhr
    QConicalGradient g(s, qRadiansToDegrees(std::atan2(-(e.y() - s.y()), e.x() - s.x())));
    g.setStops(qstops);
    return QBrush(g);
}

GradientGeometry Fill::geometry(const QSizeF& localSize) const
{
    const QSizeF size = usable(localSize);
    GradientGeometry g;
    const QPointF sMm(start.x() * size.width(), start.y() * size.height());
    const QPointF eMm(end.x() * size.width(), end.y() * size.height());
    const QPointF v = eMm - sMm;
    g.angle = qRadiansToDegrees(std::atan2(-v.y(), v.x()));
    if (g.angle < 0)
        g.angle += 360.0;
    const double a = qDegreesToRadians(g.angle);

    QPointF center;
    double pad = 0.0;
    if (type == Type::Linear) {
        center = (sMm + eMm) / 2.0;
        const double full = (std::abs(size.width() * std::cos(a)) + std::abs(size.height() * std::sin(a))) / 2.0;
        pad = full > 0 ? (1.0 - std::hypot(v.x(), v.y()) / 2.0 / full) * 100.0 : 0.0;
    } else {
        center = sMm;
        const double full = farthestCorner(center, size);
        pad = full > 0 ? (1.0 - std::hypot(v.x(), v.y()) / full) * 100.0 : 0.0;
    }
    g.edgePad = std::clamp(pad, 0.0, 49.0);
    g.offsetX = (center.x() / size.width() - 0.5) * 200.0;
    g.offsetY = -(center.y() / size.height() - 0.5) * 200.0;
    return g;
}

void Fill::setGeometry(const GradientGeometry& g, const QSizeF& localSize)
{
    const QSizeF size = usable(localSize);
    const double a = qDegreesToRadians(g.angle);
    const QPointF dir(std::cos(a), -std::sin(a));
    const double pad = 1.0 - std::clamp(g.edgePad, 0.0, 49.0) / 100.0;
    const QPointF center((0.5 + g.offsetX / 200.0) * size.width(), (0.5 - g.offsetY / 200.0) * size.height());

    QPointF sMm;
    QPointF eMm;
    if (type == Type::Linear) {
        const double half = (std::abs(size.width() * dir.x()) + std::abs(size.height() * dir.y())) / 2.0 * pad;
        sMm = center - dir * half;
        eMm = center + dir * half;
    } else {
        sMm = center;
        eMm = center + dir * (farthestCorner(center, size) * pad);
    }
    start = QPointF(sMm.x() / size.width(), sMm.y() / size.height());
    end = QPointF(eMm.x() / size.width(), eMm.y() / size.height());
}

QJsonValue Fill::toJson() const
{
    if (type == Type::None)
        return QJsonValue(QJsonValue::Null);
    if (type == Type::Solid)
        return color.toHex();
    if (type == Type::Mesh) {
        QJsonObject o = mesh.toJson();
        o[QStringLiteral("type")] = QStringLiteral("mesh");
        return o;
    }
    if (type == Type::Pattern) {
        QJsonObject o = pattern.toJson();
        o[QStringLiteral("type")] = QStringLiteral("pattern");
        return o;
    }
    QJsonArray s;
    for (const GradientStop& st : stops)
        s.append(QJsonObject{{QStringLiteral("position"), st.position}, {QStringLiteral("color"), st.color.toHex()}});
    return QJsonObject{
        {QStringLiteral("type"), QLatin1String(typeName(type))},
        {QStringLiteral("start"), QJsonArray{start.x(), start.y()}},
        {QStringLiteral("end"), QJsonArray{end.x(), end.y()}},
        {QStringLiteral("stops"), s},
    };
}

Fill Fill::fromJson(const QJsonValue& json)
{
    // Kurzform: Farbe als Text = gleichmässig, null = keine
    if (json.isString()) {
        const std::optional<Color> c = Color::fromHex(json.toString());
        return c ? solid(*c) : none();
    }
    if (!json.isObject())
        return none();
    const QJsonObject o = json.toObject();
    Fill f;
    f.type = typeFromName(o.value(QStringLiteral("type")).toString());
    if (f.type == Type::Solid) {
        f.color = Color::fromHex(o.value(QStringLiteral("color")).toString()).value_or(Color());
        return f;
    }
    if (f.type == Type::Mesh) {
        f.mesh = MeshFill::fromJson(o);
        return f;
    }
    if (f.type == Type::Pattern) {
        f.pattern = PatternFill::fromJson(o);
        return f;
    }
    const QJsonArray s = o.value(QStringLiteral("start")).toArray();
    const QJsonArray e = o.value(QStringLiteral("end")).toArray();
    if (s.size() == 2)
        f.start = QPointF(s[0].toDouble(), s[1].toDouble());
    if (e.size() == 2)
        f.end = QPointF(e[0].toDouble(), e[1].toDouble());
    for (const QJsonValue& v : o.value(QStringLiteral("stops")).toArray()) {
        const QJsonObject st = v.toObject();
        f.stops.append({st.value(QStringLiteral("position")).toDouble(),
                        Color::fromHex(st.value(QStringLiteral("color")).toString()).value_or(Color())});
    }
    f.sortStops();
    return f;
}

QJsonObject Style::toJson() const
{
    QJsonObject o;
    o[QStringLiteral("fill")] = fill.toJson();
    o[QStringLiteral("stroke")] = colorToJson(stroke);
    o[QStringLiteral("strokeWidth")] = strokeWidth;
    if (dash > 0)
        o[QStringLiteral("dash")] = dash;
    if (startArrow > 0)
        o[QStringLiteral("arrowStart")] = startArrow;
    if (endArrow > 0)
        o[QStringLiteral("arrowEnd")] = endArrow;
    if (join != Join::Miter)
        o[QStringLiteral("join")] = int(join);
    if (cap != Cap::Flat)
        o[QStringLiteral("cap")] = int(cap);
    if (position != Position::Center)
        o[QStringLiteral("position")] = int(position);
    if (behindFill)
        o[QStringLiteral("behindFill")] = true;
    if (scaleWithObject)
        o[QStringLiteral("scaleWithObject")] = true;
    if (nibStretch != 100.0)
        o[QStringLiteral("nibStretch")] = nibStretch;
    if (nibAngle != 0.0)
        o[QStringLiteral("nibAngle")] = nibAngle;
    return o;
}

Style Style::fromJson(const QJsonObject& json)
{
    Style s;
    s.fill = Fill::fromJson(json.value(QStringLiteral("fill")));
    s.stroke = colorFromJson(json.value(QStringLiteral("stroke")));
    s.strokeWidth = json.value(QStringLiteral("strokeWidth")).toDouble(0.2);
    s.dash = std::max(0, json.value(QStringLiteral("dash")).toInt());
    s.startArrow = std::max(0, json.value(QStringLiteral("arrowStart")).toInt());
    s.endArrow = std::max(0, json.value(QStringLiteral("arrowEnd")).toInt());
    s.join = Join(std::clamp(json.value(QStringLiteral("join")).toInt(), 0, 2));
    s.cap = Cap(std::clamp(json.value(QStringLiteral("cap")).toInt(), 0, 2));
    s.position = Position(std::clamp(json.value(QStringLiteral("position")).toInt(), 0, 2));
    s.behindFill = json.value(QStringLiteral("behindFill")).toBool();
    s.scaleWithObject = json.value(QStringLiteral("scaleWithObject")).toBool();
    s.nibStretch = std::clamp(json.value(QStringLiteral("nibStretch")).toDouble(100.0), 1.0, 100.0);
    s.nibAngle = json.value(QStringLiteral("nibAngle")).toDouble();
    return s;
}

} // namespace vek
