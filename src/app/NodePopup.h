#pragma once

#include "Color.h"

#include <QFrame>

#include <optional>

class QLabel;
class QSlider;
class QToolButton;

namespace vek {

class ColorButton;
class ExpressionSpinBox;

// Kleines Fenster direkt neben einem Verlaufsknoten (interaktive Füllung und Transparenz):
//  - Farbknoten: Farbknopf (Farbwähler mit Paletten), Pipette, Transparenz des Knotens,
//    bei Zwischenknoten die Lage in Prozent
//  - Wertknoten (Transparenz): nur der Regler
// Ein Kind der Zeichenfläche, kein eigenes Fenster; es bleibt offen, bis das Werkzeug es schliesst.
class NodePopup : public QFrame {
    Q_OBJECT

public:
    explicit NodePopup(QWidget* canvas);

    // at = Knoten in Bildschirmkoordinaten der Zeichenfläche; das Fenster erscheint daneben
    void showColor(const Color& color, std::optional<double> position, const QPointF& at);
    void showValue(double value, const QString& label, const QPointF& at);

    ColorButton* colorButton() const { return m_color; }
    QToolButton* eyedropperButton() const { return m_eyedropper; }
    QSlider* slider() const { return m_slider; }
    ExpressionSpinBox* positionField() const { return m_position; }

signals:
    // final = false: Vorschau beim Ziehen des Reglers, true: übernehmen (ein Undo-Schritt)
    void colorEdited(const vek::Color& color, bool final);
    void positionEdited(double position); // 0..1
    void valueEdited(double value, bool final);
    void eyedropperRequested();

private:
    void place(const QPointF& at);
    void emitSlider(bool final);

    ColorButton* m_color = nullptr;
    QToolButton* m_eyedropper = nullptr;
    QLabel* m_sliderLabel = nullptr;
    QSlider* m_slider = nullptr;
    QLabel* m_sliderValue = nullptr;
    QLabel* m_positionLabel = nullptr;
    ExpressionSpinBox* m_position = nullptr;
    Color m_current;
    bool m_valueMode = false;
};

} // namespace vek
