#pragma once

#include "Effects.h"
#include "Node.h"
#include "Tool.h"

#include <optional>

namespace vek {

class DrawObject;

// Hilfsmittel 'Blockschatten': vom Objekt in eine Richtung ziehen ergibt einen massiven,
// flachen Schatten (das Objekt entlang des Zugs geschoben). Ein bestehender Schatten zeigt
// einen Endgriff, der sich verschieben lässt. Tiefe, Richtung, Farbe, Vergrössern, Vereinfachen
// stehen in der Eigenschaftsleiste; Strg+K trennt den Schatten als Kurve ab.
class BlockShadowTool final : public Tool {
public:
    explicit BlockShadowTool(Canvas& canvas) : Tool(canvas) {}

    void activate() override;
    void deactivate() override;
    void mousePress(QMouseEvent* event, const QPointF& doc) override;
    void mouseMove(QMouseEvent* event, const QPointF& doc) override;
    void mouseRelease(QMouseEvent* event, const QPointF& doc) override;
    bool keyPress(QKeyEvent* event) override;
    void paintOverlay(QPainter& p) override;

    // Griffe: Objektmitte und Ende des Schattens (Mitte + Versatz)
    static std::pair<QPointF, QPointF> handles(const DrawObject& o, const BlockShadow& s);

private:
    NodeId m_target = 0;
    QPointF m_press;
    QPointF m_startOffset;
    bool m_dragging = false;
    bool m_dragEnd = false;
    std::optional<BlockShadow> m_preview;
};

} // namespace vek
