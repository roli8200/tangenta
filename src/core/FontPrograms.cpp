#include "FontPrograms.h"

#include <QHash>
#include <QList>
#include <QRegularExpression>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <vector>

namespace vek::fontprog {

namespace {

#include "FontTables.inc"

const QHash<QString, char16_t>& glyphList()
{
    static const QHash<QString, char16_t> list = [] {
        QHash<QString, char16_t> h;
        h.reserve(int(std::size(kGlyphList)));
        for (const AglEntry& e : kGlyphList)
            h.insert(QLatin1String(e.name), e.unicode);
        return h;
    }();
    return list;
}

// --- Big-Endian lesen -----------------------------------------------------------------------

struct Bytes {
    QByteArray d;
    bool ok(qsizetype at, qsizetype n = 1) const { return at >= 0 && n >= 0 && at + n <= d.size(); }
    int u8(qsizetype at) const { return ok(at) ? quint8(d[at]) : 0; }
    int u16(qsizetype at) const { return (u8(at) << 8) | u8(at + 1); }
    int i16(qsizetype at) const { return qint16(quint16(u16(at))); }
    quint32 u32(qsizetype at) const { return (quint32(u16(at)) << 16) | quint32(u16(at + 2)); }
    quint32 uN(qsizetype at, int n) const
    {
        quint32 v = 0;
        for (int i = 0; i < n; ++i)
            v = (v << 8) | quint32(u8(at + i));
        return v;
    }
};

// --- TrueType ---------------------------------------------------------------------------------

class TrueType final : public Program {
public:
    explicit TrueType(QByteArray data) : m_b{std::move(data)} {}

    bool parse()
    {
        const int count = m_b.u16(4);
        if (count <= 0 || count > 200)
            return false;
        for (int i = 0; i < count; ++i) {
            const qsizetype at = 12 + 16 * i;
            m_tables[m_b.d.mid(at, 4)] = {qsizetype(m_b.u32(at + 8)), qsizetype(m_b.u32(at + 12))};
        }
        for (const char* t : {"head", "loca", "glyf", "maxp"})
            if (!m_tables.count(t))
                return false;
        const qsizetype head = m_tables["head"].first;
        m_unitsPerEm = std::max(16, m_b.u16(head + 18));
        m_longLoca = m_b.i16(head + 50) == 1;
        m_numGlyphs = m_b.u16(m_tables["maxp"].first + 4);
        parseCmap();
        parsePost();
        if (m_tables.count("name"))
            parseName();
        return m_numGlyphs > 0;
    }

    int glyphCount() const override { return m_numGlyphs; }
    QTransform fontMatrix() const override { return QTransform::fromScale(1.0 / m_unitsPerEm, 1.0 / m_unitsPerEm); }
    int gidForName(const QString& name) const override { return m_names.value(name, -1); }
    int gidForUnicode(char32_t u) const override
    {
        const auto it = m_unicode.find(u);
        return it != m_unicode.end() ? it->second : -1;
    }
    int gidForBuiltinCode(int code) const override
    {
        // Symbolschriften: cmap (3,0) auf F000+code (oder direkt), sonst (1,0)
        for (int c : {code, 0xF000 + code, 0xF100 + code, 0xF200 + code}) {
            const auto it = m_symbol.find(c);
            if (it != m_symbol.end())
                return it->second;
        }
        const auto it = m_mac.find(code);
        return it != m_mac.end() ? it->second : -1;
    }
    QString fontName() const override { return m_fontName; }

    QPainterPath glyph(int gid) const override
    {
        QPainterPath path;
        path.setFillRule(Qt::WindingFill);
        addGlyph(path, gid, QTransform(), 0);
        return path;
    }

private:
    std::pair<qsizetype, qsizetype> glyphRange(int gid) const
    {
        if (gid < 0 || gid >= m_numGlyphs)
            return {0, 0};
        const qsizetype loca = m_tables.at("loca").first, glyf = m_tables.at("glyf").first;
        const qsizetype a = m_longLoca ? m_b.u32(loca + 4 * gid) : 2 * qsizetype(m_b.u16(loca + 2 * gid));
        const qsizetype e = m_longLoca ? m_b.u32(loca + 4 * gid + 4) : 2 * qsizetype(m_b.u16(loca + 2 * gid + 2));
        return {glyf + a, glyf + e};
    }

    void addGlyph(QPainterPath& path, int gid, const QTransform& t, int depth) const
    {
        const auto [start, end] = glyphRange(gid);
        if (end <= start || !m_b.ok(start, 10) || depth > 8)
            return;
        const int contours = m_b.i16(start);
        if (contours < 0) {
            // Zusammengesetzt: Teile mit Versatz und Skalierung
            qsizetype at = start + 10;
            for (int guard = 0; guard < 64; ++guard) {
                const int flags = m_b.u16(at), part = m_b.u16(at + 2);
                at += 4;
                double dx = 0, dy = 0;
                if (flags & 1) {
                    dx = m_b.i16(at);
                    dy = m_b.i16(at + 2);
                    at += 4;
                } else {
                    dx = qint8(m_b.u8(at));
                    dy = qint8(m_b.u8(at + 1));
                    at += 2;
                }
                auto f2dot14 = [&](qsizetype p) { return m_b.i16(p) / 16384.0; };
                double a = 1, b = 0, c = 0, d = 1;
                if (flags & 8) {
                    a = d = f2dot14(at);
                    at += 2;
                } else if (flags & 0x40) {
                    a = f2dot14(at);
                    d = f2dot14(at + 2);
                    at += 4;
                } else if (flags & 0x80) {
                    a = f2dot14(at);
                    b = f2dot14(at + 2);
                    c = f2dot14(at + 4);
                    d = f2dot14(at + 6);
                    at += 8;
                }
                if (!(flags & 2))
                    dx = dy = 0; // Punktabgleich statt Versatz: selten, hier ohne Verschiebung
                addGlyph(path, part, QTransform(a, b, c, d, dx, dy) * t, depth + 1);
                if (!(flags & 0x20))
                    break;
            }
            return;
        }
        QList<int> ends;
        for (int c = 0; c < contours; ++c)
            ends << m_b.u16(start + 10 + 2 * c);
        if (ends.isEmpty())
            return;
        const int points = ends.last() + 1;
        if (points <= 0 || points > 20000)
            return;
        qsizetype at = start + 10 + 2 * contours;
        at += 2 + m_b.u16(at);
        QList<int> flags;
        while (flags.size() < points && at < end) {
            const int f = m_b.u8(at++);
            flags << f;
            if (f & 8) {
                const int repeat = m_b.u8(at++);
                for (int r = 0; r < repeat && flags.size() < points; ++r)
                    flags << f;
            }
        }
        if (flags.size() < points)
            return;
        QList<double> xs, ys;
        int v = 0;
        for (int f : std::as_const(flags)) {
            if (f & 2) {
                const int d = m_b.u8(at++);
                v += (f & 16) ? d : -d;
            } else if (!(f & 16)) {
                v += m_b.i16(at);
                at += 2;
            }
            xs << v;
        }
        v = 0;
        for (int f : std::as_const(flags)) {
            if (f & 4) {
                const int d = m_b.u8(at++);
                v += (f & 32) ? d : -d;
            } else if (!(f & 32)) {
                v += m_b.i16(at);
                at += 2;
            }
            ys << v;
        }
        int from = 0;
        for (int c = 0; c < contours; ++c) {
            QList<std::pair<QPointF, bool>> seq;
            for (int k = from; k <= ends[c] && k < points; ++k)
                seq << std::pair{t.map(QPointF(xs[k], ys[k])), bool(flags[k] & 1)};
            from = ends[c] + 1;
            if (seq.size() < 2)
                continue;
            const auto on = std::find_if(seq.begin(), seq.end(), [](const auto& p) { return p.second; });
            if (on == seq.end())
                seq.prepend({(seq.first().first + seq.last().first) / 2, true});
            else
                std::rotate(seq.begin(), on, seq.end());
            seq << seq.first();
            path.moveTo(seq.first().first);
            for (qsizetype k = 1; k < seq.size();) {
                if (seq[k].second) {
                    path.lineTo(seq[k].first);
                    ++k;
                } else if (seq[k + 1].second) {
                    path.quadTo(seq[k].first, seq[k + 1].first);
                    k += 2;
                } else {
                    path.quadTo(seq[k].first, (seq[k].first + seq[k + 1].first) / 2);
                    ++k;
                }
            }
            path.closeSubpath();
        }
    }

    void readSubtable(qsizetype at, std::map<int, int>& out) const
    {
        const int format = m_b.u16(at);
        if (format == 0) {
            for (int c = 0; c < 256; ++c)
                if (const int g = m_b.u8(at + 6 + c))
                    out.emplace(c, g);
        } else if (format == 4) {
            const int segs = m_b.u16(at + 6) / 2;
            const qsizetype ends = at + 14, starts = ends + 2 * segs + 2, deltas = starts + 2 * segs, ranges = deltas + 2 * segs;
            for (int s = 0; s < segs; ++s) {
                const int e = m_b.u16(ends + 2 * s), b = m_b.u16(starts + 2 * s), delta = m_b.u16(deltas + 2 * s), range = m_b.u16(ranges + 2 * s);
                for (int c = b; c <= e && c != 0xFFFF; ++c) {
                    int g = 0;
                    if (range == 0) {
                        g = (c + delta) & 0xFFFF;
                    } else {
                        const int raw = m_b.u16(ranges + 2 * s + range + 2 * (c - b));
                        g = raw ? (raw + delta) & 0xFFFF : 0;
                    }
                    if (g)
                        out.emplace(c, g);
                }
            }
        } else if (format == 6) {
            const int first = m_b.u16(at + 6), count = m_b.u16(at + 8);
            for (int i = 0; i < count; ++i)
                if (const int g = m_b.u16(at + 10 + 2 * i))
                    out.emplace(first + i, g);
        } else if (format == 12) {
            const quint32 groups = std::min<quint32>(m_b.u32(at + 12), 100000);
            for (quint32 g = 0; g < groups; ++g) {
                const qsizetype p = at + 16 + 12 * qsizetype(g);
                const quint32 s = m_b.u32(p), e = m_b.u32(p + 4), gid = m_b.u32(p + 8);
                for (quint32 c = s; c <= e && c - s < 70000; ++c)
                    out.emplace(int(c), int(gid + c - s));
            }
        }
    }

    void parseCmap()
    {
        if (!m_tables.count("cmap"))
            return;
        const qsizetype cmap = m_tables["cmap"].first;
        for (int i = 0; i < m_b.u16(cmap + 2); ++i) {
            const int platform = m_b.u16(cmap + 4 + 8 * i), encoding = m_b.u16(cmap + 6 + 8 * i);
            const qsizetype at = cmap + m_b.u32(cmap + 8 + 8 * i);
            if (platform == 3 && encoding == 0)
                readSubtable(at, m_symbol);
            else if (platform == 1 && encoding == 0)
                readSubtable(at, m_mac);
            else if (platform == 0 || (platform == 3 && (encoding == 1 || encoding == 10))) {
                std::map<int, int> m;
                readSubtable(at, m);
                for (const auto& [c, g] : m)
                    m_unicode.emplace(char32_t(c), g);
            }
        }
    }

    void parsePost()
    {
        if (!m_tables.count("post"))
            return;
        const qsizetype post = m_tables["post"].first;
        if (m_b.u32(post) != 0x00020000)
            return;
        const int n = m_b.u16(post + 32);
        QList<int> index;
        for (int i = 0; i < n; ++i)
            index << m_b.u16(post + 34 + 2 * i);
        QStringList extra;
        qsizetype at = post + 34 + 2 * n;
        const qsizetype end = post + m_tables["post"].second;
        while (at < end && m_b.ok(at)) {
            const int len = m_b.u8(at);
            extra << QString::fromLatin1(m_b.d.mid(at + 1, len));
            at += 1 + len;
        }
        for (int g = 0; g < n; ++g) {
            const int i = index[g];
            const QString name = i < 258 ? QLatin1String(kMacGlyphOrder[i]) : extra.value(i - 258);
            if (!name.isEmpty())
                m_names.insert(name, g);
        }
    }

    void parseName()
    {
        const qsizetype name = m_tables["name"].first;
        const qsizetype strings = name + m_b.u16(name + 4);
        for (int i = 0; i < m_b.u16(name + 2); ++i) {
            const qsizetype rec = name + 6 + 12 * i;
            if (m_b.u16(rec + 6) != 1)
                continue;
            const int platform = m_b.u16(rec), len = m_b.u16(rec + 8);
            const qsizetype at = strings + m_b.u16(rec + 10);
            if (platform == 3) {
                QString s;
                for (int k = 0; k + 1 < len; k += 2)
                    s += QChar(char16_t(m_b.u16(at + k)));
                m_fontName = s;
                return;
            }
            if (platform == 1)
                m_fontName = QString::fromLatin1(m_b.d.mid(at, len));
        }
    }

    Bytes m_b;
    std::map<QByteArray, std::pair<qsizetype, qsizetype>> m_tables;
    int m_unitsPerEm = 1000;
    bool m_longLoca = false;
    int m_numGlyphs = 0;
    std::map<char32_t, int> m_unicode;
    std::map<int, int> m_symbol, m_mac;
    QHash<QString, int> m_names;
    QString m_fontName;
};

// --- CFF / Type 2 -----------------------------------------------------------------------------

using Index = QList<QByteArray>;

Index readIndex(const Bytes& b, qsizetype& at)
{
    Index out;
    const int count = b.u16(at);
    if (count == 0) {
        at += 2;
        return out;
    }
    const int offSize = b.u8(at + 2);
    if (offSize < 1 || offSize > 4) {
        at = b.d.size();
        return out;
    }
    const qsizetype offsets = at + 3;
    const qsizetype data = offsets + qsizetype(count + 1) * offSize - 1;
    for (int i = 0; i < count; ++i) {
        const qsizetype a = b.uN(offsets + qsizetype(i) * offSize, offSize), e = b.uN(offsets + qsizetype(i + 1) * offSize, offSize);
        out << b.d.mid(data + a, std::max<qsizetype>(0, e - a));
    }
    at = data + b.uN(offsets + qsizetype(count) * offSize, offSize);
    return out;
}

// DICT: Operator (12 x = 1200 + x) -> Operanden
std::map<int, QList<double>> readDict(const QByteArray& d)
{
    std::map<int, QList<double>> out;
    QList<double> ops;
    for (qsizetype i = 0; i < d.size();) {
        const int b0 = quint8(d[i]);
        if (b0 <= 21) {
            int op = b0;
            ++i;
            if (b0 == 12 && i < d.size())
                op = 1200 + quint8(d[i++]);
            out[op] = ops;
            ops.clear();
        } else if (b0 == 28 && i + 2 < d.size()) {
            ops << qint16((quint8(d[i + 1]) << 8) | quint8(d[i + 2]));
            i += 3;
        } else if (b0 == 29 && i + 4 < d.size()) {
            ops << qint32((quint32(quint8(d[i + 1])) << 24) | (quint32(quint8(d[i + 2])) << 16) | (quint32(quint8(d[i + 3])) << 8) | quint8(d[i + 4]));
            i += 5;
        } else if (b0 == 30) {
            // Gleitkommazahl in Halbbytes
            QString s;
            ++i;
            bool done = false;
            while (i < d.size() && !done) {
                const int byte = quint8(d[i++]);
                for (int nib : {byte >> 4, byte & 15}) {
                    if (nib <= 9)
                        s += QChar(u'0' + nib);
                    else if (nib == 10)
                        s += u'.';
                    else if (nib == 11)
                        s += u'E';
                    else if (nib == 12)
                        s += QStringLiteral("E-");
                    else if (nib == 14)
                        s += u'-';
                    else if (nib == 15) {
                        done = true;
                        break;
                    }
                }
            }
            ops << s.toDouble();
        } else if (b0 >= 32 && b0 <= 246) {
            ops << b0 - 139;
            ++i;
        } else if (b0 >= 247 && b0 <= 250 && i + 1 < d.size()) {
            ops << (b0 - 247) * 256 + quint8(d[i + 1]) + 108;
            i += 2;
        } else if (b0 >= 251 && b0 <= 254 && i + 1 < d.size()) {
            ops << -(b0 - 251) * 256 - quint8(d[i + 1]) - 108;
            i += 2;
        } else {
            ++i;
        }
    }
    return out;
}

int subrBias(qsizetype count)
{
    return count < 1240 ? 107 : count < 33900 ? 1131 : 32768;
}

class Cff final : public Program {
public:
    explicit Cff(QByteArray data) : m_b{std::move(data)} {}

    bool parse()
    {
        if (m_b.d.size() < 4 || m_b.u8(0) != 1)
            return false;
        qsizetype at = m_b.u8(2);
        const Index names = readIndex(m_b, at);
        const Index tops = readIndex(m_b, at);
        m_strings = readIndex(m_b, at);
        m_globalSubrs = readIndex(m_b, at);
        if (tops.isEmpty())
            return false;
        m_fontName = names.isEmpty() ? QString() : QString::fromLatin1(names.first());
        const auto top = readDict(tops.first());
        auto get = [&](int op, int i = 0, double def = 0) {
            const auto it = top.find(op);
            return it != top.end() && i < it->second.size() ? it->second[i] : def;
        };
        if (top.count(1207) && top.at(1207).size() == 6) {
            const QList<double>& m = top.at(1207);
            m_matrix = QTransform(m[0], m[1], m[2], m[3], m[4], m[5]);
        }
        qsizetype cs = qsizetype(get(17));
        m_charStrings = readIndex(m_b, cs);
        if (m_charStrings.isEmpty())
            return false;
        m_cid = top.count(1230);
        if (m_cid) {
            // FDArray: je Teilschrift ein Private-DICT mit eigenen Unterprogrammen
            qsizetype fdAt = qsizetype(get(1236));
            const Index fds = readIndex(m_b, fdAt);
            for (const QByteArray& fd : fds) {
                const auto d = readDict(fd);
                Private p;
                if (d.count(18) && d.at(18).size() >= 2)
                    p = readPrivate(qsizetype(d.at(18)[1]), qsizetype(d.at(18)[0]));
                m_privates << p;
            }
            readFdSelect(qsizetype(get(1237)));
        } else {
            m_privates << readPrivate(qsizetype(get(18, 1)), qsizetype(get(18, 0)));
        }
        readCharset(qsizetype(get(15)));
        if (!m_cid)
            readEncoding(qsizetype(get(16)));
        return true;
    }

    int glyphCount() const override { return int(m_charStrings.size()); }
    QTransform fontMatrix() const override { return m_matrix; }
    bool cidKeyed() const override { return m_cid; }
    int gidForName(const QString& name) const override { return m_nameToGid.value(name, -1); }
    int gidForCid(int cid) const override { return m_cid ? m_cidToGid.value(cid, cid == 0 ? 0 : -1) : cid; }
    int gidForBuiltinCode(int code) const override { return m_encoding.value(code, -1); }
    QString fontName() const override { return m_fontName; }

    QPainterPath glyph(int gid) const override
    {
        if (gid < 0 || gid >= m_charStrings.size())
            return {};
        Runner r(*this, m_cid ? m_fdSelect.value(gid, 0) : 0);
        r.run(m_charStrings[gid], 0);
        r.close();
        r.path.setFillRule(Qt::WindingFill);
        return r.path;
    }

private:
    struct Private {
        Index subrs;
    };

    Private readPrivate(qsizetype offset, qsizetype size) const
    {
        Private p;
        if (size <= 0 || !m_b.ok(offset, size))
            return p;
        const auto d = readDict(m_b.d.mid(offset, size));
        if (d.count(19) && !d.at(19).isEmpty()) {
            qsizetype at = offset + qsizetype(d.at(19)[0]);
            p.subrs = readIndex(m_b, at);
        }
        return p;
    }

    void readFdSelect(qsizetype at)
    {
        const int format = m_b.u8(at);
        const int n = int(m_charStrings.size());
        if (format == 0) {
            for (int g = 0; g < n; ++g)
                m_fdSelect.insert(g, m_b.u8(at + 1 + g));
        } else if (format == 3) {
            const int ranges = m_b.u16(at + 1);
            for (int r = 0; r < ranges; ++r) {
                const qsizetype p = at + 3 + 3 * r;
                const int first = m_b.u16(p), fd = m_b.u8(p + 2), next = m_b.u16(p + 3);
                for (int g = first; g < next && g < n; ++g)
                    m_fdSelect.insert(g, fd);
            }
        }
    }

    QString stringFor(int sid) const
    {
        if (sid < 391)
            return QLatin1String(kCffStandardStrings[sid]);
        return QString::fromLatin1(m_strings.value(sid - 391));
    }

    void readCharset(qsizetype at)
    {
        const int n = int(m_charStrings.size());
        QList<int> sids{0};
        if (at == 0) {
            // ISOAdobe: Glyphe i hat SID i
            for (int g = 1; g < n; ++g)
                sids << g;
        } else if (at > 2) {
            const int format = m_b.u8(at);
            qsizetype p = at + 1;
            while (sids.size() < n && m_b.ok(p)) {
                if (format == 0) {
                    sids << m_b.u16(p);
                    p += 2;
                } else {
                    const int first = m_b.u16(p);
                    const int left = format == 1 ? m_b.u8(p + 2) : m_b.u16(p + 2);
                    p += format == 1 ? 3 : 4;
                    for (int k = 0; k <= left && sids.size() < n; ++k)
                        sids << first + k;
                }
            }
        }
        for (int g = 0; g < sids.size(); ++g) {
            if (m_cid)
                m_cidToGid.insert(sids[g], g);
            else
                m_nameToGid.insert(stringFor(sids[g]), g);
        }
    }

    void readEncoding(qsizetype at)
    {
        if (at <= 1) {
            // Standardkodierung über die Namen
            for (int c = 0; c < 256; ++c) {
                const int g = gidForName(QLatin1String(kStandardEncoding[c]));
                if (g > 0 && at == 0)
                    m_encoding.insert(c, g);
            }
            return;
        }
        const int format = m_b.u8(at) & 0x7F;
        const bool supplements = m_b.u8(at) & 0x80;
        qsizetype p = at + 1;
        if (format == 0) {
            const int n = m_b.u8(p++);
            for (int i = 0; i < n; ++i)
                m_encoding.insert(m_b.u8(p++), i + 1);
        } else if (format == 1) {
            const int ranges = m_b.u8(p++);
            int g = 1;
            for (int r = 0; r < ranges; ++r) {
                const int first = m_b.u8(p), left = m_b.u8(p + 1);
                p += 2;
                for (int k = 0; k <= left; ++k)
                    m_encoding.insert(first + k, g++);
            }
        }
        if (supplements) {
            const int n = m_b.u8(p++);
            for (int i = 0; i < n; ++i) {
                const int code = m_b.u8(p), sid = m_b.u16(p + 1);
                p += 3;
                const int g = gidForName(stringFor(sid));
                if (g >= 0)
                    m_encoding.insert(code, g);
            }
        }
    }

    // Type-2-Charstring-Interpreter
    struct Runner {
        Runner(const Cff& f, int fontDict) : font(f), fd(fontDict) {}
        const Cff& font;
        int fd = 0;
        QPainterPath path;
        QList<double> stack;
        double x = 0, y = 0;
        int stems = 0;
        bool widthDone = false, open = false, ended = false;

        void moveTo(double dx, double dy)
        {
            close();
            x += dx;
            y += dy;
            path.moveTo(x, y);
            open = true;
        }
        void lineTo(double dx, double dy)
        {
            x += dx;
            y += dy;
            path.lineTo(x, y);
        }
        void curveTo(double dx1, double dy1, double dx2, double dy2, double dx3, double dy3)
        {
            const QPointF c1(x + dx1, y + dy1), c2(c1.x() + dx2, c1.y() + dy2);
            x = c2.x() + dx3;
            y = c2.y() + dy3;
            path.cubicTo(c1, c2, QPointF(x, y));
        }
        void close()
        {
            if (open)
                path.closeSubpath();
            open = false;
        }
        void width(bool hasExtra)
        {
            if (!widthDone && hasExtra && !stack.isEmpty())
                stack.removeFirst();
            widthDone = true;
        }

        void run(const QByteArray& code, int depth)
        {
            if (depth > 10)
                return;
            const Index& local = font.m_privates.value(fd).subrs;
            for (qsizetype i = 0; i < code.size() && !ended;) {
                const int b0 = quint8(code[i++]);
                if (b0 >= 32 || b0 == 28) {
                    if (b0 == 28 && i + 1 < code.size()) {
                        stack << qint16((quint8(code[i]) << 8) | quint8(code[i + 1]));
                        i += 2;
                    } else if (b0 <= 246) {
                        stack << b0 - 139;
                    } else if (b0 <= 250 && i < code.size()) {
                        stack << (b0 - 247) * 256 + quint8(code[i++]) + 108;
                    } else if (b0 <= 254 && i < code.size()) {
                        stack << -(b0 - 251) * 256 - quint8(code[i++]) - 108;
                    } else if (b0 == 255 && i + 3 < code.size()) {
                        const qint32 v = qint32((quint32(quint8(code[i])) << 24) | (quint32(quint8(code[i + 1])) << 16) | (quint32(quint8(code[i + 2])) << 8)
                                                | quint8(code[i + 3]));
                        stack << v / 65536.0;
                        i += 4;
                    }
                    continue;
                }
                const qsizetype n = stack.size();
                switch (b0) {
                case 1: case 3: case 18: case 23: // Stämme
                    width(n % 2 == 1);
                    stems += int(stack.size()) / 2;
                    stack.clear();
                    break;
                case 19: case 20: // hintmask, cntrmask (implizite vstem davor)
                    width(n % 2 == 1);
                    stems += int(stack.size()) / 2;
                    stack.clear();
                    i += (stems + 7) / 8;
                    break;
                case 21:
                    width(n > 2);
                    moveTo(stack.value(stack.size() - 2), stack.value(stack.size() - 1));
                    stack.clear();
                    break;
                case 22:
                    width(n > 1);
                    moveTo(stack.value(stack.size() - 1), 0);
                    stack.clear();
                    break;
                case 4:
                    width(n > 1);
                    moveTo(0, stack.value(stack.size() - 1));
                    stack.clear();
                    break;
                case 5:
                    for (qsizetype k = 0; k + 1 < n; k += 2)
                        lineTo(stack[k], stack[k + 1]);
                    stack.clear();
                    break;
                case 6: case 7: {
                    bool horizontal = b0 == 6;
                    for (qsizetype k = 0; k < n; ++k, horizontal = !horizontal)
                        horizontal ? lineTo(stack[k], 0) : lineTo(0, stack[k]);
                    stack.clear();
                    break;
                }
                case 8:
                    for (qsizetype k = 0; k + 5 < n; k += 6)
                        curveTo(stack[k], stack[k + 1], stack[k + 2], stack[k + 3], stack[k + 4], stack[k + 5]);
                    stack.clear();
                    break;
                case 24: { // rcurveline
                    qsizetype k = 0;
                    for (; k + 5 < n - 2; k += 6)
                        curveTo(stack[k], stack[k + 1], stack[k + 2], stack[k + 3], stack[k + 4], stack[k + 5]);
                    if (k + 1 < n)
                        lineTo(stack[k], stack[k + 1]);
                    stack.clear();
                    break;
                }
                case 25: { // rlinecurve
                    qsizetype k = 0;
                    for (; k + 1 < n - 6; k += 2)
                        lineTo(stack[k], stack[k + 1]);
                    if (k + 5 < n)
                        curveTo(stack[k], stack[k + 1], stack[k + 2], stack[k + 3], stack[k + 4], stack[k + 5]);
                    stack.clear();
                    break;
                }
                case 26: { // vvcurveto
                    qsizetype k = 0;
                    double dx1 = 0;
                    if (n % 2 == 1)
                        dx1 = stack[k++];
                    for (; k + 3 < n; k += 4) {
                        curveTo(dx1, stack[k], stack[k + 1], stack[k + 2], 0, stack[k + 3]);
                        dx1 = 0;
                    }
                    stack.clear();
                    break;
                }
                case 27: { // hhcurveto
                    qsizetype k = 0;
                    double dy1 = 0;
                    if (n % 2 == 1)
                        dy1 = stack[k++];
                    for (; k + 3 < n; k += 4) {
                        curveTo(stack[k], dy1, stack[k + 1], stack[k + 2], stack[k + 3], 0);
                        dy1 = 0;
                    }
                    stack.clear();
                    break;
                }
                case 30: case 31: { // vhcurveto, hvcurveto
                    bool horizontal = b0 == 31;
                    qsizetype k = 0;
                    while (k + 3 < n) {
                        const bool last = k + 4 == n - 1;
                        const double extra = last ? stack[k + 4] : 0;
                        if (horizontal)
                            curveTo(stack[k], 0, stack[k + 1], stack[k + 2], extra, stack[k + 3]);
                        else
                            curveTo(0, stack[k], stack[k + 1], stack[k + 2], stack[k + 3], extra);
                        k += last ? 5 : 4;
                        horizontal = !horizontal;
                    }
                    stack.clear();
                    break;
                }
                case 10: case 29: { // callsubr, callgsubr
                    if (stack.isEmpty())
                        return;
                    const Index& subrs = b0 == 10 ? local : font.m_globalSubrs;
                    const int index = int(stack.takeLast()) + subrBias(subrs.size());
                    if (index >= 0 && index < subrs.size())
                        run(subrs[index], depth + 1);
                    break;
                }
                case 11:
                    return;
                case 14: // endchar (mit 4 Argumenten: Akzent wie seac)
                    width(n == 1 || n == 5);
                    if (stack.size() >= 4) {
                        const double adx = stack[0], ady = stack[1];
                        const int bchar = int(stack[2]), achar = int(stack[3]);
                        close();
                        const int base = font.gidForName(QLatin1String(kStandardEncoding[std::clamp(bchar, 0, 255)]));
                        const int accent = font.gidForName(QLatin1String(kStandardEncoding[std::clamp(achar, 0, 255)]));
                        if (base >= 0)
                            path.addPath(font.glyph(base));
                        if (accent >= 0)
                            path.addPath(QTransform::fromTranslate(adx, ady).map(font.glyph(accent)));
                    }
                    close();
                    ended = true;
                    stack.clear();
                    break;
                case 12: {
                    const int b1 = i < code.size() ? quint8(code[i++]) : 0;
                    auto arg = [&](int k) { return stack.value(k); };
                    switch (b1) {
                    case 35: // flex
                        curveTo(arg(0), arg(1), arg(2), arg(3), arg(4), arg(5));
                        curveTo(arg(6), arg(7), arg(8), arg(9), arg(10), arg(11));
                        stack.clear();
                        break;
                    case 34: { // hflex
                        const double y0 = y;
                        curveTo(arg(0), 0, arg(1), arg(2), arg(3), 0);
                        curveTo(arg(4), 0, arg(5), y0 - y, arg(6), 0);
                        stack.clear();
                        break;
                    }
                    case 36: // hflex1: zweite Kurve endet auf der Anfangshöhe
                        curveTo(arg(0), arg(1), arg(2), arg(3), arg(4), 0);
                        curveTo(arg(5), 0, arg(6), arg(7), arg(8), -(arg(1) + arg(3) + arg(7)));
                        stack.clear();
                        break;
                    case 37: { // flex1
                        const double x0 = x, y0 = y;
                        double dx = 0, dy = 0;
                        for (int k = 0; k < 10; k += 2) {
                            dx += arg(k);
                            dy += arg(k + 1);
                        }
                        curveTo(arg(0), arg(1), arg(2), arg(3), arg(4), arg(5));
                        const double cx = x + arg(6), cy = y + arg(7);
                        const double c2x = cx + arg(8), c2y = cy + arg(9);
                        double ex, ey;
                        if (std::abs(dx) > std::abs(dy)) {
                            ex = c2x + arg(10);
                            ey = y0;
                        } else {
                            ex = x0;
                            ey = c2y + arg(10);
                        }
                        path.cubicTo(QPointF(cx, cy), QPointF(c2x, c2y), QPointF(ex, ey));
                        x = ex;
                        y = ey;
                        stack.clear();
                        break;
                    }
                    case 9: if (!stack.isEmpty()) stack.last() = std::abs(stack.last()); break;
                    case 10: if (stack.size() >= 2) { const double b = stack.takeLast(); stack.last() += b; } break;
                    case 11: if (stack.size() >= 2) { const double b = stack.takeLast(); stack.last() -= b; } break;
                    case 12: if (stack.size() >= 2) { const double b = stack.takeLast(); stack.last() = b != 0 ? stack.last() / b : 0; } break;
                    case 14: if (!stack.isEmpty()) stack.last() = -stack.last(); break;
                    case 18: if (!stack.isEmpty()) stack.removeLast(); break;
                    case 24: if (stack.size() >= 2) { const double b = stack.takeLast(); stack.last() *= b; } break;
                    case 26: if (!stack.isEmpty()) stack.last() = std::sqrt(std::abs(stack.last())); break;
                    case 27: if (!stack.isEmpty()) stack << stack.last(); break;
                    case 28: if (stack.size() >= 2) std::swap(stack[stack.size() - 1], stack[stack.size() - 2]); break;
                    default: stack.clear(); break;
                    }
                    break;
                }
                default:
                    stack.clear();
                    break;
                }
            }
        }
    };

    Bytes m_b;
    Index m_strings, m_globalSubrs, m_charStrings;
    QList<Private> m_privates;
    QHash<int, int> m_fdSelect, m_cidToGid, m_encoding;
    QHash<QString, int> m_nameToGid;
    QTransform m_matrix = QTransform::fromScale(0.001, 0.001);
    bool m_cid = false;
    QString m_fontName;
};

// --- Type 1 -----------------------------------------------------------------------------------

QByteArray decrypt(const QByteArray& in, quint16 r, int skip)
{
    QByteArray out;
    out.reserve(in.size());
    for (char ch : in) {
        const quint8 c = quint8(ch);
        out.append(char(c ^ (r >> 8)));
        r = quint16((c + r) * 52845 + 22719);
    }
    return skip > 0 ? out.mid(skip) : out;
}

class Type1 final : public Program {
public:
    bool parse(QByteArray data)
    {
        // PFB-Segmente (0x80 0x01/0x02 Länge) zusammenfügen
        if (data.size() > 6 && quint8(data[0]) == 0x80) {
            QByteArray joined;
            qsizetype at = 0;
            while (at + 6 <= data.size() && quint8(data[at]) == 0x80 && quint8(data[at + 1]) != 3) {
                const quint32 len = quint8(data[at + 2]) | (quint8(data[at + 3]) << 8) | (quint8(data[at + 4]) << 16) | (quint32(quint8(data[at + 5])) << 24);
                joined += data.mid(at + 6, len);
                at += 6 + qsizetype(len);
            }
            data = joined;
        }
        const qsizetype eexec = data.indexOf("eexec");
        if (eexec < 0)
            return false;
        const QByteArray clear = data.left(eexec);
        parseClear(clear);
        qsizetype start = eexec + 5;
        while (start < data.size() && (data[start] == '\r' || data[start] == '\n' || data[start] == ' ' || data[start] == '\t'))
            ++start;
        QByteArray encrypted = data.mid(start);
        // Hexadezimal oder binär?
        bool hex = encrypted.size() >= 4;
        for (int k = 0; k < 4 && k < encrypted.size(); ++k)
            hex = hex && std::isxdigit(quint8(encrypted[k]));
        if (hex) {
            QByteArray digits;
            for (char ch : encrypted)
                if (std::isxdigit(quint8(ch)))
                    digits += ch;
            encrypted = QByteArray::fromHex(digits);
        }
        const QByteArray priv = decrypt(encrypted, 55665, 4);
        parsePrivate(priv);
        return !m_charStrings.isEmpty();
    }

    int glyphCount() const override { return int(m_charStrings.size()); }
    QTransform fontMatrix() const override { return m_matrix; }
    int gidForName(const QString& name) const override { return m_nameToGid.value(name, -1); }
    int gidForBuiltinCode(int code) const override { return gidForName(m_encoding.value(code)); }
    QString fontName() const override { return m_fontName; }

    QPainterPath glyph(int gid) const override
    {
        if (gid < 0 || gid >= m_charStrings.size())
            return {};
        Runner r(*this);
        r.run(m_charStrings[gid], 0);
        r.close();
        r.path.setFillRule(Qt::WindingFill);
        return r.path;
    }

private:
    void parseClear(const QByteArray& clear)
    {
        static const QRegularExpression matrix(QStringLiteral(R"(/FontMatrix\s*\[\s*([-\d.eE]+)\s+([-\d.eE]+)\s+([-\d.eE]+)\s+([-\d.eE]+)\s+([-\d.eE]+)\s+([-\d.eE]+)\s*\])"));
        const QString text = QString::fromLatin1(clear);
        if (const auto m = matrix.match(text); m.hasMatch())
            m_matrix = QTransform(m.captured(1).toDouble(), m.captured(2).toDouble(), m.captured(3).toDouble(), m.captured(4).toDouble(),
                                  m.captured(5).toDouble(), m.captured(6).toDouble());
        static const QRegularExpression name(QStringLiteral(R"(/FontName\s*/(\S+))"));
        m_fontName = name.match(text).captured(1);
        if (text.contains(QLatin1String("/Encoding StandardEncoding"))) {
            for (int c = 0; c < 256; ++c)
                m_encoding.insert(c, QLatin1String(kStandardEncoding[c]));
        } else {
            static const QRegularExpression entry(QStringLiteral(R"(dup\s+(\d+)\s*/(\S+)\s+put)"));
            for (auto it = entry.globalMatch(text); it.hasNext();) {
                const auto m = it.next();
                m_encoding.insert(m.captured(1).toInt(), m.captured(2));
            }
        }
    }

    // Binärblock nach "<Länge> RD " (bzw. -| ) lesen
    static QByteArray binaryAfter(const QByteArray& d, qsizetype& at, int len)
    {
        // at steht hinter dem RD-Wort; genau ein Leerzeichen folgt
        const QByteArray out = d.mid(at + 1, len);
        at += 1 + len;
        return out;
    }

    void parsePrivate(const QByteArray& p)
    {
        static const QRegularExpression lenIV(QStringLiteral(R"(/lenIV\s+(-?\d+))"));
        const QString text = QString::fromLatin1(p);
        if (const auto m = lenIV.match(text); m.hasMatch())
            m_lenIV = m.captured(1).toInt();
        // Unterprogramme: dup <i> <n> RD <bytes> NP
        const qsizetype subrs = p.indexOf("/Subrs");
        const qsizetype chars = p.indexOf("/CharStrings");
        if (subrs >= 0) {
            static const QRegularExpression item(QStringLiteral(R"(dup\s+(\d+)\s+(\d+)\s+(RD|-\|))"));
            qsizetype at = subrs;
            const qsizetype end = chars > subrs ? chars : p.size();
            while (at < end) {
                const auto m = item.match(text, at);
                if (!m.hasMatch() || m.capturedStart() >= end)
                    break;
                qsizetype pos = m.capturedEnd();
                const QByteArray bytes = binaryAfter(p, pos, m.captured(2).toInt());
                const int index = m.captured(1).toInt();
                if (index >= 0 && index < 65536) {
                    if (m_subrs.size() <= index)
                        m_subrs.resize(index + 1);
                    m_subrs[index] = decrypt(bytes, 4330, m_lenIV);
                }
                at = pos;
            }
        }
        if (chars >= 0) {
            static const QRegularExpression item(QStringLiteral(R"(/([^\s/\[\]{}()<>]+)\s+(\d+)\s+(RD|-\|))"));
            qsizetype at = chars + 12;
            while (at < p.size()) {
                const auto m = item.match(text, at);
                if (!m.hasMatch())
                    break;
                qsizetype pos = m.capturedEnd();
                const QByteArray bytes = binaryAfter(p, pos, m.captured(2).toInt());
                m_nameToGid.insert(m.captured(1), int(m_charStrings.size()));
                m_charStrings << decrypt(bytes, 4330, m_lenIV);
                at = pos;
            }
        }
    }

    struct Runner {
        explicit Runner(const Type1& f) : font(f) {}
        const Type1& font;
        QPainterPath path;
        QList<double> stack, ps;
        double x = 0, y = 0, sbx = 0;
        bool open = false, ended = false, flexing = false;
        QList<QPointF> flex;

        void moveTo(double dx, double dy)
        {
            x += dx;
            y += dy;
            if (flexing) {
                flex << QPointF(x, y);
                return;
            }
            close();
            path.moveTo(x, y);
            open = true;
        }
        void lineTo(double dx, double dy)
        {
            x += dx;
            y += dy;
            path.lineTo(x, y);
        }
        void curveTo(double dx1, double dy1, double dx2, double dy2, double dx3, double dy3)
        {
            const QPointF c1(x + dx1, y + dy1), c2(c1.x() + dx2, c1.y() + dy2);
            x = c2.x() + dx3;
            y = c2.y() + dy3;
            path.cubicTo(c1, c2, QPointF(x, y));
        }
        void close()
        {
            if (open)
                path.closeSubpath();
            open = false;
        }

        void run(const QByteArray& code, int depth)
        {
            if (depth > 10)
                return;
            for (qsizetype i = 0; i < code.size() && !ended;) {
                const int v = quint8(code[i++]);
                if (v >= 32) {
                    if (v <= 246) {
                        stack << v - 139;
                    } else if (v <= 250 && i < code.size()) {
                        stack << (v - 247) * 256 + quint8(code[i++]) + 108;
                    } else if (v <= 254 && i < code.size()) {
                        stack << -(v - 251) * 256 - quint8(code[i++]) - 108;
                    } else if (v == 255 && i + 3 < code.size()) {
                        stack << qint32((quint32(quint8(code[i])) << 24) | (quint32(quint8(code[i + 1])) << 16) | (quint32(quint8(code[i + 2])) << 8)
                                        | quint8(code[i + 3]));
                        i += 4;
                    }
                    continue;
                }
                auto a = [&](int k) { return stack.value(k); };
                switch (v) {
                case 13: // hsbw
                    sbx = x = a(0);
                    y = 0;
                    stack.clear();
                    break;
                case 21: moveTo(a(0), a(1)); stack.clear(); break;
                case 22: moveTo(a(0), 0); stack.clear(); break;
                case 4: moveTo(0, a(0)); stack.clear(); break;
                case 5: lineTo(a(0), a(1)); stack.clear(); break;
                case 6: lineTo(a(0), 0); stack.clear(); break;
                case 7: lineTo(0, a(0)); stack.clear(); break;
                case 8: curveTo(a(0), a(1), a(2), a(3), a(4), a(5)); stack.clear(); break;
                case 30: curveTo(0, a(0), a(1), a(2), a(3), 0); stack.clear(); break;
                case 31: curveTo(a(0), 0, a(1), a(2), 0, a(3)); stack.clear(); break;
                case 9: close(); stack.clear(); break;
                case 1: case 3: stack.clear(); break;
                case 10: {
                    if (stack.isEmpty())
                        return;
                    const int index = int(stack.takeLast());
                    if (index >= 0 && index < font.m_subrs.size())
                        run(font.m_subrs[index], depth + 1);
                    break;
                }
                case 11: return;
                case 14: close(); ended = true; break;
                case 12: {
                    const int w = i < code.size() ? quint8(code[i++]) : 0;
                    switch (w) {
                    case 7: // sbw
                        sbx = x = a(0);
                        y = a(1);
                        stack.clear();
                        break;
                    case 6: { // seac: Grundzeichen + Akzent
                        const double asb = a(0), adx = a(1), ady = a(2);
                        const int bchar = int(a(3)), achar = int(a(4));
                        const double baseSbx = sbx;
                        close();
                        const int base = font.gidForName(QLatin1String(kStandardEncoding[std::clamp(bchar, 0, 255)]));
                        const int accent = font.gidForName(QLatin1String(kStandardEncoding[std::clamp(achar, 0, 255)]));
                        if (base >= 0)
                            path.addPath(font.glyph(base));
                        if (accent >= 0)
                            path.addPath(QTransform::fromTranslate(adx - asb + baseSbx, ady).map(font.glyph(accent)));
                        ended = true;
                        break;
                    }
                    case 12: // div
                        if (stack.size() >= 2) {
                            const double b = stack.takeLast();
                            stack.last() = b != 0 ? stack.last() / b : 0;
                        }
                        break;
                    case 16: { // callothersubr
                        const int other = int(stack.isEmpty() ? 0 : stack.takeLast());
                        const int n = int(stack.isEmpty() ? 0 : stack.takeLast());
                        QList<double> args;
                        for (int k = 0; k < n && !stack.isEmpty(); ++k)
                            args.prepend(stack.takeLast());
                        if (other == 1) {
                            flexing = true;
                            flex.clear();
                        } else if (other == 0 && flexing) {
                            flexing = false;
                            // Bezugspunkt + 6 Punkte -> zwei Kurven
                            if (flex.size() >= 7) {
                                path.cubicTo(flex[1], flex[2], flex[3]);
                                path.cubicTo(flex[4], flex[5], flex[6]);
                                x = flex[6].x();
                                y = flex[6].y();
                            }
                            ps << y << x;
                        } else if (other == 3) {
                            ps << 3;
                        } else {
                            for (qsizetype k = args.size() - 1; k >= 0; --k)
                                ps << args[k];
                        }
                        break;
                    }
                    case 17: // pop
                        stack << (ps.isEmpty() ? 0.0 : ps.takeLast());
                        break;
                    case 33: // setcurrentpoint
                        x = a(0);
                        y = a(1);
                        stack.clear();
                        break;
                    default:
                        stack.clear();
                        break;
                    }
                    break;
                }
                default:
                    stack.clear();
                    break;
                }
            }
        }
    };

    QList<QByteArray> m_subrs, m_charStrings;
    QHash<QString, int> m_nameToGid;
    QHash<int, QString> m_encoding;
    QTransform m_matrix = QTransform::fromScale(0.001, 0.001);
    int m_lenIV = 4;
    QString m_fontName;
};

} // namespace

QString unicodeForGlyphName(const QString& name)
{
    QString base = name.section(QLatin1Char('.'), 0, 0).section(QLatin1Char('_'), 0, 0);
    if (const auto it = glyphList().constFind(base); it != glyphList().constEnd())
        return QString(QChar(*it));
    static const QRegularExpression uni(QStringLiteral("^uni([0-9A-Fa-f]{4})"));
    static const QRegularExpression u(QStringLiteral("^u([0-9A-Fa-f]{4,6})$"));
    if (const auto m = uni.match(base); m.hasMatch())
        return QString(QChar(char16_t(m.captured(1).toUInt(nullptr, 16))));
    if (const auto m = u.match(base); m.hasMatch()) {
        const char32_t c = char32_t(m.captured(1).toUInt(nullptr, 16));
        return QString::fromUcs4(&c, 1);
    }
    return {};
}

QString standardEncoding(int code)
{
    return code >= 0 && code < 256 ? QLatin1String(kStandardEncoding[code]) : QString();
}

QString winAnsiEncoding(int code)
{
    return code >= 0 && code < 256 ? QLatin1String(kWinAnsiEncoding[code]) : QString();
}

QString macRomanEncoding(int code)
{
    return code >= 0 && code < 256 ? QLatin1String(kMacRomanEncoding[code]) : QString();
}

std::unique_ptr<Program> Program::load(const QByteArray& data)
{
    if (data.size() < 4)
        return nullptr;
    const quint32 magic = Bytes{data}.u32(0);
    if (magic == 0x4F54544F) { // "OTTO": OpenType mit CFF
        const Bytes b{data};
        for (int i = 0; i < b.u16(4); ++i) {
            const qsizetype at = 12 + 16 * i;
            if (data.mid(at, 4) == "CFF ") {
                auto cff = std::make_unique<Cff>(data.mid(b.u32(at + 8), b.u32(at + 12)));
                if (cff->parse())
                    return cff;
            }
        }
        return nullptr;
    }
    if (magic == 0x00010000 || magic == 0x74727565) {
        auto tt = std::make_unique<TrueType>(data);
        return tt->parse() ? std::move(tt) : nullptr;
    }
    if (quint8(data[0]) == 1 && quint8(data[1]) == 0) {
        auto cff = std::make_unique<Cff>(data);
        return cff->parse() ? std::move(cff) : nullptr;
    }
    if (data.startsWith("%!") || quint8(data[0]) == 0x80) {
        auto t1 = std::make_unique<Type1>();
        return t1->parse(data) ? std::move(t1) : nullptr;
    }
    return nullptr;
}

} // namespace vek::fontprog
