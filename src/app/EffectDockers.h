#pragma once

#include "Editor.h"

#include <QWidget>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QListWidget;
class QPushButton;

namespace vek {

class Canvas;

// Andockfenster "Linse" (Alt+F3): Art, Stärke/Rate, Farben, "Fixiert" (macht
// das Gezeigte zu Objekten), "Blickpunkt" (zeigt eine andere Stelle), Anwenden
class LensDocker : public QWidget {
    Q_OBJECT

public:
    explicit LensDocker(Editor& editor, QWidget* parent = nullptr);

    Lens lens() const;
    void setLensType(Lens::Type type);
    void apply();
    QComboBox* typeBox() const { return m_type; }
    QDoubleSpinBox* amountBox() const { return m_amount; }
    QCheckBox* frozenBox() const { return m_frozen; }
    QCheckBox* viewpointBox() const { return m_viewpoint; }
    QDoubleSpinBox* viewX() const { return m_vx; }
    QDoubleSpinBox* viewY() const { return m_vy; }

private:
    void load();
    void updateFields();

    Editor& m_editor;
    QComboBox* m_type = nullptr;
    QLabel* m_amountLabel = nullptr;
    QDoubleSpinBox* m_amount = nullptr;
    QPushButton* m_color = nullptr;
    QPushButton* m_color2 = nullptr;
    QLabel* m_colorLabel = nullptr;
    QLabel* m_color2Label = nullptr;
    QCheckBox* m_frozen = nullptr;
    QCheckBox* m_viewpoint = nullptr;
    QDoubleSpinBox* m_vx = nullptr;
    QDoubleSpinBox* m_vy = nullptr;
    QPushButton* m_apply = nullptr;
    Color m_c1{255, 0, 0};
    Color m_c2{255, 255, 0};
    bool m_loading = false;
};

// Andockfenster "Hülle" (Strg+F7): Vorlagen, Modus, Neue Hülle, Hülle aus
// einem Objekt erstellen, Anwenden
class EnvelopeDocker : public QWidget {
    Q_OBJECT

public:
    EnvelopeDocker(Editor& editor, Canvas& canvas, QWidget* parent = nullptr);

    QListWidget* presetList() const { return m_presets; }
    QCheckBox* addNewBox() const { return m_addNew; }
    // Gewählte Vorlage (keine = Rechteck) auf die Auswahl
    bool apply();
    // Form eines anderen Objekts als Hülle (der nächste Klick wählt es)
    void beginFromObject();
    static void setMode(Editor& editor, Envelope::Mode mode);

private:
    Editor& m_editor;
    Canvas& m_canvas;
    QListWidget* m_presets = nullptr;
    QButtonGroup* m_modes = nullptr;
    QCheckBox* m_addNew = nullptr;
};

} // namespace vek
