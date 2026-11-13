#include "CorrectionDialogs.h"
#include "ColorDialog.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QToolButton>
#include <QVBoxLayout>

#include <cmath>
#include <tuple>

namespace vek {

namespace {

QVBoxLayout* settingsLayout(QWidget* area)
{
    auto* layout = new QVBoxLayout(area);
    layout->setContentsMargins(0, 0, 0, 0);
    return layout;
}

// Zwei Optionsfelder nebeneinander als Formularzeile
std::pair<QRadioButton*, QRadioButton*> radioPair(QFormLayout* form, const QString& label, const QString& a, const QString& b,
                                                  QWidget* parent)
{
    auto* first = new QRadioButton(a, parent);
    auto* second = new QRadioButton(b, parent);
    auto* group = new QButtonGroup(parent);
    group->addButton(first);
    group->addButton(second);
    first->setChecked(true);
    auto* row = new QHBoxLayout;
    row->addWidget(first);
    row->addWidget(second);
    row->addStretch(1);
    form->addRow(label, row);
    return {first, second};
}

// Farbknopf mit Anfangsfarbe
ColorButton* colorButton(const QString& title, const Color& color, QWidget* parent)
{
    auto* button = new ColorButton(title, parent);
    button->setColor(color);
    return button;
}

QToolButton* pipetteButton(QWidget* parent)
{
    auto* button = new QToolButton(parent);
    button->setText(ImagePreviewDialog::tr("Pipette"));
    button->setToolTip(ImagePreviewDialog::tr("Farbe in der Vorschau aufnehmen"));
    button->setCheckable(true);
    return button;
}

} // namespace

// --- Zylinder ------------------------------------------------------------------------------

CylinderDialog::CylinderDialog(const QImage& image, QWidget* parent) : ImagePreviewDialog(tr("Zylinder"), image, parent)
{
    auto* layout = settingsLayout(settingsArea());
    auto* form = new QFormLayout;
    m_mode = new QComboBox(this);
    m_mode->addItems({tr("Waagrecht"), tr("Senkrecht")});
    form->addRow(tr("Modus:"), m_mode);
    m_percent = addEffectSlider(form, SliderSpec{tr("Prozent:"), -100, 100, 40}, this);
    layout->addLayout(form);
    connect(m_mode, &QComboBox::currentIndexChanged, this, &CylinderDialog::refreshPreview);
    connect(m_percent, &QSlider::valueChanged, this, &CylinderDialog::refreshPreview);
    finishLayout();
}

void CylinderDialog::setValues(int percent, bool vertical)
{
    m_mode->setCurrentIndex(vertical ? 1 : 0);
    m_percent->setValue(percent);
    refreshPreview();
}

QImage CylinderDialog::process(const QImage& image) const
{
    return filters::cylinder(image, m_percent->value(), m_mode->currentIndex() == 1);
}

// --- Seite aufrollen -----------------------------------------------------------------------

PageCurlDialog::PageCurlDialog(const QImage& image, QWidget* parent) : ImagePreviewDialog(tr("Seite aufrollen"), image, parent)
{
    auto* layout = settingsLayout(settingsArea());
    // Vier Eckknöpfe wie ein kleines Blatt angeordnet
    auto* corners = new QGridLayout;
    m_corners = new QButtonGroup(this);
    const QString arrows[4] = {QStringLiteral("◤"), QStringLiteral("◥"), QStringLiteral("◢"), QStringLiteral("◣")};
    const QString tips[4] = {tr("Ecke oben links"), tr("Ecke oben rechts"), tr("Ecke unten rechts"), tr("Ecke unten links")};
    const QPoint cells[4] = {QPoint(0, 0), QPoint(1, 0), QPoint(1, 1), QPoint(0, 1)};
    for (int i = 0; i < 4; ++i) {
        auto* b = new QToolButton(this);
        b->setText(arrows[i]);
        b->setToolTip(tips[i]);
        b->setCheckable(true);
        b->setMinimumSize(34, 30);
        m_corners->addButton(b, i);
        corners->addWidget(b, cells[i].y(), cells[i].x());
    }
    m_corners->button(filters::PageCurl::BottomRight)->setChecked(true);
    auto* cornerRow = new QHBoxLayout;
    cornerRow->addLayout(corners);
    cornerRow->addStretch(1);
    auto* form = new QFormLayout;
    form->addRow(tr("Ecke:"), cornerRow);
    std::tie(m_vertical, m_horizontal) = radioPair(form, tr("Richtung:"), tr("Senkrecht"), tr("Waagrecht"), this);
    std::tie(m_opaque, m_transparent) = radioPair(form, tr("Papier:"), tr("Undurchsichtig"), tr("Durchsichtig"), this);
    m_curlColor = colorButton(tr("Rollenfarbe"), Color(255, 255, 255), this);
    form->addRow(tr("Rollenfarbe:"), m_curlColor);
    m_background = colorButton(tr("Hintergrundfarbe"), Color(255, 255, 255), this);
    m_clearBackground = new QCheckBox(tr("Durchsichtig"), this);
    auto* bgRow = new QHBoxLayout;
    bgRow->addWidget(m_background);
    bgRow->addWidget(m_clearBackground);
    form->addRow(tr("Hintergrundfarbe:"), bgRow);
    m_width = addEffectSlider(form, SliderSpec{tr("Breite (%):"), 1, 100, 50}, this);
    m_height = addEffectSlider(form, SliderSpec{tr("Höhe (%):"), 1, 100, 50}, this);
    layout->addLayout(form);

    auto refresh = [this] { refreshPreview(); };
    connect(m_corners, &QButtonGroup::idClicked, this, refresh);
    for (QRadioButton* r : {m_vertical, m_horizontal, m_opaque, m_transparent})
        connect(r, &QRadioButton::toggled, this, [this](bool on) {
            if (on)
                refreshPreview();
        });
    connect(m_curlColor, &ColorButton::colorChanged, this, refresh);
    connect(m_background, &ColorButton::colorChanged, this, refresh);
    connect(m_clearBackground, &QCheckBox::toggled, this, [this](bool on) {
        m_background->setEnabled(!on);
        refreshPreview();
    });
    connect(m_width, &QSlider::valueChanged, this, refresh);
    connect(m_height, &QSlider::valueChanged, this, refresh);
    finishLayout();
}

QAbstractButton* PageCurlDialog::cornerButton(int corner) const
{
    return m_corners->button(corner);
}

filters::PageCurl PageCurlDialog::settings() const
{
    filters::PageCurl c;
    c.corner = filters::PageCurl::Corner(std::max(0, m_corners->checkedId()));
    c.vertical = m_vertical->isChecked();
    c.opaque = m_opaque->isChecked();
    c.curlColor = m_curlColor->color();
    c.background = m_background->color();
    c.transparentBackground = m_clearBackground->isChecked();
    c.width = m_width->value();
    c.height = m_height->value();
    return c;
}

void PageCurlDialog::setSettings(const filters::PageCurl& c)
{
    m_corners->button(c.corner)->setChecked(true);
    (c.vertical ? m_vertical : m_horizontal)->setChecked(true);
    (c.opaque ? m_opaque : m_transparent)->setChecked(true);
    m_curlColor->setColor(c.curlColor);
    m_background->setColor(c.background);
    m_clearBackground->setChecked(c.transparentBackground);
    m_width->setValue(c.width);
    m_height->setValue(c.height);
    refreshPreview();
}

QImage PageCurlDialog::process(const QImage& image) const
{
    return filters::pageCurl(image, settings());
}

// --- Proben-/Zielabgleich ------------------------------------------------------------------

SampleTargetDialog::SampleTargetDialog(const QImage& image, QWidget* parent)
    : ImagePreviewDialog(tr("Proben-/Zielabgleich"), image, parent)
{
    auto* layout = settingsLayout(settingsArea());
    auto* grid = new QGridLayout;
    grid->addWidget(new QLabel(tr("Probe"), this), 0, 1, 1, 2);
    grid->addWidget(new QLabel(tr("Ziel"), this), 0, 3);
    const QString names[3] = {tr("Schatten:"), tr("Mitteltöne:"), tr("Glanzlichter:")};
    const Color defaults[3] = {Color(0, 0, 0), Color(128, 128, 128), Color(255, 255, 255)};
    for (int row = 0; row < 3; ++row) {
        grid->addWidget(new QLabel(names[row], this), row + 1, 0);
        m_samples[std::size_t(row)] = colorButton(tr("Probe"), defaults[row], this);
        m_pick[std::size_t(row)] = pipetteButton(this);
        m_targets[std::size_t(row)] = colorButton(tr("Zielfarbe"), defaults[row], this);
        grid->addWidget(m_samples[std::size_t(row)], row + 1, 1);
        grid->addWidget(m_pick[std::size_t(row)], row + 1, 2);
        grid->addWidget(m_targets[std::size_t(row)], row + 1, 3);
        connect(m_samples[std::size_t(row)], &ColorButton::colorChanged, this, [this] { refreshPreview(); });
        connect(m_targets[std::size_t(row)], &ColorButton::colorChanged, this, [this] { refreshPreview(); });
        connect(m_pick[std::size_t(row)], &QToolButton::toggled, this, [this, row](bool on) {
            if (!on)
                return;
            for (int other = 0; other < 3; ++other)
                if (other != row)
                    m_pick[std::size_t(other)]->setChecked(false);
            beginPick([this, row](const Color& c) {
                m_pick[std::size_t(row)]->setChecked(false);
                setSample(row, c);
                // Mitteltöne: Ziel ist das neutrale Grau gleicher Helligkeit (Farbstich entfernen)
                if (row == 1) {
                    const auto gray = quint8(std::lround(0.299 * c.r + 0.587 * c.g + 0.114 * c.b));
                    setTarget(row, Color(gray, gray, gray));
                }
            });
        });
    }
    layout->addLayout(grid);
    auto* hint = new QLabel(tr("Pipette wählen und in die Vorschau klicken, um eine Probe aufzunehmen."), this);
    hint->setWordWrap(true);
    layout->addWidget(hint);
    auto* reset = new QPushButton(tr("Zurücksetzen"), this);
    layout->addWidget(reset);
    connect(reset, &QPushButton::clicked, this, [this, defaults] {
        for (int row = 0; row < 3; ++row) {
            m_samples[std::size_t(row)]->setColor(defaults[row]);
            m_targets[std::size_t(row)]->setColor(defaults[row]);
        }
        refreshPreview();
    });
    finishLayout();
}

std::array<Color, 3> SampleTargetDialog::samples() const
{
    return {m_samples[0]->color(), m_samples[1]->color(), m_samples[2]->color()};
}

std::array<Color, 3> SampleTargetDialog::targets() const
{
    return {m_targets[0]->color(), m_targets[1]->color(), m_targets[2]->color()};
}

void SampleTargetDialog::setSample(int row, const Color& color)
{
    m_samples[std::size_t(row)]->setColor(color);
    refreshPreview();
}

void SampleTargetDialog::setTarget(int row, const Color& color)
{
    m_targets[std::size_t(row)]->setColor(color);
    refreshPreview();
}

QImage SampleTargetDialog::process(const QImage& image) const
{
    return adjust::sampleTargetBalance(image, samples(), targets());
}

// --- Selektive Farbe -----------------------------------------------------------------------

SelectiveColorDialog::SelectiveColorDialog(const QImage& image, QWidget* parent)
    : ImagePreviewDialog(tr("Selektive Farbe"), image, parent)
{
    auto* layout = settingsLayout(settingsArea());
    auto* form = new QFormLayout;
    m_range = new QComboBox(this);
    m_range->addItems({tr("Rottöne"), tr("Gelbtöne"), tr("Grüntöne"), tr("Cyantöne"), tr("Blautöne"), tr("Magentatöne"),
                       tr("Weisstöne"), tr("Neutraltöne"), tr("Schwarztöne")});
    form->addRow(tr("Farbbereich:"), m_range);
    for (const QString& name : {tr("Cyan (%):"), tr("Magenta (%):"), tr("Gelb (%):"), tr("Schwarz (%):")})
        m_sliders << addEffectSlider(form, SliderSpec{name}, this);
    std::tie(m_relative, m_absolute) = radioPair(form, tr("Methode:"), tr("Relativ"), tr("Absolut"), this);
    layout->addLayout(form);
    auto* reset = new QPushButton(tr("Zurücksetzen"), this);
    layout->addWidget(reset);
    for (int i = 0; i < m_sliders.size(); ++i)
        connect(m_sliders[i], &QSlider::valueChanged, this, [this, i](int v) {
            if (m_updating)
                return;
            m_settings.cmyk[std::size_t(m_range->currentIndex())][std::size_t(i)] = v;
            refreshPreview();
        });
    connect(m_range, &QComboBox::currentIndexChanged, this, [this] { showRange(); });
    connect(m_relative, &QRadioButton::toggled, this, [this](bool on) {
        m_settings.relative = on;
        refreshPreview();
    });
    connect(reset, &QPushButton::clicked, this, [this] { setSettings(adjust::SelectiveColor()); });
    finishLayout();
}

void SelectiveColorDialog::showRange()
{
    m_updating = true;
    const auto& values = m_settings.cmyk[std::size_t(m_range->currentIndex())];
    for (int i = 0; i < m_sliders.size(); ++i)
        m_sliders[i]->setValue(values[std::size_t(i)]);
    m_updating = false;
}

void SelectiveColorDialog::setRange(int range)
{
    m_range->setCurrentIndex(range);
}

void SelectiveColorDialog::setSettings(const adjust::SelectiveColor& settings)
{
    m_settings = settings;
    (settings.relative ? m_relative : m_absolute)->setChecked(true);
    showRange();
    refreshPreview();
}

QImage SelectiveColorDialog::process(const QImage& image) const
{
    return adjust::selectiveColor(image, m_settings);
}

// --- Farben ersetzen -----------------------------------------------------------------------

ReplaceColorsDialog::ReplaceColorsDialog(const QImage& image, QWidget* parent)
    : ImagePreviewDialog(tr("Farben ersetzen"), image, parent)
{
    const adjust::ReplaceColors defaults;
    auto* layout = settingsLayout(settingsArea());
    auto* form = new QFormLayout;
    m_old = colorButton(tr("Alte Farbe"), defaults.oldColor, this);
    m_pick = pipetteButton(this);
    auto* oldRow = new QHBoxLayout;
    oldRow->addWidget(m_old);
    oldRow->addWidget(m_pick);
    form->addRow(tr("Alte Farbe:"), oldRow);
    m_new = colorButton(tr("Neue Farbe"), defaults.newColor, this);
    form->addRow(tr("Neue Farbe:"), m_new);
    m_sliders << addEffectSlider(form, SliderSpec{tr("Bereich:"), 0, 100, defaults.range}, this);
    m_sliders << addEffectSlider(form, SliderSpec{tr("Farbton:"), -180, 180, 0}, this);
    m_sliders << addEffectSlider(form, SliderSpec{tr("Sättigung:")}, this);
    m_sliders << addEffectSlider(form, SliderSpec{tr("Helligkeit:")}, this);
    layout->addLayout(form);
    auto* reset = new QPushButton(tr("Zurücksetzen"), this);
    layout->addWidget(reset);
    connect(m_old, &ColorButton::colorChanged, this, [this] { refreshPreview(); });
    connect(m_new, &ColorButton::colorChanged, this, [this] { refreshPreview(); });
    for (QSlider* s : std::as_const(m_sliders))
        connect(s, &QSlider::valueChanged, this, &ReplaceColorsDialog::refreshPreview);
    connect(m_pick, &QToolButton::toggled, this, [this](bool on) {
        if (on)
            beginPick([this](const Color& c) {
                m_pick->setChecked(false);
                m_old->setColor(c);
                refreshPreview();
            });
    });
    connect(reset, &QPushButton::clicked, this, [this] { setSettings(adjust::ReplaceColors()); });
    finishLayout();
}

adjust::ReplaceColors ReplaceColorsDialog::settings() const
{
    adjust::ReplaceColors s;
    s.oldColor = m_old->color();
    s.newColor = m_new->color();
    s.range = m_sliders[0]->value();
    s.hue = m_sliders[1]->value();
    s.saturation = m_sliders[2]->value();
    s.lightness = m_sliders[3]->value();
    return s;
}

void ReplaceColorsDialog::setSettings(const adjust::ReplaceColors& s)
{
    m_old->setColor(s.oldColor);
    m_new->setColor(s.newColor);
    const int values[4] = {s.range, s.hue, s.saturation, s.lightness};
    for (int i = 0; i < 4; ++i)
        m_sliders[i]->setValue(values[i]);
    refreshPreview();
}

QImage ReplaceColorsDialog::process(const QImage& image) const
{
    return adjust::replaceColors(image, settings());
}

// --- Kanalmixer ----------------------------------------------------------------------------

ChannelMixerDialog::ChannelMixerDialog(const QImage& image, QWidget* parent) : ImagePreviewDialog(tr("Kanalmixer"), image, parent)
{
    auto* layout = settingsLayout(settingsArea());
    auto* form = new QFormLayout;
    m_channel = new QComboBox(this);
    m_channel->addItems({tr("Rot"), tr("Grün"), tr("Blau")});
    form->addRow(tr("Ausgabekanal:"), m_channel);
    m_sliders << addEffectSlider(form, SliderSpec{tr("Rot (%):"), -200, 200, 100}, this);
    m_sliders << addEffectSlider(form, SliderSpec{tr("Grün (%):"), -200, 200, 0}, this);
    m_sliders << addEffectSlider(form, SliderSpec{tr("Blau (%):"), -200, 200, 0}, this);
    m_sliders << addEffectSlider(form, SliderSpec{tr("Konstante (%):"), -100, 100, 0}, this);
    layout->addLayout(form);
    auto* reset = new QPushButton(tr("Zurücksetzen"), this);
    layout->addWidget(reset);
    for (int i = 0; i < m_sliders.size(); ++i)
        connect(m_sliders[i], &QSlider::valueChanged, this, [this, i](int v) {
            if (m_updating)
                return;
            m_mixer.rows[std::size_t(m_channel->currentIndex())][std::size_t(i)] = v;
            refreshPreview();
        });
    connect(m_channel, &QComboBox::currentIndexChanged, this, [this] { showChannel(); });
    connect(reset, &QPushButton::clicked, this, [this] { setSettings(adjust::ChannelMixer()); });
    finishLayout();
}

void ChannelMixerDialog::showChannel()
{
    m_updating = true;
    const auto& row = m_mixer.rows[std::size_t(m_channel->currentIndex())];
    for (int i = 0; i < m_sliders.size(); ++i)
        m_sliders[i]->setValue(row[std::size_t(i)]);
    m_updating = false;
}

void ChannelMixerDialog::setOutputChannel(int channel)
{
    m_channel->setCurrentIndex(channel);
}

void ChannelMixerDialog::setSettings(const adjust::ChannelMixer& mixer)
{
    m_mixer = mixer;
    showChannel();
    refreshPreview();
}

QImage ChannelMixerDialog::process(const QImage& image) const
{
    return adjust::channelMixer(image, m_mixer);
}

// --- Effektwerte (Effektstapel, Andockfenster) -------------------------------------------

bool CylinderDialog::effectValues(QList<int>& values, QList<Color>&) const
{
    values = {m_percent->value(), m_mode->currentIndex()};
    return true;
}

void CylinderDialog::setEffectValues(const QList<int>& values, const QList<Color>&)
{
    setValues(values.value(0, 40), values.value(1, 0) == 1);
}

bool PageCurlDialog::effectValues(QList<int>& values, QList<Color>& colors) const
{
    const filters::PageCurl c = settings();
    values = fx::pageCurlValues(c);
    colors = {c.curlColor, c.background};
    return true;
}

void PageCurlDialog::setEffectValues(const QList<int>& values, const QList<Color>& colors)
{
    setSettings(fx::pageCurlSettings(values, colors));
}

bool SampleTargetDialog::effectValues(QList<int>& values, QList<Color>& colors) const
{
    values.clear();
    const auto s = samples(), t = targets();
    colors = {s[0], s[1], s[2], t[0], t[1], t[2]};
    return true;
}

void SampleTargetDialog::setEffectValues(const QList<int>&, const QList<Color>& colors)
{
    for (int row = 0; row < 3 && row + 3 < colors.size(); ++row) {
        m_samples[std::size_t(row)]->setColor(colors[row]);
        m_targets[std::size_t(row)]->setColor(colors[row + 3]);
    }
    refreshPreview();
}

bool SelectiveColorDialog::effectValues(QList<int>& values, QList<Color>& colors) const
{
    values = fx::selectiveColorValues(m_settings);
    colors.clear();
    return true;
}

void SelectiveColorDialog::setEffectValues(const QList<int>& values, const QList<Color>&)
{
    setSettings(fx::selectiveColorSettings(values));
}

bool ReplaceColorsDialog::effectValues(QList<int>& values, QList<Color>& colors) const
{
    const adjust::ReplaceColors s = settings();
    values = {s.range, s.hue, s.saturation, s.lightness};
    colors = {s.oldColor, s.newColor};
    return true;
}

void ReplaceColorsDialog::setEffectValues(const QList<int>& values, const QList<Color>& colors)
{
    setSettings(fx::replaceColorsSettings(values, colors));
}

bool ChannelMixerDialog::effectValues(QList<int>& values, QList<Color>& colors) const
{
    values = fx::channelMixerValues(m_mixer);
    colors.clear();
    return true;
}

void ChannelMixerDialog::setEffectValues(const QList<int>& values, const QList<Color>&)
{
    setSettings(fx::channelMixerSettings(values));
}

} // namespace vek
