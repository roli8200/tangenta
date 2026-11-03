#pragma once

#include "Color.h"

#include <QImage>

namespace vek::filters {

// Künstlerische Bitmap-Effekte, Ergänzung zu BitmapFilters.h mit denselben Regeln: beliebiges
// QImage hinein, Gleitkomma, Transparenz bleibt, Ergebnis Format_RGBA64, Zufall nur über seed.
// Grössen in Pixeln (Vorschau auf verkleinerter Kopie: Grössen mitskalieren); Prozentwerte 0..100,
// Helligkeit −100..100.

// Farbänderung
QImage posterize(const QImage& in, int levels); // Tontrennung, 2..32 Stufen je Kanal
QImage soften(const QImage& in, double percent); // Weichzeichnen: sanft, 0..100 %
// Künstlerische Striche
QImage impressionism(const QImage& in, double strokeSize, double coloration, double brightness, quint32 seed = 1); // kurze Pinselstriche
QImage watercolor(const QImage& in, double brushSize, double granulation, double water, double brightness,
                  quint32 seed = 1); // Flächen, verlaufene Ränder, Papierkörnung
QImage scraperboard(const QImage& in, double density, double size, bool color); // Schabkarton: helle Linien auf Schwarz
// Struktur
QImage wavyPaper(const QImage& in, double pressure, bool color); // Wellenpapier mit Faserstruktur
QImage fabric(const QImage& in, double cellSize, double complexity, double brightness, double angleDeg); // Gewebe
// Kreativ
QImage glassBlock(const QImage& in, double blockWidth, double blockHeight); // Glasbausteine
QImage smokedGlass(const QImage& in, const Color& tint, double tintPercent, double blurPercent); // Rauchglas: getönt, mattiert

} // namespace vek::filters
