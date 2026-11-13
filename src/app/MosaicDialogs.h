#pragma once

#include "Mosaic.h"
#include "Node.h"

#include <QDialog>
#include <QImage>
#include <QTransform>

#include <memory>
#include <optional>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTimer;

namespace vek {

class Canvas;
class DrawObject;
class Editor;

// Vorlage der Mosaik-Effekte: ein ausgewähltes Bild oder die Auswahl als Bild gerechnet
struct MosaicSource {
    QImage image;         // 8 Bit vormultipliziert
    QSizeF size;          // mm (lokal)
    QTransform toPage;    // lokal -> Seite
    NodeId id = InvalidNodeId; // Objekt, über das das Ergebnis kommt (das oberste der Auswahl)
    QList<NodeId> objects; // alle Objekte, die die Vorlage bilden
};
// Aus der Auswahl: genau ein Bild (bzw. das erste Bild, wenn skipShapes) direkt, sonst gerastert
std::optional<MosaicSource> mosaicSource(Canvas& canvas, double dpi = 150.0, bool imageOnly = false);

// Effekte > Fotomosaik: Ordner mit Bildern wählen, Raster, Farbangleichung, Wiederholungen,
// Ausgabe als Gruppe aus Bildern oder als ein Bild; Vorschau
class PhotoMosaicDialog : public QDialog {
    Q_OBJECT

public:
    PhotoMosaicDialog(Canvas& canvas, MosaicSource source, QWidget* parent = nullptr);
    ~PhotoMosaicDialog() override;

    mosaics::PhotoSettings settings() const;
    // Ordner laden (auch aus der Eingabezeile)
    void setFolder(const QString& folder);
    const QList<mosaics::Tile>& tiles() const { return m_tiles; }
    QList<mosaics::Cell> cells() const;
    // Ergebnis einfügen (ein Undo-Schritt); InvalidNodeId, wenn nichts entsteht
    NodeId apply();

    QLineEdit* folderEdit() const { return m_folder; }
    QLabel* foundLabel() const { return m_found; }
    QSpinBox* columnsBox() const { return m_columns; }
    QSpinBox* rowsBox() const { return m_rows; }
    QDoubleSpinBox* tintBox() const { return m_tint; }
    QSpinBox* repeatsBox() const { return m_repeats; }
    QComboBox* outputBox() const { return m_output; }
    QCheckBox* replaceBox() const { return m_replace; }
    QCheckBox* neighborsBox() const { return m_neighbors; }
    QLabel* previewLabel() const { return m_preview; }

private:
    void schedulePreview();
    void updatePreview();
    void saveSettings() const;

    Canvas& m_canvas;
    MosaicSource m_source;
    QList<mosaics::Tile> m_tiles;
    QString m_loaded;
    QLineEdit* m_folder = nullptr;
    QLabel* m_found = nullptr;
    QSpinBox* m_columns = nullptr;
    QSpinBox* m_rows = nullptr;
    QDoubleSpinBox* m_tint = nullptr;
    QSpinBox* m_repeats = nullptr;
    QCheckBox* m_neighbors = nullptr;
    QComboBox* m_output = nullptr;
    QSpinBox* m_tilePixels = nullptr;
    QCheckBox* m_replace = nullptr;
    QLabel* m_preview = nullptr;
    QPushButton* m_ok = nullptr;
    QTimer* m_timer = nullptr;
};

// Effekte > Punktraster: Vektorraster aus Kreisen, Quadraten oder einer eigenen Form (das zweite
// ausgewählte Objekt); Farbe und/oder Grösse aus dem Bild; Vorschau
class PointillizeDialog : public QDialog {
    Q_OBJECT

public:
    enum class Grouping { Single, PerColor, CombinedPerColor };
    PointillizeDialog(Canvas& canvas, MosaicSource source, const QPainterPath& customShape, QWidget* parent = nullptr);
    ~PointillizeDialog() override;

    mosaics::PointSettings settings() const;
    QList<mosaics::Dot> dots() const;
    NodeId apply();

    QComboBox* shapeBox() const { return m_shape; }
    QComboBox* modeBox() const { return m_mode; }
    QDoubleSpinBox* cellBox() const { return m_cell; }
    QDoubleSpinBox* scaleBox() const { return m_scale; }
    QDoubleSpinBox* angleBox() const { return m_angle; }
    QSpinBox* colorsBox() const { return m_colors; }
    QComboBox* groupingBox() const { return m_grouping; }
    QCheckBox* keepBox() const { return m_keep; }
    QLabel* countLabel() const { return m_count; }
    QLabel* previewLabel() const { return m_preview; }

private:
    void schedulePreview();
    void updatePreview();
    void saveSettings() const;

    Canvas& m_canvas;
    MosaicSource m_source;
    QPainterPath m_custom;
    QComboBox* m_shape = nullptr;
    QComboBox* m_mode = nullptr;
    QDoubleSpinBox* m_cell = nullptr;
    QDoubleSpinBox* m_scale = nullptr;
    QDoubleSpinBox* m_minSize = nullptr;
    QDoubleSpinBox* m_angle = nullptr;
    QDoubleSpinBox* m_rotation = nullptr;
    QDoubleSpinBox* m_jitter = nullptr;
    QSpinBox* m_colors = nullptr;
    QCheckBox* m_invert = nullptr;
    QPushButton* m_colorButton = nullptr;
    Color m_color{0, 0, 0};
    QComboBox* m_grouping = nullptr;
    QCheckBox* m_keep = nullptr;
    QLabel* m_count = nullptr;
    QLabel* m_preview = nullptr;
    QTimer* m_timer = nullptr;
};

} // namespace vek
