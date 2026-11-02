#pragma once

#include "Node.h"
#include "Ramp.h"
#include "Tool.h"

#include <optional>

namespace vek {

class DrawObject;

// Interaktive Transparenz und interaktive Unschärfe wie das Transparenzwerkzeug:
// Über ein Objekt ziehen legt einen linearen Verlauf an (Transparenz von deckend nach
// durchsichtig, Unschärfe von scharf nach unscharf); die beiden Endpunkte lassen sich danach
// verschieben. Art (gleichmässig, linear, radial, konisch) und Werte stehen in der
// Eigenschaftsleiste. Strg beim Ziehen: Winkel in 15°-Schritten. Gilt für jedes Objekt.
class EffectTool final : public Tool {
public:
    EffectTool(Canvas& canvas, Effect effect);

    Effect effect() const { return m_effect; }
    void activate() override;
    void deactivate() override;
    void mousePress(QMouseEvent* event, const QPointF& doc) override;
    void mouseMove(QMouseEvent* event, const QPointF& doc) override;
    void mouseRelease(QMouseEvent* event, const QPointF& doc) override;
    bool keyPress(QKeyEvent* event) override;
    void paintOverlay(QPainter& p) override;

    // Standardwerte, wenn ein Objekt den Effekt neu bekommt
    static Ramp defaultRamp(Effect effect, Ramp::Type type, const Ramp& current);

private:
    enum class Drag { None, Create, Start, End, Mid };

    const DrawObject* target() const;
    QPointF toUnit(const DrawObject& o, const QPointF& doc) const;
    QPointF fromUnit(const DrawObject& o, const QPointF& unit) const;
    QString label() const;

    Effect m_effect;
    Drag m_drag = Drag::None;
    NodeId m_target = InvalidNodeId;
    QPointF m_pressDoc;
    std::optional<Ramp> m_preview;
    int m_selectedEnd = -1; // 0 = Anfang, 1 = Ende
};

} // namespace vek
