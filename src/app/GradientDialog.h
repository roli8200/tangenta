#pragma once

#include "Style.h"

#include <QDialog>
#include <QWidget>

#include <optional>

class QComboBox;
class QDoubleSpinBox;
class QRadioButton;
class QSlider;
class QStackedWidget;

namespace vek {

class ColorButton;

// Leiste mit den Farbstopps eines Verlaufs: Klick wählt, Ziehen verschiebt,
// Doppelklick fügt hinzu, Entf löscht (Anfang und Ende bleiben).
class GradientStopBar : public QWidget {
    Q_OBJECT

public:
    explicit GradientStopBar(QWidget* parent = nullptr);
    const QList<GradientStop>& stops() const { return m_stops; }
    void setStops(const QList<GradientStop>& stops);
    int current() const { return m_current; }
    void setCurrentColor(const Color& c);
    void setCurrentPosition(double position);

signals:
    void stopsChanged();
    void currentChanged(int index);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;

private:
    QRect barRect() const;
    double positionAt(double x) const;
    int stopAt(const QPoint& pos) const;
    Color colorAt(double position) const;

    QList<GradientStop> m_stops;
    int m_current = 0;
    bool m_dragging = false;
};

// Dialog "Farbverlauf"
class GradientDialog : public QDialog {
    Q_OBJECT

public:
    GradientDialog(const Fill& initial, const QSizeF& objectSize, QWidget* parent = nullptr);
    Fill fill() const { return m_fill; }

    static std::optional<Fill> getFill(QWidget* parent, const Fill& initial, const QSizeF& objectSize);

private:
    void readWidgets();
    void writeWidgets();

    Fill m_fill;
    QSizeF m_size;
    bool m_updating = false;

    QComboBox* m_type = nullptr;
    QDoubleSpinBox* m_offsetX = nullptr;
    QDoubleSpinBox* m_offsetY = nullptr;
    QDoubleSpinBox* m_angle = nullptr;
    QDoubleSpinBox* m_pad = nullptr;
    QRadioButton* m_twoColors = nullptr;
    QRadioButton* m_custom = nullptr;
    QStackedWidget* m_blendPages = nullptr;
    ColorButton* m_from = nullptr;
    ColorButton* m_to = nullptr;
    QSlider* m_mid = nullptr;
    GradientStopBar* m_bar = nullptr;
    ColorButton* m_stopColor = nullptr;
    QDoubleSpinBox* m_stopPosition = nullptr;
    QWidget* m_preview = nullptr;
};

} // namespace vek
