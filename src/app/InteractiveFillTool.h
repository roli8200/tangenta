#pragma once

#include "Style.h"
#include "Tool.h"

#include "Node.h"

#include <optional>

namespace vek {

class DrawObject;

// Interaktive Füllung (G): über ein Objekt ziehen legt einen Farbverlauf
// von der Start- zur Endstelle an; die beiden Endpunkte lassen sich danach verschieben.
// Ein Klick auf einen Endpunkt wählt ihn aus, eine Palettenfarbe färbt dann diesen Endpunkt.
// Strg beim Ziehen: Winkel in 15°-Schritten. Funktioniert für jedes Zeichenobjekt.
class InteractiveFillTool final : public Tool {
public:
    using Tool::Tool;

    void activate() override;
    void deactivate() override;
    void mousePress(QMouseEvent* event, const QPointF& doc) override;
    void mouseMove(QMouseEvent* event, const QPointF& doc) override;
    void mouseRelease(QMouseEvent* event, const QPointF& doc) override;
    bool keyPress(QKeyEvent* event) override;
    bool paletteColor(const std::optional<Color>& color, bool fill) override;
    void paintOverlay(QPainter& p) override;

private:
    enum class Drag { None, Create, Start, End, Mid };

    const DrawObject* target() const;
    QPointF toUnit(const DrawObject& o, const QPointF& doc) const;
    QPointF fromUnit(const DrawObject& o, const QPointF& unit) const;
    QPointF constrained(const QPointF& from, const QPointF& to, Qt::KeyboardModifiers mods) const;
    Fill baseFill(const DrawObject& o) const;

public:
    // Mittelpunkt eines Zweifarbverlaufs wie im Verlaufsdialog (Lage des Mittelstopps mit der
    // Mischfarbe); leer bei eigenen Zwischenfarben
    static std::optional<double> midpoint(const Fill& f);
    static void setMidpoint(Fill& f, double position);

    Drag m_drag = Drag::None;
    NodeId m_target = InvalidNodeId;
    QPointF m_pressDoc;
    std::optional<Fill> m_preview;
    int m_selectedEnd = -1; // 0 = Anfang, 1 = Ende
};

} // namespace vek
