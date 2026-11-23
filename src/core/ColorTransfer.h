#pragma once

#include <QImage>

namespace vek::colortransfer {

// Überträgt Farbe und Licht eines von der KI bearbeiteten Bildes auf das Original, ohne dessen
// Pixel zu ersetzen: Das Original bleibt in voller Auflösung und Farbtiefe, Details und Schärfe
// stammen von ihm, Farben, Weissabgleich, Belichtung und grossflächige Korrekturen von der KI.
//
// 1. Globale Farbabbildung als 3D-Tabelle (17³), gelernt aus geglätteten Paaren Original -> KI;
//    Zellen ohne Daten folgen einem quadratischen Farbpolynom.
// 2. Lokaler Rest: geglättete Differenz zwischen KI-Bild und Schritt 1 (Himmel, aufgehellte
//    Bereiche). Je höher local (0..1), desto feiner. Passen die Bilder geometrisch schlecht
//    zusammen (die KI hat verschoben oder neu gezeichnet), wird der lokale Teil abgeschwächt.
struct Result {
    QImage image;
    double alignment = 0.0; // 0..1, Übereinstimmung der Kanten von Original und KI-Bild
    double localUsed = 0.0; // tatsächlich verwendeter Anteil des lokalen Rests
};

Result transfer(const QImage& original, const QImage& edited, double local);

// Übereinstimmung der Bildstruktur (normierte Korrelation der Helligkeitskanten, 0..1)
double alignment(const QImage& a, const QImage& b);

} // namespace vek::colortransfer
