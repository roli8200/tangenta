#include "PdfImport.h"

#include "CurveObject.h"
#include "FontMatch.h"
#include "FontPrograms.h"
#include "ImageObject.h"
#include "ImagePool.h"
#include "PathOps.h"
#include "PowerClipObject.h"
#include "TextObject.h"

#include <QCoreApplication>
#include <QDir>
#include <QGuiApplication>
#include <QLibrary>
#include <QMutex>
#include <QScopeGuard>
#include <QFile>
#include <QHash>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTransform>

#include <miniz.h>

#include <array>
#include <clocale>
#include <cmath>
#include <map>
#include <memory>
#include <vector>

namespace vek::pdf {

namespace {

// --- Objekte -------------------------------------------------------------------------------------

struct Obj {
    enum class Type { Null, Bool, Num, Str, Name, Array, Dict, Ref, Op };
    Type t = Type::Null;
    double n = 0;
    bool b = false;
    QByteArray s; // Zeichenkette, Name, Operator
    std::shared_ptr<std::vector<Obj>> arr;
    std::shared_ptr<std::map<QByteArray, Obj>> dict;
    int ref = 0;
    QByteArray stream; // roh (noch gefiltert)
    bool hasStream = false;

    bool isNum() const { return t == Type::Num; }
    bool isName(const char* v = nullptr) const { return t == Type::Name && (!v || s == v); }
    const Obj& operator[](const char* key) const
    {
        static const Obj none;
        if (t != Type::Dict || !dict)
            return none;
        const auto it = dict->find(QByteArray(key));
        return it == dict->end() ? none : it->second;
    }
    int size() const { return arr ? int(arr->size()) : 0; }
    const Obj& at(int i) const
    {
        static const Obj none;
        return arr && i >= 0 && i < int(arr->size()) ? (*arr)[std::size_t(i)] : none;
    }
};

bool isWhite(char c)
{
    return c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == '\f' || c == '\0';
}

bool isDelim(char c)
{
    return c == '(' || c == ')' || c == '<' || c == '>' || c == '[' || c == ']' || c == '{' || c == '}' || c == '/' || c == '%';
}

class Lexer {
public:
    Lexer(const QByteArray& d, qsizetype pos = 0) : m_d(d), m_pos(pos) {}
    qsizetype pos() const { return m_pos; }
    void setPos(qsizetype p) { m_pos = p; }
    bool atEnd()
    {
        skipSpace();
        return m_pos >= m_d.size();
    }

    void skipSpace()
    {
        while (m_pos < m_d.size()) {
            const char c = m_d[m_pos];
            if (isWhite(c)) {
                ++m_pos;
            } else if (c == '%') {
                while (m_pos < m_d.size() && m_d[m_pos] != '\n' && m_d[m_pos] != '\r')
                    ++m_pos;
            } else {
                break;
            }
        }
    }

    // Ein Objekt oder ein Operator (Inhaltsströme)
    Obj next(int depth = 0)
    {
        Obj o;
        skipSpace();
        if (m_pos >= m_d.size() || depth > 200)
            return o;
        const char c = m_d[m_pos];
        if (c == '/') {
            ++m_pos;
            QByteArray name;
            while (m_pos < m_d.size() && !isWhite(m_d[m_pos]) && !isDelim(m_d[m_pos])) {
                if (m_d[m_pos] == '#' && m_pos + 2 < m_d.size()) {
                    name += char(m_d.mid(m_pos + 1, 2).toInt(nullptr, 16));
                    m_pos += 3;
                } else {
                    name += m_d[m_pos++];
                }
            }
            o.t = Obj::Type::Name;
            o.s = name;
            return o;
        }
        if (c == '(') {
            o.t = Obj::Type::Str;
            o.s = literalString();
            return o;
        }
        if (c == '<' && m_pos + 1 < m_d.size() && m_d[m_pos + 1] == '<') {
            m_pos += 2;
            o.t = Obj::Type::Dict;
            o.dict = std::make_shared<std::map<QByteArray, Obj>>();
            while (true) {
                skipSpace();
                if (m_pos >= m_d.size())
                    break;
                if (m_d[m_pos] == '>' && m_pos + 1 < m_d.size() && m_d[m_pos + 1] == '>') {
                    m_pos += 2;
                    break;
                }
                const Obj key = next(depth + 1);
                if (key.t != Obj::Type::Name) {
                    if (key.t == Obj::Type::Null)
                        break;
                    continue;
                }
                (*o.dict)[key.s] = next(depth + 1);
            }
            return o;
        }
        if (c == '<') {
            ++m_pos;
            QByteArray hex;
            while (m_pos < m_d.size() && m_d[m_pos] != '>') {
                if (!isWhite(m_d[m_pos]))
                    hex += m_d[m_pos];
                ++m_pos;
            }
            ++m_pos;
            if (hex.size() % 2)
                hex += '0';
            o.t = Obj::Type::Str;
            o.s = QByteArray::fromHex(hex);
            return o;
        }
        if (c == '[') {
            ++m_pos;
            o.t = Obj::Type::Array;
            o.arr = std::make_shared<std::vector<Obj>>();
            while (true) {
                skipSpace();
                if (m_pos >= m_d.size())
                    break;
                if (m_d[m_pos] == ']') {
                    ++m_pos;
                    break;
                }
                Obj item = next(depth + 1);
                if (item.t == Obj::Type::Op && item.s == "R" && o.arr->size() >= 2) {
                    // "n g R" im Array
                    Obj ref;
                    ref.t = Obj::Type::Ref;
                    ref.ref = int((*o.arr)[o.arr->size() - 2].n);
                    o.arr->pop_back();
                    o.arr->pop_back();
                    o.arr->push_back(ref);
                    continue;
                }
                o.arr->push_back(item);
            }
            return o;
        }
        if (c == ']' || c == '>' || c == ')' || c == '{' || c == '}') {
            ++m_pos;
            o.t = Obj::Type::Op;
            o.s = QByteArray(1, c);
            return o;
        }
        // Zahl oder Wort
        QByteArray word;
        while (m_pos < m_d.size() && !isWhite(m_d[m_pos]) && !isDelim(m_d[m_pos]))
            word += m_d[m_pos++];
        bool ok = false;
        const double v = word.toDouble(&ok);
        if (ok || (!word.isEmpty() && (word[0] == '.' || word[0] == '-' || word[0] == '+'))) {
            o.t = Obj::Type::Num;
            o.n = ok ? v : 0.0;
            // "n g R" (Verweis) erkennen
            const qsizetype save = m_pos;
            Lexer look(m_d, m_pos);
            const Obj gen = look.peekNumber();
            if (gen.isNum()) {
                look.skipSpace();
                if (look.m_pos < m_d.size() && m_d[look.m_pos] == 'R'
                    && (look.m_pos + 1 >= m_d.size() || isWhite(m_d[look.m_pos + 1]) || isDelim(m_d[look.m_pos + 1]))) {
                    o.t = Obj::Type::Ref;
                    o.ref = int(v);
                    m_pos = look.m_pos + 1;
                    return o;
                }
            }
            m_pos = save;
            return o;
        }
        if (word == "true" || word == "false") {
            o.t = Obj::Type::Bool;
            o.b = word == "true";
            return o;
        }
        if (word == "null")
            return o;
        o.t = Obj::Type::Op;
        o.s = word;
        if (word.isEmpty())
            ++m_pos;
        return o;
    }

    Obj peekNumber()
    {
        skipSpace();
        QByteArray word;
        qsizetype p = m_pos;
        while (p < m_d.size() && !isWhite(m_d[p]) && !isDelim(m_d[p]))
            word += m_d[p++];
        Obj o;
        bool ok = false;
        const double v = word.toInt(&ok);
        if (ok) {
            o.t = Obj::Type::Num;
            o.n = v;
            m_pos = p;
        }
        return o;
    }

    QByteArray literalString()
    {
        ++m_pos; // (
        QByteArray out;
        int depth = 1;
        while (m_pos < m_d.size()) {
            const char c = m_d[m_pos++];
            if (c == '\\' && m_pos < m_d.size()) {
                const char e = m_d[m_pos++];
                switch (e) {
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case '\r':
                    if (m_pos < m_d.size() && m_d[m_pos] == '\n')
                        ++m_pos;
                    break;
                case '\n': break;
                default:
                    if (e >= '0' && e <= '7') {
                        int v = e - '0';
                        for (int k = 0; k < 2 && m_pos < m_d.size() && m_d[m_pos] >= '0' && m_d[m_pos] <= '7'; ++k)
                            v = v * 8 + (m_d[m_pos++] - '0');
                        out += char(v);
                    } else {
                        out += e;
                    }
                }
            } else if (c == '(') {
                ++depth;
                out += c;
            } else if (c == ')') {
                if (--depth == 0)
                    break;
                out += c;
            } else {
                out += c;
            }
        }
        return out;
    }

    const QByteArray& m_d;
    qsizetype m_pos;
};

// --- Filter -------------------------------------------------------------------------------------

QByteArray inflate(const QByteArray& in)
{
    size_t outLen = 0;
    void* p = tinfl_decompress_mem_to_heap(in.constData(), size_t(in.size()), &outLen, TINFL_FLAG_PARSE_ZLIB_HEADER);
    if (!p)
        p = tinfl_decompress_mem_to_heap(in.constData(), size_t(in.size()), &outLen, 0);
    if (!p) {
        // Abgeschnittene Ströme: so viel wie möglich retten
        std::vector<char> buf(std::size_t(std::max<qsizetype>(in.size() * 8, 65536)));
        mz_stream s{};
        s.next_in = reinterpret_cast<const unsigned char*>(in.constData());
        s.avail_in = mz_uint32(in.size());
        if (mz_inflateInit(&s) != MZ_OK)
            return {};
        QByteArray out;
        int r = MZ_OK;
        do {
            s.next_out = reinterpret_cast<unsigned char*>(buf.data());
            s.avail_out = mz_uint32(buf.size());
            r = mz_inflate(&s, MZ_NO_FLUSH);
            out.append(buf.data(), qsizetype(buf.size() - s.avail_out));
        } while (r == MZ_OK && s.avail_in > 0);
        mz_inflateEnd(&s);
        return out;
    }
    QByteArray out(static_cast<const char*>(p), qsizetype(outLen));
    mz_free(p);
    return out;
}

QByteArray asciiHex(const QByteArray& in)
{
    QByteArray hex;
    for (char c : in) {
        if (c == '>')
            break;
        if (!isWhite(c))
            hex += c;
    }
    if (hex.size() % 2)
        hex += '0';
    return QByteArray::fromHex(hex);
}

QByteArray ascii85(const QByteArray& in)
{
    QByteArray out;
    quint32 tuple = 0;
    int count = 0;
    for (qsizetype i = 0; i < in.size(); ++i) {
        const char c = in[i];
        if (c == '~')
            break;
        if (isWhite(c))
            continue;
        if (c == 'z' && count == 0) {
            out.append(4, '\0');
            continue;
        }
        tuple = tuple * 85 + quint32(c - '!');
        if (++count == 5) {
            out += char(tuple >> 24);
            out += char(tuple >> 16);
            out += char(tuple >> 8);
            out += char(tuple);
            tuple = 0;
            count = 0;
        }
    }
    if (count > 1) {
        for (int k = count; k < 5; ++k)
            tuple = tuple * 85 + 84;
        for (int k = 0; k < count - 1; ++k)
            out += char(tuple >> (24 - 8 * k));
    }
    return out;
}

QByteArray lzwDecode(const QByteArray& in, bool early = true)
{
    QByteArray out;
    std::vector<QByteArray> table;
    auto reset = [&] {
        table.clear();
        for (int i = 0; i < 256; ++i)
            table.push_back(QByteArray(1, char(i)));
        table.push_back({});
        table.push_back({});
    };
    reset();
    int codeLen = 9;
    quint32 buffer = 0;
    int bits = 0;
    QByteArray prev;
    for (qsizetype i = 0; i < in.size(); ++i) {
        buffer = (buffer << 8) | quint8(in[i]);
        bits += 8;
        while (bits >= codeLen) {
            const int code = int((buffer >> (bits - codeLen)) & ((1u << codeLen) - 1));
            bits -= codeLen;
            if (code == 256) {
                reset();
                codeLen = 9;
                prev.clear();
                continue;
            }
            if (code == 257)
                return out;
            QByteArray entry;
            if (code < int(table.size()))
                entry = table[std::size_t(code)];
            else if (!prev.isEmpty())
                entry = prev + prev[0];
            else
                return out;
            out += entry;
            if (!prev.isEmpty())
                table.push_back(prev + entry[0]);
            prev = entry;
            const int limit = int(table.size()) + (early ? 1 : 0);
            if (limit >= (1 << codeLen) && codeLen < 12)
                ++codeLen;
        }
    }
    return out;
}

QByteArray runLength(const QByteArray& in)
{
    QByteArray out;
    for (qsizetype i = 0; i < in.size();) {
        const int n = quint8(in[i++]);
        if (n == 128)
            break;
        if (n < 128) {
            out += in.mid(i, n + 1);
            i += n + 1;
        } else if (i < in.size()) {
            out.append(257 - n, in[i++]);
        }
    }
    return out;
}

// PNG-Vorhersage (Predictor >= 10) rückgängig machen
QByteArray unpredict(const QByteArray& in, int columns, int colors, int bpc)
{
    const int bpp = std::max(1, colors * bpc / 8);
    const int row = (columns * colors * bpc + 7) / 8;
    QByteArray out;
    QByteArray prior(row, 0);
    for (qsizetype i = 0; i + row < in.size() + 1 && i < in.size(); i += row + 1) {
        const int type = quint8(in[i]);
        QByteArray cur = in.mid(i + 1, row);
        cur.resize(row, 0);
        for (int x = 0; x < row; ++x) {
            const int a = x >= bpp ? quint8(cur[x - bpp]) : 0, b = quint8(prior[x]), c = x >= bpp ? quint8(prior[x - bpp]) : 0;
            int v = quint8(cur[x]);
            switch (type) {
            case 1: v += a; break;
            case 2: v += b; break;
            case 3: v += (a + b) / 2; break;
            case 4: {
                const int p = a + b - c, pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
                v += pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
                break;
            }
            default: break;
            }
            cur[x] = char(v);
        }
        out += cur;
        prior = cur;
    }
    return out;
}

// --- Dokument ------------------------------------------------------------------------------------

class Document {
public:
    explicit Document(const QByteArray& data) : m_data(data) {}

    bool load(QString* error)
    {
        static const QRegularExpression objRe(QStringLiteral(R"((\d+)\s+(\d+)\s+obj\b)"));
        const QString text = QString::fromLatin1(m_data);
        for (auto it = objRe.globalMatch(text); it.hasNext();) {
            const auto m = it.next();
            Lexer lex(m_data, m.capturedEnd());
            Obj o = lex.next();
            readStream(lex, o);
            m_objects[m.captured(1).toInt()] = o;
            if (o["Type"].isName("XRef") || o["Encrypt"].t != Obj::Type::Null)
                m_trailers << o;
        }
        // Objektströme
        for (auto it = m_objects.begin(); it != m_objects.end(); ++it) {
            const Obj o = it.value();
            if (!o["Type"].isName("ObjStm"))
                continue;
            const QByteArray body = decode(o);
            Lexer head(body);
            const int n = int(o["N"].n), first = int(o["First"].n);
            QList<std::pair<int, int>> entries;
            for (int k = 0; k < n; ++k) {
                const Obj num = head.next(), off = head.next();
                entries << std::pair{int(num.n), int(off.n)};
            }
            for (const auto& [num, off] : entries) {
                if (m_objects.contains(num))
                    continue;
                Lexer lex(body, first + off);
                m_pending.insert(num, lex.next());
            }
        }
        for (auto it = m_pending.begin(); it != m_pending.end(); ++it)
            m_objects.insert(it.key(), it.value());
        // Trailer (klassisch)
        qsizetype t = 0;
        while ((t = m_data.indexOf("trailer", t)) >= 0) {
            Lexer lex(m_data, t + 7);
            m_trailers << lex.next();
            t += 7;
        }
        for (const Obj& tr : std::as_const(m_trailers))
            if (tr["Encrypt"].t != Obj::Type::Null) {
                if (error)
                    *error = QObject::tr("Die PDF-Datei ist verschlüsselt.");
                return false;
            }
        return !m_objects.isEmpty();
    }

    Obj resolve(const Obj& o, int depth = 0) const
    {
        if (o.t != Obj::Type::Ref || depth > 32)
            return o;
        return resolve(m_objects.value(o.ref), depth + 1);
    }

    QByteArray decode(const Obj& stream) const
    {
        QByteArray data = stream.stream;
        Obj filters = resolve(stream["Filter"]);
        Obj params = resolve(stream["DecodeParms"]);
        QList<QByteArray> names;
        QList<Obj> parms;
        if (filters.t == Obj::Type::Name) {
            names << filters.s;
            parms << params;
        } else {
            for (int i = 0; i < filters.size(); ++i) {
                names << resolve(filters.at(i)).s;
                parms << (params.t == Obj::Type::Array ? resolve(params.at(i)) : params);
            }
        }
        for (int i = 0; i < names.size(); ++i) {
            const QByteArray& f = names[i];
            if (f == "FlateDecode" || f == "Fl")
                data = inflate(data);
            else if (f == "ASCIIHexDecode" || f == "AHx")
                data = asciiHex(data);
            else if (f == "ASCII85Decode" || f == "A85")
                data = ascii85(data);
            else if (f == "LZWDecode" || f == "LZW")
                data = lzwDecode(data, resolve(parms[i]["EarlyChange"]).t != Obj::Type::Num || resolve(parms[i]["EarlyChange"]).n != 0);
            else if (f == "RunLengthDecode" || f == "RL")
                data = runLength(data);
            else
                break; // DCTDecode usw.: Bilddaten bleiben kodiert
            const Obj p = parms.value(i);
            if (resolve(p["Predictor"]).n >= 10)
                data = unpredict(data, std::max(1, int(resolve(p["Columns"]).isNum() ? resolve(p["Columns"]).n : 1)),
                                 std::max(1, int(resolve(p["Colors"]).isNum() ? resolve(p["Colors"]).n : 1)),
                                 std::max(1, int(resolve(p["BitsPerComponent"]).isNum() ? resolve(p["BitsPerComponent"]).n : 8)));
        }
        return data;
    }

    // Letzter Filter (z.B. DCTDecode), wenn er nicht entfernt wurde
    QByteArray imageFilter(const Obj& stream) const
    {
        const Obj f = resolve(stream["Filter"]);
        if (f.t == Obj::Type::Name)
            return f.s;
        return f.size() ? resolve(f.at(f.size() - 1)).s : QByteArray();
    }

    QList<Obj> pages() const
    {
        Obj root;
        for (const Obj& tr : m_trailers)
            if (tr["Root"].t != Obj::Type::Null)
                root = resolve(tr["Root"]);
        if (root.t != Obj::Type::Dict)
            for (const Obj& o : m_objects)
                if (o["Type"].isName("Catalog"))
                    root = o;
        QList<Obj> out;
        collectPages(resolve(root["Pages"]), Obj(), Obj(), out, 0);
        if (out.isEmpty()) // kaputter Baum: alle Seitenobjekte in Dateireihenfolge
            for (const Obj& o : m_objects)
                if (o["Type"].isName("Page"))
                    out << o;
        return out;
    }

private:
    void readStream(Lexer& lex, Obj& o) const
    {
        const qsizetype save = lex.pos();
        lex.skipSpace();
        if (!m_data.mid(lex.pos(), 6).startsWith("stream")) {
            lex.setPos(save);
            return;
        }
        qsizetype start = lex.pos() + 6;
        if (start < m_data.size() && m_data[start] == '\r')
            ++start;
        if (start < m_data.size() && m_data[start] == '\n')
            ++start;
        const Obj len = o["Length"];
        qsizetype length = len.isNum() ? qsizetype(len.n) : -1;
        if (length < 0 || start + length > m_data.size() || m_data.indexOf("endstream", start + length) > start + length + 4)
            length = m_data.indexOf("endstream", start) - start;
        if (length < 0)
            return;
        o.stream = m_data.mid(start, length);
        o.hasStream = true;
    }

    void collectPages(const Obj& node, Obj mediaBox, Obj resources, QList<Obj>& out, int depth) const
    {
        if (node.t != Obj::Type::Dict || depth > 32)
            return;
        if (node["MediaBox"].t != Obj::Type::Null)
            mediaBox = resolve(node["MediaBox"]);
        if (node["Resources"].t != Obj::Type::Null)
            resources = resolve(node["Resources"]);
        if (node["Type"].isName("Page")) {
            Obj page = node;
            auto dict = std::make_shared<std::map<QByteArray, Obj>>(*node.dict);
            (*dict)["MediaBox"] = mediaBox;
            (*dict)["Resources"] = resources;
            page.dict = dict;
            out << page;
            return;
        }
        const Obj kids = resolve(node["Kids"]);
        for (int i = 0; i < kids.size(); ++i)
            collectPages(resolve(kids.at(i)), mediaBox, resources, out, depth + 1);
    }

    const QByteArray& m_data;
    QHash<int, Obj> m_objects;
    QHash<int, Obj> m_pending;
    QList<Obj> m_trailers;
};

// --- Inhaltsströme -------------------------------------------------------------------------------

struct GState {
    QTransform ctm;
    QColor fill = Qt::black;
    QColor stroke = Qt::black;
    int fillComps = 1;
    int strokeComps = 1;
    Obj fillPattern;
    double lineWidth = 1.0;
    int cap = 0;
    int join = 0;
    double fillAlpha = 1.0;
    double strokeAlpha = 1.0;
    std::optional<QPainterPath> clip; // Seitenkoordinaten (mm)
    // Text
    double fontSize = 12.0;
    Obj font;
    double leading = 0.0;
    double charSpacing = 0.0;
    double wordSpacing = 0.0;
    double hscale = 1.0;
    double rise = 0.0;
    int renderMode = 0; // 3 = unsichtbar
};

QColor fromComponents(const QList<double>& c)
{
    auto clamp = [](double v) { return std::clamp(v, 0.0, 1.0); };
    if (c.size() >= 4) {
        const double k = clamp(c[3]);
        return QColor::fromRgbF(float((1 - clamp(c[0])) * (1 - k)), float((1 - clamp(c[1])) * (1 - k)), float((1 - clamp(c[2])) * (1 - k)));
    }
    if (c.size() == 3)
        return QColor::fromRgbF(float(clamp(c[0])), float(clamp(c[1])), float(clamp(c[2])));
    if (c.size() == 1)
        return QColor::fromRgbF(float(clamp(c[0])), float(clamp(c[0])), float(clamp(c[0])));
    return Qt::black;
}

Color toColor(const QColor& c)
{
    return Color(std::uint8_t(c.red()), std::uint8_t(c.green()), std::uint8_t(c.blue()));
}

// Schriften aller Seiten eines Imports
struct FontRegistry {
    std::vector<ImportedFont> fonts;
    QHash<QString, int> indexOf;
};

class Interpreter {
public:
    Interpreter(const Document& doc, const QTransform& toMm, GroupObject& out, FontRegistry& registry)
        : m_doc(doc), m_toMm(toMm), m_out(out), m_registry(registry)
    {
    }

    void run(const QByteArray& content, const Obj& resources, int depth = 0)
    {
        if (depth > 12)
            return;
        Lexer lex(content);
        QList<Obj> ops;
        while (!lex.atEnd()) {
            Obj o = lex.next();
            if (o.t != Obj::Type::Op) {
                ops << o;
                if (ops.size() > 4096)
                    ops.removeFirst();
                continue;
            }
            if (o.s == "BI") {
                // Eingebettete Bilder: bis "EI" überspringen
                const qsizetype id = content.indexOf("ID", lex.pos());
                const qsizetype ei = content.indexOf("EI", id + 2);
                lex.setPos(ei < 0 ? content.size() : ei + 2);
                ops.clear();
                continue;
            }
            op(o.s, ops, resources, depth);
            ops.clear();
        }
        flushText();
    }

private:
    double n(const QList<Obj>& a, int i) const { return i < a.size() ? a[i].n : 0.0; }

    QList<double> numsOf(const QList<Obj>& a) const
    {
        QList<double> v;
        for (const Obj& o : a)
            if (o.isNum())
                v << o.n;
        return v;
    }

    int components(const Obj& space, const Obj& resources) const
    {
        Obj cs = m_doc.resolve(space);
        if (cs.t == Obj::Type::Name) {
            if (cs.s == "DeviceGray" || cs.s == "G" || cs.s == "CalGray")
                return 1;
            if (cs.s == "DeviceRGB" || cs.s == "RGB" || cs.s == "CalRGB" || cs.s == "Lab")
                return 3;
            if (cs.s == "DeviceCMYK" || cs.s == "CMYK")
                return 4;
            if (cs.s == "Pattern")
                return 0;
            const Obj named = m_doc.resolve(m_doc.resolve(resources["ColorSpace"])[cs.s.constData()]);
            if (named.t != Obj::Type::Null)
                return components(named, resources);
            return 3;
        }
        if (cs.t == Obj::Type::Array) {
            const QByteArray family = m_doc.resolve(cs.at(0)).s;
            if (family == "ICCBased")
                return int(m_doc.resolve(m_doc.resolve(cs.at(1))["N"]).n);
            if (family == "Separation")
                return -1; // Tönung 0..1
            if (family == "DeviceN")
                return -m_doc.resolve(cs.at(1)).size();
            if (family == "Indexed")
                return 1;
            if (family == "Pattern")
                return 0;
            return components(cs.at(0), resources);
        }
        return 3;
    }

    QColor colorFor(const QList<double>& v, int comps) const
    {
        if (comps == -1) // Separation: Tönung als Grau
            return fromComponents({1.0 - v.value(0)});
        if (comps < -1)
            return fromComponents({1.0 - std::clamp(v.value(0), 0.0, 1.0)});
        return fromComponents(v.mid(0, std::max(1, comps)));
    }

    void op(const QByteArray& o, const QList<Obj>& a, const Obj& resources, int depth)
    {
        GState& g = m_g;
        // Pfade
        if (o == "m") {
            m_path.moveTo(n(a, 0), n(a, 1));
        } else if (o == "l") {
            m_path.lineTo(n(a, 0), n(a, 1));
        } else if (o == "c") {
            m_path.cubicTo(n(a, 0), n(a, 1), n(a, 2), n(a, 3), n(a, 4), n(a, 5));
        } else if (o == "v") {
            m_path.cubicTo(m_path.currentPosition(), QPointF(n(a, 0), n(a, 1)), QPointF(n(a, 2), n(a, 3)));
        } else if (o == "y") {
            m_path.cubicTo(QPointF(n(a, 0), n(a, 1)), QPointF(n(a, 2), n(a, 3)), QPointF(n(a, 2), n(a, 3)));
        } else if (o == "h") {
            m_path.closeSubpath();
        } else if (o == "re") {
            m_path.addRect(QRectF(n(a, 0), n(a, 1), n(a, 2), n(a, 3)));
        } else if (o == "S" || o == "s" || o == "f" || o == "F" || o == "f*" || o == "B" || o == "B*" || o == "b" || o == "b*" || o == "n") {
            if (o == "s" || o == "b" || o == "b*")
                m_path.closeSubpath();
            const bool fill = o.startsWith('f') || o.startsWith('F') || o.startsWith('B') || o.startsWith('b');
            const bool stroke = o == "S" || o == "s" || o.startsWith('B') || o.startsWith('b');
            const bool evenOdd = o.endsWith('*');
            if (o != "n")
                paint(fill, stroke, evenOdd, resources);
            if (m_pendingClip) {
                const QPainterPath c = *m_pendingClip;
                g.clip = g.clip ? pathops::intersected(*g.clip, c) : c;
                m_pendingClip.reset();
            }
            m_path = QPainterPath();
        } else if (o == "W" || o == "W*") {
            QPainterPath c = (g.ctm * m_toMm).map(m_path);
            c.setFillRule(o == "W*" ? Qt::OddEvenFill : Qt::WindingFill);
            m_pendingClip = c;
        }
        // Grafikzustand
        else if (o == "q") {
            m_stack << g;
        } else if (o == "Q") {
            if (!m_stack.isEmpty())
                g = m_stack.takeLast();
        } else if (o == "cm") {
            g.ctm = QTransform(n(a, 0), n(a, 1), n(a, 2), n(a, 3), n(a, 4), n(a, 5)) * g.ctm;
        } else if (o == "w") {
            g.lineWidth = n(a, 0);
        } else if (o == "J") {
            g.cap = int(n(a, 0));
        } else if (o == "j") {
            g.join = int(n(a, 0));
        } else if (o == "gs") {
            const Obj ext = m_doc.resolve(m_doc.resolve(resources["ExtGState"])[a.value(0).s.constData()]);
            if (ext["ca"].isNum())
                g.fillAlpha = ext["ca"].n;
            if (ext["CA"].isNum())
                g.strokeAlpha = ext["CA"].n;
            if (ext["LW"].isNum())
                g.lineWidth = ext["LW"].n;
        }
        // Farben
        else if (o == "g") {
            g.fill = fromComponents({n(a, 0)});
            g.fillComps = 1;
            g.fillPattern = Obj();
        } else if (o == "G") {
            g.stroke = fromComponents({n(a, 0)});
            g.strokeComps = 1;
        } else if (o == "rg") {
            g.fill = fromComponents({n(a, 0), n(a, 1), n(a, 2)});
            g.fillComps = 3;
            g.fillPattern = Obj();
        } else if (o == "RG") {
            g.stroke = fromComponents({n(a, 0), n(a, 1), n(a, 2)});
            g.strokeComps = 3;
        } else if (o == "k") {
            g.fill = fromComponents({n(a, 0), n(a, 1), n(a, 2), n(a, 3)});
            g.fillComps = 4;
            g.fillPattern = Obj();
        } else if (o == "K") {
            g.stroke = fromComponents({n(a, 0), n(a, 1), n(a, 2), n(a, 3)});
            g.strokeComps = 4;
        } else if (o == "cs") {
            g.fillComps = components(a.value(0), resources);
            g.fill = g.fillComps == 4 ? QColor(Qt::black) : QColor(Qt::black);
        } else if (o == "CS") {
            g.strokeComps = components(a.value(0), resources);
        } else if (o == "sc" || o == "scn") {
            if (!a.isEmpty() && a.last().t == Obj::Type::Name) {
                g.fillPattern = m_doc.resolve(m_doc.resolve(resources["Pattern"])[a.last().s.constData()]);
            } else {
                g.fill = colorFor(numsOf(a), g.fillComps);
                g.fillPattern = Obj();
            }
        } else if (o == "SC" || o == "SCN") {
            if (a.isEmpty() || a.last().t != Obj::Type::Name)
                g.stroke = colorFor(numsOf(a), g.strokeComps);
        }
        // Verläufe
        else if (o == "sh") {
            const Obj shading = m_doc.resolve(m_doc.resolve(resources["Shading"])[a.value(0).s.constData()]);
            QPainterPath area = g.clip ? *g.clip : m_pageArea;
            addShading(shading, g.ctm, area);
        }
        // Objekte
        else if (o == "Do") {
            const Obj x = m_doc.resolve(m_doc.resolve(resources["XObject"])[a.value(0).s.constData()]);
            if (x["Subtype"].isName("Form")) {
                const Obj m = m_doc.resolve(x["Matrix"]);
                m_stack << g;
                if (m.size() == 6)
                    g.ctm = QTransform(m_doc.resolve(m.at(0)).n, m_doc.resolve(m.at(1)).n, m_doc.resolve(m.at(2)).n, m_doc.resolve(m.at(3)).n,
                                       m_doc.resolve(m.at(4)).n, m_doc.resolve(m.at(5)).n)
                            * g.ctm;
                const Obj res = x["Resources"].t != Obj::Type::Null ? m_doc.resolve(x["Resources"]) : resources;
                const QPainterPath savedPath = m_path;
                m_path = QPainterPath();
                run(m_doc.decode(x), res, depth + 1);
                m_path = savedPath;
                g = m_stack.takeLast();
            } else if (x["Subtype"].isName("Image")) {
                addImage(x, resources);
            }
        }
        // Text
        else if (o == "BT") {
            m_tm = m_tlm = QTransform();
        } else if (o == "ET") {
            flushText();
        } else if (o == "Tf") {
            g.font = m_doc.resolve(m_doc.resolve(resources["Font"])[a.value(0).s.constData()]);
            g.fontSize = n(a, 1);
        } else if (o == "Td") {
            m_tlm = QTransform::fromTranslate(n(a, 0), n(a, 1)) * m_tlm;
            m_tm = m_tlm;
        } else if (o == "TD") {
            g.leading = -n(a, 1);
            m_tlm = QTransform::fromTranslate(n(a, 0), n(a, 1)) * m_tlm;
            m_tm = m_tlm;
        } else if (o == "Tm") {
            m_tlm = m_tm = QTransform(n(a, 0), n(a, 1), n(a, 2), n(a, 3), n(a, 4), n(a, 5));
        } else if (o == "T*") {
            m_tlm = QTransform::fromTranslate(0, -g.leading) * m_tlm;
            m_tm = m_tlm;
        } else if (o == "TL") {
            g.leading = n(a, 0);
        } else if (o == "Tc") {
            g.charSpacing = n(a, 0);
        } else if (o == "Tw") {
            g.wordSpacing = n(a, 0);
        } else if (o == "Tz") {
            g.hscale = n(a, 0) / 100.0;
        } else if (o == "Ts") {
            g.rise = n(a, 0);
        } else if (o == "Tr") {
            g.renderMode = int(n(a, 0));
        } else if (o == "Tj" || o == "'" || o == "\"") {
            if (o != "Tj") {
                m_tlm = QTransform::fromTranslate(0, -g.leading) * m_tlm;
                m_tm = m_tlm;
            }
            showText(a.isEmpty() ? QByteArray() : a.last().s);
        } else if (o == "TJ") {
            const Obj arr = a.value(0);
            for (int i = 0; i < arr.size(); ++i) {
                const Obj item = arr.at(i);
                if (item.t == Obj::Type::Str)
                    showText(item.s);
                else if (item.isNum())
                    m_tm = QTransform::fromTranslate(-item.n / 1000.0 * g.fontSize * g.hscale, 0) * m_tm;
            }
        }
    }

    void paint(bool fill, bool stroke, bool evenOdd, const Obj& resources)
    {
        const GState& g = m_g;
        QPainterPath mm = (g.ctm * m_toMm).map(m_path);
        mm.setFillRule(evenOdd ? Qt::OddEvenFill : Qt::WindingFill);
        if (mm.isEmpty())
            return;
        // Beschneidung: nur wenn sie wirklich etwas wegnimmt
        if (fill && g.clip && !g.clip->contains(mm.boundingRect())) {
            mm = pathops::intersected(mm, *g.clip);
            if (mm.isEmpty())
                return;
        }
        // Musterfüllung mit Verlauf: als Verlauf in der Form
        if (fill && g.fillPattern.t == Obj::Type::Dict && g.fillPattern["PatternType"].n == 2) {
            const Obj sh = m_doc.resolve(g.fillPattern["Shading"]);
            const Obj m = m_doc.resolve(g.fillPattern["Matrix"]);
            QTransform pm;
            if (m.size() == 6)
                pm = QTransform(m.at(0).n, m.at(1).n, m.at(2).n, m.at(3).n, m.at(4).n, m.at(5).n);
            addShading(sh, pm * m_baseCtm, mm);
            fill = false;
            if (!stroke)
                return;
        }
        Q_UNUSED(resources);
        std::unique_ptr<CurveObject> c = CurveObject::fromPath(mm);
        if (c->subPaths().isEmpty())
            return;
        vek::Style s;
        s.fill = fill ? Fill::solid(toColor(g.fill)) : Fill::none();
        if (stroke) {
            s.stroke = toColor(g.stroke);
            const double scale = std::sqrt(std::abs((g.ctm * m_toMm).determinant()));
            s.strokeWidth = std::max(0.05, g.lineWidth * scale);
            s.cap = g.cap == 1 ? vek::Style::Cap::Round : g.cap == 2 ? vek::Style::Cap::Square : vek::Style::Cap::Flat;
            s.join = g.join == 1 ? vek::Style::Join::Round : g.join == 2 ? vek::Style::Join::Bevel : vek::Style::Join::Miter;
        } else {
            s.stroke.reset();
        }
        c->setStyle(s);
        const double alpha = fill ? g.fillAlpha : g.strokeAlpha;
        if (alpha < 0.999)
            c->setEffect(Effect::Transparency, Ramp::uniform((1.0 - alpha) * 100.0));
        attachDetachedChild(m_out, std::move(c));
    }

    // Verlauf (ShadingType 2/3) als gefüllte Kurve in area (mm)
    void addShading(const Obj& sh, const QTransform& ctm, const QPainterPath& area)
    {
        if (sh.t != Obj::Type::Dict || area.isEmpty())
            return;
        const int type = int(sh["ShadingType"].n);
        if (type != 2 && type != 3)
            return;
        const Obj coords = m_doc.resolve(sh["Coords"]);
        const int comps = components(sh["ColorSpace"], Obj());
        // Farben aus der Funktion (Typ 2 oder zusammengesetzt aus Typ 2)
        QList<GradientStop> stops;
        const Obj fn = m_doc.resolve(sh["Function"]);
        auto addType2 = [&](const Obj& f, double t0, double t1) {
            const Obj c0 = m_doc.resolve(f["C0"]), c1 = m_doc.resolve(f["C1"]);
            QList<double> a, b;
            for (int i = 0; i < c0.size(); ++i)
                a << m_doc.resolve(c0.at(i)).n;
            for (int i = 0; i < c1.size(); ++i)
                b << m_doc.resolve(c1.at(i)).n;
            if (a.isEmpty())
                a = QList<double>(std::max(1, comps), 0.0);
            if (b.isEmpty())
                b = QList<double>(std::max(1, comps), 1.0);
            const int k = comps > 0 ? comps : int(a.size());
            if (stops.isEmpty() || std::abs(stops.last().position - t0) > 1e-6)
                stops << GradientStop{t0, toColor(colorFor(a, k))};
            stops << GradientStop{t1, toColor(colorFor(b, k))};
        };
        if (fn["FunctionType"].n == 2) {
            addType2(fn, 0, 1);
        } else if (fn["FunctionType"].n == 3) {
            const Obj fns = m_doc.resolve(fn["Functions"]), bounds = m_doc.resolve(fn["Bounds"]);
            QList<double> edges{0.0};
            for (int i = 0; i < bounds.size(); ++i)
                edges << m_doc.resolve(bounds.at(i)).n;
            edges << 1.0;
            for (int i = 0; i < fns.size(); ++i)
                if (const Obj f = m_doc.resolve(fns.at(i)); f["FunctionType"].n == 2)
                    addType2(f, edges.value(i), edges.value(i + 1, 1.0));
        }
        if (stops.size() < 2)
            stops = {GradientStop{0, Color(0, 0, 0)}, GradientStop{1, Color(255, 255, 255)}};
        const QRectF b = area.boundingRect();
        if (b.width() <= 0 || b.height() <= 0)
            return;
        const QTransform toPage = ctm * m_toMm;
        auto frac = [&](double x, double y) {
            const QPointF p = toPage.map(QPointF(x, y));
            return QPointF((p.x() - b.left()) / b.width(), (p.y() - b.top()) / b.height());
        };
        Fill f;
        f.stops = stops;
        if (type == 2) {
            f.type = Fill::Type::Linear;
            f.start = frac(m_doc.resolve(coords.at(0)).n, m_doc.resolve(coords.at(1)).n);
            f.end = frac(m_doc.resolve(coords.at(2)).n, m_doc.resolve(coords.at(3)).n);
        } else {
            f.type = Fill::Type::Radial;
            const double cx = m_doc.resolve(coords.at(3)).n, cy = m_doc.resolve(coords.at(4)).n, r = m_doc.resolve(coords.at(5)).n;
            f.start = frac(cx, cy);
            f.end = frac(cx + r, cy);
        }
        f.sortStops();
        std::unique_ptr<CurveObject> c = CurveObject::fromPath(area);
        if (c->subPaths().isEmpty())
            return;
        vek::Style s;
        s.fill = f;
        s.stroke.reset();
        c->setStyle(s);
        attachDetachedChild(m_out, std::move(c));
    }

    void addImage(const Obj& x, const Obj& resources)
    {
        const int w = int(m_doc.resolve(x["Width"]).n), h = int(m_doc.resolve(x["Height"]).n);
        if (w <= 0 || h <= 0)
            return;
        const QByteArray filter = m_doc.imageFilter(x);
        const QByteArray data = m_doc.decode(x);
        QString key;
        if (filter == "DCTDecode" || filter == "DCT") {
            key = imagepool::add(data);
        } else if (filter == "JPXDecode") {
            key = imagepool::add(data); // nur mit passendem Bildmodul
        } else {
            QImage img = rawImage(x, data, w, h, resources);
            if (img.isNull())
                return;
            key = imagepool::addImage(img);
        }
        if (key.isEmpty())
            return;
        // Weiche Maske (Alpha) dazu
        if (const Obj smask = m_doc.resolve(x["SMask"]); smask.hasStream) {
            QImage base = imagepool::image(key).convertToFormat(QImage::Format_ARGB32);
            const QByteArray alpha = m_doc.decode(smask);
            const int mw = int(m_doc.resolve(smask["Width"]).n), mh = int(m_doc.resolve(smask["Height"]).n);
            if (mw > 0 && mh > 0 && alpha.size() >= qsizetype(mw) * mh) {
                for (int y = 0; y < base.height(); ++y)
                    for (int xx = 0; xx < base.width(); ++xx) {
                        const int a = quint8(alpha[qsizetype(y * mh / base.height()) * mw + xx * mw / base.width()]);
                        base.setPixel(xx, y, (base.pixel(xx, y) & 0x00ffffff) | (quint32(a) << 24));
                    }
                key = imagepool::addImage(base);
            }
        }
        auto image = std::make_unique<ImageObject>(key, QSizeF(w, h));
        // Bildpunkte -> Einheitsquadrat (Zeile 0 oben) -> Seite
        image->setTransform(QTransform::fromScale(1.0 / w, -1.0 / h) * QTransform::fromTranslate(0, 1) * m_g.ctm * m_toMm);
        if (m_g.clip && !m_g.clip->contains(image->boundingRect())) {
            // Beschnittenes Bild: PowerClip in der Form der Beschneidung
            auto clip = std::make_unique<PowerClipObject>();
            std::unique_ptr<CurveObject> frame = CurveObject::fromPath(*m_g.clip);
            vek::Style s;
            s.fill = Fill::none();
            s.stroke.reset();
            frame->setStyle(s);
            attachDetachedChild(*clip, std::move(frame));
            attachDetachedChild(*clip, std::move(image));
            attachDetachedChild(m_out, std::move(clip));
            return;
        }
        attachDetachedChild(m_out, std::move(image));
    }

    QImage rawImage(const Obj& x, const QByteArray& data, int w, int h, const Obj& resources) const
    {
        const int bpc = std::max(1, int(m_doc.resolve(x["BitsPerComponent"]).isNum() ? m_doc.resolve(x["BitsPerComponent"]).n : 8));
        const bool mask = m_doc.resolve(x["ImageMask"]).b;
        Obj cs = m_doc.resolve(x["ColorSpace"]);
        if (cs.t == Obj::Type::Name && cs.s != "DeviceRGB" && cs.s != "DeviceGray" && cs.s != "DeviceCMYK")
            cs = m_doc.resolve(m_doc.resolve(resources["ColorSpace"])[cs.s.constData()]);
        QByteArray palette;
        int comps = mask ? 1 : components(cs, resources);
        int paletteComps = 3;
        if (cs.t == Obj::Type::Array && m_doc.resolve(cs.at(0)).s == "Indexed") {
            paletteComps = std::max(1, components(cs.at(1), resources));
            const Obj lookup = m_doc.resolve(cs.at(3));
            palette = lookup.hasStream ? m_doc.decode(lookup) : lookup.s;
            comps = 1;
        }
        if (comps < 1)
            comps = 1;
        const qsizetype rowBytes = (qsizetype(w) * comps * bpc + 7) / 8;
        if (data.size() < rowBytes * h)
            return {};
        QImage img(w, h, QImage::Format_ARGB32);
        auto sample = [&](int y, int idx) -> int { // 0..255 bzw. Index
            const qsizetype bit = qsizetype(idx) * bpc;
            const quint8 byte = quint8(data[qsizetype(y) * rowBytes + bit / 8]);
            if (bpc == 8)
                return byte;
            if (bpc == 16)
                return quint8(data[qsizetype(y) * rowBytes + bit / 8]);
            const int v = (byte >> (8 - bpc - bit % 8)) & ((1 << bpc) - 1);
            return palette.isEmpty() ? v * 255 / ((1 << bpc) - 1) : v;
        };
        for (int y = 0; y < h; ++y)
            for (int xx = 0; xx < w; ++xx) {
                QRgb px;
                if (mask) {
                    const int v = sample(y, xx);
                    px = v == 0 ? qRgba(m_g.fill.red(), m_g.fill.green(), m_g.fill.blue(), 255) : qRgba(0, 0, 0, 0);
                } else if (!palette.isEmpty()) {
                    const int i = sample(y, xx);
                    QList<double> c;
                    for (int k = 0; k < paletteComps; ++k)
                        {
                        const qsizetype at = qsizetype(i) * paletteComps + k;
                        c << (at < palette.size() ? quint8(palette[at]) : 0) / 255.0;
                    }
                    px = fromComponents(c).rgb();
                } else {
                    QList<double> c;
                    for (int k = 0; k < comps; ++k)
                        c << sample(y, xx * comps + k) / 255.0;
                    px = fromComponents(c).rgb();
                }
                img.setPixel(xx, y, px);
            }
        return img;
    }

    // --- Text ---
    // Schrift eines Textlaufs: Kodierung, Breiten und (falls eingebettet) das Schriftprogramm
    struct FontState {
        int index = -1; // in m_fonts
        bool cid = false;
        bool trueType = false;
        bool symbolic = false;
        std::shared_ptr<fontprog::Program> program;
        QTransform matrix = QTransform::fromScale(0.001, 0.001); // Glyphe -> Textraum
        std::array<QString, 256> names;                          // Glyphenname je Code (einfache Schriften)
        int firstChar = 0;
        QList<double> widths;
        double missingWidth = 0;
        QHash<int, double> cidWidths;
        double defaultWidth = 1000;
        QByteArray cidToGid; // leer = Identity
        QHash<quint32, QString> unicode;
        QHash<int, QPainterPath> outlines; // Glyphe -> Umriss im Textraum
    };

    FontState& fontState(const Obj& font)
    {
        const void* key = font.dict.get();
        if (auto it = m_fontStates.find(key); it != m_fontStates.end())
            return **it;
        auto st = std::make_shared<FontState>();
        const bool type0 = font["Subtype"].isName("Type0");
        Obj desc = font;
        if (type0) {
            desc = m_doc.resolve(m_doc.resolve(font["DescendantFonts"]).at(0));
            st->cid = true;
        }
        const Obj fd = m_doc.resolve(desc["FontDescriptor"]);
        const int flags = int(m_doc.resolve(fd["Flags"]).n);
        st->symbolic = flags & 4;
        for (const char* file : {"FontFile2", "FontFile3", "FontFile"}) {
            const Obj ff = m_doc.resolve(fd[file]);
            if (!ff.hasStream)
                continue;
            st->program = fontprog::Program::load(m_doc.decode(ff));
            if (st->program) {
                st->matrix = st->program->fontMatrix();
                st->trueType = QByteArray(file) == "FontFile2";
                break;
            }
        }
        st->trueType = st->trueType || font["Subtype"].isName("TrueType") || desc["Subtype"].isName("CIDFontType2");
        if (st->cid) {
            st->defaultWidth = desc["DW"].t == Obj::Type::Null ? 1000.0 : m_doc.resolve(desc["DW"]).n;
            // W: [c [w1 w2 ...]  c1 c2 w ...]
            const Obj w = m_doc.resolve(desc["W"]);
            for (int i = 0; i < w.size();) {
                const int first = int(m_doc.resolve(w.at(i)).n);
                const Obj next = m_doc.resolve(w.at(i + 1));
                if (next.t == Obj::Type::Array) {
                    for (int k = 0; k < next.size(); ++k)
                        st->cidWidths.insert(first + k, m_doc.resolve(next.at(k)).n);
                    i += 2;
                } else {
                    const int last = int(next.n);
                    const double width = m_doc.resolve(w.at(i + 2)).n;
                    for (int c = first; c <= last && c - first < 65536; ++c)
                        st->cidWidths.insert(c, width);
                    i += 3;
                }
            }
            const Obj map = m_doc.resolve(desc["CIDToGIDMap"]);
            if (map.hasStream)
                st->cidToGid = m_doc.decode(map);
        } else {
            st->firstChar = int(m_doc.resolve(font["FirstChar"]).n);
            const Obj widths = m_doc.resolve(font["Widths"]);
            for (int i = 0; i < widths.size(); ++i)
                st->widths << m_doc.resolve(widths.at(i)).n;
            st->missingWidth = m_doc.resolve(fd["MissingWidth"]).n;
            // Kodierung: Grundkodierung plus Differences
            const Obj enc = m_doc.resolve(font["Encoding"]);
            QByteArray base = enc.isName() ? enc.s : m_doc.resolve(enc["BaseEncoding"]).s;
            const bool builtin = base.isEmpty() && (st->symbolic || (st->program && !st->trueType && enc.t == Obj::Type::Null));
            for (int c = 0; c < 256 && !builtin; ++c)
                st->names[std::size_t(c)] = base == "WinAnsiEncoding"    ? fontprog::winAnsiEncoding(c)
                                            : base == "MacRomanEncoding" ? fontprog::macRomanEncoding(c)
                                                                         : fontprog::standardEncoding(c);
            const Obj diff = m_doc.resolve(enc["Differences"]);
            int code = 0;
            for (int i = 0; i < diff.size(); ++i) {
                const Obj item = m_doc.resolve(diff.at(i));
                if (item.isNum())
                    code = int(item.n);
                else if (item.isName() && code >= 0 && code < 256)
                    st->names[std::size_t(code++)] = QString::fromLatin1(item.s);
            }
        }
        st->unicode = toUnicode(font);
        // Für die Schriftersetzung sammeln (dieselbe Schrift auf mehreren Seiten nur einmal)
        QString baseName = QString::fromLatin1(m_doc.resolve(font["BaseFont"]).s);
        if (baseName.size() > 7 && baseName[6] == QLatin1Char('+'))
            baseName = baseName.mid(7);
        if (baseName.isEmpty())
            baseName = QStringLiteral("Unbenannt");
        const QString key2 = baseName + (st->program ? QStringLiteral("|e") : QString());
        auto found = m_registry.indexOf.constFind(key2);
        if (found == m_registry.indexOf.constEnd()) {
            ImportedFont f;
            f.baseName = baseName;
            f.family = fontmatch::familyFromBaseName(baseName, &f.bold, &f.italic);
            f.bold = f.bold || (flags & 262144) || m_doc.resolve(fd["FontWeight"]).n >= 600;
            f.italic = f.italic || (flags & 64) || std::abs(m_doc.resolve(fd["ItalicAngle"]).n) > 0.5;
            f.serif = flags & 2;
            f.fixedPitch = flags & 1;
            f.symbolic = st->symbolic;
            f.embedded = st->program != nullptr;
            if (qobject_cast<QGuiApplication*>(QCoreApplication::instance()))
                f.installed = fontmatch::installedFamily(f.family);
            m_registry.fonts.push_back(std::move(f));
            found = m_registry.indexOf.insert(key2, int(m_registry.fonts.size()) - 1);
        }
        st->index = *found;
        m_fontStates.insert(key, st);
        return *st;
    }

    QString unicodeFor(const FontState& st, quint32 code) const
    {
        if (const auto it = st.unicode.constFind(code); it != st.unicode.constEnd())
            return *it;
        if (!st.cid) {
            const QString& name = st.names[std::size_t(code & 0xFF)];
            if (!name.isEmpty())
                if (const QString u = fontprog::unicodeForGlyphName(name); !u.isEmpty())
                    return u;
            if (st.program && name.isEmpty())
                return QString(QChar(char16_t(code))); // Symbolschrift: Code selbst
            return QString(QChar(char16_t(code))); // WinAnsi ≈ Latin-1
        }
        return {};
    }

    int glyphFor(const FontState& st, quint32 code, const QString& unicode) const
    {
        const fontprog::Program* p = st.program.get();
        if (!p)
            return -1;
        if (st.cid) {
            const int cid = int(code);
            if (st.trueType) {
                if (st.cidToGid.isEmpty())
                    return cid;
                const qsizetype at = 2 * qsizetype(cid);
                return at + 1 < st.cidToGid.size() ? (quint8(st.cidToGid[at]) << 8) | quint8(st.cidToGid[at + 1]) : 0;
            }
            return p->cidKeyed() ? p->gidForCid(cid) : cid;
        }
        const QString& name = st.names[std::size_t(code & 0xFF)];
        int g = -1;
        if (st.trueType) {
            if (!st.symbolic && !name.isEmpty()) {
                const QString u = fontprog::unicodeForGlyphName(name);
                if (!u.isEmpty())
                    g = p->gidForUnicode(u.toUcs4().value(0));
                if (g < 0)
                    g = p->gidForName(name);
            }
            if (g < 0)
                g = p->gidForBuiltinCode(int(code));
            if (g < 0 && !unicode.isEmpty())
                g = p->gidForUnicode(unicode.toUcs4().value(0));
        } else {
            if (!name.isEmpty())
                g = p->gidForName(name);
            if (g < 0)
                g = p->gidForBuiltinCode(int(code));
        }
        return g;
    }

    double widthFor(const FontState& st, quint32 code) const
    {
        if (st.cid)
            return st.cidWidths.value(int(code), st.defaultWidth);
        const int i = int(code) - st.firstChar;
        if (i >= 0 && i < st.widths.size())
            return st.widths[i];
        return st.missingWidth > 0 ? st.missingWidth : 500.0;
    }

    const QPainterPath& outline(FontState& st, int gid)
    {
        auto it = st.outlines.find(gid);
        if (it == st.outlines.end())
            it = st.outlines.insert(gid, st.matrix.map(st.program->glyph(gid)));
        return *it;
    }

    QHash<quint32, QString> toUnicode(const Obj& font) const
    {
        const Obj tu = m_doc.resolve(font["ToUnicode"]);
        if (!tu.hasStream)
            return {};
        const auto cached = m_cmaps.constFind(tu.stream.size() ^ qHash(tu.stream.left(64)));
        if (cached != m_cmaps.constEnd())
            return *cached;
        QHash<quint32, QString> map;
        const QByteArray cmap = m_doc.decode(tu);
        auto hexToString = [](const QByteArray& hex) {
            QString s;
            const QByteArray raw = QByteArray::fromHex(hex);
            for (qsizetype i = 0; i + 1 < raw.size(); i += 2)
                s += QChar(char16_t((quint8(raw[i]) << 8) | quint8(raw[i + 1])));
            return s;
        };
        static const QRegularExpression charRe(QStringLiteral(R"(<([0-9A-Fa-f]+)>\s*<([0-9A-Fa-f]+)>)"));
        static const QRegularExpression rangeRe(QStringLiteral(R"(<([0-9A-Fa-f]+)>\s*<([0-9A-Fa-f]+)>\s*<([0-9A-Fa-f]+)>)"));
        const QString text = QString::fromLatin1(cmap);
        qsizetype pos = 0;
        while ((pos = text.indexOf(QLatin1String("beginbfrange"), pos)) >= 0) {
            const qsizetype end = text.indexOf(QLatin1String("endbfrange"), pos);
            for (auto it = rangeRe.globalMatch(text.mid(pos, end - pos)); it.hasNext();) {
                const auto m = it.next();
                const quint32 lo = m.captured(1).toUInt(nullptr, 16), hi = m.captured(2).toUInt(nullptr, 16);
                const QString base = hexToString(m.captured(3).toLatin1());
                for (quint32 c = lo; c <= hi && c - lo < 65536 && !base.isEmpty(); ++c) {
                    QString s = base;
                    s[s.size() - 1] = QChar(char16_t(s.back().unicode() + (c - lo)));
                    map.insert(c, s);
                }
            }
            pos = end < 0 ? text.size() : end;
        }
        pos = 0;
        while ((pos = text.indexOf(QLatin1String("beginbfchar"), pos)) >= 0) {
            const qsizetype end = text.indexOf(QLatin1String("endbfchar"), pos);
            for (auto it = charRe.globalMatch(text.mid(pos, end - pos)); it.hasNext();) {
                const auto m = it.next();
                map.insert(m.captured(1).toUInt(nullptr, 16), hexToString(m.captured(2).toLatin1()));
            }
            pos = end < 0 ? text.size() : end;
        }
        m_cmaps.insert(tu.stream.size() ^ qHash(tu.stream.left(64)), map);
        return map;
    }

    void showText(const QByteArray& bytes)
    {
        if (m_g.font.t != Obj::Type::Dict)
            return;
        FontState& st = fontState(m_g.font);
        ImportedFont& info = m_registry.fonts[std::size_t(st.index)];
        const bool invisible = m_g.renderMode == 3 || m_g.renderMode == 7; // z.B. OCR-Ebene gescannter Seiten
        const QTransform trm = QTransform::fromTranslate(0, m_g.rise) * m_tm * m_g.ctm * m_toMm;
        const QPointF origin = trm.map(QPointF(0, 0));
        const double scale = std::sqrt(std::abs(trm.determinant()));
        const double sizeMm = m_g.fontSize * scale;
        if (!invisible) {
            // An den laufenden Text anhängen, wenn er dort weitergeht
            const bool continues = m_text && std::abs(m_textSize - sizeMm) < 0.01 && QLineF(m_textEnd, origin).length() < sizeMm * 0.6 && m_textFontIndex == st.index;
            if (!continues) {
                flushText();
                m_text = QString();
                m_textOrigin = origin;
                m_textSize = sizeMm;
                m_textFontIndex = st.index;
                m_textColor = m_g.fill;
                m_textAngle = std::atan2(trm.m12(), trm.m11());
            }
        }
        const int step = st.cid ? 2 : 1;
        for (qsizetype i = 0; i + step - 1 < bytes.size(); i += step) {
            const quint32 code = st.cid ? (quint32(quint8(bytes[i])) << 8) | quint8(bytes[i + 1]) : quint8(bytes[i]);
            const QString u = unicodeFor(st, code);
            const double w0 = widthFor(st, code);
            if (!invisible) {
                *m_text += u;
                const int gid = glyphFor(st, code, u);
                if (gid >= 0) {
                    const QPainterPath& g = outline(st, gid);
                    const QTransform toMm = QTransform(m_g.fontSize * m_g.hscale, 0, 0, m_g.fontSize, 0, m_g.rise) * m_tm * m_g.ctm * m_toMm;
                    m_textCurves.addPath(toMm.map(g));
                    m_textHasCurves = true;
                }
                // Breiten und Formen für den Schriftvergleich (1/1000 Geviert, y nach oben)
                const QList<uint> ucs = u.toUcs4();
                if (ucs.size() == 1) {
                    const char32_t ch = char32_t(ucs[0]);
                    if (!info.widths.contains(ch))
                        info.widths.insert(ch, w0);
                    if (gid >= 0 && !info.glyphs.contains(ch))
                        info.glyphs.insert(ch, QTransform::fromScale(1000, 1000).map(outline(st, gid)));
                }
            }
            const double tx = (w0 / 1000.0 * m_g.fontSize + m_g.charSpacing + (!st.cid && code == 32 ? m_g.wordSpacing : 0.0)) * m_g.hscale;
            m_tm = QTransform::fromTranslate(tx, 0) * m_tm;
        }
        m_textEnd = (QTransform::fromTranslate(0, m_g.rise) * m_tm * m_g.ctm * m_toMm).map(QPointF(0, 0));
    }

    void flushText()
    {
        if (!m_text || m_text->trimmed().isEmpty() || m_textSize <= 0 || m_textFontIndex < 0) {
            m_text.reset();
            m_textCurves = QPainterPath();
            m_textHasCurves = false;
            return;
        }
        ImportedFont& info = m_registry.fonts[std::size_t(m_textFontIndex)];
        QString family = info.installed.isEmpty() ? info.family : info.installed;
        if (family.isEmpty())
            family = QStringLiteral("Arial");
        auto t = std::make_unique<TextObject>(TextObject::Kind::Artistic);
        t->setPlainText(*m_text, CharFormat(family, m_textSize * 72.0 / 25.4, info.bold, info.italic));
        QTransform rot;
        rot.rotateRadians(m_textAngle);
        const QTransform baseline = rot * QTransform::fromTranslate(m_textOrigin.x(), m_textOrigin.y());
        const double runWidth = QLineF(m_textOrigin, m_textEnd).length();
        if (qobject_cast<QGuiApplication*>(QCoreApplication::instance()))
            fontmatch::placeOnBaseline(*t, baseline, runWidth);
        else
            t->setTransform(QTransform::fromTranslate(0, -m_textSize * 0.8) * baseline);
        vek::Style s;
        s.fill = Fill::solid(toColor(m_textColor));
        s.stroke.reset();
        t->setStyle(s);
        // Dieselbe Stelle als Kurven aus den eingebetteten Umrissen (für "In Kurven" bei der Ersetzung)
        std::unique_ptr<DrawObject> curves;
        if (m_textHasCurves && !m_textCurves.isEmpty()) {
            m_textCurves.setFillRule(Qt::WindingFill);
            std::unique_ptr<CurveObject> c = CurveObject::fromPath(m_textCurves);
            c->setStyle(s);
            c->setName(m_text->trimmed().left(40));
            curves = std::move(c);
        }
        if (info.sample.size() < 80)
            info.sample += (info.sample.isEmpty() ? QString() : QStringLiteral(" ")) + m_text->simplified();
        info.texts << t.get();
        info.curves.push_back(std::move(curves));
        info.baselines << baseline;
        info.runWidths << runWidth;
        attachDetachedChild(m_out, std::move(t));
        m_text.reset();
        m_textCurves = QPainterPath();
        m_textHasCurves = false;
    }

public:
    QTransform m_baseCtm;
    QPainterPath m_pageArea;
    GState m_g;

private:
    const Document& m_doc;
    QTransform m_toMm;
    GroupObject& m_out;
    QList<GState> m_stack;
    QPainterPath m_path;
    std::optional<QPainterPath> m_pendingClip;
    QTransform m_tm, m_tlm;
    std::optional<QString> m_text;
    QPointF m_textOrigin, m_textEnd;
    double m_textSize = 0;
    int m_textFontIndex = -1;
    QColor m_textColor;
    double m_textAngle = 0;
    QPainterPath m_textCurves;
    bool m_textHasCurves = false;
    FontRegistry& m_registry;
    QHash<const void*, std::shared_ptr<FontState>> m_fontStates;
    mutable QHash<quint32, QHash<quint32, QString>> m_cmaps;
};

} // namespace

std::vector<svg::Import> read(const QByteArray& data, QString* error)
{
    std::vector<svg::Import> result;
    if (!data.contains("%PDF")) {
        if (error)
            *error = QObject::tr("Keine PDF-Datei.");
        return result;
    }
    Document doc(data);
    if (!doc.load(error))
        return result;
    const QList<Obj> pages = doc.pages();
    FontRegistry fonts;
    for (const Obj& page : pages) {
        Obj box = doc.resolve(page["CropBox"]);
        if (box.size() != 4)
            box = doc.resolve(page["MediaBox"]);
        QRectF media(0, 0, 595.276, 841.89);
        if (box.size() == 4) {
            const double x0 = doc.resolve(box.at(0)).n, y0 = doc.resolve(box.at(1)).n, x1 = doc.resolve(box.at(2)).n, y1 = doc.resolve(box.at(3)).n;
            media = QRectF(QPointF(std::min(x0, x1), std::min(y0, y1)), QPointF(std::max(x0, x1), std::max(y0, y1)));
        }
        const double k = 25.4 / 72.0;
        const QTransform toMm = QTransform::fromTranslate(-media.left(), -media.bottom()) * QTransform(k, 0, 0, -k, 0, 0);
        svg::Import imp;
        imp.size = QSizeF(media.width() * k, media.height() * k);
        imp.group = std::make_unique<GroupObject>();
        Interpreter in(doc, toMm, *imp.group, fonts);
        in.m_pageArea.addRect(QRectF(QPointF(0, 0), imp.size));
        // Inhalt: ein Strom oder ein Array von Strömen
        const Obj contents = doc.resolve(page["Contents"]);
        QByteArray content;
        if (contents.hasStream)
            content = doc.decode(contents);
        else
            for (int i = 0; i < contents.size(); ++i)
                content += doc.decode(doc.resolve(contents.at(i))) + '\n';
        in.run(content, doc.resolve(page["Resources"]));
        result.push_back(std::move(imp));
    }
    if (result.empty() && error)
        *error = QObject::tr("Keine Seiten gefunden.");
    if (!result.empty())
        result.front().fonts = std::move(fonts.fonts);
    return result;
}

namespace {

// Ghostscript-Bibliothek (gsapi) im eigenen Prozess: gsdll64.dll liegt neben tangenta.exe, unter
// Linux libgs.so. Ghostscript steht unter AGPLv3; mit TANGENTA (GPLv3) kombinierbar (GPLv3 § 13).
struct GsApi {
    using NewInstance = int (*)(void**, void*);
    using DeleteInstance = void (*)(void*);
    using SetArgEncoding = int (*)(void*, int);
    using StdFn = int (*)(void*, const char*, int);
    using StdInFn = int (*)(void*, char*, int);
    using SetStdio = int (*)(void*, StdInFn, StdFn, StdFn);
    using InitWithArgs = int (*)(void*, int, char**);
    using Exit = int (*)(void*);

    QLibrary lib;
    NewInstance newInstance = nullptr;
    DeleteInstance deleteInstance = nullptr;
    SetArgEncoding setArgEncoding = nullptr;
    SetStdio setStdio = nullptr;
    InitWithArgs initWithArgs = nullptr;
    Exit exit = nullptr;

    GsApi()
    {
        QStringList candidates;
        const QString appDir = QCoreApplication::applicationDirPath();
#ifdef Q_OS_WIN
        candidates << appDir + QStringLiteral("/gsdll64.dll") << appDir + QStringLiteral("/ghostscript/gsdll64.dll");
        // Installiertes Ghostscript: C:\Program Files\gs\gs10.xx\bin
        QDir gsRoot(QStringLiteral("C:/Program Files/gs"));
        for (const QString& v : gsRoot.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed))
            candidates << gsRoot.filePath(v + QStringLiteral("/bin/gsdll64.dll"));
#else
        candidates << appDir + QStringLiteral("/libgs.so") << QStringLiteral("libgs.so.10") << QStringLiteral("libgs.so.9") << QStringLiteral("libgs");
#endif
        for (const QString& c : std::as_const(candidates)) {
            lib.setFileName(c);
            if (!lib.load())
                continue;
            newInstance = reinterpret_cast<NewInstance>(lib.resolve("gsapi_new_instance"));
            deleteInstance = reinterpret_cast<DeleteInstance>(lib.resolve("gsapi_delete_instance"));
            setArgEncoding = reinterpret_cast<SetArgEncoding>(lib.resolve("gsapi_set_arg_encoding"));
            setStdio = reinterpret_cast<SetStdio>(lib.resolve("gsapi_set_stdio"));
            initWithArgs = reinterpret_cast<InitWithArgs>(lib.resolve("gsapi_init_with_args"));
            exit = reinterpret_cast<Exit>(lib.resolve("gsapi_exit"));
            if (newInstance && deleteInstance && initWithArgs && exit)
                return;
            lib.unload();
        }
        newInstance = nullptr;
    }
    bool ok() const { return newInstance != nullptr; }

    static GsApi& get()
    {
        static GsApi api;
        return api;
    }
};

QMutex& gsMutex()
{
    static QMutex m;
    return m;
}

int gsSink(void* handle, const char* text, int len)
{
    if (handle && len > 0)
        static_cast<QByteArray*>(handle)->append(text, len);
    return len;
}

int gsNoInput(void*, char*, int)
{
    return 0;
}

QString executable()
{
    for (const char* name : {"gswin64c", "gswin32c", "gs"})
        if (const QString p = QStandardPaths::findExecutable(QLatin1String(name)); !p.isEmpty())
            return p;
    return {};
}

} // namespace

QString ghostscript()
{
    if (GsApi::get().ok())
        return GsApi::get().lib.fileName();
    return executable();
}

bool runGhostscript(const QStringList& args, QString* error)
{
    QByteArray messages;
    GsApi& api = GsApi::get();
    if (api.ok()) {
        QMutexLocker lock(&gsMutex()); // Ghostscript erlaubt nur eine Instanz zugleich
        // Ghostscript liest Zahlen mit der C-Bibliothek: unter Linux setzt Qt die Systemsprache
        // (de_DE: Dezimalkomma), dann scheitern Zahlen wie 51.52 in pdfmarks -> kurz auf "C"
        const QByteArray numeric = std::setlocale(LC_NUMERIC, nullptr);
        std::setlocale(LC_NUMERIC, "C");
        const auto restoreLocale = qScopeGuard([&] { std::setlocale(LC_NUMERIC, numeric.constData()); });
        void* instance = nullptr;
        if (api.newInstance(&instance, &messages) < 0 || !instance) {
            if (error)
                *error = QObject::tr("Ghostscript liess sich nicht starten.");
            return false;
        }
        if (api.setArgEncoding)
            api.setArgEncoding(instance, 1); // UTF-8
        if (api.setStdio)
            api.setStdio(instance, gsNoInput, gsSink, gsSink);
        QList<QByteArray> utf8{QByteArrayLiteral("gs")};
        for (const QString& a : args)
            utf8 << a.toUtf8();
        std::vector<char*> argv;
        for (QByteArray& a : utf8)
            argv.push_back(a.data());
        int code = api.initWithArgs(instance, int(argv.size()), argv.data());
        const int exitCode = api.exit(instance);
        if (code == -101) // gs_error_Quit: regulär beendet
            code = 0;
        api.deleteInstance(instance);
        if (code < 0 || exitCode < 0) {
            if (error)
                *error = QObject::tr("Ghostscript meldet einen Fehler.\n%1").arg(QString::fromLocal8Bit(messages).trimmed());
            return false;
        }
        return true;
    }
    const QString gs = executable();
    if (gs.isEmpty()) {
        if (error)
            *error = QObject::tr("Ghostscript fehlt (gsdll64.dll im Programmordner).");
        return false;
    }
    QProcess p;
    p.start(gs, args);
    if (!p.waitForFinished(300000) || p.exitCode() != 0) {
        if (error)
            *error = QObject::tr("Ghostscript meldet einen Fehler.\n%1").arg(QString::fromLocal8Bit(p.readAllStandardError()));
        return false;
    }
    return true;
}

QByteArray postscriptToPdf(const QByteArray& ps, QString* error)
{
    QTemporaryDir dir;
    const QString in = dir.filePath(QStringLiteral("in.eps")), out = dir.filePath(QStringLiteral("out.pdf"));
    QFile f(in);
    if (!f.open(QIODevice::WriteOnly) || f.write(ps) != ps.size())
        return {};
    f.close();
    if (!runGhostscript({QStringLiteral("-dNOPAUSE"), QStringLiteral("-dBATCH"), QStringLiteral("-dSAFER"), QStringLiteral("-dQUIET"),
                         QStringLiteral("-sDEVICE=pdfwrite"), QStringLiteral("-dEPSCrop"), QStringLiteral("-dCompressPages=false"),
                         QStringLiteral("-sOutputFile=") + out, in},
                        error))
        return {};
    QFile pdf(out);
    return pdf.open(QIODevice::ReadOnly) ? pdf.readAll() : QByteArray();
}

} // namespace vek::pdf
