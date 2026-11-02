#pragma once

#include "ImageAdjust.h"

#include <QDialog>
#include <QImage>
#include <QWidget>

#include <functional>

class QComboBox;
class QLabel;
class QSlider;
class QSpinBox;

namespace vek {

// Histogramm der drei Farbkanäle
class HistogramWidget : public QWidget {
public:
    explicit HistogramWidget(QWidget* parent = nullptr);
    void setImage(const QImage& image);
    QSize sizeHint() const override { return QSize(220, 70); }

protected:
    void paintEvent(QPaintEvent*) override;

private:
    std::array<std::array<int, 256>, 3> m_counts{};
};

// Gemeinsame Grundlage: Vorschau links (verkleinertes Bild, live), Einstellungen rechts
class ImagePreviewDialog : public QDialog {
    Q_OBJECT

public:
    ImagePreviewDialog(const QString& title, const QImage& image, QWidget* parent = nullptr);
    // Ergebnis in voller Auflösung
    QImage result() const { return process(m_original); }

protected:
    // Bild mit den aktuellen Einstellungen bearbeiten
    virtual QImage process(const QImage& image) const = 0;
    void refreshPreview();
    QWidget* settingsArea() const { return m_settings; }
    HistogramWidget* histogram() const { return m_histogram; }
    void finishLayout();

    QImage m_original;
    QImage m_small;

public:
    // Vorschau: 0 = nur Ergebnis, 1 = vorher/nachher (links Original, rechts Ergebnis)
    int previewMode() const;
    void setPreviewMode(int mode);
    QImage previewImage() const;

private:
    QComboBox* m_mode = nullptr;
    QImage m_shown;
    QLabel* m_preview = nullptr;
    QWidget* m_settings = nullptr;
    HistogramWidget* m_histogram = nullptr;
};

// Regler mit Zahlenfeld
struct SliderSpec {
    QString label;
    int minimum = -100;
    int maximum = 100;
    int value = 0;
};

// "Bildanpassungseditor": Automatische Anpassung, Temperatur, Tönung,
// Sättigung, Helligkeit, Kontrast, Glanzlichter, Schatten, Mitteltöne, Histogramm
class ImageAdjustDialog : public ImagePreviewDialog {
    Q_OBJECT

public:
    explicit ImageAdjustDialog(const QImage& image, QWidget* parent = nullptr);
    adjust::Basic basic() const;
    void setBasic(const adjust::Basic& basic);

protected:
    QImage process(const QImage& image) const override;

private:
    QList<QSlider*> m_sliders;
    bool m_auto = false;
};

// Effekt mit einigen Reglern (Helligkeit/Kontrast/Intensität, Gamma, Farbton/Sättigung/
// Helligkeit, Farbbalance, Abstufen)
class EffectDialog : public ImagePreviewDialog {
    Q_OBJECT

public:
    using Function = std::function<QImage(const QImage&, const QList<int>&)>;
    EffectDialog(const QString& title, const QImage& image, const QList<SliderSpec>& sliders, Function fn,
                 QWidget* parent = nullptr);
    QList<int> values() const;
    void setValues(const QList<int>& values);

protected:
    QImage process(const QImage& image) const override;

private:
    QList<QSlider*> m_sliders;
    Function m_fn;
};

// Kurve mit ziehbaren Punkten: Klick fügt hinzu, aus dem Feld ziehen entfernt
class CurveWidget : public QWidget {
    Q_OBJECT

public:
    explicit CurveWidget(QWidget* parent = nullptr);
    QList<QPointF> points() const { return m_points; }
    void setPoints(const QList<QPointF>& points);
    QSize sizeHint() const override { return QSize(260, 260); }

signals:
    void changed();

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;

private:
    QRectF plot() const;
    QPointF toValue(const QPointF& pos) const;
    QPointF toPos(const QPointF& value) const;

    QList<QPointF> m_points; // 0..255
    int m_dragged = -1;
};

// "Tonkurve": je Kanal (RGB, Rot, Grün, Blau) eine Kurve
class ToneCurveDialog : public ImagePreviewDialog {
    Q_OBJECT

public:
    explicit ToneCurveDialog(const QImage& image, QWidget* parent = nullptr);
    CurveWidget* curve() const { return m_curve; }

protected:
    QImage process(const QImage& image) const override;

private:
    CurveWidget* m_curve = nullptr;
    QComboBox* m_channel = nullptr;
    std::array<QList<QPointF>, 4> m_curves; // RGB, R, G, B
};

} // namespace vek
