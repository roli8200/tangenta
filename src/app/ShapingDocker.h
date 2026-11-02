#pragma once

#include "Editor.h"

#include <QWidget>

#include <array>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;

namespace vek {

class Canvas;

// Andockfenster "Formen": Operation wählen, Vorschau, "Originalquellobjekt
// beibehalten" / "Originalzielobjekt beibehalten". Verschmelzen, Zuschneiden und Schnittmenge
// wirken von der Auswahl (Quelle) auf ein Ziel, das man nach dem Knopf anklickt; die übrigen
// Operationen gelten für die Auswahl.
class ShapingDocker : public QWidget {
    Q_OBJECT

public:
    ShapingDocker(Editor& editor, Canvas& canvas, QWidget* parent = nullptr);

    Editor::ShapeOp operation() const;
    void setOperation(Editor::ShapeOp op);
    // Knopf (Verschmelzen mit / Zuschneiden / Schneiden mit / Anwenden)
    void apply();
    QPushButton* applyButton() const { return m_apply; }
    QCheckBox* keepSourceBox() const { return m_keepSource; }
    QCheckBox* keepTargetBox() const { return m_keepTarget; }

private:
    static bool needsTarget(Editor::ShapeOp op);
    void refresh();
    void drawPreview();

    Editor& m_editor;
    Canvas& m_canvas;
    QComboBox* m_ops = nullptr;
    QLabel* m_preview = nullptr;
    QCheckBox* m_keepSource = nullptr;
    QCheckBox* m_keepTarget = nullptr;
    QPushButton* m_apply = nullptr;
    // Häkchen je Operation, mit den Vorgaben
    std::array<std::pair<bool, bool>, 7> m_keep{};
};

} // namespace vek
