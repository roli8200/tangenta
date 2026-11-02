#pragma once

#include "Style.h"

#include <QDialog>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QWidget;

namespace vek {

class ColorButton;

// Konturstift (F12): Farbe, Breite, Stil, Ecken, Linienenden, Lage der
// Kontur, Pfeilspitzen, Kalligrafie (Strecken, Winkel), "Hinter Füllung", "Mit Objekt skalieren"
class OutlinePenDialog : public QDialog {
    Q_OBJECT

public:
    explicit OutlinePenDialog(const Style& style, QWidget* parent = nullptr);

    // Ausgangsstil mit den Einstellungen des Dialogs
    Style style() const;

    QDoubleSpinBox* widthBox() const { return m_width; }
    QButtonGroup* joinGroup() const { return m_join; }
    QButtonGroup* capGroup() const { return m_cap; }
    QButtonGroup* positionGroup() const { return m_position; }
    QCheckBox* behindFillBox() const { return m_behind; }
    QCheckBox* scaleBox() const { return m_scale; }
    QDoubleSpinBox* stretchBox() const { return m_stretch; }
    QDoubleSpinBox* angleBox() const { return m_angle; }

private:
    Style m_style;
    ColorButton* m_color = nullptr;
    QCheckBox* m_noOutline = nullptr;
    QDoubleSpinBox* m_width = nullptr;
    QComboBox* m_dash = nullptr;
    QButtonGroup* m_join = nullptr;
    QButtonGroup* m_cap = nullptr;
    QButtonGroup* m_position = nullptr;
    QComboBox* m_startArrow = nullptr;
    QComboBox* m_endArrow = nullptr;
    QDoubleSpinBox* m_stretch = nullptr;
    QDoubleSpinBox* m_angle = nullptr;
    QCheckBox* m_behind = nullptr;
    QCheckBox* m_scale = nullptr;
    QWidget* m_preview = nullptr;
    QWidget* m_nib = nullptr;
};

} // namespace vek
