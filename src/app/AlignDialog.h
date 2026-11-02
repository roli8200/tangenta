#pragma once

#include "Align.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QRadioButton;
class QTabWidget;

namespace vek {

class Editor;

// Anordnen > Ausrichten und verteilen …: Reiter "Ausrichten" (Links/
// Zentriert/Rechts, Oben/Mitte/Unten, Bezug) und "Verteilen" (Links/Zentriert/Abstand/Rechts,
// Oben/Mitte/Abstand/Unten, Auswahl- oder Seitenumfang). Nicht modal: "Zuweisen" wirkt auf die
// aktuelle Auswahl, das Fenster bleibt offen.
class AlignDialog final : public QDialog {
    Q_OBJECT

public:
    explicit AlignDialog(Editor& editor, QWidget* parent = nullptr);

    void showAlignTab();
    void showDistributeTab();
    // Für Tests: Häkchen setzen und zuweisen
    void setAlign(align::H h, align::V v, int target = 0);
    void setDistribute(align::Spread h, align::Spread v, bool toPage);
    void apply();

private:
    void exclusive(QCheckBox* box, const QList<QCheckBox*>& group);

    Editor& m_editor;
    QTabWidget* m_tabs;
    QCheckBox* m_left;
    QCheckBox* m_hCenter;
    QCheckBox* m_right;
    QCheckBox* m_top;
    QCheckBox* m_vCenter;
    QCheckBox* m_bottom;
    QComboBox* m_target;
    QDoubleSpinBox* m_pointX;
    QDoubleSpinBox* m_pointY;
    QList<QCheckBox*> m_dh; // Links, Zentriert, Abstand, Rechts
    QList<QCheckBox*> m_dv; // Oben, Mitte, Abstand, Unten
    QRadioButton* m_toSelection;
    QRadioButton* m_toPage;
};

} // namespace vek
