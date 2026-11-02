#pragma once

#include <QList>
#include <QPointF>
#include <QRectF>
#include <QSizeF>

namespace vek::align {

// Ausrichten und Verteilen (Anordnen > Ausrichten und verteilen). Alles in
// Seitenkoordinaten (y nach unten); die Funktionen liefern nur Verschiebungen, das Bewegen
// übernimmt der Editor.
enum class H { None, Left, Center, Right };
enum class V { None, Top, Center, Bottom };

// Verschiebung, die r an ref ausrichtet (Kanten bzw. Mitten)
QPointF offset(const QRectF& r, const QRectF& ref, H h, V v);
// Verschiebung auf die nächste Gitterlinie (Kante bzw. Mitte je Richtung)
QPointF toGrid(const QRectF& r, const QSizeF& grid, H h, V v);

enum class Spread { None, Start, Center, Spacing, End }; // Links/Oben, Mitte, Abstand, Rechts/Unten

// Verschiebung je Rechteck, damit die Bezugspunkte (bzw. die Abstände) gleichmässig über extent
// verteilt sind. Das erste und das letzte Objekt (nach Lage) liegen an den Enden von extent;
// mit dem Auswahlumfang bleiben sie damit stehen.
QList<QPointF> distribute(const QList<QRectF>& rects, const QRectF& extent, Spread h, Spread v);

} // namespace vek::align
