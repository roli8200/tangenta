#include "MediaArt.h"

#include "CurveObject.h"
#include "GroupObject.h"
#include "ImageObject.h"
#include "PathOps.h"

#include <QCoreApplication>
#include <QPainterPathStroker>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <vector>

namespace vek::media {

namespace {

// Rückgrat als dicht abgetasteter Linienzug mit Bogenlänge und geglätteten Normalen
class SpineSampler {
public:
    explicit SpineSampler(const QPainterPath& spine)
    {
        // Feiner unterteilen als Qt in mm von sich aus täte
        constexpr double scale = 20.0;
        const QList<QPolygonF> polys = spine.toSubpathPolygons(QTransform::fromScale(scale, scale));
        for (const QPolygonF& poly : polys) {
            for (const QPointF& q : poly) {
                const QPointF p = q / scale;
                if (m_points.isEmpty() || std::hypot(p.x() - m_points.last().x(), p.y() - m_points.last().y()) > 1e-6)
                    m_points.append(p);
            }
            break; // ein Teilpfad
        }
        if (m_points.size() < 2)
            return;
        m_length.reserve(m_points.size());
        m_length.append(0.0);
        QList<QPointF> segmentNormals;
        for (int i = 1; i < m_points.size(); ++i) {
            const QPointF d = m_points[i] - m_points[i - 1];
            const double len = std::hypot(d.x(), d.y());
            m_length.append(m_length.last() + len);
            segmentNormals.append(QPointF(-d.y(), d.x()) / len);
        }
        for (int i = 0; i < m_points.size(); ++i) {
            const QPointF a = segmentNormals[std::max(0, i - 1)];
            const QPointF b = segmentNormals[std::min(i, int(segmentNormals.size()) - 1)];
            QPointF n = a + b;
            const double len = std::hypot(n.x(), n.y());
            m_normals.append(len > 1e-9 ? n / len : b);
        }
    }

    bool isValid() const { return m_points.size() >= 2; }
    double length() const { return m_length.isEmpty() ? 0.0 : m_length.last(); }

    // Punkt bei Bogenlänge s, um offset quer verschoben (positiv: rechts der Laufrichtung,
    // bei y nach unten also unterhalb eines Strichs von links nach rechts)
    QPointF map(double s, double offset) const
    {
        const int last = int(m_points.size()) - 1;
        int i = int(std::upper_bound(m_length.begin(), m_length.end(), s) - m_length.begin()) - 1;
        i = std::clamp(i, 0, last - 1);
        const double seg = std::max(1e-12, m_length[i + 1] - m_length[i]);
        const double t = (s - m_length[i]) / seg; // ausserhalb 0..1: geradlinig verlängert
        const QPointF p = m_points[i] + (m_points[i + 1] - m_points[i]) * t;
        const double u = std::clamp(t, 0.0, 1.0);
        QPointF n = m_normals[i] * (1.0 - u) + m_normals[i + 1] * u;
        const double len = std::hypot(n.x(), n.y());
        if (len > 1e-9)
            n /= len;
        return p + n * offset;
    }

private:
    QList<QPointF> m_points;
    QList<double> m_length;
    QList<QPointF> m_normals;
};

QRectF boundsOf(const ArtItem& pieces)
{
    QRectF r;
    for (const ArtPiece& p : pieces)
        r = r.isNull() ? p.path.boundingRect() : r.united(p.path.boundingRect());
    return r;
}

ArtItem transformed(const ArtItem& pieces, const QTransform& m)
{
    ArtItem out;
    for (const ArtPiece& p : pieces)
        out.append(ArtPiece{m.map(p.path), p.color});
    return out;
}

QPainterPath rect(double x, double y, double w, double h)
{
    QPainterPath p;
    p.addRect(QRectF(x, y, w, h));
    return p;
}

QPainterPath circle(double x, double y, double r)
{
    QPainterPath p;
    p.addEllipse(QPointF(x, y), r, r);
    return p;
}

// Blatt mit Spitze bei tip, Ansatz bei base, Breite width
QPainterPath leaf(const QPointF& base, const QPointF& tip, double width)
{
    const QPointF d = tip - base;
    const QPointF n = QPointF(-d.y(), d.x()) / std::hypot(d.x(), d.y()) * width;
    const QPointF mid = (base + tip) / 2.0;
    QPainterPath p(base);
    p.quadTo(mid + n, tip);
    p.quadTo(mid - n, base);
    p.closeSubpath();
    return p;
}

struct BuiltInBrush {
    const char* name;
    ArtItem art;
    double aspect;
    bool repeat;
};

const std::vector<BuiltInBrush>& builtInBrushes()
{
    static const std::vector<BuiltInBrush> brushes = [] {
        std::vector<BuiltInBrush> all;
        auto add = [&all](const char* name, const ArtItem& raw, bool repeat) {
            double aspect = 1.0, height = 1.0;
            const ArtItem art = normalizedBrush(raw, &aspect, &height);
            all.push_back({name, art, aspect, repeat});
        };
        // Regenbogen: sechs Streifen
        {
            const Color colors[] = {Color(229, 57, 53), Color(251, 140, 0), Color(253, 216, 53),
                                    Color(67, 160, 71), Color(30, 136, 229), Color(142, 36, 170)};
            ArtItem art;
            for (int i = 0; i < 6; ++i)
                art.append({rect(0, i, 40, 1.02), colors[i]});
            add(QT_TRANSLATE_NOOP("media", "Regenbogen"), art, false);
        }
        // Pfeil: Schaft, Spitze und Federn
        {
            QPainterPath head;
            head.addPolygon(QPolygonF{QPointF(31, -4), QPointF(40, 0), QPointF(31, 4)});
            head.closeSubpath();
            QPainterPath feather;
            feather.addPolygon(QPolygonF{QPointF(0, -3), QPointF(4, -3), QPointF(7, -0.6), QPointF(7, 0.6),
                                         QPointF(4, 3), QPointF(0, 3), QPointF(3, 0)});
            feather.closeSubpath();
            add(QT_TRANSLATE_NOOP("media", "Pfeil"),
                ArtItem{{rect(2, -0.7, 30, 1.4), Color(55, 71, 79)}, {feather, Color(216, 67, 21)}, {head, Color(198, 40, 40)}}, false);
        }
        // Perlenkette: wiederholte Perlen an einem Faden
        {
            add(QT_TRANSLATE_NOOP("media", "Perlenkette"),
                ArtItem{{rect(0, -0.15, 5, 0.3), Color(120, 120, 120)},
                        {circle(2.5, 0, 2), Color(255, 179, 0)},
                        {circle(1.9, -0.6, 0.55), Color(255, 241, 200)}},
                true);
        }
        // Ranke: Stiel mit Blättern abwechselnd oben und unten
        {
            add(QT_TRANSLATE_NOOP("media", "Ranke"),
                ArtItem{{rect(0, -0.3, 10, 0.6), Color(56, 142, 60)},
                        {leaf(QPointF(2, 0), QPointF(5, -3.4), 1.0), Color(102, 187, 106)},
                        {leaf(QPointF(7, 0), QPointF(10, 3.4), 1.0), Color(124, 179, 66)}},
                true);
        }
        // Flamme: zulaufender Strich mit hellem Kern
        {
            QPainterPath outer(QPointF(0, 0));
            outer.cubicTo(QPointF(8, -4), QPointF(30, -3.5), QPointF(40, 0));
            outer.cubicTo(QPointF(30, 3.5), QPointF(8, 4), QPointF(0, 0));
            outer.closeSubpath();
            QPainterPath inner(QPointF(3, 0));
            inner.cubicTo(QPointF(10, -2), QPointF(26, -1.6), QPointF(34, 0));
            inner.cubicTo(QPointF(26, 1.6), QPointF(10, 2), QPointF(3, 0));
            inner.closeSubpath();
            add(QT_TRANSLATE_NOOP("media", "Flamme"), ArtItem{{outer, Color(230, 81, 0)}, {inner, Color(255, 202, 40)}}, false);
        }
        return all;
    }();
    return brushes;
}

QString encodePath(const QPainterPath& path)
{
    QStringList parts;
    auto num = [](double v) { return QString::number(v, 'g', 17); };
    for (int i = 0; i < path.elementCount(); ++i) {
        const QPainterPath::Element e = path.elementAt(i);
        if (e.type == QPainterPath::MoveToElement)
            parts << QStringLiteral("M") << num(e.x) << num(e.y);
        else if (e.type == QPainterPath::LineToElement)
            parts << QStringLiteral("L") << num(e.x) << num(e.y);
        else if (e.type == QPainterPath::CurveToElement && i + 2 < path.elementCount()) {
            const QPainterPath::Element c2 = path.elementAt(i + 1), end = path.elementAt(i + 2);
            parts << QStringLiteral("C") << num(e.x) << num(e.y) << num(c2.x) << num(c2.y) << num(end.x) << num(end.y);
            i += 2;
        }
    }
    return parts.join(QLatin1Char(' '));
}

QPainterPath decodePath(const QString& text, bool evenOdd)
{
    QPainterPath path;
    path.setFillRule(evenOdd ? Qt::OddEvenFill : Qt::WindingFill);
    const QStringList t = text.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    auto at = [&t](int i) { return i < t.size() ? t[i].toDouble() : 0.0; };
    for (int i = 0; i < t.size();) {
        const QString& op = t[i];
        if (op == QLatin1String("M")) {
            path.moveTo(at(i + 1), at(i + 2));
            i += 3;
        } else if (op == QLatin1String("L")) {
            path.lineTo(at(i + 1), at(i + 2));
            i += 3;
        } else if (op == QLatin1String("C")) {
            path.cubicTo(at(i + 1), at(i + 2), at(i + 3), at(i + 4), at(i + 5), at(i + 6));
            i += 7;
        } else {
            ++i;
        }
    }
    return path;
}

} // namespace

// --- Vorlagen aus Objekten --------------------------------------------------------------------

ArtItem piecesOf(const DrawObject& object)
{
    ArtItem out;
    if (const auto* group = node_cast<GroupObject>(&object)) {
        for (int i = 0; i < group->childCount(); ++i)
            if (const auto* child = node_cast<DrawObject>(group->child(i)))
                out += piecesOf(*child);
        return out;
    }
    if (node_cast<ImageObject>(&object))
        return out; // Bitmaps haben keine Vektorflächen
    if (const auto* curve = node_cast<CurveObject>(&object); curve && curve->media().hasPieces())
        return transformed(curve->mediaPieces(), curve->transform());
    const Style& s = object.style();
    const QPainterPath path = object.path();
    if (path.isEmpty())
        return out;
    bool fillable = true;
    if (const auto* curve = node_cast<CurveObject>(&object); curve && !curve->media().isActive()) {
        fillable = std::any_of(curve->subPaths().begin(), curve->subPaths().end(), [](const SubPath& sp) { return sp.closed; });
    }
    if (const auto* curve = node_cast<CurveObject>(&object); curve && curve->media().isActive()) {
        // Medienstrich: der Umriss trägt die Füllung
        if (const std::optional<Color> c = s.fill.representativeColor())
            out.append({path, *c});
        return out;
    }
    if (fillable && !s.fill.isNone()) {
        if (const std::optional<Color> c = s.fill.representativeColor())
            out.append({path, *c});
    }
    if (s.stroke && s.strokeWidth > 0) {
        QPainterPathStroker stroker;
        stroker.setWidth(s.strokeWidth);
        stroker.setJoinStyle(s.join == Style::Join::Round ? Qt::RoundJoin : s.join == Style::Join::Bevel ? Qt::BevelJoin : Qt::MiterJoin);
        stroker.setCapStyle(s.cap == Style::Cap::Round ? Qt::RoundCap : s.cap == Style::Cap::Square ? Qt::SquareCap : Qt::FlatCap);
        QPainterPath outline = stroker.createStroke(path);
        outline.setFillRule(Qt::WindingFill);
        out.append({outline, *s.stroke});
    }
    return out;
}

ArtItem normalizedBrush(const ArtItem& pieces, double* aspect, double* height)
{
    const QRectF b = boundsOf(pieces);
    const double w = std::max(b.width(), 1e-6), h = std::max(b.height(), 1e-6);
    if (aspect)
        *aspect = w / h;
    if (height)
        *height = h;
    QTransform m;
    m.scale(1.0 / w, 1.0 / h);
    m.translate(-b.left(), -b.center().y());
    return transformed(pieces, m);
}

ArtItem normalizedSprayItem(const ArtItem& pieces)
{
    const QRectF b = boundsOf(pieces);
    const double size = std::max({b.width(), b.height(), 1e-6});
    QTransform m;
    m.scale(1.0 / size, 1.0 / size);
    m.translate(-b.center().x(), -b.center().y());
    return transformed(pieces, m);
}

// --- Pinsel -----------------------------------------------------------------------------------

ArtItem brushPieces(const QPainterPath& spine, const ArtisticMedia& m)
{
    ArtItem out;
    double aspect = m.artAspect;
    bool repeat = m.brushRepeat;
    const ArtItem* art = nullptr;
    if (!m.art.isEmpty() && !m.art.first().isEmpty())
        art = &m.art.first();
    else
        art = &builtInBrush(0, &aspect, &repeat);
    const SpineSampler sampler(spine);
    const double total = sampler.length();
    if (!sampler.isValid() || total < 1e-6 || m.width <= 0)
        return out;
    int tiles = 1;
    if (repeat)
        tiles = std::clamp(int(std::lround(total / std::max(0.05, m.width * aspect))), 1, 2000);
    const double tileLength = total / tiles;
    // Gerade Kanten der Vorlage unterteilen, damit sie sich der Kurve anschmiegen
    const double step = std::clamp(total / 400.0, 0.05, 0.5);
    for (int k = 0; k < tiles; ++k) {
        const double start = k * tileLength;
        for (const ArtPiece& piece : *art) {
            QPainterPath mapped;
            mapped.setFillRule(piece.path.fillRule());
            for (const QPolygonF& poly : piece.path.toSubpathPolygons(QTransform::fromScale(tileLength, m.width))) {
                if (poly.size() < 2)
                    continue;
                QPolygonF result;
                for (int i = 0; i < poly.size(); ++i) {
                    const QPointF a = poly[i];
                    if (i > 0) {
                        const QPointF prev = poly[i - 1];
                        const int parts = std::min(200, int(std::abs(a.x() - prev.x()) / step));
                        for (int j = 1; j < parts; ++j) {
                            const QPointF q = prev + (a - prev) * (double(j) / parts);
                            result << sampler.map(start + q.x(), q.y());
                        }
                    }
                    result << sampler.map(start + a.x(), a.y());
                }
                mapped.addPolygon(result);
                mapped.closeSubpath();
            }
            out.append(ArtPiece{mapped, piece.color});
        }
    }
    return out;
}

int builtInBrushCount()
{
    return int(builtInBrushes().size());
}

QString builtInBrushName(int index)
{
    return QCoreApplication::translate("media", builtInBrushes().at(std::clamp(index, 0, builtInBrushCount() - 1)).name);
}

const ArtItem& builtInBrush(int index, double* aspect, bool* repeat)
{
    const BuiltInBrush& b = builtInBrushes().at(std::clamp(index, 0, builtInBrushCount() - 1));
    if (aspect)
        *aspect = b.aspect;
    if (repeat)
        *repeat = b.repeat;
    return b.art;
}

// --- Speichern --------------------------------------------------------------------------------

QJsonObject pieceToJson(const ArtPiece& piece)
{
    QJsonObject o{{QStringLiteral("d"), encodePath(piece.path)}, {QStringLiteral("color"), piece.color.toHex()}};
    if (piece.path.fillRule() == Qt::WindingFill)
        o[QStringLiteral("winding")] = true;
    return o;
}

ArtPiece pieceFromJson(const QJsonObject& json)
{
    ArtPiece p;
    p.path = decodePath(json.value(QStringLiteral("d")).toString(), !json.value(QStringLiteral("winding")).toBool());
    p.color = Color::fromHex(json.value(QStringLiteral("color")).toString()).value_or(Color(0, 0, 0));
    return p;
}

QJsonArray artToJson(const QList<ArtItem>& art)
{
    QJsonArray items;
    for (const ArtItem& item : art) {
        QJsonArray pieces;
        for (const ArtPiece& p : item)
            pieces << pieceToJson(p);
        items << pieces;
    }
    return items;
}

QList<ArtItem> artFromJson(const QJsonArray& json)
{
    QList<ArtItem> art;
    for (const QJsonValue& item : json) {
        ArtItem pieces;
        for (const QJsonValue& p : item.toArray())
            pieces << pieceFromJson(p.toObject());
        art << pieces;
    }
    return art;
}

QJsonObject toJson(const ArtisticMedia& m)
{
    static const char* kinds[] = {"none", "preset", "calligraphy", "pressure", "sprayer", "brush"};
    QJsonObject o{
        {QStringLiteral("kind"), QLatin1String(kinds[int(m.kind)])},
        {QStringLiteral("preset"), m.preset},
        {QStringLiteral("width"), m.width},
        {QStringLiteral("angle"), m.angle},
    };
    if (m.kind == ArtisticMedia::Kind::Pressure) {
        QJsonArray p;
        for (double v : m.pressure)
            p << v;
        o[QStringLiteral("pressure")] = p;
    }
    if (m.kind == ArtisticMedia::Kind::Sprayer) {
        static const char* orders[] = {"random", "sequential", "direction"};
        static const char* offsets[] = {"none", "alternate", "random"};
        o[QStringLiteral("spray")] = m.sprayList;
        o[QStringLiteral("spacing")] = m.spacing;
        o[QStringLiteral("dabCount")] = m.dabCount;
        o[QStringLiteral("sizeVariation")] = m.sizeVariation;
        o[QStringLiteral("randomRotate")] = m.randomRotate;
        o[QStringLiteral("order")] = QLatin1String(orders[int(m.order)]);
        o[QStringLiteral("offsetMode")] = QLatin1String(offsets[int(m.offsetMode)]);
        o[QStringLiteral("offset")] = m.offset;
        o[QStringLiteral("seed")] = qint64(m.seed);
    }
    if (m.kind == ArtisticMedia::Kind::Brush) {
        o[QStringLiteral("aspect")] = m.artAspect;
        o[QStringLiteral("repeat")] = m.brushRepeat;
    }
    if (!m.art.isEmpty())
        o[QStringLiteral("art")] = artToJson(m.art);
    if (!m.artName.isEmpty())
        o[QStringLiteral("artName")] = m.artName;
    return o;
}

ArtisticMedia fromJson(const QJsonObject& o)
{
    ArtisticMedia m;
    const QString kind = o.value(QStringLiteral("kind")).toString();
    m.kind = kind == QLatin1String("calligraphy") ? ArtisticMedia::Kind::Calligraphy
             : kind == QLatin1String("pressure")  ? ArtisticMedia::Kind::Pressure
             : kind == QLatin1String("sprayer")   ? ArtisticMedia::Kind::Sprayer
             : kind == QLatin1String("brush")     ? ArtisticMedia::Kind::Brush
                                                  : ArtisticMedia::Kind::Preset;
    for (const QJsonValue& v : o.value(QStringLiteral("pressure")).toArray())
        m.pressure << std::clamp(v.toDouble(), 0.0, 1.0);
    m.sprayList = std::clamp(o.value(QStringLiteral("spray")).toInt(), 0, pathops::sprayListCount() - 1);
    m.spacing = std::max(0.2, o.value(QStringLiteral("spacing")).toDouble(8.0));
    m.dabCount = std::clamp(o.value(QStringLiteral("dabCount")).toInt(1), 1, 50);
    m.sizeVariation = std::clamp(o.value(QStringLiteral("sizeVariation")).toDouble(40.0), 0.0, 100.0);
    m.randomRotate = o.value(QStringLiteral("randomRotate")).toBool(true);
    // Ältere Dateien: nur "randomOrder"
    const QString order = o.value(QStringLiteral("order")).toString();
    if (order == QLatin1String("sequential"))
        m.order = ArtisticMedia::Order::Sequential;
    else if (order == QLatin1String("direction"))
        m.order = ArtisticMedia::Order::Direction;
    else if (order.isEmpty() && !o.value(QStringLiteral("randomOrder")).toBool(true))
        m.order = ArtisticMedia::Order::Sequential;
    const QString offsetMode = o.value(QStringLiteral("offsetMode")).toString();
    m.offsetMode = offsetMode == QLatin1String("none")        ? ArtisticMedia::Offset::None
                   : offsetMode == QLatin1String("alternate") ? ArtisticMedia::Offset::Alternate
                                                              : ArtisticMedia::Offset::Random;
    m.offset = std::clamp(o.value(QStringLiteral("offset")).toDouble(30.0), 0.0, 1000.0);
    m.seed = quint32(o.value(QStringLiteral("seed")).toInteger(1));
    m.preset = std::clamp(o.value(QStringLiteral("preset")).toInt(), 0, pathops::mediaPresetCount() - 1);
    m.width = std::max(0.05, o.value(QStringLiteral("width")).toDouble(10.0));
    m.angle = o.value(QStringLiteral("angle")).toDouble();
    m.artAspect = std::max(1e-3, o.value(QStringLiteral("aspect")).toDouble(4.0));
    m.brushRepeat = o.value(QStringLiteral("repeat")).toBool(false);
    m.art = artFromJson(o.value(QStringLiteral("art")).toArray());
    m.artName = o.value(QStringLiteral("artName")).toString();
    return m;
}

} // namespace vek::media
