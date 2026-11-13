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
    QComboBox* effectBox() const { return m_effect; }

private:
    void load();
    void updateFields();

    Editor& m_editor;
    QComboBox* m_type = nullptr;
    QLabel* m_amountLabel = nullptr;
    QDoubleSpinBox* m_amount = nullptr;
    QComboBox* m_effect = nullptr;
    QLabel* m_effectLabel = nullptr;
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
    QComboBox* mappingBox() const { return m_mapping; }
    // Gewählte Vorlage (keine = Rechteck) auf die Auswahl
    bool apply();
    // Form eines anderen Objekts als Hülle (der nächste Klick wählt es)
    void beginFromObject();
    static void setMode(Editor& editor, Envelope::Mode mode);
    // Zuordnungsmodus für neue Hüllen (Optionen) und die ausgewählten Objekte
    static Envelope::Mapping mapping();
    static void setMapping(Editor& editor, Envelope::Mapping mapping);

private:
    Editor& m_editor;
    Canvas& m_canvas;
    QListWidget* m_presets = nullptr;
    QButtonGroup* m_modes = nullptr;
    QCheckBox* m_addNew = nullptr;
    QComboBox* m_mapping = nullptr;
};

// Andockfenster "Effekte": der Effektstapel (nicht-destruktive Bitmap-Effekte) des ausgewählten
// Objekts. Ein/Aus mit dem Auge (Häkchen), Doppelklick bzw. "Bearbeiten" öffnet den Dialog des
// Effekts mit den gespeicherten Werten, Reihenfolge, Löschen, "Effekte anwenden" brennt ein.
class BitmapEffectsDocker : public QWidget {
    Q_OBJECT

public:
    BitmapEffectsDocker(Editor& editor, Canvas& canvas, QWidget* parent = nullptr);

    QListWidget* list() const { return m_list; }
    // Objekt, dessen Stapel gezeigt wird (erstes ausgewähltes), sonst nullptr
    const DrawObject* target() const;
    void setEffectEnabled(int row, bool on);
    void moveEffect(int row, int delta);
    void removeEffect(int row);
    bool editEffect(int row);
    // Objekt samt Effekten in ein Bild umwandeln (Auflösung: Dokumentauflösung)
    NodeId applyEffects();

    // Bild für den Dialog eines Effekts: Objekt mit den ersten count Effekten, höchstens
    // ~1200 Pixel; referenceWidth = Breite in Referenzpixeln (für die Pixelregler)
    static QImage dialogImage(const Canvas& canvas, const DrawObject& o, int count, double* referenceWidth);

private:
    void reload();

    Editor& m_editor;
    Canvas& m_canvas;
    QListWidget* m_list = nullptr;
    QPushButton* m_edit = nullptr;
    QPushButton* m_up = nullptr;
    QPushButton* m_down = nullptr;
    QPushButton* m_remove = nullptr;
    QPushButton* m_apply = nullptr;
    QLabel* m_info = nullptr;
    bool m_loading = false;
};

} // namespace vek
