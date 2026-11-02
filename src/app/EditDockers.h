#pragma once

#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QToolButton;

namespace vek {

class Editor;

// Andockfenster "Objekteigenschaften" (Bearbeiten > Objekteigenschaften, Alt+Eingabe):
// Füllung, Kontur, Transparenz/Unschärfe und Allgemeines der Auswahl auf einen Blick.
class ObjectPropertiesDocker final : public QWidget {
    Q_OBJECT

public:
    explicit ObjectPropertiesDocker(Editor& editor, QWidget* parent = nullptr);

private:
    void refresh();

    Editor& m_editor;
    QLabel* m_kind;
    QLineEdit* m_name;
    QToolButton* m_fill;
    QToolButton* m_stroke;
    QDoubleSpinBox* m_width;
    QComboBox* m_dash;
    QDoubleSpinBox* m_transparency;
    QDoubleSpinBox* m_blur;
    QCheckBox* m_locked;
    bool m_updating = false;
};

// Andockfenster "Symbol-Manager" (Bearbeiten > Symbol): Symbole des Dokuments, einfügen,
// umbenennen, unbenutzte löschen.
class SymbolDocker final : public QWidget {
    Q_OBJECT

public:
    explicit SymbolDocker(Editor& editor, QWidget* parent = nullptr);
    void refresh();

private:
    Editor& m_editor;
    QListWidget* m_list;
};

} // namespace vek
