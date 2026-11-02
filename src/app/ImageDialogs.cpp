#include "ImageDialogs.h"
#include <QSettings>
#include "Options.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
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
    return m_shown;
}

void ImagePreviewDialog::refreshPreview()
{
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

QImage ImageAdjustDialog::process(const QImage& image) const
{
    return adjust::apply(m_auto ? adjust::autoAdjust(image) : image, basic());
}

// --- Effekt mit Reglern --------------------------------------------------------------------

EffectDialog::EffectDialog(const QString& title, const QImage& image, const QList<SliderSpec>& sliders, Function fn,
                           QWidget* parent)
    : ImagePreviewDialog(title, image, parent), m_fn(std::move(fn))
{
    auto* layout = new QVBoxLayout(settingsArea());
    layout->setContentsMargins(0, 0, 0, 0);
    auto* form = new QFormLayout;
    for (const SliderSpec& spec : sliders)
        m_sliders << addSlider(form, spec, this);
    layout->addLayout(form);
    auto* reset = new QPushButton(tr("Zurücksetzen"), this);
    layout->addWidget(reset);
    for (QSlider* s : std::as_const(m_sliders))
        connect(s, &QSlider::valueChanged, this, &EffectDialog::refreshPreview);
    connect(reset, &QPushButton::clicked, this, [this, sliders] {
        QList<int> defaults;
        for (const SliderSpec& spec : sliders)
            defaults << spec.value;
        setValues(defaults);
    });
    finishLayout();
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
    return m_fn(image, values());
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
