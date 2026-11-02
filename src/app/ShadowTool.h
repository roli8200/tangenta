#pragma once

#include "Effects.h"
#include "Node.h"
#include "Tool.h"

#include <optional>

namespace vek {

class DrawObject;

// Hilfsmittel 'Hinterlegter Schatten': vom Objekt wegziehen ergibt einen
// flachen Schatten mit diesem Versatz; an der Unter- oder Oberkante angesetzt einen
// perspektivischen Schatten, dessen Richtung und Länge das Ziehen bestimmt. Bestehende Schatten
// zeigen Anfangs- und Endgriff; der Endgriff lässt sich verschieben. Deckung, Federung, Farbe
// usw. stehen in der Eigenschaftsleiste.
class ShadowTool final : public Tool {
public:
    explicit ShadowTool(Canvas& canvas) : Tool(canvas) {}

    void activate() override;
    void deactivate() override;
    void mousePress(QMouseEvent* event, const QPointF& doc) override;
    void mouseMove(QMouseEvent* event, const QPointF& doc) override;
    void mouseRelease(QMouseEvent* event, const QPointF& doc) override;
    bool keyPress(QKeyEvent* event) override;
    void paintOverlay(QPainter& p) override;

    // Flacher Schatten: der Zug ist 3-fach untersetzt (Versatz = Zuglänge / 3). Der Endgriff
    // liegt entsprechend weiter weg, damit der Deckungsregler auf der Linie genug Weg hat.
    static constexpr double kGearing = 3.0;
    // Schatten aus einem Zug von press nach to auf o (für Tests und die Interaktion)
    static DropShadow shadowFor(const DrawObject& o, const QPointF& press, const QPointF& to, const DropShadow& base);
    // Griffe: Anfang (Objektmitte bzw. Kantenmitte) und Ende des Schattens
    static std::pair<QPointF, QPointF> handles(const DrawObject& o, const DropShadow& s);

private:
    const DrawObject* target() const;

    NodeId m_target = 0;
    QPointF m_press;
    bool m_dragging = false;
    bool m_dragEnd = false;     // vorhandenen Endgriff ziehen
    bool m_dragOpacity = false; // Deckungsregler auf der Linie ziehen
    std::optional<DropShadow> m_preview;
};

} // namespace vek
