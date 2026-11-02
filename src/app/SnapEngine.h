#pragma once

#include "Node.h"
#include "Page.h"

#include <QList>
#include <QColor>
#include <QLineF>
#include <QPointF>
#include <QSizeF>
#include <QString>

namespace vek {

class Canvas;

struct SnapResult {
    QPointF point;
    QString hint; // "Knoten", "Mittelpunkt", "Rand", "Hilfslinie", "Seite"; leer = nicht eingerastet
    // Zu zeichnende Hilfslinien (dynamische bzw. Ausrichtungshilfslinien), Seitenkoordinaten
    QList<QLineF> lines{};
    QColor lineColor{};
    bool snapped() const { return !hint.isEmpty(); }
};

// Einrasten an Objektknoten, Objektmitten, Objekträndern, Hilfslinien und Seite.
// Fangradius in Bildschirmpixeln, damit er bei jedem Zoom gleich gross wirkt.
class SnapEngine {
public:
    SnapEngine(const Canvas& canvas, const QList<NodeId>& exclude = {});

    // Rastet einen Punkt ein
    SnapResult snap(const QPointF& point) const;
    // Verschiebt mehrere Quellpunkte gemeinsam um delta und korrigiert delta so, dass der
    // am nächsten liegende Quellpunkt einrastet. Liefert das korrigierte delta.
    QPointF snapDelta(const QList<QPointF>& sources, const QPointF& delta, SnapResult* result) const;

    static constexpr double ThresholdPx = 8.0;

private:
    struct Target {
        QPointF point;
        QString hint;
    };
    struct Line {
        double value;
        QString hint;
        // Ausrichtungshilfslinie: Ausdehnung des Objekts quer zur Linie (für die Anzeige)
        double from = 0.0;
        double to = 0.0;
        bool alignment = false;
    };
    struct Anchor {
        QPointF point;
    };
    SnapResult snapToDynamicGuides(const QPointF& p) const;

    SnapResult snapToPoints(const QPointF& p) const;
    SnapResult snapToLines(const QPointF& p) const;

    double m_threshold; // mm
    QSizeF m_grid;      // Gitterabstand in mm, leer = nicht am Gitter
    QList<Target> m_points;
    QList<QPolygonF> m_outlines;
    QList<Line> m_xLines; // senkrechte Linien x = value
    QList<Line> m_yLines; // waagrechte Linien y = value
    QList<Guide> m_slanted; // schräge Hilfslinien
    // Dynamische Hilfslinien: Ausgangspunkte und Winkel (Grad gegen den Uhrzeigersinn)
    QList<QPointF> m_anchors;
    QList<double> m_angles;
    double m_markDistance = 0.0; // 0 = nicht an Marken
    bool m_angleTip = true;
    bool m_distanceTip = true;
    QColor m_alignColor;
    double m_pixel = 0.0; // Pixelgitter in mm, 0 = aus
};

} // namespace vek
