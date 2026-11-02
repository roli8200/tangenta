#pragma once

#include "Patterns.h"

#include <QDialog>

class QButtonGroup;
class QDoubleSpinBox;
class QLabel;
class QListWidget;
class QPushButton;

namespace vek {

class ColorButton;

// Musterfüllung und Texturfüllung: zweifarbige Muster, Bitmap-Muster (Bild
// laden) und berechnete Texturen; Farben, Kachelgrösse, "Neu würfeln" bei Texturen
class PatternFillDialog : public QDialog {
    Q_OBJECT

public:
    PatternFillDialog(const PatternFill& pattern, QWidget* parent = nullptr);

    PatternFill pattern() const { return m_pattern; }
    void setKind(PatternFill::Kind kind);
    QListWidget* list() const { return m_list; }
    QDoubleSpinBox* tileBox() const { return m_tile; }

private:
    void fillList();
    void refresh();

    PatternFill m_pattern;
    QButtonGroup* m_kinds = nullptr;
    QListWidget* m_list = nullptr;
    ColorButton* m_fg = nullptr;
    ColorButton* m_bg = nullptr;
    QLabel* m_fgLabel = nullptr;
    QLabel* m_bgLabel = nullptr;
    QDoubleSpinBox* m_tile = nullptr;
    QPushButton* m_load = nullptr;
    QPushButton* m_shuffle = nullptr;
    QLabel* m_preview = nullptr;
};

} // namespace vek
