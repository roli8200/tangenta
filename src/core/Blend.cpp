#include "Blend.h"

#include <QCoreApplication>

#include <algorithm>
#include <cmath>

namespace vek::blending {

namespace {

struct Info {
    BlendMode mode;
    const char* id;
    const char* label;
    const char* css;
};

// Reihenfolge der Auswahlliste wie im klassischen Transparenzwerkzeug
const Info kModes[] = {
    {BlendMode::Normal, "normal", QT_TRANSLATE_NOOP("vek::Blend", "Normal"), "normal"},
    {BlendMode::Add, "add", QT_TRANSLATE_NOOP("vek::Blend", "Addieren"), "plus-lighter"},
    {BlendMode::Subtract, "subtract", QT_TRANSLATE_NOOP("vek::Blend", "Subtrahieren"), ""},
    {BlendMode::Difference, "difference", QT_TRANSLATE_NOOP("vek::Blend", "Differenz"), "difference"},
    {BlendMode::Multiply, "multiply", QT_TRANSLATE_NOOP("vek::Blend", "Multiplizieren"), "multiply"},
    {BlendMode::Divide, "divide", QT_TRANSLATE_NOOP("vek::Blend", "Dividieren"), ""},
    {BlendMode::Lighten, "lighten", QT_TRANSLATE_NOOP("vek::Blend", "Wenn heller"), "lighten"},
    {BlendMode::Darken, "darken", QT_TRANSLATE_NOOP("vek::Blend", "Wenn dunkler"), "darken"},
    {BlendMode::Color, "color", QT_TRANSLATE_NOOP("vek::Blend", "Farbe"), "color"},
    {BlendMode::Hue, "hue", QT_TRANSLATE_NOOP("vek::Blend", "Farbton"), "hue"},
    {BlendMode::Saturation, "saturation", QT_TRANSLATE_NOOP("vek::Blend", "Sättigung"), "saturation"},
    {BlendMode::Luminosity, "luminosity", QT_TRANSLATE_NOOP("vek::Blend", "Helligkeit"), "luminosity"},
    {BlendMode::Invert, "invert", QT_TRANSLATE_NOOP("vek::Blend", "Invertieren"), ""},
    {BlendMode::And, "and", QT_TRANSLATE_NOOP("vek::Blend", "Logisch UND"), ""},
    {BlendMode::Or, "or", QT_TRANSLATE_NOOP("vek::Blend", "Logisch ODER"), ""},
    {BlendMode::Xor, "xor", QT_TRANSLATE_NOOP("vek::Blend", "Logisch XOR"), ""},
    {BlendMode::Behind, "behind", QT_TRANSLATE_NOOP("vek::Blend", "Dahinter"), ""},
    {BlendMode::Screen, "screen", QT_TRANSLATE_NOOP("vek::Blend", "Negativ multiplizieren"), "screen"},
    {BlendMode::Overlay, "overlay", QT_TRANSLATE_NOOP("vek::Blend", "Überlagern"), "overlay"},
    {BlendMode::SoftLight, "soft-light", QT_TRANSLATE_NOOP("vek::Blend", "Weiches Licht"), "soft-light"},
    {BlendMode::HardLight, "hard-light", QT_TRANSLATE_NOOP("vek::Blend", "Hartes Licht"), "hard-light"},
    {BlendMode::ColorDodge, "color-dodge", QT_TRANSLATE_NOOP("vek::Blend", "Farbig abwedeln"), "color-dodge"},
    {BlendMode::ColorBurn, "color-burn", QT_TRANSLATE_NOOP("vek::Blend", "Farbig nachbelichten"), "color-burn"},
    {BlendMode::Exclusion, "exclusion", QT_TRANSLATE_NOOP("vek::Blend", "Ausschluss"), "exclusion"},
    {BlendMode::Red, "red", QT_TRANSLATE_NOOP("vek::Blend", "Rot"), ""},
    {BlendMode::Green, "green", QT_TRANSLATE_NOOP("vek::Blend", "Grün"), ""},
    {BlendMode::Blue, "blue", QT_TRANSLATE_NOOP("vek::Blend", "Blau"), ""},
};

const Info& info(BlendMode mode)
{
    for (const Info& i : kModes)
        if (i.mode == mode)
            return i;
    return kModes[0];
}

// --- nicht trennbare Modi (W3C) ---------------------------------------------------------
double lum(const double c[3])
{
    return 0.3 * c[0] + 0.59 * c[1] + 0.11 * c[2];
}

void clipColor(double c[3])
{
    const double l = lum(c);
    const double n = std::min({c[0], c[1], c[2]});
    const double x = std::max({c[0], c[1], c[2]});
    for (int i = 0; i < 3; ++i) {
        if (n < 0.0 && l - n > 1e-12)
            c[i] = l + (c[i] - l) * l / (l - n);
        if (x > 1.0 && x - l > 1e-12)
            c[i] = l + (c[i] - l) * (1.0 - l) / (x - l);
    }
}

void setLum(const double c[3], double l, double out[3])
{
    const double d = l - lum(c);
    for (int i = 0; i < 3; ++i)
        out[i] = c[i] + d;
    clipColor(out);
}

double sat(const double c[3])
{
    return std::max({c[0], c[1], c[2]}) - std::min({c[0], c[1], c[2]});
}

void setSat(const double c[3], double s, double out[3])
{
    // Grösste Komponente -> s, kleinste -> 0, mittlere anteilig
    int imax = 0, imin = 0;
    for (int i = 1; i < 3; ++i) {
        if (c[i] > c[imax])
            imax = i;
        if (c[i] < c[imin])
            imin = i;
    }
    if (imax == imin) {
        out[0] = out[1] = out[2] = 0.0;
        return;
    }
    const int imid = 3 - imax - imin;
    const double range = c[imax] - c[imin];
    out[imid] = (c[imid] - c[imin]) * s / range;
    out[imax] = s;
    out[imin] = 0.0;
}

double softLight(double cb, double cs)
{
    if (cs <= 0.5)
        return cb - (1.0 - 2.0 * cs) * cb * (1.0 - cb);
    const double d = cb <= 0.25 ? ((16.0 * cb - 12.0) * cb + 4.0) * cb : std::sqrt(cb);
    return cb + (2.0 * cs - 1.0) * (d - cb);
}

double hardLight(double cb, double cs)
{
    return cs <= 0.5 ? cb * 2.0 * cs : cb + (2.0 * cs - 1.0) - cb * (2.0 * cs - 1.0);
}

int byte(double v)
{
    return std::clamp(int(std::lround(v * 255.0)), 0, 255);
}

} // namespace

QList<BlendMode> all()
{
    QList<BlendMode> list;
    for (const Info& i : kModes)
        list << i.mode;
    return list;
}

QString label(BlendMode mode)
{
    return QCoreApplication::translate("vek::Blend", info(mode).label);
}

QString id(BlendMode mode)
{
    return QString::fromLatin1(info(mode).id);
}

std::optional<BlendMode> fromId(const QString& id)
{
    for (const Info& i : kModes)
        if (id == QLatin1String(i.id))
            return i.mode;
    return std::nullopt;
}

QString cssName(BlendMode mode)
{
    return QString::fromLatin1(info(mode).css);
}

std::optional<QPainter::CompositionMode> painterMode(BlendMode mode)
{
    // Nur wo Qt dieselbe Formel rechnet; Weiches Licht rechnet Qt nach SVG 1.2 (anders als W3C)
    switch (mode) {
    case BlendMode::Normal: return QPainter::CompositionMode_SourceOver;
    case BlendMode::Multiply: return QPainter::CompositionMode_Multiply;
    case BlendMode::Screen: return QPainter::CompositionMode_Screen;
    case BlendMode::Overlay: return QPainter::CompositionMode_Overlay;
    case BlendMode::Darken: return QPainter::CompositionMode_Darken;
    case BlendMode::Lighten: return QPainter::CompositionMode_Lighten;
    case BlendMode::HardLight: return QPainter::CompositionMode_HardLight;
    case BlendMode::Difference: return QPainter::CompositionMode_Difference;
    case BlendMode::Exclusion: return QPainter::CompositionMode_Exclusion;
    case BlendMode::Add: return QPainter::CompositionMode_Plus;
    case BlendMode::Behind: return QPainter::CompositionMode_DestinationOver;
    default: return std::nullopt;
    }
}

void mix(BlendMode mode, const double cb[3], const double cs[3], double out[3])
{
    switch (mode) {
    case BlendMode::Hue: {
        double t[3];
        setSat(cs, sat(cb), t);
        setLum(t, lum(cb), out);
        return;
    }
    case BlendMode::Saturation: {
        double t[3];
        setSat(cb, sat(cs), t);
        setLum(t, lum(cb), out);
        return;
    }
    case BlendMode::Color: setLum(cs, lum(cb), out); return;
    case BlendMode::Luminosity: setLum(cb, lum(cs), out); return;
    case BlendMode::Red:
    case BlendMode::Green:
    case BlendMode::Blue: {
        // Nur dieser Kanal kommt vom Objekt, die anderen vom Untergrund
        const int k = mode == BlendMode::Red ? 0 : mode == BlendMode::Green ? 1 : 2;
        for (int i = 0; i < 3; ++i)
            out[i] = i == k ? cs[i] : cb[i];
        return;
    }
    default: break;
    }
    for (int i = 0; i < 3; ++i) {
        const double b = cb[i], s = cs[i];
        double r = s;
        switch (mode) {
        case BlendMode::Multiply: r = b * s; break;
        case BlendMode::Screen: r = b + s - b * s; break;
        case BlendMode::Overlay: r = hardLight(s, b); break;
        case BlendMode::Darken: r = std::min(b, s); break;
        case BlendMode::Lighten: r = std::max(b, s); break;
        case BlendMode::ColorDodge: r = b <= 0.0 ? 0.0 : s >= 1.0 ? 1.0 : std::min(1.0, b / (1.0 - s)); break;
        case BlendMode::ColorBurn: r = b >= 1.0 ? 1.0 : s <= 0.0 ? 0.0 : 1.0 - std::min(1.0, (1.0 - b) / s); break;
        case BlendMode::HardLight: r = hardLight(b, s); break;
        case BlendMode::SoftLight: r = softLight(b, s); break;
        case BlendMode::Difference: r = std::abs(b - s); break;
        case BlendMode::Exclusion: r = b + s - 2.0 * b * s; break;
        case BlendMode::Add: r = std::min(1.0, b + s); break;
        case BlendMode::Subtract: r = std::max(0.0, b - s); break;
        case BlendMode::Divide: r = s <= 1e-9 ? (b > 0.0 ? 1.0 : 0.0) : std::min(1.0, b / s); break;
        case BlendMode::Invert: r = 1.0 - b; break; // Untergrund ins Negativ
        case BlendMode::And: r = (byte(b) & byte(s)) / 255.0; break;
        case BlendMode::Or: r = (byte(b) | byte(s)) / 255.0; break;
        case BlendMode::Xor: r = (byte(b) ^ byte(s)) / 255.0; break;
        default: break;
        }
        out[i] = std::clamp(r, 0.0, 1.0);
    }
}

namespace {

// Ergebnis (vormultipliziert, 0..1) von Quelle über Hintergrund
void compositeF(BlendMode mode, QRgb backdrop, QRgb source, double res[4], double bp[4], double& as)
{
    const double ab = qAlpha(backdrop) / 255.0;
    as = qAlpha(source) / 255.0;
    bp[0] = qRed(backdrop) / 255.0;
    bp[1] = qGreen(backdrop) / 255.0;
    bp[2] = qBlue(backdrop) / 255.0;
    bp[3] = ab;
    const double sp[3] = {qRed(source) / 255.0, qGreen(source) / 255.0, qBlue(source) / 255.0};
    res[3] = as + ab - as * ab;
    if (mode == BlendMode::Behind) {
        for (int i = 0; i < 3; ++i)
            res[i] = bp[i] + sp[i] * (1.0 - ab);
        return;
    }
    double cb[3], cs[3], m[3];
    for (int i = 0; i < 3; ++i) {
        cb[i] = ab > 0.0 ? std::clamp(bp[i] / ab, 0.0, 1.0) : 0.0;
        cs[i] = as > 0.0 ? std::clamp(sp[i] / as, 0.0, 1.0) : 0.0;
    }
    mix(mode, cb, cs, m);
    for (int i = 0; i < 3; ++i)
        res[i] = sp[i] * (1.0 - ab) + bp[i] * (1.0 - as) + as * ab * m[i];
}

QRgb pack(const double c[4])
{
    const int a = byte(c[3]);
    auto ch = [a](double v) { return std::clamp(int(std::lround(v * 255.0)), 0, a); };
    return qRgba(ch(c[0]), ch(c[1]), ch(c[2]), a);
}

} // namespace

QRgb composite(BlendMode mode, QRgb backdrop, QRgb source)
{
    if (qAlpha(source) == 0)
        return backdrop;
    double res[4], bp[4], as;
    compositeF(mode, backdrop, source, res, bp, as);
    return pack(res);
}

QRgb layerPixel(BlendMode mode, QRgb backdrop, QRgb source)
{
    if (qAlpha(source) == 0)
        return 0;
    double res[4], bp[4], as;
    compositeF(mode, backdrop, source, res, bp, as);
    // X + B(1 - as) = R  =>  X = R - B(1 - as), Deckkraft as
    double x[4];
    for (int i = 0; i < 3; ++i)
        x[i] = std::clamp(res[i] - bp[i] * (1.0 - as), 0.0, as);
    x[3] = as;
    return pack(x);
}

QImage layer(BlendMode mode, const QImage& source, const QImage& backdrop)
{
    QImage src = source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const QImage back = backdrop.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const int w = std::min(src.width(), back.width()), h = std::min(src.height(), back.height());
    for (int y = 0; y < src.height(); ++y) {
        auto* s = reinterpret_cast<QRgb*>(src.scanLine(y));
        const auto* b = y < h ? reinterpret_cast<const QRgb*>(back.constScanLine(y)) : nullptr;
        for (int x = 0; x < src.width(); ++x)
            s[x] = layerPixel(mode, b && x < w ? b[x] : 0u, s[x]);
    }
    return src;
}

} // namespace vek::blending
