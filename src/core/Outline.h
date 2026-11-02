#pragma once

#include "Style.h"

#include <QList>
#include <QPainterPath>
#include <QString>

namespace vek::outline {

// --- Linienstile ---------------------------------------------------------------------------
// Index 0 ist die durchgehende Linie. Muster in Vielfachen der Konturbreite (Strich, Lücke, …).
int dashCount();
QList<double> dashPattern(int index);

// --- Pfeilspitzen --------------------------------------------------------------------------
// Index 0 = keine Pfeilspitze. Grösse folgt der Konturbreite.
int arrowCount();
QString arrowName(int index);
// Fläche der Pfeilspitze in Einheiten der Konturbreite: Spitze im Ursprung, zeigt nach +x
QPainterPath arrowShape(int index);

// Kontur mit Pfeilspitzen: die Linie ist an den Enden mit Pfeil so weit gekürzt, dass sie
// nicht durch die Spitze sticht; heads sind die Pfeilflächen (Seitenkoordinaten).
// Geschlossene Teilpfade bekommen keine Pfeile.
struct Stroke {
    QPainterPath line;
    QPainterPath heads;
};
Stroke withArrows(const QPainterPath& path, const Style& style);

// Grösse einer Einheit der Pfeilspitzen in mm
double arrowUnit(const Style& style);

} // namespace vek::outline
