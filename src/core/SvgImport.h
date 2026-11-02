#pragma once

#include "GroupObject.h"

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QPainterPath>
#include <QSizeF>
#include <QTransform>
#include <QString>

#include <memory>
#include <vector>

namespace vek {

class TextObject;

// Im PDF verwendete Schrift – für die Schriftersetzung nach dem Import (wie):
// installierte gleichnamige Schrift, ähnlichste installierte Schrift oder exakt als Kurven.
struct ImportedFont {
    QString baseName;           // wie im PDF, ohne Teilmengen-Präfix, z.B. "Garamond-BoldItalic"
    QString family;             // Familie, z.B. "Garamond"
    QString installed;          // gleichnamige installierte Familie (leer = fehlt)
    bool bold = false, italic = false, serif = false, fixedPitch = false, symbolic = false;
    bool embedded = false;      // Umrisse aus dem PDF vorhanden
    QHash<char32_t, double> widths;       // Breite je Zeichen in 1/1000 Geviert
    QHash<char32_t, QPainterPath> glyphs; // Umriss je Zeichen, 1/1000 Geviert, y nach oben
    QString sample;                       // Text aus dem Dokument (Vorschau)
    QList<TextObject*> texts;             // importierte Textobjekte in dieser Schrift
    std::vector<std::unique_ptr<DrawObject>> curves; // dieselben Texte als Kurven, gleiche Reihenfolge
    QList<QTransform> baselines;          // je Text: Grundlinie (lokal, x = Laufrichtung) -> Seite
    QList<double> runWidths;              // je Text: Breite im PDF in mm
};

} // namespace vek

namespace vek::svg {

// SVG als bearbeitbare Objekte einlesen (wie der SVG-Import): Pfade und Grundformen
// werden Kurven, <g> wird Gruppe, Text wird Grafiktext, eingebettete Bilder werden Bildobjekte.
// Unterstützt: path, rect, circle, ellipse, line, polyline, polygon, text/tspan, image, use, g,
// svg, defs, style (Klassen), linearGradient/radialGradient, transform, opacity.
// Koordinaten in mm, (0,0) = linke obere Ecke der SVG-Zeichenfläche.
struct Import {
    std::unique_ptr<GroupObject> group; // alle Objekte; leer bei Fehler
    QSizeF size;                        // Grösse der Zeichenfläche in mm
    // PDF: Schriften aller Seiten (nur in der ersten Seite gefüllt); Zeiger in die Gruppen
    std::vector<ImportedFont> fonts;
};
Import read(const QByteArray& data, QString* error = nullptr);

// Pfaddaten (Attribut d) in einen QPainterPath (Benutzereinheiten)
QPainterPath parsePathData(const QString& d);

} // namespace vek::svg
