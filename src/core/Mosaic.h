#pragma once

#include "Color.h"

#include <QColor>
#include <QImage>
#include <QList>
#include <QPainterPath>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>

#include <functional>

namespace vek::mosaics {

// --- Fotomosaik (Effekte > Fotomosaik) --------------------------------------------------------
// Das Vorlagebild wird in ein Raster zerlegt; jede Zelle bekommt das Bild aus einem Ordner,
// dessen mittlere Farbe der Zelle am nächsten kommt.

struct Tile {
    QString path;
    QImage thumb; // quadratisch aus der Mitte geschnitten, 8 Bit vormultipliziert
    QColor mean;
};

// Mittlere Farbe (deckungsgewichtet) eines Bereichs; leeres Rechteck = ganzes Bild
QColor meanColor(const QImage& image, const QRect& area = {});
// Farbabstand (gewichtetes RGB, ungefähr wahrnehmungsgerecht)
double colorDistance(const QColor& a, const QColor& b);
// Bilddateien eines Ordners (nicht rekursiv), sortiert
QStringList imageFiles(const QString& folder);
// Kacheln laden: auf thumbSize Pixel verkleinert; progress(i, n) darf abbrechen (false)
QList<Tile> loadTiles(const QString& folder, int thumbSize = 96,
                      const std::function<bool(int, int)>& progress = {});

struct PhotoSettings {
    int columns = 24;
    int rows = 0;          // 0 = aus dem Seitenverhältnis
    double tint = 30.0;    // Farbangleichung der Kacheln an die Zelle in %
    int maxRepeats = 0;    // wie oft eine Kachel höchstens vorkommt, 0 = beliebig
    bool avoidNeighbors = true; // gleiche Kachel nicht direkt links/oben daneben
};

struct Cell {
    int row = 0;
    int column = 0;
    QRectF rect;  // in Einheiten des Vorlagebilds (0..1 bezogen auf Breite/Höhe)
    QColor target; // mittlere Farbe der Zelle in der Vorlage
    int tile = -1; // Index in tiles
};

// Zeilenzahl aus Spalten und Seitenverhältnis (quadratische Zellen)
int autoRows(const QSize& image, int columns);
// Kacheln den Zellen zuordnen; durchsichtige Zellen (Deckung < 50 %) bekommen tile = -1
QList<Cell> matchTiles(const QImage& target, const QList<Tile>& tiles, const PhotoSettings& settings);
// Kachelbild für eine Zelle: aus der Mitte auf das Seitenverhältnis von size geschnitten,
// um tint % zur Zellfarbe verschoben
QImage tileImage(const Tile& tile, const QSize& size, const QColor& target, double tint);
// Ganzes Mosaik als ein Bild, cellPixels Pixel je Zellbreite
QImage renderMosaic(const QImage& target, const QList<Tile>& tiles, const QList<Cell>& cells, const PhotoSettings& settings,
                    int cellPixels);

// --- Punktraster (Effekte > Punktraster) --------------------------------------------------------
// Aus einem Bild ein Vektorraster aus Formen: je Zelle eine Form, Farbe und/oder Grösse aus dem Bild.

struct PointSettings {
    enum class Shape { Circle, Square, Custom };
    enum class Mode { Color, Size, ColorAndSize };
    Shape shape = Shape::Circle;
    Mode mode = Mode::Color;
    double cell = 3.0;       // Rasterweite in mm
    double scale = 90.0;     // grösste Form in % der Rasterweite
    double minSize = 5.0;    // Grössenmodulation: kleinste Form in % (hellste Stelle)
    double angle = 0.0;      // Rasterwinkel in Grad
    double rotation = 0.0;   // Drehung jeder Form in Grad
    double jitter = 0.0;     // Zufallsversatz in % der Rasterweite
    int limitColors = 0;     // Farben begrenzen (0 = alle)
    bool invert = false;     // Grössenmodulation: helle Stellen gross
    Color color{0, 0, 0};    // Farbe bei reiner Grössenmodulation
    quint32 seed = 1;
};

struct Dot {
    QPointF center; // mm, im Rechteck des Bildes (0..size)
    double size = 0; // Kantenlänge in mm
    Color color;
    double rotation = 0; // Grad
};

// Raster über ein Bild, das sizeMm gross ist; Zellen in durchsichtigen Bereichen entfallen
QList<Dot> pointillize(const QImage& image, const QSizeF& sizeMm, const PointSettings& settings);
// Form einer Zelle: Einheitsform (custom: beliebiger Pfad, in ein Quadrat eingepasst) mit
// Mittelpunkt und Grösse des Punkts
QPainterPath dotPath(const Dot& dot, PointSettings::Shape shape, const QPainterPath& custom = {});

} // namespace vek::mosaics
