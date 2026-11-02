#include "FontFile.h"

#include "PathOps.h"

#include <QDateTime>
#include <QObject>
#include <QPolygonF>
#include <QRectF>
#include <QTimeZone>
#include <QTransform>

#include <algorithm>
#include <cmath>
#include <map>

namespace vek::ttf {

namespace {

// --- Big-Endian schreiben und lesen ------------------------------------------------------

struct Out {
    QByteArray d;
    void u8(int v) { d.append(char(v & 0xFF)); }
    void u16(int v)
    {
        u8(v >> 8);
        u8(v);
    }
    void u32(quint32 v)
    {
        u16(int(v >> 16));
        u16(int(v & 0xFFFF));
    }
    void i64(qint64 v)
    {
        u32(quint32(quint64(v) >> 32));
        u32(quint32(quint64(v) & 0xFFFFFFFFu));
    }
    void tag(const char* t) { d.append(t, 4); }
    void pad4()
    {
        while (d.size() % 4)
            u8(0);
    }
};

struct In {
    const QByteArray& d;
    bool ok(qsizetype at, qsizetype n) const { return at >= 0 && at + n <= d.size(); }
    int u8(qsizetype at) const { return ok(at, 1) ? quint8(d[at]) : 0; }
    int u16(qsizetype at) const { return (u8(at) << 8) | u8(at + 1); }
    int i16(qsizetype at) const { return qint16(quint16(u16(at))); }
    quint32 u32(qsizetype at) const { return (quint32(u16(at)) << 16) | quint32(u16(at + 2)); }
};

quint32 checksum(const QByteArray& data)
{
    quint32 sum = 0;
    for (qsizetype i = 0; i < data.size(); i += 4) {
        quint32 v = 0;
        for (int k = 0; k < 4; ++k)
            v = (v << 8) | quint32(i + k < data.size() ? quint8(data[i + k]) : 0);
        sum += v;
    }
    return sum;
}

// --- Umriss -> TrueType-Konturen ----------------------------------------------------------

struct Point {
    int x = 0, y = 0;
    bool on = true;
};
using Contour = QList<Point>;

void addCubic(Contour& c, QPointF p0, QPointF c1, QPointF c2, QPointF p3, int depth)
{
    // Fehler der quadratischen Näherung mit Kontrollpunkt (3(c1+c2)-(p0+p3))/4
    const QPointF d = p3 - 3 * c2 + 3 * c1 - p0;
    const double err = std::hypot(d.x(), d.y()) * std::sqrt(3.0) / 36.0;
    if (err <= 0.6 || depth >= 8) {
        const QPointF q = (3 * (c1 + c2) - p0 - p3) / 4;
        c << Point{int(std::lround(q.x())), int(std::lround(q.y())), false};
        c << Point{int(std::lround(p3.x())), int(std::lround(p3.y())), true};
        return;
    }
    // bei t = 0.5 teilen
    const QPointF a = (p0 + c1) / 2, b = (c1 + c2) / 2, e = (c2 + p3) / 2;
    const QPointF ab = (a + b) / 2, be = (b + e) / 2, m = (ab + be) / 2;
    addCubic(c, p0, a, ab, m, depth + 1);
    addCubic(c, m, be, e, p3, depth + 1);
}

QList<Contour> contours(const QPainterPath& path)
{
    QList<Contour> out;
    QPointF last;
    for (int i = 0; i < path.elementCount(); ++i) {
        const QPainterPath::Element el = path.elementAt(i);
        if (el.isMoveTo()) {
            out << Contour();
            out.last() << Point{int(std::lround(el.x)), int(std::lround(el.y)), true};
            last = el;
        } else if (el.isLineTo()) {
            if (out.isEmpty())
                out << Contour();
            out.last() << Point{int(std::lround(el.x)), int(std::lround(el.y)), true};
            last = el;
        } else if (el.isCurveTo() && i + 2 < path.elementCount()) {
            if (out.isEmpty())
                out << Contour();
            const QPointF c1 = el, c2 = path.elementAt(i + 1), p3 = path.elementAt(i + 2);
            addCubic(out.last(), last, c1, c2, p3, 0);
            last = p3;
            i += 2;
        }
    }
    // Schlusspunkt = Anfang entfernen, doppelte Punkte, zu kleine Konturen
    QList<Contour> clean;
    for (Contour c : out) {
        while (c.size() > 1 && c.last().on && c.last().x == c.first().x && c.last().y == c.first().y)
            c.removeLast();
        Contour d;
        for (const Point& p : c)
            if (d.isEmpty() || p.x != d.last().x || p.y != d.last().y || p.on != d.last().on)
                d << p;
        if (d.size() >= 3)
            clean << d;
    }
    // Ausrichtung: Tiefe gerade (aussen) im Uhrzeigersinn, ungerade (Loch) dagegen
    QList<QPolygonF> polys;
    for (const Contour& c : clean) {
        QPolygonF poly;
        for (const Point& p : c)
            poly << QPointF(p.x, p.y);
        polys << poly;
    }
    for (qsizetype i = 0; i < clean.size(); ++i) {
        double area = 0;
        for (qsizetype k = 0; k < polys[i].size(); ++k) {
            const QPointF& a = polys[i][k];
            const QPointF& b = polys[i][(k + 1) % polys[i].size()];
            area += a.x() * b.y() - b.x() * a.y();
        }
        int depth = 0;
        const QPointF probe = polys[i].first();
        for (qsizetype j = 0; j < clean.size(); ++j)
            if (j != i && polys[j].containsPoint(probe, Qt::OddEvenFill))
                ++depth;
        const bool clockwise = area < 0;
        if (clockwise != (depth % 2 == 0)) {
            // Umkehren, Startpunkt bleibt auf der Kurve
            Contour r;
            r << clean[i].first();
            for (qsizetype k = clean[i].size() - 1; k >= 1; --k)
                r << clean[i][k];
            clean[i] = r;
        }
    }
    return clean;
}

struct Built {
    QByteArray data;
    int contourCount = 0;
    int pointCount = 0;
    int xMin = 0, yMin = 0, xMax = 0, yMax = 0;
    bool empty = true;
};

Built buildGlyph(const QPainterPath& outline)
{
    Built b;
    const QList<Contour> cs = contours(outline);
    if (cs.isEmpty())
        return b;
    b.empty = false;
    b.xMin = b.yMin = 32767;
    b.xMax = b.yMax = -32768;
    for (const Contour& c : cs)
        for (const Point& p : c) {
            b.xMin = std::min(b.xMin, p.x);
            b.yMin = std::min(b.yMin, p.y);
            b.xMax = std::max(b.xMax, p.x);
            b.yMax = std::max(b.yMax, p.y);
            ++b.pointCount;
        }
    b.contourCount = int(cs.size());
    Out o;
    o.u16(b.contourCount);
    o.u16(b.xMin);
    o.u16(b.yMin);
    o.u16(b.xMax);
    o.u16(b.yMax);
    int end = -1;
    for (const Contour& c : cs) {
        end += int(c.size());
        o.u16(end);
    }
    o.u16(0); // keine Hinting-Anweisungen
    for (const Contour& c : cs)
        for (const Point& p : c)
            o.u8(p.on ? 1 : 0);
    int x = 0, y = 0;
    for (const Contour& c : cs)
        for (const Point& p : c) {
            o.u16(p.x - x);
            x = p.x;
        }
    for (const Contour& c : cs)
        for (const Point& p : c) {
            o.u16(p.y - y);
            y = p.y;
        }
    o.pad4();
    b.data = o.d;
    return b;
}

void searchParams(int count, int unitSize, Out& o)
{
    int pow2 = 1, log2 = 0;
    while (pow2 * 2 <= count) {
        pow2 *= 2;
        ++log2;
    }
    o.u16(pow2 * unitSize);
    o.u16(log2);
    o.u16(count * unitSize - pow2 * unitSize);
}

QByteArray nameTable(const QString& family)
{
    QString ps;
    for (QChar ch : family)
        if (ch.unicode() > 32 && ch.unicode() < 127 && ch != QLatin1Char('(') && ch != QLatin1Char(')') && ch != QLatin1Char('/')
            && ch != QLatin1Char('%'))
            ps += ch;
    if (ps.isEmpty())
        ps = QStringLiteral("TANGENTA");
    const QList<std::pair<int, QString>> names{{1, family},
                                                {2, QStringLiteral("Regular")},
                                                {3, QStringLiteral("TANGENTA: %1").arg(family)},
                                                {4, family},
                                                {5, QStringLiteral("Version 1.0")},
                                                {6, ps + QStringLiteral("-Regular")}};
    QByteArray strings;
    Out o;
    o.u16(0);
    o.u16(int(names.size()));
    o.u16(6 + 12 * int(names.size()));
    for (const auto& [id, text] : names) {
        QByteArray utf16;
        for (QChar ch : text) {
            utf16.append(char(ch.unicode() >> 8));
            utf16.append(char(ch.unicode() & 0xFF));
        }
        o.u16(3);
        o.u16(1);
        o.u16(0x409);
        o.u16(id);
        o.u16(int(utf16.size()));
        o.u16(int(strings.size()));
        strings += utf16;
    }
    return o.d + strings;
}

QByteArray cmapTable(const std::map<char32_t, int>& map)
{
    // Format 4 (BMP, ein Abschnitt je Zeichen) und Format 12 (alle Zeichen)
    QList<std::pair<int, int>> bmp;
    for (const auto& [code, glyph] : map)
        if (code < 0xFFFF)
            bmp << std::pair{int(code), glyph};
    const int segs = int(bmp.size()) + 1;
    Out f4;
    f4.u16(4);
    f4.u16(16 + 8 * segs);
    f4.u16(0);
    f4.u16(segs * 2);
    {
        int pow2 = 1, log2 = 0;
        while (pow2 * 2 <= segs) {
            pow2 *= 2;
            ++log2;
        }
        f4.u16(2 * pow2);
        f4.u16(log2);
        f4.u16(2 * segs - 2 * pow2);
    }
    for (const auto& [code, glyph] : bmp)
        f4.u16(code);
    f4.u16(0xFFFF);
    f4.u16(0);
    for (const auto& [code, glyph] : bmp)
        f4.u16(code);
    f4.u16(0xFFFF);
    for (const auto& [code, glyph] : bmp)
        f4.u16((glyph - code) & 0xFFFF);
    f4.u16(1);
    for (int i = 0; i < segs; ++i)
        f4.u16(0);
    Out f12;
    f12.u16(12);
    f12.u16(0);
    f12.u32(quint32(16 + 12 * map.size()));
    f12.u32(0);
    f12.u32(quint32(map.size()));
    for (const auto& [code, glyph] : map) {
        f12.u32(code);
        f12.u32(code);
        f12.u32(quint32(glyph));
    }
    Out o;
    o.u16(0);
    o.u16(2);
    o.u16(3);
    o.u16(1);
    o.u32(4 + 16);
    o.u16(3);
    o.u16(10);
    o.u32(quint32(4 + 16 + f4.d.size()));
    return o.d + f4.d + f12.d;
}

} // namespace

QByteArray write(const Font& font)
{
    const int em = font.unitsPerEm;
    // Glyphen: 0 .notdef, dann nach Zeichen sortiert; Leerzeichen ergänzen
    QList<Glyph> glyphs = font.glyphs;
    if (std::none_of(glyphs.begin(), glyphs.end(), [](const Glyph& g) { return g.code == U' '; }))
        glyphs << Glyph{U' ', {}, em / 4};
    std::sort(glyphs.begin(), glyphs.end(), [](const Glyph& a, const Glyph& b) { return a.code < b.code; });
    Glyph notdef;
    notdef.advance = em / 2;
    notdef.outline.addRect(QRectF(50, 0, em / 2 - 100, font.ascent * 0.9));
    notdef.outline.addRect(QRectF(100, 50, em / 2 - 200, font.ascent * 0.9 - 100));
    glyphs.prepend(notdef);

    QList<Built> built;
    for (const Glyph& g : std::as_const(glyphs))
        built << buildGlyph(g.outline);
    const int numGlyphs = int(glyphs.size());

    Out glyf, loca, hmtx;
    int xMin = 0, yMin = 0, xMax = 0, yMax = 0, maxPoints = 0, maxContours = 0, advanceMax = 0;
    int minLsb = 32767, minRsb = 32767, maxExtent = -32768;
    bool first = true;
    double widthSum = 0;
    for (int i = 0; i < numGlyphs; ++i) {
        const Built& b = built[i];
        loca.u32(quint32(glyf.d.size()));
        glyf.d += b.data;
        const int advance = std::max(0, glyphs[i].advance);
        hmtx.u16(advance);
        hmtx.u16(b.empty ? 0 : b.xMin);
        advanceMax = std::max(advanceMax, advance);
        widthSum += advance;
        if (b.empty)
            continue;
        maxPoints = std::max(maxPoints, b.pointCount);
        maxContours = std::max(maxContours, b.contourCount);
        minLsb = std::min(minLsb, b.xMin);
        minRsb = std::min(minRsb, advance - b.xMax);
        maxExtent = std::max(maxExtent, b.xMax);
        if (first) {
            xMin = b.xMin, yMin = b.yMin, xMax = b.xMax, yMax = b.yMax;
            first = false;
        } else {
            xMin = std::min(xMin, b.xMin), yMin = std::min(yMin, b.yMin), xMax = std::max(xMax, b.xMax), yMax = std::max(yMax, b.yMax);
        }
    }
    loca.u32(quint32(glyf.d.size()));
    if (first)
        minLsb = minRsb = maxExtent = 0;

    std::map<char32_t, int> cmap;
    char32_t firstChar = 0xFFFF, lastChar = 0;
    for (int i = 1; i < numGlyphs; ++i) {
        cmap[glyphs[i].code] = i;
        firstChar = std::min(firstChar, glyphs[i].code);
        lastChar = std::max(lastChar, glyphs[i].code);
    }

    const qint64 since1904 = QDateTime(QDate(1904, 1, 1), QTime(0, 0), QTimeZone::UTC).secsTo(QDateTime::currentDateTimeUtc());
    Out head;
    head.u32(0x00010000);
    head.u32(0x00010000);
    head.u32(0); // checkSumAdjustment, unten eingesetzt
    head.u32(0x5F0F3CF5);
    head.u16(0x000B);
    head.u16(em);
    head.i64(since1904);
    head.i64(since1904);
    head.u16(xMin);
    head.u16(yMin);
    head.u16(xMax);
    head.u16(yMax);
    head.u16(0);
    head.u16(8);
    head.u16(2);
    head.u16(1); // loca mit 32 Bit
    head.u16(0);

    Out hhea;
    hhea.u32(0x00010000);
    hhea.u16(font.ascent);
    hhea.u16(-font.descent);
    hhea.u16(0);
    hhea.u16(advanceMax);
    hhea.u16(minLsb);
    hhea.u16(minRsb);
    hhea.u16(maxExtent);
    hhea.u16(1);
    hhea.u16(0);
    hhea.u16(0);
    for (int i = 0; i < 5; ++i)
        hhea.u16(0);
    hhea.u16(numGlyphs);

    Out maxp;
    maxp.u32(0x00010000);
    maxp.u16(numGlyphs);
    maxp.u16(maxPoints);
    maxp.u16(maxContours);
    maxp.u16(0);
    maxp.u16(0);
    maxp.u16(2);
    for (int i = 0; i < 8; ++i)
        maxp.u16(0);

    Out os2;
    os2.u16(4);
    os2.u16(int(std::lround(widthSum / numGlyphs)));
    os2.u16(400);
    os2.u16(5);
    os2.u16(0); // fsType: installierbar
    for (int v : {em * 65 / 100, em * 60 / 100, 0, em * 75 / 1000, em * 65 / 100, em * 60 / 100, 0, em * 35 / 100, em / 20, em * 3 / 10})
        os2.u16(v);
    os2.u16(0);
    for (int i = 0; i < 10; ++i)
        os2.u8(0);
    os2.u32(3);
    os2.u32(0);
    os2.u32(0);
    os2.u32(0);
    os2.tag("TANG");
    os2.u16(0x40);
    os2.u16(int(std::min<char32_t>(firstChar, 0xFFFF)));
    os2.u16(int(std::min<char32_t>(lastChar, 0xFFFF)));
    os2.u16(font.ascent);
    os2.u16(-font.descent);
    os2.u16(0);
    os2.u16(std::max(font.ascent, yMax));
    os2.u16(std::max(font.descent, -yMin));
    os2.u32(1);
    os2.u32(0);
    os2.u16(em / 2);
    os2.u16(em * 7 / 10);
    os2.u16(0);
    os2.u16(32);
    os2.u16(0);

    Out post;
    post.u32(0x00030000);
    post.u32(0);
    post.u16(-em / 10);
    post.u16(em / 20);
    for (int i = 0; i < 5; ++i)
        post.u32(0);

    std::map<QByteArray, QByteArray> tables{{"OS/2", os2.d}, {"cmap", cmapTable(cmap)}, {"glyf", glyf.d}, {"head", head.d},
                                            {"hhea", hhea.d}, {"hmtx", hmtx.d}, {"loca", loca.d},  {"maxp", maxp.d},
                                            {"name", nameTable(font.family)}, {"post", post.d}};
    Out file;
    file.u32(0x00010000);
    file.u16(int(tables.size()));
    searchParams(int(tables.size()), 16, file);
    qsizetype offset = 12 + 16 * qsizetype(tables.size());
    qsizetype headOffset = 0;
    QByteArray body;
    for (const auto& [tag, data] : tables) {
        file.tag(tag.constData());
        file.u32(checksum(data));
        file.u32(quint32(offset + body.size()));
        file.u32(quint32(data.size()));
        if (tag == "head")
            headOffset = offset + body.size();
        body += data;
        while (body.size() % 4)
            body.append('\0');
    }
    QByteArray result = file.d + body;
    const quint32 adjust = 0xB1B0AFBAu - checksum(result);
    for (int k = 0; k < 4; ++k)
        result[headOffset + 8 + k] = char((adjust >> (24 - 8 * k)) & 0xFF);
    return result;
}

std::optional<Font> read(const QByteArray& data, QString* error)
{
    auto fail = [&](const QString& msg) -> std::optional<Font> {
        if (error)
            *error = msg;
        return std::nullopt;
    };
    const In in{data};
    const quint32 version = in.u32(0);
    if (version != 0x00010000 && version != 0x74727565)
        return fail(QObject::tr("Keine TrueType-Schrift (OpenType mit CFF-Umrissen wird nicht unterstützt)."));
    std::map<QByteArray, std::pair<qsizetype, qsizetype>> tables;
    const int count = in.u16(4);
    for (int i = 0; i < count; ++i) {
        const qsizetype at = 12 + 16 * i;
        tables[data.mid(at, 4)] = {qsizetype(in.u32(at + 8)), qsizetype(in.u32(at + 12))};
    }
    for (const char* t : {"head", "hhea", "maxp", "hmtx", "loca", "glyf", "cmap"})
        if (!tables.count(t) || !in.ok(tables[t].first, tables[t].second))
            return fail(QObject::tr("Die Schrift ist unvollständig (Tabelle %1 fehlt).").arg(QLatin1String(t)));
    Font font;
    const qsizetype head = tables["head"].first, hhea = tables["hhea"].first;
    font.unitsPerEm = in.u16(head + 18);
    const bool longLoca = in.i16(head + 50) == 1;
    font.ascent = in.i16(hhea + 4);
    font.descent = -in.i16(hhea + 6);
    const int numMetrics = in.u16(hhea + 34);
    const int numGlyphs = in.u16(tables["maxp"].first + 4);
    const qsizetype hmtx = tables["hmtx"].first, loca = tables["loca"].first, glyf = tables["glyf"].first;

    // Zeichen je Glyphe aus cmap (3,10) Format 12 oder (3,1)/(0,x) Format 4
    std::map<int, char32_t> codes;
    const qsizetype cmap = tables["cmap"].first;
    qsizetype sub4 = -1, sub12 = -1;
    for (int i = 0; i < in.u16(cmap + 2); ++i) {
        const int platform = in.u16(cmap + 4 + 8 * i), encoding = in.u16(cmap + 6 + 8 * i);
        const qsizetype at = cmap + in.u32(cmap + 8 + 8 * i);
        if (in.u16(at) == 12 && (platform == 3 || platform == 0))
            sub12 = at;
        else if (in.u16(at) == 4 && (platform == 0 || (platform == 3 && encoding == 1)))
            sub4 = at;
    }
    if (sub12 >= 0) {
        const quint32 groups = in.u32(sub12 + 12);
        for (quint32 g = 0; g < groups && g < 100000; ++g) {
            const qsizetype at = sub12 + 16 + 12 * qsizetype(g);
            const quint32 start = in.u32(at), end = in.u32(at + 4), glyph = in.u32(at + 8);
            for (quint32 c = start; c <= end && c - start < 70000; ++c)
                codes.try_emplace(int(glyph + c - start), char32_t(c));
        }
    } else if (sub4 >= 0) {
        const int segs = in.u16(sub4 + 6) / 2;
        const qsizetype ends = sub4 + 14, starts = ends + 2 * segs + 2, deltas = starts + 2 * segs, ranges = deltas + 2 * segs;
        for (int s = 0; s < segs; ++s) {
            const int end = in.u16(ends + 2 * s), start = in.u16(starts + 2 * s), delta = in.u16(deltas + 2 * s);
            const int range = in.u16(ranges + 2 * s);
            for (int c = start; c <= end && c != 0xFFFF; ++c) {
                int glyph = 0;
                if (range == 0) {
                    glyph = (c + delta) & 0xFFFF;
                } else {
                    const int g = in.u16(ranges + 2 * s + range + 2 * (c - start));
                    glyph = g ? (g + delta) & 0xFFFF : 0;
                }
                if (glyph)
                    codes.try_emplace(glyph, char32_t(c));
            }
        }
    }
    if (tables.count("name")) {
        const qsizetype name = tables["name"].first;
        const qsizetype strings = name + in.u16(name + 4);
        for (int i = 0; i < in.u16(name + 2); ++i) {
            const qsizetype rec = name + 6 + 12 * i;
            if (in.u16(rec) == 3 && in.u16(rec + 6) == 1) {
                const int len = in.u16(rec + 8);
                const qsizetype at = strings + in.u16(rec + 10);
                QString s;
                for (int k = 0; k + 1 < len; k += 2)
                    s += QChar(char16_t(in.u16(at + k)));
                font.family = s;
                break;
            }
        }
    }
    for (const auto& [index, code] : codes) {
        if (index <= 0 || index >= numGlyphs)
            continue;
        Glyph g;
        g.code = code;
        g.advance = in.u16(hmtx + 4 * std::min(index, numMetrics - 1));
        const qsizetype start = glyf + (longLoca ? in.u32(loca + 4 * index) : 2 * in.u16(loca + 2 * index));
        const qsizetype end = glyf + (longLoca ? in.u32(loca + 4 * index + 4) : 2 * in.u16(loca + 2 * index + 2));
        const int nContours = end > start ? in.i16(start) : 0;
        if (nContours > 0) {
            QList<int> ends;
            for (int c = 0; c < nContours; ++c)
                ends << in.u16(start + 10 + 2 * c);
            const int nPoints = ends.last() + 1;
            qsizetype at = start + 10 + 2 * nContours;
            at += 2 + in.u16(at);
            QList<int> flags;
            while (flags.size() < nPoints && at < end) {
                const int f = in.u8(at++);
                flags << f;
                if (f & 8) {
                    const int repeat = in.u8(at++);
                    for (int r = 0; r < repeat; ++r)
                        flags << f;
                }
            }
            QList<QPointF> pts;
            int x = 0;
            QList<int> xs, ys;
            for (int f : std::as_const(flags)) {
                if (f & 2) {
                    const int dx = in.u8(at++);
                    x += (f & 16) ? dx : -dx;
                } else if (!(f & 16)) {
                    x += in.i16(at);
                    at += 2;
                }
                xs << x;
            }
            int y = 0;
            for (int f : std::as_const(flags)) {
                if (f & 4) {
                    const int dy = in.u8(at++);
                    y += (f & 32) ? dy : -dy;
                } else if (!(f & 32)) {
                    y += in.i16(at);
                    at += 2;
                }
                ys << y;
            }
            int from = 0;
            for (int c = 0; c < nContours && ends[c] < flags.size(); ++c) {
                QList<std::pair<QPointF, bool>> seq;
                for (int k = from; k <= ends[c]; ++k)
                    seq << std::pair{QPointF(xs[k], ys[k]), bool(flags[k] & 1)};
                from = ends[c] + 1;
                if (seq.size() < 2)
                    continue;
                // mit einem Punkt auf der Kurve beginnen
                const auto onIt = std::find_if(seq.begin(), seq.end(), [](const auto& p) { return p.second; });
                if (onIt == seq.end())
                    seq.prepend({(seq.first().first + seq.last().first) / 2, true});
                else
                    std::rotate(seq.begin(), onIt, seq.end());
                seq << seq.first();
                g.outline.moveTo(seq.first().first);
                for (qsizetype k = 1; k < seq.size();) {
                    if (seq[k].second) {
                        g.outline.lineTo(seq[k].first);
                        ++k;
                    } else if (seq[k + 1].second) {
                        g.outline.quadTo(seq[k].first, seq[k + 1].first);
                        k += 2;
                    } else {
                        g.outline.quadTo(seq[k].first, (seq[k].first + seq[k + 1].first) / 2);
                        ++k;
                    }
                }
                g.outline.closeSubpath();
            }
        }
        font.glyphs << g;
    }
    return font;
}

Glyph glyphFromOutline(const QPainterPath& mm, char32_t code, int height, int bottom, int bearing)
{
    Glyph g;
    g.code = code;
    const QRectF r = mm.boundingRect();
    if (r.height() <= 0 || r.width() <= 0) {
        g.advance = 2 * bearing;
        return g;
    }
    const double s = height / r.height();
    const QTransform t = QTransform::fromTranslate(-r.left(), -r.bottom()) * QTransform(s, 0, 0, -s, bearing, bottom);
    QPainterPath units = t.map(mm);
    units.setFillRule(Qt::WindingFill);
    // Überlappungen vereinigen (Schnittmenge rechnet mit Linienzügen), dann wieder Kurven anpassen
    const QPainterPath merged = units.simplified();
    g.outline = CurveObject::toPainterPath(pathops::fitRegion(merged, 0.5));
    g.advance = int(std::lround(r.width() * s)) + 2 * bearing;
    return g;
}

} // namespace vek::ttf
