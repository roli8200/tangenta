#pragma once

#include "Color.h"

#include <QImage>
#include <QList>
#include <QString>

namespace vek::filters {

// Bitmap-Filter wie im Menü Bitmaps. Alle nehmen und liefern QImage; gerechnet wird
// in Gleitkomma (16-Bit-Bilder behalten ihre Tiefe, Ergebnis Format_RGBA64).

// Unschärfe
QImage gaussian(const QImage& in, double radius);
QImage motion(const QImage& in, int distance, double angleDeg);
QImage radialBlur(const QImage& in, double amount); // Drehung um die Bildmitte
QImage zoomBlur(const QImage& in, double amount);   // aus der Mitte heraus
QImage smooth(const QImage& in, int amount);        // Glätten (Mittelwert 3×3, mehrfach)
// Schärfe
QImage unsharpMask(const QImage& in, double percent, double radius, int threshold);
QImage sharpen(const QImage& in, double amount);
// Rauschen
QImage addNoise(const QImage& in, double amount, bool monochrome, quint32 seed = 1);
QImage median(const QImage& in, int radius); // Rauschen entfernen
// Kontur
QImage findEdges(const QImage& in, double strength); // dunkle Kanten auf Weiss
QImage detectEdges(const QImage& in, double strength); // helle Kanten auf Schwarz
QImage traceContour(const QImage& in, int threshold); // Konturverfolgung (Umrisse)
// Farbänderung
QImage solarize(const QImage& in, int level);
QImage psychedelic(const QImage& in, int shift);
QImage halftone(const QImage& in, double cell);
// Verzerren
QImage pixelate(const QImage& in, int cell);
QImage ripple(const QImage& in, double amplitude, double period);
QImage swirl(const QImage& in, double angleDeg);
QImage wind(const QImage& in, int strength, quint32 seed = 1);
QImage tiles(const QImage& in, int count, double offset, quint32 seed = 1);
// Kreativ
QImage crystallize(const QImage& in, int cell, quint32 seed = 1);
QImage vignette(const QImage& in, double amount, double size, const Color& color);
QImage mosaic(const QImage& in, int cell);
// 3D-Effekte
// Bild als Ebene im Raum gedreht (Grad, ±75), mit Fluchtpunkt; passt ins alte Format, Rest durchsichtig
QImage rotate3d(const QImage& in, double verticalDeg, double horizontalDeg);
// Perspektive: + verkürzt die rechte bzw. obere Kante, − die linke bzw. untere (−100 … 100 %)
QImage perspective(const QImage& in, double horizontal, double vertical);
QImage emboss(const QImage& in, double depth, double angleDeg);
QImage pinch(const QImage& in, double amount); // Ein-/Ausbuchtung (+ = aus, − = ein)
QImage sphere(const QImage& in, double amount);
// Künstlerische Striche
QImage charcoal(const QImage& in, double size, double edge);
QImage pencil(const QImage& in, double radius);
QImage oilPaint(const QImage& in, int radius, int levels);
// Kamera
QImage diffuse(const QImage& in, int distance, quint32 seed = 1);

// Bitmap-Befehle
QImage toBlackWhite(const QImage& in, int threshold); // Modus > Schwarzweiss (1 Bit)
QImage toGrayscale(const QImage& in);                 // Modus > Graustufen (volle Tiefe)
QImage toGrayscale(const QImage& in, int bits);       // 8 oder 16 Bit je Kanal
// Modus > Duoton: Grauwerte mit zwei Druckfarben, balance 0 … 100 (mehr Farbe 2 bei höherem Wert)
QImage duotone(const QImage& in, const Color& ink1, const Color& ink2, double balance, int bits);
// Bild geraderichten: um die Mitte drehen; crop schneidet die leeren Ecken weg (gleiches Seitenverhältnis)
QImage straighten(const QImage& in, double angleDeg, bool crop);
QImage toPalette(const QImage& in, int colors);       // Modus > Palette (8 Bit)
QImage colorMask(const QImage& in, const QList<Color>& colors, double tolerance); // Farben ausblenden

} // namespace vek::filters
