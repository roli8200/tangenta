#pragma once

#include "Vectorizer.h"

#include <QDialog>
#include <QImage>

#include <memory>

class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
class QRadioButton;
class QSlider;
class QSpinBox;
class QTimer;
class QToolButton;

namespace vek {

class TracePreview;
struct TraceView;

// "SuperTrace": Vorschau (Vorher und Nachher, grosse Vorschau,
// Drahtmodell-Überlagerung mit Transparenz; zoomen, verschieben), Bildtyp als Voreinstellung,
// Register "Einstellungen" (Detail, Glättung, Kantenglättung, Optionen, Hintergrund) und
// "Farben" (Anzahl, Liste, zusammenführen, bearbeiten); Rückgängig/Wiederholen der
// Einstellungen, Zurücksetzen; zählt Kurven, Knoten, Farben.
class TraceDialog : public QDialog {
    Q_OBJECT

public:
    TraceDialog(const QImage& image, trace::Preset preset, QWidget* parent = nullptr);
    ~TraceDialog() override;

    const trace::Result& result() const { return m_result; }
    trace::Settings settings() const;
    trace::Preset preset() const;
    bool deleteOriginal() const;
    // Sofort rechnen (sonst kurz nach der letzten Änderung)
    void recompute();

    // Für Tests und das Register "Farben"
    void setSettings(const trace::Settings& settings);
    void mergeColors(const QList<QRgb>& sources);
    void editColor(QRgb source, const QColor& color);
    bool canUndo() const { return m_historyPos > 0; }
    bool canRedo() const { return m_historyPos + 1 < m_history.size(); }
    void undo();
    void redo();
    void reset();
    // 0 = Vorher und Nachher, 1 = Grosse Vorschau, 2 = Drahtmodell-Überlagerung
    void setPreviewMode(int mode);

signals:
    // Knopf "Optionen": Seite SuperTrace im Dialog Optionen
    void optionsRequested();

private:
    void applyPreset(trace::Preset preset);
    void schedule();
    void updateEnabled();
    void updateColors();
    void showBackgroundColor();
    void pickBackground(const QColor& color);
    void showHelp();

    QImage m_image;
    trace::Result m_result;
    std::unique_ptr<TraceView> m_view; // Zoom und Ausschnitt beider Vorschaufenster
    QTimer* m_timer = nullptr;
    TracePreview* m_before = nullptr;
    TracePreview* m_after = nullptr;
    QComboBox* m_previewMode = nullptr;
    QSlider* m_transparency = nullptr;
    QComboBox* m_preset = nullptr;
    QSlider* m_detail = nullptr;
    QSlider* m_smoothing = nullptr;
    QSlider* m_corners = nullptr;
    QSpinBox* m_smoothingValue = nullptr;
    QSpinBox* m_cornersValue = nullptr;
    QSpinBox* m_colors = nullptr;
    QCheckBox* m_deleteOriginal = nullptr;
    QCheckBox* m_removeBackground = nullptr;
    QRadioButton* m_backgroundAuto = nullptr;
    QRadioButton* m_backgroundGiven = nullptr;
    QToolButton* m_backgroundSwatch = nullptr;
    QToolButton* m_pipette = nullptr;
    QCheckBox* m_removeEverywhere = nullptr;
    QCheckBox* m_mergeAdjacent = nullptr;
    QCheckBox* m_removeOverlap = nullptr;
    QCheckBox* m_groupByColor = nullptr;
    QComboBox* m_colorSort = nullptr;
    QListWidget* m_colorList = nullptr;
    QPushButton* m_mergeButton = nullptr;
    QPushButton* m_editButton = nullptr;
    QToolButton* m_undoButton = nullptr;
    QToolButton* m_redoButton = nullptr;
    QLabel* m_curves = nullptr;
    QLabel* m_nodes = nullptr;
    QLabel* m_colorCount = nullptr;
    QColor m_background = Qt::white;
    QHash<QRgb, QRgb> m_recolor;
    QList<trace::Settings> m_history;
    int m_historyPos = -1;
    bool m_restoring = false;
};

} // namespace vek
