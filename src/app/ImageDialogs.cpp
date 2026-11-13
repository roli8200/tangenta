#include "ImageDialogs.h"
#include "ColorDialog.h"
#include "CorrectionDialogs.h"
#include <QSettings>
#include "Options.h"
#include "BitmapFrame.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QCoreApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace vek {

namespace {

constexpr QSize kPreview(520, 380);

// Regler und Zahlenfeld nebeneinander, beide gekoppelt
QSlider* addSlider(QFormLayout* form, const SliderSpec& spec, QWidget* parent)
{
    auto* slider = new QSlider(Qt::Horizontal, parent);
    slider->setRange(spec.minimum, spec.maximum);
    slider->setValue(spec.value);
    slider->setMinimumWidth(170);
    auto* spin = new QSpinBox(parent);
    spin->setRange(spec.minimum, spec.maximum);
    spin->setValue(spec.value);
    QObject::connect(slider, &QSlider::valueChanged, spin, &QSpinBox::setValue);
    QObject::connect(spin, &QSpinBox::valueChanged, slider, &QSlider::setValue);
    auto* row = new QHBoxLayout;
    row->addWidget(slider, 1);
    row->addWidget(spin);
    form->addRow(spec.label, row);
    return slider;
}

} // namespace

QSlider* addEffectSlider(QFormLayout* form, const SliderSpec& spec, QWidget* parent)
{
    return addSlider(form, spec, parent);
}

// --- Histogramm ----------------------------------------------------------------------------

HistogramWidget::HistogramWidget(QWidget* parent) : QWidget(parent)
{
    setMinimumSize(200, 64);
}

void HistogramWidget::setImage(const QImage& image)
{
    m_counts = adjust::histogram(image);
    update();
}

void HistogramWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(0x50, 0x50, 0x50));
    int peak = 1;
    for (const auto& channel : m_counts)
        for (int i = 1; i < 255; ++i) // die Ränder (reines Schwarz/Weiss) bestimmen den Massstab nicht
            peak = std::max(peak, channel[std::size_t(i)]);
    const QColor colors[3] = {QColor(255, 60, 60, 160), QColor(60, 230, 60, 160), QColor(70, 110, 255, 160)};
    p.setRenderHint(QPainter::Antialiasing);
    p.setCompositionMode(QPainter::CompositionMode_Plus);
    for (int c = 0; c < 3; ++c) {
        QPainterPath path(QPointF(0, height()));
        for (int i = 0; i < 256; ++i) {
            const double h = std::min(1.0, double(m_counts[std::size_t(c)][std::size_t(i)]) / peak) * (height() - 2);
            path.lineTo(i * (width() - 1) / 255.0, height() - h);
        }
        path.lineTo(width(), height());
        p.fillPath(path, colors[c]);
    }
}

// --- Grundlage mit Vorschau ----------------------------------------------------------------

ImagePreviewDialog::ImagePreviewDialog(const QString& title, const QImage& image, QWidget* parent)
    : QDialog(parent), m_original(image.convertToFormat(QImage::Format_RGBA64))
{
    setWindowTitle(title);
    // Vorschau auf einer verkleinerten Kopie, damit die Regler flüssig bleiben
    m_small = m_original.width() > kPreview.width() || m_original.height() > kPreview.height()
                  ? m_original.scaled(kPreview, Qt::KeepAspectRatio, Qt::SmoothTransformation)
                  : m_original;
    m_preview = new QLabel(this);
    m_preview->setFixedSize(kPreview + QSize(8, 8));
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setFrameShape(QFrame::StyledPanel);
    m_preview->installEventFilter(this);
    // Vorschau gebündelt kurz nach der letzten Änderung rechnen: ein Klick auf OK direkt nach einer
    // Eingabe wird so nicht von der Rechnung aufgehalten (und geht nicht verloren)
    m_previewTimer = new QTimer(this);
    m_previewTimer->setSingleShot(true);
    m_previewTimer->setInterval(60);
    connect(m_previewTimer, &QTimer::timeout, this, &ImagePreviewDialog::renderPreview);
    m_settings = new QWidget(this);
    m_histogram = new HistogramWidget(this);
}

void ImagePreviewDialog::finishLayout()
{
    // Optionen > Global > Bitmap Effects: Vorschaumethode beim Öffnen
    m_mode = new QComboBox(this);
    m_mode->addItems({tr("Nur Ergebnis"), tr("Vorher/Nachher")});
    QSettings settings;
    const Options& o = Options::instance();
    switch (o.integer(QStringLiteral("global/bitmapfx/preview"))) {
    case 1: m_mode->setCurrentIndex(1); break;
    case 3: m_mode->setCurrentIndex(std::clamp(settings.value(QStringLiteral("bitmapfx/previewMode")).toInt(), 0, 1)); break;
    default: m_mode->setCurrentIndex(0); break;
    }
    connect(m_mode, &QComboBox::currentIndexChanged, this, [this](int i) {
        QSettings().setValue(QStringLiteral("bitmapfx/previewMode"), i);
        refreshPreview();
    });
    // Zuletzt verwendete Werte je Dialog
    const QList<QSlider*> sliders = m_settings->findChildren<QSlider*>();
    const QString key = QStringLiteral("bitmapfx/values/") + windowTitle();
    if (o.flag(QStringLiteral("global/bitmapfx/lastValues"))) {
        const QVariantList last = settings.value(key).toList();
        for (int i = 0; i < sliders.size() && i < last.size(); ++i)
            sliders[i]->setValue(last[i].toInt());
    }
    connect(this, &QDialog::accepted, this, [sliders, key] {
        QVariantList values;
        for (const QSlider* s : sliders)
            values << s->value();
        QSettings().setValue(key, values);
    });

    auto* left = new QVBoxLayout;
    left->addWidget(m_preview);
    left->addWidget(m_mode);
    auto* right = new QVBoxLayout;
    right->addWidget(m_settings);
    right->addWidget(m_histogram);
    right->addStretch(1);
    auto* columns = new QHBoxLayout;
    columns->addLayout(left);
    columns->addLayout(right);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Abbrechen"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    auto* layout = new QVBoxLayout(this);
    layout->addLayout(columns);
    layout->addWidget(buttons);
    refreshPreview();
}

int ImagePreviewDialog::previewMode() const
{
    return m_mode ? m_mode->currentIndex() : 0;
}

void ImagePreviewDialog::setPreviewMode(int mode)
{
    if (m_mode)
        m_mode->setCurrentIndex(std::clamp(mode, 0, 1));
}

QImage ImagePreviewDialog::previewImage() const
{
    // Ausstehende Vorschau zuerst rechnen
    if (m_previewTimer->isActive())
        const_cast<ImagePreviewDialog*>(this)->renderPreview();
    return m_shown;
}

void ImagePreviewDialog::beginPick(std::function<void(const Color&)> done)
{
    m_pick = std::move(done);
    m_preview->setCursor(Qt::CrossCursor);
}

bool ImagePreviewDialog::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_preview && m_pick && event->type() == QEvent::MouseButtonPress) {
        if (m_previewTimer->isActive())
            renderPreview();
        const auto* e = static_cast<QMouseEvent*>(event);
        // Vorschau liegt mittig im Feld; gemessen wird immer im Original (auch bei Vorher/Nachher)
        const QRect area = m_preview->contentsRect();
        const QPoint offset = area.topLeft() + QPoint((area.width() - m_shown.width()) / 2, (area.height() - m_shown.height()) / 2);
        const QPoint at = e->position().toPoint() - offset;
        if (e->button() != Qt::LeftButton || !QRect(QPoint(0, 0), m_small.size()).contains(at))
            return true;
        const QColor c = m_small.pixelColor(at);
        auto done = std::move(m_pick);
        m_pick = nullptr;
        m_preview->unsetCursor();
        done(Color(quint8(c.red()), quint8(c.green()), quint8(c.blue())));
        return true;
    }
    return QDialog::eventFilter(watched, event);
}

void ImagePreviewDialog::refreshPreview()
{
    m_previewTimer->start();
}

void ImagePreviewDialog::renderPreview()
{
    m_previewTimer->stop();
    const QImage result = process(m_small);
    m_histogram->setImage(result);
    m_shown = result;
    if (previewMode() == 1) {
        // Vorher/Nachher: links das Original, rechts das Ergebnis, dazwischen eine Linie
        QImage split = m_small.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        QPainter p(&split);
        const int half = split.width() / 2;
        p.drawImage(QRect(half, 0, split.width() - half, split.height()), result,
                    QRect(half, 0, split.width() - half, split.height()));
        p.setPen(QPen(Qt::white, 1));
        p.drawLine(half, 0, half, split.height());
        p.end();
        m_shown = split;
    }
    m_preview->setPixmap(QPixmap::fromImage(m_shown));
}

// --- Bildanpassungseditor -----------------------------------------------------------------

ImageAdjustDialog::ImageAdjustDialog(const QImage& image, QWidget* parent)
    : ImagePreviewDialog(tr("Bildanpassungseditor"), image, parent)
{
    auto* layout = new QVBoxLayout(settingsArea());
    layout->setContentsMargins(0, 0, 0, 0);
    auto* autoButton = new QPushButton(tr("Automatische Anpassung"), this);
    autoButton->setCheckable(true);
    m_autoButton = autoButton;
    layout->addWidget(autoButton);
    auto* form = new QFormLayout;
    for (const QString& name : {tr("Temperatur:"), tr("Tönung:"), tr("Sättigung:"), tr("Helligkeit:"), tr("Kontrast:"),
                                tr("Glanzlichter:"), tr("Schatten:"), tr("Mitteltöne:")})
        m_sliders << addSlider(form, SliderSpec{name}, this);
    layout->addLayout(form);
    auto* reset = new QPushButton(tr("Auf Original zurücksetzen"), this);
    layout->addWidget(reset);
    for (QSlider* s : std::as_const(m_sliders))
        connect(s, &QSlider::valueChanged, this, &ImageAdjustDialog::refreshPreview);
    connect(autoButton, &QPushButton::toggled, this, [this](bool on) {
        m_auto = on;
        refreshPreview();
    });
    connect(reset, &QPushButton::clicked, this, [this, autoButton] {
        autoButton->setChecked(false);
        setBasic(adjust::Basic());
    });
    finishLayout();
}

adjust::Basic ImageAdjustDialog::basic() const
{
    adjust::Basic b;
    int* fields[] = {&b.temperature, &b.tint, &b.saturation, &b.brightness, &b.contrast, &b.highlights, &b.shadows, &b.midtones};
    for (int i = 0; i < m_sliders.size(); ++i)
        *fields[i] = m_sliders[i]->value();
    return b;
}

void ImageAdjustDialog::setBasic(const adjust::Basic& b)
{
    const int values[] = {b.temperature, b.tint, b.saturation, b.brightness, b.contrast, b.highlights, b.shadows, b.midtones};
    // Ohne Signalsperre, damit die Zahlenfelder mitlaufen; jede Änderung frischt die Vorschau auf
    for (int i = 0; i < m_sliders.size(); ++i)
        m_sliders[i]->setValue(values[i]);
    refreshPreview();
}

void ImageAdjustDialog::setAutoAdjust(bool on)
{
    m_autoButton->setChecked(on);
}

QImage ImageAdjustDialog::process(const QImage& image) const
{
    return adjust::apply(m_auto ? adjust::autoAdjust(image) : image, basic());
}

// --- Effekt mit Reglern --------------------------------------------------------------------

EffectDialog::EffectDialog(const QString& title, const QImage& image, const QList<SliderSpec>& sliders, Function fn,
                           QWidget* parent)
    : ImagePreviewDialog(title, image, parent),
      m_fn([fn = std::move(fn)](const QImage& i, const QList<int>& v, const QList<Color>&) { return fn(i, v); })
{
    build({}, sliders);
}

EffectDialog::EffectDialog(const QString& title, const QImage& image, const QList<ColorSpec>& colors,
                           const QList<SliderSpec>& sliders, ColorFunction fn, QWidget* parent)
    : ImagePreviewDialog(title, image, parent), m_fn(std::move(fn))
{
    build(colors, sliders);
}

void EffectDialog::build(const QList<ColorSpec>& colors, const QList<SliderSpec>& sliders)
{
    auto* layout = new QVBoxLayout(settingsArea());
    layout->setContentsMargins(0, 0, 0, 0);
    auto* form = new QFormLayout;
    for (const ColorSpec& spec : colors) {
        const int index = int(m_colors.size());
        m_colors << spec.value;
        auto* button = new QPushButton(this);
        button->setMinimumWidth(120);
        m_colorButtons << button;
        form->addRow(spec.label, button);
        showColor(index);
        connect(button, &QPushButton::clicked, this, [this, index, label = spec.label] {
            if (const std::optional<Color> c = ColorDialog::getColor(this, label, m_colors[index])) {
                m_colors[index] = *c;
                showColor(index);
                refreshPreview();
            }
        });
    }
    for (const SliderSpec& spec : sliders)
        m_sliders << addSlider(form, spec, this);
    layout->addLayout(form);
    auto* reset = new QPushButton(tr("Zurücksetzen"), this);
    layout->addWidget(reset);
    for (QSlider* s : std::as_const(m_sliders))
        connect(s, &QSlider::valueChanged, this, &EffectDialog::refreshPreview);
    connect(reset, &QPushButton::clicked, this, [this, sliders, colors] {
        QList<Color> defaultColors;
        for (const ColorSpec& spec : colors)
            defaultColors << spec.value;
        setColors(defaultColors);
        QList<int> defaults;
        for (const SliderSpec& spec : sliders)
            defaults << spec.value;
        setValues(defaults);
    });
    finishLayout();
}

void EffectDialog::showColor(int index)
{
    const Color& c = m_colors[index];
    QPushButton* button = m_colorButtons[index];
    button->setText(c.toHex());
    // Schrift hell auf dunklem Grund
    const bool dark = 0.299 * c.r + 0.587 * c.g + 0.114 * c.b < 128;
    button->setStyleSheet(QStringLiteral("background:%1; color:%2").arg(c.toQColor().name(), dark ? QStringLiteral("white") : QStringLiteral("black")));
}

void EffectDialog::setColors(const QList<Color>& colors)
{
    for (int i = 0; i < m_colors.size() && i < colors.size(); ++i) {
        m_colors[i] = colors[i];
        showColor(i);
    }
    refreshPreview();
}

QList<int> EffectDialog::values() const
{
    QList<int> v;
    for (const QSlider* s : m_sliders)
        v << s->value();
    return v;
}

void EffectDialog::setValues(const QList<int>& values)
{
    for (int i = 0; i < m_sliders.size() && i < values.size(); ++i)
        m_sliders[i]->setValue(values[i]);
    refreshPreview();
}

QImage EffectDialog::process(const QImage& image) const
{
    return m_fn(image, values(), m_colors);
}

// --- Rahmen --------------------------------------------------------------------------------

FrameDialog::FrameDialog(const QString& title, const QImage& image, const QList<ColorSpec>& colors, const QList<SliderSpec>& sliders,
                         ColorFunction fn, QWidget* parent)
    : EffectDialog(title, image, colors, sliders, std::move(fn), parent)
{
    const QList<QSlider*> all = settingsArea()->findChildren<QSlider*>();
    auto* layout = qobject_cast<QVBoxLayout*>(settingsArea()->layout());
    auto* form = layout ? qobject_cast<QFormLayout*>(layout->itemAt(0)->layout()) : nullptr;
    if (all.size() < 7 || !form)
        return;
    QSlider* shape = all[0];
    QSlider* border = all[6];
    // Regler für Vorlage und Modus verstecken, dafür Bilderliste und Auswahl
    for (int row = 0; row < form->rowCount(); ++row) {
        QLayoutItem* item = form->itemAt(row, QFormLayout::FieldRole);
        if (item && item->layout() && (item->layout()->indexOf(shape) >= 0 || item->layout()->indexOf(border) >= 0))
            form->setRowVisible(row, false);
    }
    m_templates = new QListWidget(this);
    m_templates->setObjectName(QStringLiteral("frameTemplates"));
    m_templates->setViewMode(QListView::IconMode);
    m_templates->setIconSize(QSize(64, 48));
    m_templates->setMovement(QListView::Static);
    m_templates->setResizeMode(QListView::Adjust);
    m_templates->setGridSize(QSize(96, 78));
    m_templates->setFixedHeight(2 * 78 + 8);
    for (int i = 0; i < filters::frameShapeCount(); ++i)
        m_templates->addItem(new QListWidgetItem(QIcon(QPixmap::fromImage(filters::frameThumbnail(i, QSize(64, 48)))),
                                                 QCoreApplication::translate("vek::fx", filters::frameShapeName(i))));
    m_templates->setCurrentRow(shape->value());
    m_mode = new QComboBox(this);
    m_mode->setObjectName(QStringLiteral("frameMode"));
    m_mode->addItems({tr("Maske (Rand durchsichtig)"), tr("Farbrand")});
    m_mode->setCurrentIndex(border->value());
    form->insertRow(0, tr("Modus:"), m_mode);
    layout->insertWidget(0, m_templates);
    layout->insertWidget(0, new QLabel(tr("Rahmenvorlage:"), this));
    connect(m_templates, &QListWidget::currentRowChanged, shape, [shape](int row) {
        if (row >= 0)
            shape->setValue(row);
    });
    connect(shape, &QSlider::valueChanged, m_templates, [this](int v) {
        const QSignalBlocker block(m_templates);
        m_templates->setCurrentRow(v);
    });
    connect(m_mode, &QComboBox::currentIndexChanged, border, &QSlider::setValue);
    connect(border, &QSlider::valueChanged, m_mode, [this](int v) {
        const QSignalBlocker block(m_mode);
        m_mode->setCurrentIndex(v);
    });
}

// --- Dialoge der Bitmap-Effekte ------------------------------------------------------------

std::unique_ptr<ImagePreviewDialog> createBitmapEffectDialog(const fx::Definition& def, const QImage& image, double referenceWidth,
                                                             QWidget* parent)
{
    if (def.dialog == fx::Dialog::None)
        return nullptr;
    if (def.dialog == fx::Dialog::ImageAdjust)
        return std::make_unique<ImageAdjustDialog>(image, parent);
    if (def.dialog == fx::Dialog::Custom) {
        if (def.id == QLatin1String("3d.cylinder"))
            return std::make_unique<CylinderDialog>(image, parent);
        if (def.id == QLatin1String("3d.pagecurl"))
            return std::make_unique<PageCurlDialog>(image, parent);
        if (def.id == QLatin1String("adjust.sampletarget"))
            return std::make_unique<SampleTargetDialog>(image, parent);
        if (def.id == QLatin1String("adjust.selectivecolor"))
            return std::make_unique<SelectiveColorDialog>(image, parent);
        if (def.id == QLatin1String("adjust.replacecolors"))
            return std::make_unique<ReplaceColorsDialog>(image, parent);
        if (def.id == QLatin1String("adjust.channelmixer"))
            return std::make_unique<ChannelMixerDialog>(image, parent);
        Q_ASSERT(false);
        return nullptr;
    }
    QList<SliderSpec> sliders;
    for (const fx::Slider& s : def.sliders)
        sliders << SliderSpec{fx::translate(s.label), s.minimum, s.maximum, s.value};
    QList<ColorSpec> colors;
    for (const fx::ColorField& c : def.colors)
        colors << ColorSpec{fx::translate(c.label), c.value};
    // Pixelwerte auf die Grösse des bearbeiteten Bildes (Vorschau oder Original) umrechnen
    const double width = std::max(1.0, referenceWidth);
    EffectDialog::ColorFunction fn = [&def, width](const QImage& i, const QList<int>& v, const QList<Color>& c) {
        return def.apply(i, fx::scaledValues(def, v, i.width() / width), c);
    };
    if (def.dialog == fx::Dialog::Frame)
        return std::make_unique<FrameDialog>(fx::translate(def.name), image, colors, sliders, fn, parent);
    return std::make_unique<EffectDialog>(fx::translate(def.name), image, colors, sliders, fn, parent);
}

void setBitmapEffectValues(ImagePreviewDialog& dialog, const BitmapEffect& effect)
{
    const BitmapEffect e = fx::normalized(effect);
    if (auto* adjustDialog = qobject_cast<ImageAdjustDialog*>(&dialog)) {
        const QList<int>& v = e.values;
        if (v.size() >= 9) {
            adjustDialog->setBasic(adjust::Basic{v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7]});
            adjustDialog->setAutoAdjust(v[8] != 0);
        }
    } else if (auto* effectDialog = qobject_cast<EffectDialog*>(&dialog)) {
        effectDialog->setColors(e.colors);
        effectDialog->setValues(e.values);
    } else {
        dialog.setEffectValues(e.values, e.colors);
    }
}

BitmapEffect bitmapEffectValues(const ImagePreviewDialog& dialog, const fx::Definition& def)
{
    BitmapEffect e = fx::defaults(def);
    if (const auto* adjustDialog = qobject_cast<const ImageAdjustDialog*>(&dialog)) {
        const adjust::Basic b = adjustDialog->basic();
        e.values = {b.temperature, b.tint, b.saturation, b.brightness, b.contrast, b.highlights, b.shadows, b.midtones,
                    adjustDialog->autoAdjust() ? 1 : 0};
    } else if (const auto* effectDialog = qobject_cast<const EffectDialog*>(&dialog)) {
        e.values = effectDialog->values();
        e.colors = effectDialog->colors();
    } else {
        dialog.effectValues(e.values, e.colors);
    }
    return fx::normalized(e);
}

bool editBitmapEffect(QWidget* parent, const QImage& image, double referenceWidth, BitmapEffect& effect)
{
    const fx::Definition* def = fx::find(effect.id);
    if (!def)
        return false;
    std::unique_ptr<ImagePreviewDialog> dialog = createBitmapEffectDialog(*def, image, referenceWidth, parent);
    if (!dialog)
        return true;
    setBitmapEffectValues(*dialog, effect);
    if (dialog->exec() != QDialog::Accepted)
        return false;
    const bool enabled = effect.enabled;
    effect = bitmapEffectValues(*dialog, *def);
    effect.enabled = enabled;
    return true;
}

// --- Tonkurve ------------------------------------------------------------------------------

CurveWidget::CurveWidget(QWidget* parent) : QWidget(parent)
{
    setMinimumSize(220, 220);
    setPoints({});
}

void CurveWidget::setPoints(const QList<QPointF>& points)
{
    m_points = points.isEmpty() ? QList<QPointF>{QPointF(0, 0), QPointF(255, 255)} : points;
    update();
    emit changed();
}

QRectF CurveWidget::plot() const
{
    return QRectF(rect()).adjusted(6, 6, -6, -6);
}

QPointF CurveWidget::toValue(const QPointF& pos) const
{
    const QRectF r = plot();
    return QPointF(std::clamp((pos.x() - r.left()) / r.width() * 255.0, 0.0, 255.0),
                   std::clamp((r.bottom() - pos.y()) / r.height() * 255.0, 0.0, 255.0));
}

QPointF CurveWidget::toPos(const QPointF& value) const
{
    const QRectF r = plot();
    return QPointF(r.left() + value.x() / 255.0 * r.width(), r.bottom() - value.y() / 255.0 * r.height());
}

void CurveWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), Qt::white);
    const QRectF r = plot();
    p.setPen(QColor(0xdd, 0xdd, 0xdd));
    for (int i = 1; i < 8; ++i) {
        p.drawLine(QPointF(r.left() + r.width() * i / 8, r.top()), QPointF(r.left() + r.width() * i / 8, r.bottom()));
        p.drawLine(QPointF(r.left(), r.top() + r.height() * i / 8), QPointF(r.right(), r.top() + r.height() * i / 8));
    }
    p.setPen(QColor(0x99, 0x99, 0x99));
    p.drawRect(r);
    p.drawLine(r.bottomLeft(), r.topRight());
    const std::array<int, 256> table = adjust::curveTable(m_points);
    QPainterPath curve;
    for (int x = 0; x < 256; ++x) {
        const QPointF pos = toPos(QPointF(x, table[std::size_t(x)]));
        if (x == 0)
            curve.moveTo(pos);
        else
            curve.lineTo(pos);
    }
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(Qt::black, 1.6));
    p.drawPath(curve);
    p.setBrush(Qt::white);
    p.setPen(QPen(Qt::black, 1.0));
    for (const QPointF& pt : m_points)
        p.drawRect(QRectF(toPos(pt) - QPointF(3.5, 3.5), QSizeF(7, 7)));
}

void CurveWidget::mousePressEvent(QMouseEvent* e)
{
    m_dragged = -1;
    for (int i = 0; i < m_points.size(); ++i) {
        if (QLineF(toPos(m_points[i]), e->position()).length() <= 6)
            m_dragged = i;
    }
    if (m_dragged < 0) {
        m_points.append(toValue(e->position()));
        m_dragged = int(m_points.size()) - 1;
    }
    update();
    emit changed();
}

void CurveWidget::mouseMoveEvent(QMouseEvent* e)
{
    if (m_dragged < 0)
        return;
    m_points[m_dragged] = toValue(e->position());
    update();
    emit changed();
}

void CurveWidget::mouseReleaseEvent(QMouseEvent* e)
{
    // Weit aus dem Feld gezogen: Punkt entfernen (mindestens zwei bleiben)
    if (m_dragged >= 0 && m_points.size() > 2 && !rect().adjusted(-20, -20, 20, 20).contains(e->position().toPoint())) {
        m_points.removeAt(m_dragged);
        update();
        emit changed();
    }
    m_dragged = -1;
}

ToneCurveDialog::ToneCurveDialog(const QImage& image, QWidget* parent)
    : ImagePreviewDialog(tr("Tonkurve"), image, parent)
{
    auto* layout = new QVBoxLayout(settingsArea());
    layout->setContentsMargins(0, 0, 0, 0);
    auto* form = new QFormLayout;
    m_channel = new QComboBox(this);
    m_channel->addItems({tr("RGB"), tr("Rot"), tr("Grün"), tr("Blau")});
    form->addRow(tr("Aktiver Kanal:"), m_channel);
    layout->addLayout(form);
    m_curve = new CurveWidget(this);
    layout->addWidget(m_curve);
    auto* reset = new QPushButton(tr("Aktiven Kanal zurücksetzen"), this);
    layout->addWidget(reset);
    connect(m_curve, &CurveWidget::changed, this, [this] {
        m_curves[std::size_t(m_channel->currentIndex())] = m_curve->points();
        refreshPreview();
    });
    connect(m_channel, &QComboBox::currentIndexChanged, this,
            [this](int index) { m_curve->setPoints(m_curves[std::size_t(index)]); });
    connect(reset, &QPushButton::clicked, this, [this] { m_curve->setPoints({}); });
    finishLayout();
}

QImage ToneCurveDialog::process(const QImage& image) const
{
    QImage out = image;
    for (int c = 0; c < 4; ++c) {
        if (!m_curves[std::size_t(c)].isEmpty())
            out = adjust::toneCurve(out, m_curves[std::size_t(c)], c - 1);
    }
    return out;
}

} // namespace vek
