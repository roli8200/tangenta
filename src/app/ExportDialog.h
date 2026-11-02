#pragma once

#include "Output.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QRadioButton;
class QSpinBox;

namespace vek {

class Canvas;

// Exportieren: nach der Dateiwahl die Einstellungen des Formats. Allen
// gemeinsam ist "Nur markierte Objekte"; Bitmaps bekommen Auflösung, Grösse, Farbart,
// Transparenz und Qualität, PDF die Seitenwahl.
class ExportDialog : public QDialog {
    Q_OBJECT

public:
    enum class Kind { Bitmap, Vector, Pdf };
    ExportDialog(Kind kind, const QString& suffix, Canvas& canvas, QWidget* parent = nullptr);

    output::ExportOptions options() const;
    QCheckBox* selectionOnlyBox() const { return m_selectionOnly; }
    QSpinBox* dpiBox() const { return m_dpi; }

private:
    void updateSize();

    Kind m_kind;
    QString m_suffix;
    Canvas& m_canvas;
    QCheckBox* m_selectionOnly = nullptr;
    QRadioButton* m_allPages = nullptr;
    QSpinBox* m_dpi = nullptr;
    QSpinBox* m_width = nullptr;
    QSpinBox* m_height = nullptr;
    QComboBox* m_colorMode = nullptr;
    QCheckBox* m_transparent = nullptr;
    QSpinBox* m_quality = nullptr;
    QSpinBox* m_flattenDpi = nullptr;
    QLabel* m_area = nullptr;
    bool m_updating = false;
};

} // namespace vek
