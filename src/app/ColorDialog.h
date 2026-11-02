#pragma once

#include "Color.h"

#include <QDialog>
#include <QToolButton>

#include <optional>

class QComboBox;
class QLabel;
class QLineEdit;
class QSlider;
class QSpinBox;
class QStackedWidget;

namespace vek {

class SaturationValueField;
class HueStrip;
class PaletteGrid;
class ColorPreview;

// Farbauswahl wie "Gleichmässige Füllung":
//  Reiter "Modelle"  – Farbfeld (Sättigung/Helligkeit) und Farbtonleiste, Modell RGB oder HSB
//  Reiter "Paletten" – Standardpalette mit Tönung
//  rechts            – Pipette, Vorschau alt/neu, Hex, Zahlenfelder, Name
class ColorDialog : public QDialog {
    Q_OBJECT

public:
    // Farbe eines Bildschirmpunkts (alle Bildschirme); leer, wenn das System es nicht erlaubt
    static std::optional<Color> screenColorAt(const QPoint& globalPos);
    ColorDialog(const QString& title, const Color& initial, QWidget* parent = nullptr);

    Color color() const { return m_color; }
    void setColor(const Color& c);

    static std::optional<Color> getColor(QWidget* parent, const QString& title, const Color& initial);

protected:
    void mouseMoveEvent(QMouseEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;

private:
    void updateWidgets();
    void setFromHsv(double h, double s, double v);
    void startEyedropper();
    void stopEyedropper(bool accept);

    Color m_initial;
    Color m_color;
    double m_hue = 0.0; // bleibt bei Grautönen erhalten
    bool m_updating = false;
    bool m_picking = false;
    Color m_beforePicking;

    SaturationValueField* m_field = nullptr;
    HueStrip* m_hueStrip = nullptr;
    PaletteGrid* m_grid = nullptr;
    QSlider* m_tint = nullptr;
    ColorPreview* m_preview = nullptr;
    QComboBox* m_model = nullptr;
    QLineEdit* m_hex = nullptr;
    QLabel* m_labels[3] = {};
    QSpinBox* m_spins[3] = {};
    QComboBox* m_name = nullptr;
    QToolButton* m_eyedropper = nullptr;
};

// Farbknopf: zeigt eine Farbe, öffnet beim Klick den Farbdialog
class ColorButton : public QToolButton {
    Q_OBJECT

public:
    explicit ColorButton(const QString& dialogTitle, QWidget* parent = nullptr);
    Color color() const { return m_color; }
    void setColor(const Color& c);

signals:
    void colorChanged(const vek::Color& color);

private:
    QString m_title;
    Color m_color;
};

} // namespace vek
