#pragma once

#include "Color.h"

#include <QImage>
#include <QSize>

namespace vek::filters {

// Kreativ > Rahmen: legt einen Zierrand um das Bild. Die Vorlagen sind prozedural erzeugt
// (gerissenes Papier, Pinselstrich, weicher Rand, Briefmarke …) und skalieren mit dem Bild,
// das Ergebnis hängt also nicht von der Auflösung ab. Als Maske macht der Rand das Bild
// durchsichtig, als Farbrand wird er in der Rahmenfarbe über das Bild gelegt (Deckkraft bleibt).
struct FrameSettings {
    int shape = 0;            // Vorlage, 0 .. frameShapeCount()-1
    double width = 12.0;      // Randbreite in % der kürzeren Seite
    double opacity = 100.0;   // Deckkraft des Rahmens in %
    double softness = 10.0;   // Weichheit der Kante in %
    double scale = 100.0;     // Grösse des Rahmens in % des Bildes
    double rotation = 0.0;    // Drehung des Rahmens in Grad
    bool border = false;      // Farbrand statt Maske
    Color color{255, 255, 255};
    quint32 seed = 1;         // Variation der Zufallsstruktur
};

int frameShapeCount();
// Name der Vorlage (deutsch, für QCoreApplication::translate("vek::fx", …))
const char* frameShapeName(int shape);

// Deckung des Rahmens an einer Stelle: 1 = Bild, 0 = Rand. x, y in Pixeln eines Bildes w × h.
double frameCoverage(const FrameSettings& s, double x, double y, int w, int h);
QImage frame(const QImage& in, const FrameSettings& s);
// Vorschaubild der Vorlage: schwarzer Rand auf Weiss
QImage frameThumbnail(int shape, const QSize& size);

} // namespace vek::filters
