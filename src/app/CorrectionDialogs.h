#pragma once

#include "BitmapEffect.h"
#include "BitmapFilters.h"
#include "ColorDialog.h"
#include "ImageCorrect.h"
#include "ImageDialogs.h"

#include <array>
#include <functional>

class QAbstractButton;
class QButtonGroup;
class QCheckBox;
class QComboBox;
class QRadioButton;
class QSlider;
class QToolButton;

namespace vek {

// Bitmaps > 3D-Effekte > Zylinder
class CylinderDialog : public ImagePreviewDialog {
    Q_OBJECT

public:
    explicit CylinderDialog(const QImage& image, QWidget* parent = nullptr);
    void setValues(int percent, bool vertical);

    bool effectValues(QList<int>& values, QList<Color>& colors) const override;
    void setEffectValues(const QList<int>& values, const QList<Color>& colors) override;
protected:
    QImage process(const QImage& image) const override;

private:
    QComboBox* m_mode = nullptr;
    QSlider* m_percent = nullptr;
};

// Bitmaps > 3D-Effekte > Seite aufrollen
class PageCurlDialog : public ImagePreviewDialog {
    Q_OBJECT

public:
    explicit PageCurlDialog(const QImage& image, QWidget* parent = nullptr);
    filters::PageCurl settings() const;
    void setSettings(const filters::PageCurl& curl);
    QAbstractButton* cornerButton(int corner) const;

    bool effectValues(QList<int>& values, QList<Color>& colors) const override;
    void setEffectValues(const QList<int>& values, const QList<Color>& colors) override;
protected:
    QImage process(const QImage& image) const override;

private:
    QButtonGroup* m_corners = nullptr;
    QRadioButton* m_vertical = nullptr;
    QRadioButton* m_horizontal = nullptr;
    QRadioButton* m_opaque = nullptr;
    QRadioButton* m_transparent = nullptr;
    ColorButton* m_curlColor = nullptr;
    ColorButton* m_background = nullptr;
    QCheckBox* m_clearBackground = nullptr;
    QSlider* m_width = nullptr;
    QSlider* m_height = nullptr;
};

// Effekte > Anpassen > Proben-/Zielabgleich
class SampleTargetDialog : public ImagePreviewDialog {
    Q_OBJECT

public:
    explicit SampleTargetDialog(const QImage& image, QWidget* parent = nullptr);
    std::array<Color, 3> samples() const;
    std::array<Color, 3> targets() const;
    void setSample(int row, const Color& color);
    void setTarget(int row, const Color& color);
    // Pipette der Zeile (0 Schatten, 1 Mitteltöne, 2 Glanzlichter)
    QToolButton* pickButton(int row) const { return m_pick[std::size_t(row)]; }

    bool effectValues(QList<int>& values, QList<Color>& colors) const override;
    void setEffectValues(const QList<int>& values, const QList<Color>& colors) override;
protected:
    QImage process(const QImage& image) const override;

private:
    std::array<ColorButton*, 3> m_samples{};
    std::array<ColorButton*, 3> m_targets{};
    std::array<QToolButton*, 3> m_pick{};
};

// Effekte > Anpassen > Selektive Farbe
class SelectiveColorDialog : public ImagePreviewDialog {
    Q_OBJECT

public:
    explicit SelectiveColorDialog(const QImage& image, QWidget* parent = nullptr);
    adjust::SelectiveColor settings() const { return m_settings; }
    void setSettings(const adjust::SelectiveColor& settings);
    void setRange(int range);

    bool effectValues(QList<int>& values, QList<Color>& colors) const override;
    void setEffectValues(const QList<int>& values, const QList<Color>& colors) override;
protected:
    QImage process(const QImage& image) const override;

private:
    void showRange();

    adjust::SelectiveColor m_settings;
    QComboBox* m_range = nullptr;
    QList<QSlider*> m_sliders;
    QRadioButton* m_relative = nullptr;
    QRadioButton* m_absolute = nullptr;
    bool m_updating = false;
};

// Effekte > Anpassen > Farben ersetzen
class ReplaceColorsDialog : public ImagePreviewDialog {
    Q_OBJECT

public:
    explicit ReplaceColorsDialog(const QImage& image, QWidget* parent = nullptr);
    adjust::ReplaceColors settings() const;
    void setSettings(const adjust::ReplaceColors& settings);
    QToolButton* pickButton() const { return m_pick; }

    bool effectValues(QList<int>& values, QList<Color>& colors) const override;
    void setEffectValues(const QList<int>& values, const QList<Color>& colors) override;
protected:
    QImage process(const QImage& image) const override;

private:
    ColorButton* m_old = nullptr;
    ColorButton* m_new = nullptr;
    QToolButton* m_pick = nullptr;
    QList<QSlider*> m_sliders; // Bereich, Farbton, Sättigung, Helligkeit
};

// Effekte > Anpassen > Kanalmixer
class ChannelMixerDialog : public ImagePreviewDialog {
    Q_OBJECT

public:
    explicit ChannelMixerDialog(const QImage& image, QWidget* parent = nullptr);
    adjust::ChannelMixer settings() const { return m_mixer; }
    void setSettings(const adjust::ChannelMixer& mixer);
    void setOutputChannel(int channel);

    bool effectValues(QList<int>& values, QList<Color>& colors) const override;
    void setEffectValues(const QList<int>& values, const QList<Color>& colors) override;
protected:
    QImage process(const QImage& image) const override;

private:
    void showChannel();

    adjust::ChannelMixer m_mixer;
    QComboBox* m_channel = nullptr;
    QList<QSlider*> m_sliders; // Rot, Grün, Blau, Konstante
    bool m_updating = false;
};

} // namespace vek
