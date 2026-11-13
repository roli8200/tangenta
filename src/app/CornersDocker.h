#pragma once

#include "PathOps.h"

#include <QWidget>

class QButtonGroup;
class QDoubleSpinBox;
class QLabel;
class QPushButton;

namespace vek {

class Canvas;
class Editor;

// Andockfenster "Ecken" (Fenster > Andockfenster): Abrunden, Ausbuchten oder Abschrägen der
// spitzen Ecken von Kurven und Grundformen mit Radius bzw. Abstand. Sind im Formwerkzeug Knoten
// gewählt, wirkt es nur auf diese; Grundformen werden dabei in Kurven umgewandelt.
class CornersDocker final : public QWidget {
    Q_OBJECT

public:
    CornersDocker(Editor& editor, Canvas& canvas, QWidget* parent = nullptr);

    pathops::CornerOp operation() const;
    void setOperation(pathops::CornerOp op);
    double radius() const;
    void setRadius(double mm);
    bool apply();
    QPushButton* applyButton() const { return m_apply; }

private:
    QList<NodeRef> selectedNodes() const;
    void refresh();

    Editor& m_editor;
    Canvas& m_canvas;
    QButtonGroup* m_ops = nullptr;
    QLabel* m_radiusLabel = nullptr;
    QDoubleSpinBox* m_radius = nullptr;
    QLabel* m_scope = nullptr;
    QPushButton* m_apply = nullptr;
};

} // namespace vek
