#pragma once

#include <QDialog>
#include <QImage>
#include <QList>

#include <atomic>
#include <memory>

class QAbstractButton;
class QCheckBox;
class QComboBox;
class QLabel;
class QProgressBar;
class QPushButton;
class QRadioButton;
class QSlider;
class QSpinBox;
class QTimer;
class QToolButton;

namespace vek {

class CutoutView;

// Freistellungs-Assistent (Bitmaps > Freistellungs-Assistent): Mit dem grünen Marker ein Band über
// die Kante des Motivs ziehen, das Innere füllen, Vorschau. Die Kante rechnet cutout::solveAlpha
// pixelgenau aus den Farben (auch Haare und Fell); mit den Pinseln "Detail behalten/entfernen"
// lässt sich das Ergebnis nachbessern. Alles läuft lokal.
class CutoutAssistant : public QDialog {
    Q_OBJECT

public:
    enum class Tool { Marker, Eraser, Fill, Keep, Remove, Pan };

    explicit CutoutAssistant(const QImage& image, QWidget* parent = nullptr);
    ~CutoutAssistant() override;

    // Freigestelltes Bild in voller Auflösung (rechnet nach, falls die Vorschau veraltet ist)
    QImage result();
    // Ausschnitt und Originalbild: das Original bleibt als eigenes Objekt darunter
    bool keepOriginal() const;

    // Für Tests und Videos
    CutoutView* view() const { return m_view; }
    void setTool(Tool tool);
    Tool tool() const { return m_tool; }
    void setBrushSize(int pixels);
    int brushSize() const;
    // Pinselstrich in Bildkoordinaten mit dem aktuellen Werkzeug
    void stroke(const QList<QPointF>& points);
    void fillAt(const QPoint& imagePoint);
    void undo();
    void redo();
    void reset();
    // Vorschau rechnen (im Hintergrund); wait = blockierend
    void preview(bool wait = false);
    bool working() const { return m_busy; }
    QImage alpha() const { return m_alpha; }
    QImage trimapImage() const;

signals:
    void previewReady();

private:
    friend class CutoutView;
    struct Layers {
        QImage marker, fill, keep, remove;
        // nur in Rückgängig-Schritten: Ergebnis dieses Stands, damit Rückgängig nicht neu rechnen muss
        QImage alpha, composite;
    };
    Layers snapshot() const;
    void restore(Layers state);
    void pushUndo();
    void paintSegment(const QPointF& from, const QPointF& to);
    void strokeFinished();
    void layersChanged(const QRect& imageRect);
    void updateHint(const QString& extra = {});
    void updateButtons();
    void startSolve();
    void startCompose();
    QImage refined() const;

    QImage m_image;   // Original (Farbtiefe wie geliefert)
    Layers m_layers;
    QList<Layers> m_undo, m_redo;
    int m_undoLimit = 30;
    Tool m_tool = Tool::Marker;

    QImage m_alpha;        // berechnete Deckkraft
    QImage m_alphaTrimap;  // Trimap, zu der m_alpha gehört (für das schnelle Nachrechnen eines Bereichs)
    QImage m_composite;    // Vorschau des Ergebnisses (ARGB32)
    bool m_alphaStale = true;
    bool m_busy = false;
    int m_generation = 0;
    std::shared_ptr<std::atomic_bool> m_cancel;
    QImage m_prior;        // Modell (BiRefNet), einmal je Bild

    CutoutView* m_view = nullptr;
    QList<QToolButton*> m_toolButtons;
    QSpinBox* m_size = nullptr;
    QSlider* m_sizeSlider = nullptr;
    QCheckBox* m_showMarker = nullptr;
    QCheckBox* m_showFill = nullptr;
    QCheckBox* m_showOriginal = nullptr;
    QComboBox* m_background = nullptr;
    QCheckBox* m_decontaminate = nullptr;
    QSlider* m_shift = nullptr;
    QSlider* m_feather = nullptr;
    QSlider* m_hardness = nullptr;
    QRadioButton* m_resultCutout = nullptr;
    QRadioButton* m_resultBoth = nullptr;
    QPushButton* m_previewButton = nullptr;
    QPushButton* m_ok = nullptr;
    QToolButton* m_undoButton = nullptr;
    QToolButton* m_redoButton = nullptr;
    QLabel* m_hint = nullptr;
    QLabel* m_status = nullptr;
    QProgressBar* m_progress = nullptr;
    QTimer* m_autoPreview = nullptr;
    QTimer* m_recompose = nullptr;
    QList<class QThread*> m_threads;
};

// Arbeitsfläche: Bild, Marker (grün), Füllung (blau), Detailpinsel; Zoom mit dem Mausrad,
// Verschieben mit Hand, mittlerer Maustaste oder Leertaste
class CutoutView : public QWidget {
    Q_OBJECT

public:
    explicit CutoutView(CutoutAssistant& owner);

    void fitToWindow();
    void setZoom(double zoom, const QPointF& anchorWidget = QPointF(-1, -1));
    double zoom() const { return m_zoom; }
    QPointF toImage(const QPointF& widget) const;
    QPointF toWidget(const QPointF& image) const;
    void updateOverlay(const QRect& imageRect);
    void rebuildOverlay();
    QColor markerColor = QColor(40, 220, 60);
    QColor fillColor = QColor(30, 80, 230);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    CutoutAssistant& m_owner;
    QImage m_display;  // Original als ARGB32 für die Anzeige
    QImage m_overlay;  // Ebenen eingefärbt (ARGB32_Premultiplied, Bildgrösse)
    double m_zoom = 1.0;
    QPointF m_offset;  // Bildpunkt an der linken oberen Ecke
    QPointF m_mouse{-1, -1};
    QList<QPointF> m_stroke;
    bool m_panning = false;
    bool m_space = false;
    bool m_fitted = false;
    QPoint m_panStart;
    QPointF m_panOffset;
};

} // namespace vek
