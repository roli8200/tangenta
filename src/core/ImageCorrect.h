#pragma once

#include "Color.h"

#include <QImage>

#include <array>

namespace vek::adjust {

// Weitere Bildkorrekturen (Effekte > Anpassen). Wie ImageAdjust: 16 Bit je Kanal,
// Ergebnis Format_RGBA64, Deckkraft bleibt unverändert.

// Lokaler Ausgleich: Helligkeit je Fenster (Breite × Höhe in Pixeln) ausgleichen, weich
// zwischen den Fenstern überblendet; hebt den Kontrast in flauen Bildteilen
QImage localEqualize(const QImage& image, int windowWidth, int windowHeight);

// Proben-/Zielabgleich: je Kanal eine Kurve, die die Proben (Schatten, Mitteltöne,
// Glanzlichter) auf die Zielfarben legt
QImage sampleTargetBalance(const QImage& image, const std::array<Color, 3>& samples, const std::array<Color, 3>& targets);

// Selektive Farbe: Bereiche Rot, Gelb, Grün, Cyan, Blau, Magenta, Weiss, Neutral, Schwarz
enum class ColorRange { Reds, Yellows, Greens, Cyans, Blues, Magentas, Whites, Neutrals, Blacks };
constexpr int kColorRanges = 9;
// Je Bereich Cyan, Magenta, Gelb, Schwarz in −100 … 100 %
struct SelectiveColor {
    std::array<std::array<int, 4>, kColorRanges> cmyk{};
    bool relative = true; // relativ: abhängig von der vorhandenen Farbmenge, sonst absolut
    bool isIdentity() const;
};
QImage selectiveColor(const QImage& image, const SelectiveColor& settings);
// Anteil (0 … 1), mit dem ein Pixel zum Bereich gehört
double colorRangeWeight(double r, double g, double b, ColorRange range);

// Farben ersetzen: Farben nahe oldColor (range 0 … 100) werden um den Unterschied
// oldColor → newColor verschoben, dazu Farbton (−180 … 180), Sättigung und Helligkeit (−100 … 100)
struct ReplaceColors {
    Color oldColor{255, 0, 0};
    Color newColor{0, 0, 255};
    int range = 30;
    int hue = 0;
    int saturation = 0;
    int lightness = 0;
};
QImage replaceColors(const QImage& image, const ReplaceColors& settings);

// Kanalmixer: je Ausgabekanal (R, G, B) Anteile von Rot, Grün, Blau (−200 … 200 %) und eine Konstante (−100 … 100 %)
struct ChannelMixer {
    std::array<std::array<int, 4>, 3> rows{{{100, 0, 0, 0}, {0, 100, 0, 0}, {0, 0, 100, 0}}};
    bool isIdentity() const;
};
QImage channelMixer(const QImage& image, const ChannelMixer& mixer);

} // namespace vek::adjust
