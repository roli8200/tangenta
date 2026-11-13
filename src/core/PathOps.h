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
// kinks: auch kleine, aber abrupte Knicke als Ecke (Ergebnisse Boolescher Operationen: wo eine
// Gerade in einen Bogen läuft, ist der Knick oft nur wenige Grad; über ihn hinweg angepasst
// entstanden Schlaufen und Sporne)
SubPath fitPolyline(QList<QPointF> points, bool closed, double tolerance, double cornerAngle = 40.0, bool kinks = false);
// Jeden Teilpfad einer Fläche (z.B. Ergebnis einer Schnittmenge) anpassen
QList<SubPath> fitRegion(const QPainterPath& region, double tolerance);
// Fein unterteilte Linienzüge (Hülle, Fischauge) anpassen; geschlossene bleiben geschlossen,
// offene offen. tolerance <= 0: aus der Grösse des Pfads
QList<SubPath> fitPath(const QPainterPath& path, double tolerance = 0.0);
// Fläche mit Nonzero-Regel (Schriftzeichen) so umbauen, dass auch die Gerade-ungerade-Regel der
// Kurven dasselbe zeigt: sich überschneidende Teilpfade (variable Schriften, nicht zusammengeführte
// TrueType-Konturen) werden vereinigt und wieder als Kurven angepasst, alle übrigen bleiben unverändert.
// tolerance = grösste Abweichung in den Einheiten des Pfads
QPainterPath withoutOverlaps(const QPainterPath& path, double tolerance);
// Löcher einer Fläche schliessen: Teilpfade, die in einer ungeraden Zahl anderer liegen, fallen weg.
// keepWiderThan >= 0: nur Löcher schmaler als dieser Wert (2 * Fläche / Umfang, beim Kreis der Radius)
QPainterPath withoutHoles(const QPainterPath& path, double keepWiderThan = -1.0);

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

// --- Ecken (Andockfenster "Ecken") ------------------------------------------------------
// Abrunden (Kreisbogen tangential an beide Segmente), Ausbuchten (nach innen gewölbter Bogen um
// die Ecke) oder Abschrägen (Gerade). radius: Radius bzw. beim Abschrägen der Abstand von der
// Ecke entlang der Segmente. Glatte Knoten und Enden offener Linienzüge bleiben unverändert; der
// Radius wird auf die verfügbare Segmentlänge begrenzt (bearbeitete Nachbarecken teilen sie).
// only: nur diese Knoten (leer = alle spitzen Ecken).
enum class CornerOp { Fillet, Scallop, Chamfer };
QList<SubPath> applyCorners(const QList<SubPath>& paths, CornerOp op, double radius, const QList<NodeRef>& only = {});

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
using SprayPiece = ArtPiece;
QList<SprayPiece> sprayPieces(const QPainterPath& spine, const ArtisticMedia& media);
// Pinsel oder Sprühdose: die farbigen Teile entlang spine (sonst leer)
QList<ArtPiece> mediaPieces(const QPainterPath& spine, const ArtisticMedia& media);
int sprayListCount();
QString sprayListName(int index);
// Objekte einer eingebauten Sprühliste (um den Ursprung, Grösse 1)
const QList<ArtItem>& sprayListItems(int index);

} // namespace vek::pathops
