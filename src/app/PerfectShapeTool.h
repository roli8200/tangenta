#pragma once

#include "PerfectShape.h"
#include "Tool.h"

#include <QIcon>

namespace vek {

// Perfekte Formen: Ziehen erstellt die gewählte Form der Gruppe; der rote
// Glyphengriff einer ausgewählten Form verändert sie (Einzug, Dicke, Zeigerspitze …).
class PerfectShapeTool final : public Tool {
public:
    PerfectShapeTool(Canvas& canvas, perfect::Category category) : Tool(canvas), m_category(category) {}

    perfect::Category category() const { return m_category; }
    void activate() override;
    void deactivate() override;
    void mousePress(QMouseEvent* event, const QPointF& doc) override;
    void mouseMove(QMouseEvent* event, const QPointF& doc) override;
    void mouseRelease(QMouseEvent* event, const QPointF& doc) override;
    void paintOverlay(QPainter& p) override;

    // Gewählte Form je Gruppe (Optionen ws/tools/perfect/<Gruppe>)
    static int currentKind(perfect::Category category);
    static void setCurrentKind(perfect::Category category, int kind);
    static QIcon icon(int kind);

private:
    const PerfectShapeObject* selectedShape() const;
    enum class Drag { None, Create, Glyph };
    Drag m_drag = Drag::None;
    QPointF m_press;
    QPointF m_current;
    QPointF m_glyphPreview; // Einheitskoordinaten während des Ziehens
    perfect::Category m_category;
};

} // namespace vek
