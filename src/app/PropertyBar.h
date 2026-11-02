#pragma once

#include "Property.h"
#include "Ramp.h"

#include <QHash>
#include <QToolBar>

#include <functional>

class QBoxLayout;
class QComboBox;
class QDoubleSpinBox;
class QSlider;
class QToolButton;

namespace vek {

class Canvas;
class Editor;

// Eigenschaftsleiste. Der Inhalt richtet sich nach der Auswahl:
//  - nichts ausgewählt: Seitengrösse und Ausrichtung
//  - Hilfslinie: Ankerpunkt und Winkel
//  - Beschneiden, Radierer, künstlerische Medien: die Einstellungen des Werkzeugs
//  - interaktives Füllwerkzeug: Füllart, Verlaufsfarben, Winkel, Randauffüllung
//  - Objekte: Lage, Grösse, Skalierung, Drehwinkel, Spiegeln, dann die typeigenen
//    Eigenschaften (DrawObject::propertyDescriptors), zuletzt die Konturbreite
class PropertyBar : public QToolBar {
    Q_OBJECT

public:
    PropertyBar(Editor& editor, Canvas& canvas, QWidget* parent = nullptr);

signals:
    // Bitmap-Befehle; das Hauptfenster öffnet die Dialoge
    void editBitmapRequested();
    void traceRequested(int preset, bool quick);
    void alignDialogRequested();

private:
    void rebuild();
    void sync(); // neu aufbauen oder nur auffrischen
    void refresh();
    void buildSections();
    QString layoutSignature() const;

    void buildPageSection();
    void buildGuideSection();
    void buildFillSection();
    void buildNodeSection();
    void buildCropSection();
    void buildEraserSection();
    void buildMediaSection();
    // Ellipsen- und Polygonwerkzeug ohne Auswahl: Vorgaben für neue Objekte
    void buildShapeDefaultsSection();
    void buildMeshSection();
    void buildShadowSection();
    void buildInteractiveSection();
    void buildGeometrySection();
    void buildTypeSection(const QList<PropertyDescriptor>& descriptors);
    void buildOutlineSection();
    // Schrift, Grösse, Fett/Kursiv/Unterstrichen, Ausrichtung: beim Bearbeiten für die
    // Markierung, sonst für ausgewählten Text als Ganzes bzw. als Vorgabe für neuen Text
    void buildTextSection();
    // Bitmap: Bitmap bearbeiten, vektorisieren, Rahmen zurücksetzen
    void buildImageSection();
    // Interaktive Transparenz/Unschärfe: Art und Werte am Anfang und Ende
    void buildEffectSection(Effect effect);
    // Unschärfe-Schieberegler für jede Objektleiste
    void buildBlurSlider();
    // Formen-Knöpfe bei mehreren ausgewählten Objekten
    void buildShapingSection();
    // Schieberegler mit Zahlenfeld für einen Effekt der Auswahl; beim Ziehen nur Vorschau,
    // beim Loslassen ein Undo-Schritt
    std::pair<QSlider*, QDoubleSpinBox*> addEffectSlider(Effect effect, const QString& iconName, const QString& tooltip,
                                                         double maximum, double step, const QString& suffix,
                                                         int decimals, std::function<double(const Ramp&)> get,
                                                         std::function<void(Ramp&, double)> modify, const QString& label);

    // Bausteine
    QDoubleSpinBox* addField(QBoxLayout* into, const QString& iconName, const QString& tooltip, const QString& suffix,
                             int decimals, std::function<double()> get, std::function<void(double)> set);
    // Feld mit Pfeilknöpfen (Seitengrösse, Abstände)
    void addSpinButtons(QDoubleSpinBox* spin, double step);
    QComboBox* addArrowCombo(bool atStart);
    QIcon arrowIcon(int index, bool atStart, const QSize& size) const;
    QIcon dashIcon(int index, const QSize& size) const;
    QBoxLayout* addStack();
    QToolButton* addButton(const QString& iconName, const QString& tooltip, std::function<void()> onClick);
    void addSeparator();

    Editor& m_editor;
    Canvas& m_canvas;
    // Aufbewahrte Zusammensetzungen der Leiste, je Signatur eine
    struct Section {
        QWidget* host = nullptr;
        QBoxLayout* row = nullptr;
        QList<std::function<void()>> refreshers;
    };
    QHash<QString, Section> m_sections;
    QWidget* m_frame = nullptr;
    QWidget* m_host = nullptr; // die gerade sichtbare Leiste
    QBoxLayout* m_row = nullptr;
    QString m_signature;
    QList<std::function<void()>> m_refreshers;
    bool m_lockRatio = true;
    bool m_allPages = true; // Seitengrösse gilt für alle Seiten oder nur die aktuelle
};

} // namespace vek
