#pragma once

#include "CurveObject.h"
#include "SnapEngine.h"
#include "Tool.h"

#include <QList>
#include <QPointF>

namespace vek {

// Offenes Ende einer ausgewählten Kurve, an dem ein Zeichenwerkzeug weiterzeichnen kann
struct CurveEnd {
    NodeId object = InvalidNodeId;
    int path = -1;
    bool atStart = false;
    QPointF doc;
};
CurveEnd curveEndAt(const Canvas& canvas, const QPointF& screen);

// Freihandwerkzeug (F5):
//  - Klick, Klick: gerade Linie vom ersten zum zweiten Punkt
//  - Ziehen: freie Linie entlang der Maus
//  - Klick auf einen Endknoten der ausgewählten Kurve setzt sie dort fort;
//    endet die Fortsetzung auf dem anderen Ende, schliesst sich die Kurve
//  - Doppelklick am Ende einer Linie beginnt sofort das nächste Segment (Linienzug)
// Das Werkzeug bleibt aktiv; Esc bricht eine angefangene Linie ab.
// Als 2-Punkt-Linie zieht das Ziehen statt einer freien Linie eine gerade Strecke.
// Strg rastet die Richtung in 15°-Schritten ein.
class FreehandTool final : public Tool {
public:
    enum class Mode { Freehand, Line };
    FreehandTool(Canvas& canvas, Mode mode) : Tool(canvas), m_mode(mode) {}

    void activate() override;
    void deactivate() override;
    void mousePress(QMouseEvent* event, const QPointF& doc) override;
    void mouseMove(QMouseEvent* event, const QPointF& doc) override;
    void mouseRelease(QMouseEvent* event, const QPointF& doc) override;
    void mouseDoubleClick(QMouseEvent* event, const QPointF& doc) override;
    bool keyPress(QKeyEvent* event) override;
    void paintOverlay(QPainter& p) override;

private:
    using EndNode = CurveEnd;
    // Einrasten; mit Strg stattdessen Richtung vom Startpunkt in 15°-Schritten
    QPointF snapped(const QPointF& doc, Qt::KeyboardModifiers mods = {});
    // Punkte (Seitenkoordinaten) als neue Kurve anlegen oder an m_continue anhängen
    void commit(const QList<QPointF>& points);
    void reset();

    Mode m_mode;
    SnapResult m_snap;
    bool m_pressed = false;
    bool m_freehand = false;  // Ziehen statt Klicken (freie Linie bzw. gerade Strecke)
    bool m_pending = false;   // erster Punkt gesetzt, zweiter Klick fehlt noch
    QPointF m_pressScreen;
    QPointF m_start;
    QPointF m_current;
    QList<QPointF> m_stroke;
    EndNode m_continue;       // Fortsetzung einer bestehenden Kurve
    EndNode m_lastEnd;        // Ende der zuletzt gezeichneten Linie (für Doppelklick)
};

} // namespace vek
