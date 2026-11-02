#pragma once

#include "CurveObject.h"
#include "Tool.h"

#include <QPolygonF>

namespace vek {

// Effekte > Perspektive hinzufügen: vier Eckgriffe am Begrenzungsrahmen,
// Ziehen verzerrt das Objekt perspektivisch (Projektion des Rechtecks auf das Viereck).
// Das Objekt wird dafür in Kurven umgewandelt; jede Griffbewegung ist ein Rückgängig-Schritt,
// gerechnet immer vom Zustand beim Start aus. Esc oder Werkzeugwechsel beendet.
class PerspectiveTool final : public Tool {
public:
    explicit PerspectiveTool(Canvas& canvas) : Tool(canvas) {}

    void activate() override;
    void deactivate() override;
    void mousePress(QMouseEvent* event, const QPointF& doc) override;
    void mouseMove(QMouseEvent* event, const QPointF& doc) override;
    void mouseRelease(QMouseEvent* event, const QPointF& doc) override;
    bool keyPress(QKeyEvent* event) override;
    void paintOverlay(QPainter& p) override;

    bool active() const { return m_target != 0; }
    const QPolygonF& quad() const { return m_quad; }
    // Für Tests: Eckpunkt i (0 = oben links, im Uhrzeigersinn) auf doc setzen
    void moveCorner(int corner, const QPointF& doc);

private:
    QList<SubPath> mapped(const QPolygonF& quad) const;

    NodeId m_target = 0;
    QList<SubPath> m_original;
    QRectF m_box;
    QPolygonF m_quad;
    int m_drag = -1;
};

} // namespace vek
