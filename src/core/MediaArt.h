#pragma once

#include "Color.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QPainterPath>
#include <QString>

namespace vek {

struct ArtisticMedia;
class DrawObject;

// Ein farbiger Teil einer Strich-Vorlage (Pinsel) oder eines Sprühobjekts: gefüllte Fläche
// in normierten Koordinaten. Konturen der Vorlage werden beim Speichern zu Flächen.
struct ArtPiece {
    QPainterPath path;
    Color color;

    friend bool operator==(const ArtPiece& a, const ArtPiece& b) { return a.color == b.color && a.path == b.path; }
};

// Ein Objekt der Vorlage: Pinsel = die ganze Vorlage, Sprühliste = ein Eintrag je Objekt
using ArtItem = QList<ArtPiece>;

namespace media {

// --- Vorlagen aus Objekten --------------------------------------------------------------------
// Farbige Flächen der Objekte in Seitenkoordinaten (Gruppen aufgelöst, Konturen als Fläche,
// Medienkurven mit ihren Strichen)
ArtItem piecesOf(const DrawObject& object);
// Pinselvorlage: x 0..1 entlang des Strichs, y -0.5..0.5 quer; *aspect = Länge/Höhe,
// *height = Höhe in mm (natürliche Strichbreite)
ArtItem normalizedBrush(const ArtItem& pieces, double* aspect, double* height);
// Sprühobjekt: um den Ursprung, grösste Ausdehnung 1
ArtItem normalizedSprayItem(const ArtItem& pieces);

// --- Abbildung entlang des Pfads --------------------------------------------------------------
// Pinsel: die Vorlage (media.art[0], sonst der erste eingebaute Pinsel) wird entlang spine
// gestreckt (oder wiederholt) und quer auf media.width gebracht; Farben bleiben
ArtItem brushPieces(const QPainterPath& spine, const ArtisticMedia& m);

// --- Eingebaute Vorlagen ----------------------------------------------------------------------
int builtInBrushCount();
QString builtInBrushName(int index);
// Vorlage samt Seitenverhältnis und ob sie wiederholt wird
const ArtItem& builtInBrush(int index, double* aspect = nullptr, bool* repeat = nullptr);

// --- Speichern --------------------------------------------------------------------------------
QJsonObject pieceToJson(const ArtPiece& piece);
ArtPiece pieceFromJson(const QJsonObject& json);
QJsonArray artToJson(const QList<ArtItem>& art);
QList<ArtItem> artFromJson(const QJsonArray& json);
QJsonObject toJson(const ArtisticMedia& m);
ArtisticMedia fromJson(const QJsonObject& json);

} // namespace media

} // namespace vek
