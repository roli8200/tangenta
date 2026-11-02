#pragma once

#include <QList>
#include <QPointF>
#include <QSizeF>
#include <QString>

namespace vek {

// Etikettenbogen (Dokument > Etikett): Im Etikettenmodus ist eine Seite ein
// Etikett; beim Drucken werden die Seiten auf die Bögen verteilt. Masse in mm.
struct LabelStock {
    QString maker;
    QString name;
    QSizeF sheet{210, 297};
    QSizeF label;
    int columns = 1;
    int rows = 1;
    double left = 0.0;   // Rand links
    double top = 0.0;    // Rand oben
    double hGap = 0.0;   // Abstand zwischen den Spalten
    double vGap = 0.0;   // Abstand zwischen den Zeilen

    // Lage eines Etiketts (0-basiert, zeilenweise) auf dem Bogen
    QPointF labelOrigin(int index) const
    {
        const int c = index % columns;
        const int r = (index / columns) % rows;
        return {left + c * (label.width() + hGap), top + r * (label.height() + vGap)};
    }
    int perSheet() const { return columns * rows; }
};

// Mitgelieferte Bögen (A4, Avery Zweckform)
QList<LabelStock> labelStocks();

} // namespace vek
