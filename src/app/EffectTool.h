#pragma once

#include "Blend.h"
#include "Node.h"
#include "Ramp.h"
#include "Tool.h"

class QComboBox;
class QWidget;

#include <optional>

namespace vek {

class DrawObject;
class NodePopup;

// Auswahlliste der Mischmodi (Transparenz, Schatten, Objekteigenschaften); Daten = BlendMode
QComboBox* createBlendModeCombo(QWidget* parent, const QString& tooltip);
void setBlendModeComboValue(QComboBox* combo, BlendMode mode);

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
    // Transparenz: Fenster neben dem angeklickten Endpunkt (Regler für dessen Wert)
    NodePopup* popup() const { return m_popup; }

private:
    enum class Drag { None, Create, Start, End, Mid };

    const DrawObject* target() const;
    QPointF toUnit(const DrawObject& o, const QPointF& doc) const;
    QPointF fromUnit(const DrawObject& o, const QPointF& unit) const;
    QString label() const;
    void showPopup();
    void hidePopup();

    Effect m_effect;
    Drag m_drag = Drag::None;
    NodeId m_target = InvalidNodeId;
    QPointF m_pressDoc;
    std::optional<Ramp> m_preview;
    int m_selectedEnd = -1; // 0 = Anfang, 1 = Ende
    QPointF m_pressScreen;
    bool m_moved = false;
    NodePopup* m_popup = nullptr;
};

} // namespace vek
