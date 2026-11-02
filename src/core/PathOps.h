#pragma once

#include "CurveObject.h"

#include <QList>
#include <QPainterPath>
#include <QPolygonF>

#include <memory>

namespace vek::pathops {

// --- Kurvenanpassung ---------------------------------------------------------------------
// Polylinie in wenige Bézierknoten zurückverwandeln: Ecken bleiben spitz, Bögen werden zu
// glatten Kurvensegmenten, gerade Stücke zu Linien. tolerance = grösste Abweichung.
SubPath fitPolyline(QList<QPointF> points, bool closed, double tolerance, double cornerAngle = 40.0);
// Jeden Teilpfad einer Fläche (z.B. Ergebnis einer Schnittmenge) anpassen
QList<SubPath> fitRegion(const QPainterPath& region, double tolerance);
// Fein unterteilte Linienzüge (Hülle, Fischauge) anpassen; geschlossene bleiben geschlossen,
// offene offen. tolerance <= 0: aus der Grösse des Pfads
QList<SubPath> fitPath(const QPainterPath& path, double tolerance = 0.0);

// --- B-Spline und Teilstücke --------------------------------------------------------------
// Kubischer B-Spline durch die Steuerpunkte (offen: beginnt und endet an den Endpunkten) als
// Bézier-Linienzug
SubPath bspline(const QList<QPointF>& controlPoints, bool closed);

// Teilstück eines Linienzugs zwischen zwei Schnittpunkten (Virtuelles Segment löschen).
// from/to sind Kurvenparameter: Segmentnummer + t (Segment k endet im Knoten k + 1); bei
// geschlossenen Linienzügen kann to über das Ende hinaus weiterlaufen.
struct CurvePiece {
    int path = 0;
    double from = 0.0;
    double to = 0.0;
    QPolygonF poly; // fein abgetastet, für Treffer und Rahmen
};
// Alle Teilstücke zwischen Schnittpunkten mit sich selbst und mit others (Linienzüge)
QList<CurvePiece> piecesBetweenIntersections(const QList<SubPath>& paths, const QList<QPolygonF>& others);
// Linienzüge ohne die Teilstücke mit den Indizes removed; Stücke daneben bleiben verbunden
QList<SubPath> withoutPieces(const QList<SubPath>& paths, const QList<CurvePiece>& pieces, const QList<int>& removed);
// Teil eines Linienzugs zwischen den Parametern a und b (a < b)
SubPath subPath(const SubPath& path, double a, double b);
// Pfad fein in Linienzüge zerlegen (Kurven mit höchstens step langen Stücken)
QList<QPolygonF> flattenFine(const QPainterPath& path, double step);

// --- Geometrie eines Objekts in Seitenkoordinaten -----------------------------------------
// Geschlossene Teilpfade zählen als Fläche (auch ohne Füllung),
// offene als Linien.
struct Geometry {
    QPainterPath area;
    QList<QPolygonF> lines;
    bool isEmpty() const { return area.isEmpty() && lines.isEmpty(); }
};
Geometry geometryOf(const DrawObject& object);

// Flächen schneiden; arbeitet intern in 1/100 mm, damit Kurven fein genug unterteilt werden
QPainterPath intersected(const QPainterPath& a, const QPainterPath& b);
QPainterPath subtracted(const QPainterPath& a, const QPainterPath& b);
QPainterPath united(const QPainterPath& a, const QPainterPath& b);
// Linien an der Fläche region teilen; behält die Stücke innerhalb (keepInside) oder ausserhalb
QList<QPolygonF> clipLines(const QList<QPolygonF>& lines, const QPainterPath& region, bool keepInside);

// Neues Kurvenobjekt (Seitenkoordinaten) mit Stil, Unschärfe und Name von original;
// nullptr, wenn nichts übrig bleibt
std::unique_ptr<CurveObject> rebuild(const DrawObject& original, const Geometry& geometry, double tolerance);

// Fläche auf der linken Seite einer Schnittlinie (Messer). Die Enden werden geradlinig weit
// über bounds hinaus verlängert, damit die Linie die Objekte ganz durchtrennt.
QPainterPath halfPlane(const QPolygonF& cut, const QRectF& bounds);

// --- Künstlerische Medien ----------------------------------------------------------------
int mediaPresetCount();
QString mediaPresetName(int preset);
// Breitenverlauf 0..1 an der Stelle t (0 = Anfang, 1 = Ende) eines Strichs der Länge length
double mediaProfile(int preset, double t, double length, double width);
// Umriss eines Strichs entlang spine (ein Teilpfad) als gefüllte Fläche
QPainterPath mediaOutline(const QPainterPath& spine, const ArtisticMedia& media);
// Sprühdose: die versprühten Objekte entlang spine mit ihren Farben
struct SprayPiece {
    QPainterPath path;
    Color color;
};
QList<SprayPiece> sprayPieces(const QPainterPath& spine, const ArtisticMedia& media);
int sprayListCount();
QString sprayListName(int index);

} // namespace vek::pathops
