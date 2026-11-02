#pragma once

#include "FreehandTool.h"

#include <QList>
#include <QPointF>

#include <optional>

namespace vek {

// Bézierwerkzeug: setzt Knoten für Knoten
//  - Klick setzt einen Knoten, die Strecke dorthin ist gerade
//  - Klicken und Ziehen setzt einen symmetrischen Knoten, die Strecke wird zur Kurve
//  - Klick auf den ersten Knoten schliesst die Kurve
//  - Doppelklick, Eingabe oder Werkzeugwechsel beendet die Kurve, Esc verwirft sie
//  - Beginnt der erste Klick auf einem Endknoten der ausgewählten Kurve, wird sie fortgesetzt
class BezierTool final : public Tool {
public:
    // pen: Stiftwerkzeug (Vorschau mit der echten Kontur der neuen Kurve)
    explicit BezierTool(Canvas& canvas, bool pen = false) : Tool(canvas), m_pen(pen) {}

    void activate() override;
    void deactivate() override;
    void mousePress(QMouseEvent* event, const QPointF& doc) override;
    void mouseMove(QMouseEvent* event, const QPointF& doc) override;
    void mouseRelease(QMouseEvent* event, const QPointF& doc) override;
    void mouseDoubleClick(QMouseEvent* event, const QPointF& doc) override;
    bool keyPress(QKeyEvent* event) override;
    void paintOverlay(QPainter& p) override;

    int pendingNodes() const { return int(m_nodes.size()); }

private:
    // Gesetzter Knoten in Seitenkoordinaten mit seinen Steuerpunkten
    struct Placed {
        QPointF point;
        std::optional<QPointF> in;
        std::optional<QPointF> out;
    };
    static CurveNode segmentTo(const Placed& from, const Placed& to);
    void finish(bool closed);
    void reset();

    bool m_pen = false;
    QList<Placed> m_nodes;
    CurveEnd m_continue;
    SnapResult m_snap;
    bool m_pressed = false;
    bool m_dragging = false;
    QPointF m_pressScreen;
    QPointF m_mouse;
};

} // namespace vek
