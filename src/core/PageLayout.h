#pragma once

#include <QList>
#include <QRectF>
#include <QSizeF>

namespace vek::pagelayout {

// Anordnung der Mehrseitenansicht: Raster mit Spaltenzahl, eine Zeile, eine Spalte
enum class Arrangement { Grid = 0, Horizontal = 1, Vertical = 2 };

struct Settings {
    Arrangement arrangement = Arrangement::Grid;
    int columns = 4;      // Seiten je Zeile im Raster (bei Doppelseiten: Hälfte = Bögen je Zeile)
    double spacing = 40.0; // Abstand zwischen Seiten bzw. Bögen in mm
    bool facing = false;   // Doppelseiten: je zwei Seiten als Bogen ohne Abstand
    bool startRight = true; // Doppelseiten: Seite 1 allein rechts
};

// Lage jeder Seite im Arbeitsbereich (mm, y nach unten), Index = Seitenindex. Zellen des Rasters
// sind so breit bzw. hoch wie ihr grösster Inhalt; Seiten stehen oben, waagrecht in der Zellmitte.
QList<QRectF> arrange(const QList<QSizeF>& sizes, const Settings& settings);

// Bögen bei Doppelseiten (Seitenindizes je Bogen); ohne Doppelseiten je eine Seite
QList<QList<int>> spreads(int pageCount, bool facing, bool startRight);

} // namespace vek::pagelayout
