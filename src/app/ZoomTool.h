#pragma once

#include "Tool.h"

#include <QPointF>

namespace vek {

// Zoomwerkzeug: Klick vergrössert, Rechtsklick oder Umschalt+Klick verkleinert,
// Rahmen aufziehen zoomt auf den Rahmen.
// Hilfsmittel 'Schwenken' (H): Ansicht mit der Maus verschieben; rechte Maustaste verkleinert
// oder zeigt das Zoom-Kontextmenü (Optionen > Zoom/Schwenken)
class PanTool final : public Tool {
public:
    using Tool::Tool;
    void activate() override;
    void mousePress(QMouseEvent* event, const QPointF& doc) override;
    void mouseMove(QMouseEvent* event, const QPointF& doc) override;
    void mouseRelease(QMouseEvent* event, const QPointF& doc) override;
    void paintOverlay(QPainter&) override {}

private:
    bool m_dragging = false;
    QPointF m_last;
};

class ZoomTool final : public Tool {
public:
    using Tool::Tool;

    void activate() override;
    void mousePress(QMouseEvent* event, const QPointF& doc) override;
    void mouseMove(QMouseEvent* event, const QPointF& doc) override;
    void mouseRelease(QMouseEvent* event, const QPointF& doc) override;
    void paintOverlay(QPainter& p) override;

private:
    bool m_dragging = false;
    QPointF m_startScreen;
    QPointF m_currentScreen;
};

} // namespace vek
