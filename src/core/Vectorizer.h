#pragma once

#include "CurveObject.h"

#include <QColor>
#include <QHash>
#include <QImage>
#include <QList>
#include <QString>

#include <optional>

namespace vek::trace {

// Bitmap vektorisieren wie SuperTrace (Umrissvektorisierung):
//  1. Farben auf höchstens `colors` reduzieren (k-Means), fast gleiche Farben zusammenlegen
//     und Kantenglättungs-Pixel (Mischfarben ohne eigene Fläche) der Nachbarfarbe zuschlagen,
//  2. Flecken unter einer Mindestgrösse dem Nachbarn zuschlagen (Detail),
//  3. optional den Hintergrund entfernen (häufigste Randfarbe oder vorgegebene Farbe),
//  4. je Farbe die Umrisse entlang der Pixelkanten verfolgen,
//  5. mit Potrace in Geraden, Ecken und Bézierkurven umwandeln.

enum class Preset { LineArt, Logo, DetailedLogo, Clipart, LowQuality, HighQuality };
QList<Preset> presets();
QString presetName(Preset preset);

struct Settings {
    int colors = 8;          // 2..64
    int detail = 50;         // 0..100: höher = kleinere Flecken bleiben
    int smoothing = 25;      // 0..100: Glättung der Umrisse
    int cornerSmoothing = 0; // 0..100: Kantenglättung, höher = rundere Ecken
    bool removeBackground = false;
    std::optional<QColor> background; // leer = automatisch (häufigste Farbe am Rand)
    // "Farbe aus ganzem Bild entfernen": sonst nur die mit dem Bildrand verbundenen Flächen
    bool removeColorEverywhere = false;
    // false: Flächen liegen gestapelt übereinander (keine Haarlinien zwischen Farben);
    // true: jede Fläche genau ihr eigener Bereich ("Überstand des Objekts entfernen")
    bool removeOverlap = false;
    // "Benachbarte gleichfarbige Objekte verbinden": gleichfarbige Flächen in ein Objekt,
    // soweit dazwischen nichts anderes liegt
    bool mergeAdjacent = true;
    bool groupByColor = false; // "Objekte farblich gruppieren"
    // Register "Farben": ermittelte Farbe -> Ausgabefarbe (Bearbeiten, Zusammenführen)
    QHash<QRgb, QRgb> recolor;
    int maxSide = 1000; // grössere Bilder werden vorher verkleinert

    friend bool operator==(const Settings&, const Settings&) = default;
};
Settings presetSettings(Preset preset);

struct Shape {
    QList<SubPath> paths; // in Pixeln des Eingabebilds
    QColor color;
    double area = 0.0;
};

// Eine Farbe des Ergebnisses: `source` ist die ermittelte Farbe (Schlüssel für recolor)
struct PaletteEntry {
    QRgb source = 0;
    QColor color;
    int pixels = 0;
};

struct Result {
    QList<Shape> shapes; // in Zeichenreihenfolge (unten zuerst)
    QSize size;          // Pixelgrösse des Eingabebilds
    int nodes = 0;
    int colors = 0;
    QList<PaletteEntry> palette; // nach Pixelanzahl, grösste zuerst
    bool groupByColor = false;
};

Result run(const QImage& image, const Settings& settings);

} // namespace vek::trace
