#pragma once

#include <QImage>
#include <QPoint>
#include <QRect>

#include <atomic>
#include <functional>

namespace vek::cutout {

// Freistellungs-Assistent: Der Benutzer zieht mit dem Marker ein Band über die Kante des Motivs
// (unbekannt) und füllt das Innere (Motiv). Alles andere ist Hintergrund. Daraus entsteht eine
// Trimap; im Band rechnet Closed-Form-Matting (Levin, Lischinski, Weiss 2008) die Deckkraft jedes
// Pixels aus den Farben der Umgebung, auch für halbdurchsichtige Kanten wie Haare. Optional hilft
// ein Segmentierungsmodell (Vorwissen, wo das Motiv liegt). Zum Schluss werden die Kantenfarben
// vom Hintergrund befreit (Forte & Pitié 2021), damit kein Himmelsblau in den Haaren bleibt.

enum : uchar { Background = 0, Unknown = 128, Foreground = 255 };

// Fläche, die vom Punkt seed aus erreichbar ist, ohne den Marker zu überqueren (Grayscale8,
// 255 = gefüllt). Der Bildrand schliesst die Fläche ab. Leeres Bild, wenn seed auf dem Marker liegt.
QImage floodFill(const QImage& marker, const QPoint& seed);

// Trimap aus den Ebenen (alle Grayscale8 in Bildgrösse, >0 = gesetzt): fill = Motiv,
// marker = Kante (unbekannt), keep/remove = Pinsel "Detail behalten/entfernen" (haben Vorrang)
QImage trimap(const QImage& marker, const QImage& fill, const QImage& keep = {}, const QImage& remove = {});

struct Settings {
    int radius = 1;             // Rand um das Band (Fenster des Matting-Laplace sind 3x3)
    double epsilon = 1e-6;      // Regularisierung
    int maxIterations = 3000;
    double tolerance = 1e-6;
    double priorWeight = 0.0;   // Gewicht des Modells (0 = ohne)
    // Grössere Bereiche werden verkleinert gerechnet und geführt hochskaliert
    qint64 workPixels = 8'000'000;
    qint64 maxUnknown = 100'000; // gemessen: gleiche Qualität, 2–4 s für ein Porträt
};

struct Progress {
    std::function<void(int percent)> report;
    std::atomic_bool* cancel = nullptr;
};

// Deckkraft (Grayscale8 in Bildgrösse) aus Bild und Trimap. prior (optional, Grayscale8 in
// Bildgrösse): Einschätzung eines Modells, 255 = Motiv. Leeres Bild bei Abbruch.
QImage solveAlpha(const QImage& image, const QImage& trimap, const QImage& prior = {}, const Settings& settings = {},
                  const Progress& progress = {});

// Nur einen Bereich neu rechnen (nach einem Pinselstrich): previous ist die bisherige Deckkraft;
// unbekannte Pixel am Rand des Bereichs behalten ihren Wert als feste Randbedingung. Viel schneller als
// solveAlpha, wenn sich nur wenig geändert hat.
QImage updateAlpha(const QImage& image, const QImage& trimap, const QImage& previous, const QRect& region,
                   const Settings& settings = {}, const Progress& progress = {});
// Rechteck, in dem sich zwei gleich grosse Graustufenbilder unterscheiden (leer = gleich)
QRect changedBounds(const QImage& a, const QImage& b);

// Kante nachbearbeiten: shift in Pixeln (+ = Motiv grösser, - = kleiner), feather = weiche Kante
// (Radius in Pixeln), hardness 0..100 = steilere Kante
QImage refineAlpha(const QImage& alpha, int shift, int feather, int hardness);

// Freigestelltes Bild: Alpha anwenden, Farben im Kantenbereich vom Hintergrund befreien
// (decontaminate). Behält die Farbtiefe (16 Bit bleibt 16 Bit).
QImage compose(const QImage& image, const QImage& alpha, bool decontaminate = true);

// Bereich, in dem die Trimap unbekannt ist (für Ausschnitte)
QRect unknownBounds(const QImage& trimap);

} // namespace vek::cutout
