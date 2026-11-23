#include "CdrExport.h"

#include <QBuffer>
#include <QColor>
#include <QFile>
#include <QGradient>
#include <QLineF>
#include <QObject>
#include <QPainter>
#include <QTransform>
#include <QUuid>
#include <QtEndian>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <vector>

#include <miniz.h>

namespace vek::cdr {

namespace {

constexpr double kUnitsPerMm = 10000.0; // 1/254000 Zoll
constexpr quint32 kVersion = 1300;
// Grafikstil der Vorlage, auf den die Kurven verweisen
constexpr quint32 kGraphicStyle = 0x05783398;

void put16(QByteArray& b, quint16 v)
{
    char c[2];
    qToLittleEndian(v, c);
    b.append(c, 2);
}

void put32(QByteArray& b, quint32 v)
{
    char c[4];
    qToLittleEndian(v, c);
    b.append(c, 4);
}

void putS32(QByteArray& b, double v)
{
    put32(b, quint32(qint32(std::lround(v))));
}

void putDouble(QByteArray& b, double v)
{
    char c[8];
    qToLittleEndian(v, c);
    b.append(c, 8);
}

void patch16(QByteArray& b, int off, quint16 v)
{
    qToLittleEndian(v, b.data() + off);
}

void patch32(QByteArray& b, int off, quint32 v)
{
    qToLittleEndian(v, b.data() + off);
}

QByteArray u32(quint32 v)
{
    QByteArray b;
    put32(b, v);
    return b;
}

// --- RIFF-Baum ----------------------------------------------------------------

struct Chunk {
    QByteArray id;   // vier Zeichen
    QByteArray kind; // Listentyp bei RIFF/LIST, sonst leer
    QByteArray data; // Inhalt eines Blocks; bei 'stlt' die rohe Liste
    std::vector<Chunk> children;

    static Chunk leaf(const char* id, const QByteArray& data) { return Chunk{id, {}, data, {}}; }
    static Chunk list(const char* kind, std::vector<Chunk> children = {}) { return Chunk{"LIST", kind, {}, std::move(children)}; }

    bool isList() const { return !kind.isEmpty(); }
    Chunk* findList(const char* k)
    {
        for (Chunk& c : children)
            if (c.isList() && c.kind == k)
                return &c;
        return nullptr;
    }
    Chunk* findLeaf(const char* i)
    {
        for (Chunk& c : children)
            if (!c.isList() && c.id == i)
                return &c;
        return nullptr;
    }
};

// Ungepackter RIFF-Baum (die Vorlage); false bei kaputten Längen
bool parse(const QByteArray& d, qsizetype off, qsizetype end, std::vector<Chunk>& out)
{
    while (off + 8 <= end) {
        Chunk c;
        c.id = d.mid(off, 4);
        const qsizetype len = qFromLittleEndian<quint32>(d.constData() + off + 4);
        const qsizetype body = off + 8;
        if (len > end - body)
            return false;
        if (c.id == "RIFF" || c.id == "LIST") {
            if (len < 4)
                return false;
            c.kind = d.mid(body, 4);
            if (c.kind == "stlt")
                c.data = d.mid(body + 4, len - 4);
            else if (!parse(d, body + 4, body + len, c.children))
                return false;
        } else {
            c.data = d.mid(body, len);
        }
        out.push_back(std::move(c));
        off = body + len + (len & 1);
    }
    return true;
}

// Gepackter Abschnitt: Kopf "CPng" 1 4, danach zlib
QByteArray cpng(const QByteArray& raw)
{
    QByteArray b("CPng");
    put16(b, 1);
    put16(b, 4);
    if (raw.isEmpty())
        b += QByteArray::fromHex("78da030000000001");
    else
        b += qCompress(raw, 9).mid(4); // ohne Qts Längenvorsatz
    return b;
}

QByteArray wrap(const QByteArray& id, const QByteArray& body, std::vector<quint32>* table)
{
    // In gepackten Abschnitten steht statt der Länge ihr Index in der Längentabelle
    QByteArray b = id;
    if (table) {
        put32(b, quint32(table->size()));
        table->push_back(quint32(body.size()));
    } else {
        put32(b, quint32(body.size()));
    }
    b += body;
    if (body.size() & 1)
        b.append('\0');
    return b;
}

QByteArray serialize(const Chunk& c, std::vector<quint32>* table)
{
    if (!c.isList())
        return wrap(c.id, c.data, table);
    if (c.kind == "cmpr") {
        std::vector<quint32> lengths;
        QByteArray inner;
        for (const Chunk& ch : c.children)
            inner += serialize(ch, &lengths);
        QByteArray blocks;
        for (quint32 v : lengths)
            put32(blocks, v);
        const QByteArray cdata = cpng(inner), cblocks = cpng(blocks);
        QByteArray body("cmpr");
        put32(body, quint32(cdata.size()));
        put32(body, quint32(inner.size()));
        put32(body, quint32(cblocks.size()));
        put32(body, quint32(blocks.size()));
        return wrap(c.id, body + cdata + cblocks, table);
    }
    if (table) {
        // Index der Liste vor denen ihrer Kinder
        const std::size_t idx = table->size();
        table->push_back(0);
        QByteArray inner = c.kind + c.data;
        for (const Chunk& ch : c.children)
            inner += serialize(ch, table);
        (*table)[idx] = quint32(inner.size());
        QByteArray b = c.id;
        put32(b, quint32(idx));
        b += inner;
        if (inner.size() & 1)
            b.append('\0');
        return b;
    }
    QByteArray body = c.kind + c.data;
    for (const Chunk& ch : c.children)
        body += serialize(ch, nullptr);
    return wrap(c.id, body, nullptr);
}

// Objektdaten: Kopf, Versätze, Endmarke, Typen rückwärts, Daten
QByteArray loda(quint32 chunkType, const std::vector<std::pair<quint32, QByteArray>>& args)
{
    const quint32 n = quint32(args.size());
    const quint32 startArgs = 20, startTypes = startArgs + 4 * n + 4, dataStart = startTypes + 4 * n;
    QByteArray offsets, body;
    for (const auto& [type, data] : args) {
        put32(offsets, dataStart + quint32(body.size()));
        body += data;
    }
    const quint32 total = dataStart + quint32(body.size());
    QByteArray b;
    for (quint32 v : {total, n, startArgs, startTypes, chunkType})
        put32(b, v);
    b += offsets;
    put32(b, total);
    for (auto it = args.rbegin(); it != args.rend(); ++it)
        put32(b, it->first);
    return b + body;
}

QByteArray trfdIdentity(bool x6)
{
    QByteArray b;
    for (quint32 v : {0x60u, 1u, 0x10u, 0xffffu, 0x18u, 0x60u})
        put32(b, v);
    put32(b, kVersion);
    put32(b, x6 ? 0 : 0x40);
    put16(b, 8);
    b.append(6, '\0');
    for (double v : {1.0, 0.0, 0.0, 0.0, 1.0, 0.0})
        putDouble(b, v);
    b.append(8, '\0');
    return b;
}

QByteArray ftilIdentity()
{
    QByteArray b;
    for (double v : {1.0, 0.0, 0.0, 0.0, 1.0, 0.0})
        putDouble(b, v);
    return b;
}

// RGB-Farbe: Modell 5, Palette 5, dann B G R und ein Zusatzbyte
QByteArray colorBytes(const QColor& color, char extra = 0)
{
    const QColor c = color.toRgb();
    QByteArray b;
    put16(b, 5);
    put16(b, 5);
    put32(b, 0);
    b.append(char(c.blue()));
    b.append(char(c.green()));
    b.append(char(c.red()));
    b.append(extra);
    return b;
}

// Version 16 hängt an jede Farbe 26 Byte an (Kennung 0x1007, Rest leer)
QByteArray colorBytes6(const QColor& color)
{
    return colorBytes(color) + QByteArray::fromHex("0710") + QByteArray(24, '\0');
}

QByteArray fildNone(quint32 id, bool x6)
{
    return u32(id) + QByteArray::fromHex(x6 ? "140500000200000000000000000000000000" : "140500000a00000000000000000000000000");
}

QByteArray fildSolid(quint32 id, const QColor& color, bool x6)
{
    if (x6)
        return u32(id) + QByteArray::fromHex("140500004d0000000100140500003b000000010c000000") + colorBytes6(color)
            + QByteArray::fromHex("000000000000000040a5ae023c00000000000000000000000000000000000000");
    return u32(id) + QByteArray::fromHex("14050000400000000100140500002e000000010c000000") + colorBytes(color)
        + QByteArray::fromHex("0000000000000000000000000040a5ae023c00000000000000000000000000000000000000");
}

struct GradientStop {
    int position; // 0..100
    QColor color;
};

QByteArray fildGradient(quint32 id, int type, double angleDeg, int edge, int cx, int cy, const std::vector<GradientStop>& stops, bool x6)
{
    QByteArray b = u32(id);
    put32(b, kVersion);
    put32(b, 0); // Länge, unten
    put16(b, 2);
    put32(b, kVersion);
    put32(b, 0); // Länge, unten
    b.append(char(type));
    b += QByteArray::fromHex("00000000000000000040a5ae023c000000");
    put16(b, quint16(qint16(edge)));
    putS32(b, angleDeg * 1e6);
    put32(b, quint32(qint32(cx)));
    put32(b, quint32(qint32(cy)));
    b.append(2, '\0');
    put32(b, 0x10000 | (stops.size() > 2 ? 3 : 0));
    b.append(char(50)); // Mittelpunkt
    b.append('\0');
    put16(b, quint16(stops.size()));
    for (const GradientStop& s : stops) {
        if (x6) {
            b += QByteArray::fromHex("010c000000") + colorBytes6(s.color);
        } else {
            b += QByteArray::fromHex("010c000000") + colorBytes(s.color, 100);
            b.append(5, '\0');
        }
        put16(b, quint16(s.position));
    }
    b.append(16, '\0');
    // Längen der beiden Abschnitte ab Kopf bzw. ab Art
    patch32(b, 8, quint32(b.size() - (x6 ? 20 : 12)));
    patch32(b, 18, quint32(b.size() - (x6 ? 38 : 30)));
    return b;
}

// Kontur der Vorlage (Haarlinie, schwarz); Art bei 24, Enden 26, Ecken 28, Breite 30,
// Farbe 88, Strichmuster ab 116 (Anzahl, dann Längen in Linienbreiten)
const char* const kOutlineTemplate =
    "70a59205030000000400000040a5ae020100000082000000020000000000fa020000640000000000000000000000f03f00000000000000000000"
    "0000000000000000000000000000000000000000f03f0000000000000000020005000000000000000064000000000000000040a5ae023c0000"
    "000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000";

// Breite 0 = Haarlinie wie in der Vorlage
quint32 outlineWidth(const QPen& pen)
{
    const double width = pen.widthF() > 0 ? pen.widthF() * kUnitsPerMm : 762.0;
    return quint32(std::max<long>(1, std::lround(width)));
}

QColor penColor(const QPen& pen)
{
    if (const QGradient* g = pen.brush().gradient(); g && !g->stops().isEmpty())
        return g->stops().first().second;
    return pen.color();
}

// Version 16: nach dem ersten Abschnitt ein Farbabschnitt (Art 2, 43 Byte); alles Weitere wie
// in Version 13, um 51 Byte verschoben
QByteArray outline(quint32 id, const std::optional<QPen>& pen, bool x6)
{
    QByteArray b = QByteArray::fromHex(kOutlineTemplate);
    if (x6) {
        b.replace(12, 4, QByteArray::fromHex("404b4c00"));
        QByteArray block = u32(2) + u32(43) + QByteArray::fromHex("010c000000") + colorBytes6(QColor(Qt::black));
        b.insert(16, block);
    }
    const int shift = x6 ? 51 : 0;
    patch32(b, 0, id);
    if (!pen) {
        patch16(b, 24 + shift, 1); // keine Kontur
        patch32(b, 30 + shift, 0);
        return b;
    }
    const QList<qreal> dashes = pen->style() == Qt::SolidLine ? QList<qreal>() : pen->dashPattern();
    const quint16 cap = pen->capStyle() == Qt::RoundCap ? 1 : pen->capStyle() == Qt::SquareCap ? 2 : 0;
    const quint16 join = pen->joinStyle() == Qt::RoundJoin ? 1 : pen->joinStyle() == Qt::BevelJoin ? 2 : 0;
    patch16(b, 24 + shift, dashes.isEmpty() ? 2 : 6);
    patch16(b, 26 + shift, cap);
    patch16(b, 28 + shift, join);
    patch32(b, 30 + shift, outlineWidth(*pen));
    const QColor color = penColor(*pen);
    b.replace(88 + shift, 12, colorBytes(color));
    if (x6)
        b.replace(29, 12, colorBytes(color));
    const int n = int(std::min<qsizetype>(dashes.size(), 10));
    patch16(b, 116 + shift, quint16(n));
    for (int i = 0; i < n; ++i)
        patch16(b, 118 + shift + 2 * i, quint16(std::clamp<long>(std::lround(dashes[i]), 1, 0xffff)));
    return b;
}

// Begrenzung in Dateieinheiten (y nach oben)
struct Box {
    double x0 = 0, top = 0, x1 = 0, bottom = 0;
    bool valid = false;

    void add(const QPointF& p)
    {
        if (!valid) {
            x0 = x1 = p.x();
            top = bottom = p.y();
            valid = true;
            return;
        }
        x0 = std::min(x0, p.x());
        x1 = std::max(x1, p.x());
        top = std::max(top, p.y());
        bottom = std::min(bottom, p.y());
    }
    void add(const Box& o)
    {
        if (!o.valid)
            return;
        add(QPointF(o.x0, o.top));
        add(QPointF(o.x1, o.bottom));
    }
    QByteArray bytes() const
    {
        QByteArray b;
        for (double v : {x0, top, x1, bottom})
            putS32(b, v);
        return b;
    }
};

// Farbe eines Qt-Verlaufs an der Stelle t (0..1, ausserhalb nach Fortsetzungsart)
QColor colorAt(const QGradientStops& stops, QGradient::Spread spread, double t)
{
    if (spread == QGradient::RepeatSpread) {
        t -= std::floor(t);
    } else if (spread == QGradient::ReflectSpread) {
        t = std::fmod(std::abs(t), 2.0);
        if (t > 1.0)
            t = 2.0 - t;
    }
    if (t <= stops.first().first)
        return stops.first().second;
    if (t >= stops.last().first)
        return stops.last().second;
    for (qsizetype i = 1; i < stops.size(); ++i) {
        if (t <= stops[i].first) {
            const double span = stops[i].first - stops[i - 1].first;
            const double f = span > 0 ? (t - stops[i - 1].first) / span : 1.0;
            const QColor a = stops[i - 1].second.toRgb(), b = stops[i].second.toRgb();
            auto mix = [f](int x, int y) { return int(std::lround(x + (y - x) * f)); };
            return QColor(mix(a.red(), b.red()), mix(a.green(), b.green()), mix(a.blue(), b.blue()), mix(a.alpha(), b.alpha()));
        }
    }
    return stops.last().second;
}

// Stopps der Datei (0..100) aus einer Farbfunktion über u = 0..1; an den Stellen candidates
// liegen die Stopps des Qt-Verlaufs
std::vector<GradientStop> sampleStops(const std::function<QColor(double)>& colorFor, std::vector<double> candidates, bool dense)
{
    candidates.push_back(0.0);
    candidates.push_back(1.0);
    if (dense)
        for (int i = 1; i < 20; ++i)
            candidates.push_back(i / 20.0);
    std::map<int, QColor> byPosition;
    for (double u : candidates) {
        if (!std::isfinite(u) || u < 0.0 || u > 1.0)
            continue;
        const int pos = int(std::lround(u * 100.0));
        byPosition.emplace(pos, colorFor(pos / 100.0));
    }
    std::vector<GradientStop> stops;
    for (const auto& [pos, color] : byPosition)
        stops.push_back({pos, color});
    return stops;
}

// Füllung unabhängig von der Dateiversion
struct FillSpec {
    enum class Kind { None, Solid, Gradient };
    Kind kind = Kind::None;
    QColor color;
    int type = 1; // Verlauf: 1 linear, 2 radial, 3 konisch
    double angle = 0.0;
    int cx = 0, cy = 0;
    std::vector<GradientStop> stops;

    static FillSpec solid(const QColor& c) { return FillSpec{Kind::Solid, c, 1, 0.0, 0, 0, {}}; }
    static FillSpec gradient(int type, double angle, int cx, int cy, std::vector<GradientStop> stops)
    {
        return FillSpec{Kind::Gradient, QColor(), type, angle, cx, cy, std::move(stops)};
    }
};

QByteArray encodeFill(quint32 id, const FillSpec& f, bool x6)
{
    switch (f.kind) {
    case FillSpec::Kind::Solid:
        return fildSolid(id, f.color, x6);
    case FillSpec::Kind::Gradient:
        return fildGradient(id, f.type, f.angle, 0, f.cx, f.cy, f.stops, x6);
    case FillSpec::Kind::None:
        break;
    }
    return fildNone(id, x6);
}

QString colorString(const QColor& color)
{
    const QColor c = color.toRgb();
    return QStringLiteral("RGB255,USER,%1,%2,%3,100,00000000-0000-0000-0000-000000000000").arg(c.red()).arg(c.green()).arg(c.blue());
}

// Version 16: Stil des Objekts als JSON (wie das Programm es selbst schreibt), Länge in Zeichen davor
QByteArray styleJson(const FillSpec& f, const std::optional<QPen>& pen)
{
    QStringList fill;
    switch (f.kind) {
    case FillSpec::Kind::Solid:
        fill << QStringLiteral("\"primaryColor\": \"%1\"").arg(colorString(f.color)) << QStringLiteral("\"type\": \"1\"");
        break;
    case FillSpec::Kind::Gradient:
        fill << QStringLiteral("\"angle\": \"%1\"").arg(qRound(f.angle)) << QStringLiteral("\"fountainType\": \"%1\"").arg(f.type)
             << QStringLiteral("\"padding\": \"0\"") << QStringLiteral("\"primaryColor\": \"%1\"").arg(colorString(f.stops.front().color))
             << QStringLiteral("\"rateValue\": \"50\"")
             << QStringLiteral("\"secondaryColor\": \"%1\"").arg(colorString(f.stops.back().color))
             << QStringLiteral("\"type\": \"2\"") << QStringLiteral("\"xCenterOffset\": \"%1\"").arg(f.cx)
             << QStringLiteral("\"yCenterOffset\": \"%1\"").arg(f.cy);
        break;
    case FillSpec::Kind::None:
        fill << QStringLiteral("\"type\": \"0\"");
        break;
    }
    QStringList outline;
    if (pen)
        outline << QStringLiteral("\"color\": \"%1\"").arg(colorString(penColor(*pen)))
                << QStringLiteral("\"width\": \"%1\"").arg(outlineWidth(*pen));
    else
        outline << QStringLiteral("\"width\": \"0\"");
    const QString json = QStringLiteral("{\n  \"fill\": {\n    ") + fill.join(QStringLiteral(",\n    "))
                         + QStringLiteral("\n  },\n  \"outline\": {\n    ") + outline.join(QStringLiteral(",\n    "))
                         + QStringLiteral("\n  }\n}");
    QByteArray b = u32(1) + u32(quint32(json.size()));
    for (QChar ch : json)
        put16(b, ch.unicode());
    return b;
}

} // namespace

struct Writer::Private {
    struct Item {
        Chunk chunk;
        Box box;
    };
    struct Layer {
        QString name;
        std::vector<Item> items;
    };
    struct Page {
        std::vector<Layer> layers;
        std::size_t current = 0;
    };

    QSizeF size;
    bool x6 = false;
    std::vector<Page> pages;
    std::vector<std::vector<Item>> groups;
    std::vector<QByteArray> fills, outlines;
    quint32 nextId = 0x7a000000;
    quint32 serial = 1000;
    quint32 objectNumber = 1; // Version 16: fortlaufende Objektnummer
    QImage preview;

    quint32 id() { return nextId += 0x20; }

    std::vector<Item>& target()
    {
        if (!groups.empty())
            return groups.back();
        if (pages.empty())
            pages.emplace_back();
        Page& page = pages.back();
        if (page.layers.empty())
            page.layers.push_back({QObject::tr("Ebene 1"), {}});
        return page.layers[page.current].items;
    }

    QPointF toFile(const QPointF& mm) const
    {
        return QPointF((mm.x() - size.width() / 2.0) * kUnitsPerMm, (size.height() / 2.0 - mm.y()) * kUnitsPerMm);
    }

    // Koordinaten samt Punktarten; leer, wenn nichts zu zeichnen ist
    QByteArray coordinates(const QPainterPath& path, bool filled, Box& box) const
    {
        std::vector<QPointF> points;
        std::vector<quint8> types;
        std::size_t first = 0;
        QPointF start;
        auto finish = [&] {
            const std::size_t count = points.size() - first;
            if (count == 0)
                return;
            if (count == 1) { // einzelner Startpunkt
                points.pop_back();
                types.pop_back();
                return;
            }
            const QPointF last = points.back();
            bool closed = std::abs(last.x() - start.x()) < 0.5 && std::abs(last.y() - start.y()) < 0.5;
            if (!closed && filled) {
                points.push_back(start);
                types.push_back(0x44);
                closed = true;
            }
            if (closed) {
                types[first] |= 0x08;
                types.back() = quint8((types.back() & ~0x04) | 0x08);
            }
        };
        for (int i = 0; i < path.elementCount(); ++i) {
            const QPainterPath::Element e = path.elementAt(i);
            const QPointF p = toFile(QPointF(e.x, e.y));
            switch (e.type) {
            case QPainterPath::MoveToElement:
                finish();
                first = points.size();
                start = p;
                points.push_back(p);
                types.push_back(0x04);
                break;
            case QPainterPath::LineToElement:
                points.push_back(p);
                types.push_back(0x44);
                break;
            case QPainterPath::CurveToElement:
                if (i + 2 >= path.elementCount())
                    return {};
                points.push_back(p);
                points.push_back(toFile(path.elementAt(i + 1)));
                points.push_back(toFile(path.elementAt(i + 2)));
                types.insert(types.end(), {0xC0, 0xC0, 0x84});
                i += 2;
                break;
            case QPainterPath::CurveToDataElement:
                break;
            }
        }
        finish();
        if (points.empty() || points.size() > 0xffff)
            return {};
        QByteArray b;
        put16(b, quint16(points.size()));
        put16(b, 0);
        for (const QPointF& p : points) {
            putS32(b, p.x());
            putS32(b, p.y());
            box.add(QPointF(std::round(p.x()), std::round(p.y())));
        }
        for (quint8 t : types)
            b.append(char(t));
        if (!x6)
            b.append(8, '\0');
        return b;
    }

    // Füllung; Verläufe bezogen auf das Begrenzungsrechteck r der Form (mm), wie der Import sie liest
    FillSpec fill(const QBrush& brush, const QRectF& r) const
    {
        const QGradient* g = brush.gradient();
        if (!g || g->stops().isEmpty())
            return FillSpec::solid(brush.color());
        const QGradientStops stops = g->stops();
        if (r.width() <= 0 || r.height() <= 0 || stops.size() == 1)
            return FillSpec::solid(stops.last().second);
        const bool objectMode = g->coordinateMode() == QGradient::ObjectBoundingMode || g->coordinateMode() == QGradient::ObjectMode;
        const QTransform t = (objectMode ? QTransform(r.width(), 0, 0, r.height(), r.left(), r.top()) : QTransform()) * brush.transform();
        const double scale = std::sqrt(std::abs(t.determinant()));
        const bool dense = g->spread() != QGradient::PadSpread;
        const QPointF c = r.center();
        auto offsets = [&](const QPointF& m, int& cx, int& cy) {
            cx = int(std::lround((m.x() - c.x()) / (r.width() / 2.0) * 100.0));
            cy = int(std::lround(-(m.y() - c.y()) / (r.height() / 2.0) * 100.0));
            return QPointF(c.x() + cx / 100.0 * r.width() / 2.0, c.y() - cy / 100.0 * r.height() / 2.0);
        };
        switch (g->type()) {
        case QGradient::LinearGradient: {
            const auto* lg = static_cast<const QLinearGradient*>(g);
            const QPointF p0 = t.map(lg->start()), p1 = t.map(lg->finalStop());
            const double len = QLineF(p0, p1).length();
            if (len < 1e-9)
                return FillSpec::solid(stops.last().second);
            const QPointF dir = (p1 - p0) / len;
            const double half = (std::abs(r.width() * dir.x()) + std::abs(r.height() * dir.y())) / 2.0;
            const double base = QPointF::dotProduct(c - p0, dir);
            // u = 0..1 über die Ausdehnung in Richtung des Winkels
            auto colorFor = [&](double u) { return colorAt(stops, g->spread(), (base + (2.0 * u - 1.0) * half) / len); };
            std::vector<double> candidates;
            for (const auto& s : stops)
                candidates.push_back(((s.first * len - base) / half + 1.0) / 2.0);
            const double angle = qRadiansToDegrees(std::atan2(-dir.y(), dir.x()));
            return FillSpec::gradient(1, angle, 0, 0, sampleStops(colorFor, candidates, dense));
        }
        case QGradient::RadialGradient: {
            const auto* rg = static_cast<const QRadialGradient*>(g);
            int cx = 0, cy = 0;
            const QPointF m = offsets(t.map(rg->center()), cx, cy);
            const double radius = rg->radius() * scale;
            double reach = 0.0; // bis zur entferntesten Ecke
            for (const QPointF& corner : {r.topLeft(), r.topRight(), r.bottomLeft(), r.bottomRight()})
                reach = std::max(reach, QLineF(m, corner).length());
            if (radius < 1e-9 || reach < 1e-9)
                return FillSpec::solid(stops.last().second);
            // Stopps der Datei von aussen (0) nach innen (100)
            auto colorFor = [&](double u) { return colorAt(stops, g->spread(), (1.0 - u) * reach / radius); };
            std::vector<double> candidates;
            for (const auto& s : stops)
                candidates.push_back(1.0 - s.first * radius / reach);
            return FillSpec::gradient(2, 0.0, cx, cy, sampleStops(colorFor, candidates, dense));
        }
        case QGradient::ConicalGradient: {
            const auto* cg = static_cast<const QConicalGradient*>(g);
            int cx = 0, cy = 0;
            const QPointF center = t.map(cg->center());
            offsets(center, cx, cy);
            const double a = qDegreesToRadians(cg->angle());
            const QPointF dir = t.map(cg->center() + QPointF(std::cos(a), -std::sin(a))) - center;
            auto colorFor = [&](double u) { return colorAt(stops, QGradient::PadSpread, u); };
            std::vector<double> candidates;
            for (const auto& s : stops)
                candidates.push_back(s.first);
            return FillSpec::gradient(3, qRadiansToDegrees(std::atan2(-dir.y(), dir.x())), cx, cy, sampleStops(colorFor, candidates, false));
        }
        default:
            return FillSpec::solid(stops.last().second);
        }
    }

    static Chunk object(const char* kind, const QByteArray& flags, const Box& box, quint32 serial, std::vector<Chunk> lgob)
    {
        return Chunk::list(kind, {Chunk::leaf("flgs", QByteArray::fromHex(flags)), Chunk::leaf("bbox", box.bytes()),
                                  Chunk::leaf("obbx", box.bytes() + box.bytes()), Chunk::leaf("usdn", u32(serial)),
                                  Chunk::list("lgob", std::move(lgob))});
    }

    static QByteArray utf16z(const QString& text)
    {
        QByteArray b;
        for (QChar ch : text)
            put16(b, ch.unicode());
        return b + QByteArray(2, '\0');
    }

    static QByteArray layerLoda(const QString& name)
    {
        return loda(0, {{0x2ee0, u32(0)}, {0x7d0, QByteArray::fromHex("010c0000000500050000000000ff804f000000000000")}, {0x3e8, utf16z(name)}});
    }

    // Die Datei führt Objekte von vorne nach hinten: das erste liegt zuoberst
    static void appendFrontToBack(Chunk& list, const std::vector<Item>& items)
    {
        for (auto it = items.rbegin(); it != items.rend(); ++it)
            list.children.push_back(it->chunk);
    }

    // Ebenen des Dokuments: alle Namen in der Reihenfolge ihres Auftretens (unterste zuerst)
    QStringList layerNames() const
    {
        QStringList names;
        for (const auto& page : pages)
            for (const auto& layer : page.layers)
                if (!names.contains(layer.name))
                    names << layer.name;
        if (names.isEmpty())
            names << QObject::tr("Ebene 1");
        return names;
    }

    QByteArray finishX6(QString* error);

    QByteArray displayImage() const
    {
        // 96 × 96, 8 Bit mit Farbtabelle (Farbwürfel 6 × 6 × 6 und Grautöne), Zeilen von unten
        QList<QRgb> table;
        for (int r = 0; r < 6; ++r)
            for (int g = 0; g < 6; ++g)
                for (int b = 0; b < 6; ++b)
                    table << qRgb(r * 51, g * 51, b * 51);
        for (int i = 0; table.size() < 256; ++i)
            table << qRgb(i * 255 / 39, i * 255 / 39, i * 255 / 39);
        QImage canvas(96, 96, QImage::Format_RGB32);
        canvas.fill(Qt::white);
        if (!preview.isNull()) {
            QPainter p(&canvas);
            p.setRenderHint(QPainter::SmoothPixmapTransform);
            const QImage scaled = preview.scaled(96, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            p.drawImage((96 - scaled.width()) / 2, (96 - scaled.height()) / 2, scaled);
        }
        const QImage indexed = canvas.convertToFormat(QImage::Format_Indexed8, table);
        QByteArray b;
        put32(b, 8);
        for (quint32 v : {40u, 96u, 96u})
            put32(b, v);
        put16(b, 1);
        put16(b, 8);
        for (quint32 v : {0u, 96u * 96u, 3779u, 3779u, 256u, 0u})
            put32(b, v);
        for (QRgb c : table) {
            b.append(char(qBlue(c)));
            b.append(char(qGreen(c)));
            b.append(char(qRed(c)));
            b.append('\0');
        }
        for (int y = 95; y >= 0; --y)
            b.append(reinterpret_cast<const char*>(indexed.constScanLine(y)), 96);
        return b;
    }
};

Writer::Writer(const QSizeF& pageMm, Version version)
    : d(std::make_unique<Private>())
{
    d->size = pageMm;
    d->x6 = version == Version::X6;
}

Writer::~Writer() = default;

void Writer::beginPage()
{
    d->groups.clear();
    d->pages.emplace_back();
}

void Writer::beginLayer(const QString& name)
{
    d->groups.clear();
    if (d->pages.empty())
        d->pages.emplace_back();
    Private::Page& page = d->pages.back();
    // gleichnamige Ebenen (Hauptseite und Seite) zusammenlegen
    auto it = std::find_if(page.layers.begin(), page.layers.end(), [&](const Private::Layer& l) { return l.name == name; });
    if (it == page.layers.end())
        it = page.layers.insert(page.layers.end(), {name, {}});
    page.current = std::size_t(it - page.layers.begin());
}

void Writer::beginGroup()
{
    d->target();
    d->groups.emplace_back();
}

void Writer::endGroup()
{
    if (d->groups.empty())
        return;
    std::vector<Private::Item> children = std::move(d->groups.back());
    d->groups.pop_back();
    if (children.empty())
        return;
    if (children.size() == 1) {
        d->target().push_back(std::move(children.front()));
        return;
    }
    Box box;
    for (const auto& c : children)
        box.add(c.box);
    std::vector<Chunk> lgob;
    if (d->x6)
        lgob = {Chunk::leaf("loda", loda(0, {{0x4ae2, QByteArray::fromHex("010000000100000000000000")}, {0x64, u32(++d->objectNumber)}})),
                Chunk::list("trfl", {Chunk::leaf("trfd", trfdIdentity(true))})};
    else
        lgob = {Chunk::leaf("loda", loda(0, {{0x2ee0, u32(0)}}))};
    Chunk group = Private::object("grp ", "00000010", box, ++d->serial, std::move(lgob));
    Private::appendFrontToBack(group, children);
    d->target().push_back({std::move(group), box});
}

void Writer::addPath(const QPainterPath& path, const std::optional<QBrush>& fill, const std::optional<QPen>& stroke)
{
    const bool filled = fill && fill->style() != Qt::NoBrush;
    const bool stroked = stroke && stroke->style() != Qt::NoPen;
    if (path.isEmpty() || (!filled && !stroked))
        return;
    Box box;
    const QByteArray coords = d->coordinates(path, filled, box);
    if (coords.isEmpty())
        return;
    const FillSpec spec = filled ? d->fill(*fill, path.boundingRect()) : FillSpec();
    const std::optional<QPen> pen = stroked ? stroke : std::nullopt;
    const quint32 fid = d->id();
    d->fills.push_back(encodeFill(fid, spec, d->x6));
    const quint32 oid = d->id();
    d->outlines.push_back(outline(oid, pen, d->x6));
    std::vector<std::pair<quint32, QByteArray>> args;
    if (d->x6)
        args = {{0x4ae2, QByteArray::fromHex("010000000100000000000000")}, {0x2ee0, u32(0)}, {0xc9, styleJson(spec, pen)},
                {0x64, u32(++d->objectNumber)}, {0x1e, coords}, {0x14, u32(fid)}, {0xa, u32(oid)}};
    else
        args = {{0x2ee0, u32(0)}, {0x3f2, u32(0)}, {0xc8, u32(kGraphicStyle)}, {0x64, u32(d->id())},
                {0x1e, coords}, {0x14, u32(fid)}, {0xa, u32(oid)}};
    std::vector<Chunk> lgob{Chunk::leaf("loda", loda(3, args))};
    if (filled)
        lgob.push_back(Chunk::leaf("ftil", ftilIdentity()));
    lgob.push_back(Chunk::list("trfl", {Chunk::leaf("trfd", trfdIdentity(d->x6))}));
    d->target().push_back({Private::object("obj ", "00020008", box, ++d->serial, std::move(lgob)), box});
}

void Writer::setPreview(const QImage& image)
{
    d->preview = image;
}

QByteArray Writer::finish(QString* error)
{
    while (!d->groups.empty())
        endGroup();
    if (d->pages.empty())
        d->pages.emplace_back();
    if (d->x6)
        return d->finishX6(error);
    auto fail = [&](const QString& message) {
        if (error)
            *error = message;
        return QByteArray();
    };
    QFile file(QStringLiteral(":/cdr/x3-template.riff"));
    std::vector<Chunk> top;
    if (!file.open(QIODevice::ReadOnly) || !parse(file.readAll(), 0, file.size(), top) || top.size() != 1)
        return fail(QObject::tr("CDR-Vorlage fehlt."));
    Chunk& tpl = top.front();
    Chunk* docTpl = tpl.findList("doc ");
    Chunk* masterTpl = tpl.findList("page");
    Chunk* sumiTpl = tpl.findLeaf("sumi");
    Chunk* masterLayers = masterTpl ? masterTpl->findList("gobj") : nullptr;
    if (!docTpl || !sumiTpl || !masterLayers || masterLayers->children.size() != 4 || sumiTpl->data.size() < 16)
        return fail(QObject::tr("CDR-Vorlage ist beschädigt."));
    Chunk doc = *docTpl;
    Chunk* mcfg = doc.findLeaf("mcfg");
    Chunk* ptrt = doc.findLeaf("ptrt");
    Chunk* filt = doc.findList("filt");
    Chunk* otlt = doc.findList("otlt");
    if (!mcfg || mcfg->data.size() < 20 || !ptrt || !filt || !otlt)
        return fail(QObject::tr("CDR-Vorlage ist beschädigt."));
    // Hilfslinien, Desktop, Ebene, Raster
    const Chunk guides = masterLayers->children[0], desktop = masterLayers->children[1], layerTpl = masterLayers->children[2],
                grid = masterLayers->children[3];

    const QStringList names = d->layerNames();

    Box extent;
    std::vector<Chunk> pages;
    for (const auto& page : d->pages) {
        Chunk gobj = Chunk::list("gobj", {guides, desktop});
        Box pageBox;
        for (auto name = names.crbegin(); name != names.crend(); ++name) {
            Chunk layer = Chunk::list("layr", {Chunk::leaf("flgs", QByteArray::fromHex("00000098")),
                                               Chunk::list("lgob", {Chunk::leaf("loda", Private::layerLoda(*name))})});
            for (const auto& l : page.layers)
                if (l.name == *name) {
                    Private::appendFrontToBack(layer, l.items);
                    for (const auto& item : l.items)
                        pageBox.add(item.box);
                }
            gobj.children.push_back(std::move(layer));
        }
        gobj.children.push_back(grid);
        extent.add(pageBox);
        pages.push_back(Chunk::list("page", {Chunk::leaf("flgs", QByteArray::fromHex("40000090")), Chunk::leaf("bbox", pageBox.bytes()),
                                             Chunk::leaf("obbx", pageBox.bytes() + pageBox.bytes()), std::move(gobj),
                                             Chunk::list("lgob", {Chunk::leaf("loda", loda(0, {{0x2ee0, u32(0)}}))})}));
    }

    // Hauptseite: Ebenen wie die Seiten
    Chunk master = *masterTpl;
    Chunk* mg = master.findList("gobj");
    mg->children = {guides, desktop};
    for (auto name = names.crbegin(); name != names.crend(); ++name) {
        Chunk layer = layerTpl;
        if (Chunk* lgob = layer.findList("lgob"))
            lgob->children = {Chunk::leaf("loda", Private::layerLoda(*name))};
        mg->children.push_back(std::move(layer));
    }
    mg->children.push_back(grid);

    patch32(mcfg->data, 12, quint32(std::lround(d->size.width() * kUnitsPerMm)));
    patch32(mcfg->data, 16, quint32(std::lround(d->size.height() * kUnitsPerMm)));
    for (const QByteArray& f : d->fills)
        filt->children.push_back(Chunk::list("filc", {Chunk::leaf("fild", f)}));
    for (const QByteArray& o : d->outlines)
        otlt->children.push_back(Chunk::leaf("outl", o));
    ptrt->data = extent.bytes();

    Chunk body = Chunk::list("cmpr", {std::move(doc), std::move(master)});
    for (Chunk& p : pages)
        body.children.push_back(std::move(p));
    QByteArray sumi = sumiTpl->data;
    sumi.replace(0, 16, extent.bytes());
    QByteArray version;
    put16(version, quint16(kVersion));
    const Chunk root{"RIFF", "CDRD", {},
                     {Chunk::leaf("vrsn", version), Chunk::leaf("DISP", d->displayImage()),
                      Chunk::list("INFO", {Chunk::leaf("IKEY", QByteArray(2, '\0')), Chunk::leaf("ICMT", QByteArray(2, '\0'))}),
                      Chunk::list("cmpr"), std::move(body), Chunk::leaf("sumi", sumi)}};
    return serialize(root, nullptr);
}

// Version 16: ZIP-Container. Im root.dat stehen die Listen wie in Version 13, jeder Block ist
// aber ein 16-Byte-Verweis: bis 8 Byte direkt (ffffffff, Länge, Daten), sonst Datei, Länge,
// Versatz in einer der Datendateien (Dokument, Hauptseite, je Seite eine).
QByteArray Writer::Private::finishX6(QString* error)
{
    auto fail = [&](const QString& message) {
        if (error)
            *error = message;
        return QByteArray();
    };
    QFile file(QStringLiteral(":/cdr/x6-template.riff"));
    std::vector<Chunk> top;
    if (!file.open(QIODevice::ReadOnly) || !parse(file.readAll(), 0, file.size(), top) || top.size() != 1)
        return fail(QObject::tr("CDR-Vorlage fehlt."));
    Chunk& tpl = top.front();
    Chunk* docTpl = tpl.findList("doc ");
    Chunk* styles = tpl.findLeaf("cdss");
    if (!docTpl || !styles)
        return fail(QObject::tr("CDR-Vorlage ist beschädigt."));
    Chunk doc = *docTpl;
    Chunk* mcfg = doc.findLeaf("mcfg");
    Chunk* ptrt = doc.findLeaf("ptrt");
    Chunk* filt = doc.findList("filt");
    Chunk* otlt = doc.findList("otlt");
    if (!mcfg || mcfg->data.size() < 20 || !ptrt || !filt || !otlt)
        return fail(QObject::tr("CDR-Vorlage ist beschädigt."));

    // Hilfslinien rot, Desktop und Ebenen CMYK-Schwarz, Raster grau
    const QByteArray prefix = QByteArray::fromHex("010c000000");
    const QByteArray red = prefix + colorBytes6(QColor(255, 0, 0));
    const QByteArray black = prefix + QByteArray::fromHex("020005000000000000000064") + QByteArray::fromHex("0710") + QByteArray(24, '\0');
    const QByteArray gray = prefix + colorBytes6(QColor(194, 194, 193));
    auto layer = [](const char* flags, quint32 type, const std::vector<std::pair<quint32, QByteArray>>& args) {
        return Chunk::list("layr", {Chunk::leaf("flgs", QByteArray::fromHex(flags)), Chunk::list("lgob", {Chunk::leaf("loda", loda(type, args))})});
    };
    auto guid = [] { return QUuid::createUuid().toRfc4122(); };
    // Hauptseite: nur Hilfslinien, Desktop und Raster; die Seiten verweisen über die Kennung darauf
    const QByteArray guides = guid(), desktop = guid(), grid = guid();
    const QByteArray emptyBox = QByteArray::fromHex("feffffff0200000002000000feffffff");
    const Chunk master = Chunk::list(
        "page", {Chunk::leaf("flgs", QByteArray::fromHex("40000190")), Chunk::leaf("bbox", emptyBox), Chunk::leaf("obbx", emptyBox + emptyBox),
                 Chunk::list("gobj", {layer("0a000198", 12, {{0x9c72, guides}, {0x3e81, u32(2)}, {0x2ee0, u32(0)}, {0x7d0, red}, {0x3e8, utf16z(QStringLiteral("Guides"))}}),
                                      layer("08000198", 17, {{0x9c72, desktop}, {0x2ee0, u32(0)}, {0x7d0, black}, {0x3e8, utf16z(QStringLiteral("Desktop"))}}),
                                      layer("5a010198", 11, {{0x9c72, grid}, {0x2ee0, u32(0)}, {0x7d0, gray}, {0x3e8, utf16z(QStringLiteral("Grid"))}})}),
                 Chunk::list("lgob", {Chunk::leaf("loda", loda(0, {{0x2ee0, u32(0)}}))})});

    const QStringList names = layerNames();
    Box extent;
    std::vector<Chunk> pageChunks;
    for (const auto& page : pages) {
        Chunk gobj = Chunk::list("gobj", {layer("0a000198", 12, {{0x9c72, guides}, {0x3e81, u32(2)}, {0x2ee0, u32(0)}, {0x7d0, red}}),
                                          layer("08000198", 17, {{0x9c72, desktop}, {0x2ee0, u32(0)}, {0x7d0, black}}),
                                          layer("0a000098", 12, {{0x9c72, guid()}, {0x3e81, u32(2)}, {0x2ee0, u32(0)}, {0x7d0, red},
                                                                 {0x3e8, utf16z(QStringLiteral("Guides"))}})});
        Box pageBox;
        for (auto name = names.crbegin(); name != names.crend(); ++name) {
            Chunk content = layer("00000098", 0, {{0x9c72, guid()}, {0x2ee0, u32(0)}, {0x7d0, black}, {0x3e8, utf16z(*name)}});
            for (const auto& l : page.layers)
                if (l.name == *name) {
                    appendFrontToBack(content, l.items);
                    for (const auto& item : l.items)
                        pageBox.add(item.box);
                }
            gobj.children.push_back(std::move(content));
        }
        gobj.children.push_back(layer("5a010198", 11, {{0x9c72, grid}, {0x2ee0, u32(0)}, {0x7d0, gray}}));
        extent.add(pageBox);
        pageChunks.push_back(Chunk::list("page", {Chunk::leaf("flgs", QByteArray::fromHex("40000090")), Chunk::leaf("bbox", pageBox.bytes()),
                                                  Chunk::leaf("obbx", pageBox.bytes() + pageBox.bytes()), std::move(gobj)}));
    }

    patch32(mcfg->data, 12, quint32(std::lround(size.width() * kUnitsPerMm)));
    patch32(mcfg->data, 16, quint32(std::lround(size.height() * kUnitsPerMm)));
    for (const QByteArray& f : fills)
        filt->children.push_back(Chunk::list("filc", {Chunk::leaf("fild", f)}));
    for (const QByteArray& o : outlines)
        otlt->children.push_back(Chunk::leaf("outl", o));
    ptrt->data = extent.bytes();

    QStringList fileNames{QStringLiteral("data1.dat"), QStringLiteral("masterPage.dat")};
    for (std::size_t i = 0; i < pageChunks.size(); ++i)
        fileNames << QStringLiteral("page%1.dat").arg(i + 1);
    std::vector<QByteArray> files(std::size_t(fileNames.size()));
    std::function<QByteArray(const Chunk&, std::size_t)> write = [&](const Chunk& c, std::size_t target) {
        QByteArray b = c.id;
        if (c.isList()) {
            QByteArray body = c.kind + c.data;
            for (const Chunk& ch : c.children)
                body += write(ch, target);
            put32(b, quint32(body.size()));
            return b + body;
        }
        put32(b, 16);
        if (c.data.size() <= 8) {
            put32(b, 0xffffffff);
            put32(b, quint32(c.data.size()));
            return b + c.data + QByteArray(8 - c.data.size(), '\0');
        }
        put32(b, quint32(target));
        put32(b, quint32(c.data.size()));
        put32(b, quint32(files[target].size()));
        put32(b, 0);
        files[target] += c.data;
        return b;
    };
    QByteArray body("CDRG");
    QByteArray version;
    put16(version, 1600);
    body += write(Chunk::leaf("fver", QByteArray::fromHex("4006010000001000")), 0);
    body += write(Chunk::leaf("vrsn", version), 0);
    body += write(doc, 0);
    body += write(master, 1);
    for (std::size_t i = 0; i < pageChunks.size(); ++i)
        body += write(pageChunks[i], 2 + i);
    QByteArray root("RIFF");
    put32(root, quint32(body.size()));
    root += body;

    // Vorschaubild 256 × 256 für Dateiauswahl und Explorer
    QImage thumb(256, 256, QImage::Format_RGB888);
    thumb.fill(Qt::white);
    if (!preview.isNull()) {
        QPainter p(&thumb);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        const QImage scaled = preview.scaled(256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        p.drawImage((256 - scaled.width()) / 2, (256 - scaled.height()) / 2, scaled);
    }
    QByteArray thumbBytes;
    QBuffer thumbBuffer(&thumbBytes);
    thumbBuffer.open(QIODevice::WriteOnly);
    thumb.save(&thumbBuffer, "BMP");

    std::vector<std::pair<QByteArray, QByteArray>> entries;
    for (qsizetype i = 0; i < fileNames.size(); ++i)
        entries.emplace_back("content/data/" + fileNames[i].toLatin1(), files[std::size_t(i)]);
    entries.emplace_back("content/dataFileList.dat", fileNames.join(QLatin1Char('\n')).toLatin1());
    entries.emplace_back("content/root.dat", root);
    entries.emplace_back("metadata/thumbnails/thumbnail.bmp", thumbBytes);
    entries.emplace_back("color/color.xml",
                         "<Color><ColorContext><ColorModel>Rgb</ColorModel><RenderingIntent>RelativeColorimetric</RenderingIntent></ColorContext>"
                         "<HasRgbObjects>true</HasRgbObjects><HasCmykObjects>false</HasCmykObjects><HasGrayscaleObjects>false</HasGrayscaleObjects></Color>");
    entries.emplace_back("styles/document.cdss", styles->data);
    entries.emplace_back("links.xml", "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<linkInfo />\n");

    mz_zip_archive zip;
    mz_zip_zero_struct(&zip);
    if (!mz_zip_writer_init_heap(&zip, 0, 256 * 1024))
        return fail(QObject::tr("ZIP-Container konnte nicht angelegt werden."));
    bool ok = true;
    for (const auto& [name, data] : entries)
        ok = ok && mz_zip_writer_add_mem(&zip, name.constData(), data.constData(), size_t(data.size()), MZ_DEFAULT_COMPRESSION);
    void* buffer = nullptr;
    size_t length = 0;
    ok = ok && mz_zip_writer_finalize_heap_archive(&zip, &buffer, &length);
    QByteArray result;
    if (ok)
        result = QByteArray(static_cast<const char*>(buffer), qsizetype(length));
    mz_zip_writer_end(&zip);
    if (buffer)
        mz_free(buffer);
    if (!ok)
        return fail(QObject::tr("ZIP-Container konnte nicht geschrieben werden."));
    return result;
}

} // namespace vek::cdr
