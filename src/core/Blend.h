#pragma once

#include <QImage>
#include <QList>
#include <QPainter>
#include <QRgb>
#include <QString>

#include <optional>

namespace vek {

// Mischmodus (Transparenz, Schatten): wie die Farbe eines Objekts mit dem verrechnet wird, was
// darunter liegt. Die trennbaren Modi folgen der W3C-Spezifikation "Compositing and Blending"
// (wie PDF und CSS), dazu die klassischen Modi Addieren, Subtrahieren, Dividieren, Invertieren,
// logisch UND/ODER/XOR, Dahinter und Rot/Grün/Blau.
enum class BlendMode {
    Normal,
    Multiply,
    Screen,
    Overlay,
    Darken,     // Wenn dunkler
    Lighten,    // Wenn heller
    ColorDodge,
    ColorBurn,
    HardLight,
    SoftLight,
    Difference,
    Exclusion,
    Hue,
    Saturation,
    Color,
    Luminosity, // Helligkeit
    Add,
    Subtract,
    Divide,
    Invert,
    And,
    Or,
    Xor,
    Behind,
    Red,
    Green,
    Blue,
};

namespace blending {

// Alle Modi in der Reihenfolge der Auswahlliste
QList<BlendMode> all();
// Bezeichnung in der Oberfläche (übersetzbar, Kontext "vek::Blend")
QString label(BlendMode mode);
// Kennung in der Datei, z.B. "multiply"; unbekannt -> nullopt
QString id(BlendMode mode);
std::optional<BlendMode> fromId(const QString& id);
// CSS mix-blend-mode (SVG-Ausgabe), leer wenn es keine Entsprechung gibt
QString cssName(BlendMode mode);
// Gleichwertiger QPainter-Modus für Rastergeräte (gleiche Formel wie mix), sonst nullopt
std::optional<QPainter::CompositionMode> painterMode(BlendMode mode);

// Mischfunktion B(cb, cs) für nicht vormultiplizierte Farben 0..1 (Hintergrund, Quelle)
void mix(BlendMode mode, const double cb[3], const double cs[3], double out[3]);
// Quelle über Hintergrund, beides vormultipliziertes ARGB
QRgb composite(BlendMode mode, QRgb backdrop, QRgb source);
// Bildpunkt, der normal (Source-Over) über backdrop gezeichnet genau composite() ergibt:
// so lässt sich das Ergebnis auf jedes Gerät (PDF, SVG, Drucker) zeichnen
QRgb layerPixel(BlendMode mode, QRgb backdrop, QRgb source);
// Ganzes Bild: source und backdrop gleich gross, Ergebnis Format_ARGB32_Premultiplied
QImage layer(BlendMode mode, const QImage& source, const QImage& backdrop);

} // namespace blending

} // namespace vek
