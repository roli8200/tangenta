#include "TraceDialog.h"

#include "Icons.h"
#include "Options.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <cmath>
#include <functional>
#include <memory>

namespace vek {

namespace {

// Schachbrett für entfernte (transparente) Bereiche
void paintChecker(QPainter& p, const QRectF& r)
{
    const int cell = 8;
    p.save();
    p.setClipRect(r);
    for (double y = r.top(); y < r.bottom(); y += cell)
        for (double x = r.left(); x < r.right(); x += cell)
            p.fillRect(QRectF(x, y, cell, cell),
                       int((x - r.left()) / cell + (y - r.top()) / cell) % 2 ? QColor(0xe6, 0xe6, 0xe6) : Qt::white);
    p.restore();
}

QSlider* slider(QWidget* parent)
{
    auto* s = new QSlider(Qt::Horizontal, parent);
    s->setRange(0, 100);
    s->setMinimumWidth(150);
    return s;
}

QIcon swatch(const QColor& color, int size = 16)
{
    QPixmap pm(size, size);
    pm.fill(color);
    QPainter p(&pm);
    p.setPen(QColor(0x80, 0x80, 0x80));
    p.drawRect(0, 0, size - 1, size - 1);
    return QIcon(pm);
}

} // namespace

// Gemeinsame Ansicht beider Vorschaufenster: Zoom relativ zu "ganzes Bild", Mitte in Bildpixeln
struct TraceView {
    double zoom = 1.0;
    QPointF center;
    std::function<void()> changed;
};

// Ein Vorschaufenster: Original, Ergebnis oder Ergebnis-Umrisse über dem Original
class TracePreview : public QWidget {
public:
    enum class Mode { Original, Result, Wireframe };

    TracePreview(const QImage& image, const trace::Result& result, TraceView& view, QWidget* parent)
        : QWidget(parent), m_image(image), m_result(result), m_view(view)
    {
        setMinimumSize(260, 260);
        setCursor(Qt::OpenHandCursor);
    }

    void setMode(Mode mode)
    {
        m_mode = mode;
        update();
    }
    void setTransparency(int t)
    {
        m_transparency = t;
        update();
    }
    void setPicking(bool on)
    {
        m_picking = on;
        setCursor(on ? Qt::CrossCursor : Qt::OpenHandCursor);
    }
    std::function<void(const QColor&)> picked;

protected:
    QTransform toWidget() const
    {
        const double fit = std::min(width() / double(m_image.width()), height() / double(m_image.height())) * 0.96;
        QTransform t;
        t.translate(width() / 2.0, height() / 2.0);
        t.scale(fit * m_view.zoom, fit * m_view.zoom);
        t.translate(-m_view.center.x(), -m_view.center.y());
        return t;
    }

    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.fillRect(rect(), palette().window());
        const QTransform t = toWidget();
        const QRectF bounds = t.mapRect(QRectF(QPointF(0, 0), QSizeF(m_image.size())));
        if (m_mode == Mode::Result) {
            paintChecker(p, bounds);
        } else {
            p.fillRect(bounds, Qt::white);
            p.setRenderHint(QPainter::SmoothPixmapTransform, m_view.zoom < 4.0);
            p.setOpacity(m_mode == Mode::Wireframe ? 1.0 - m_transparency / 100.0 : 1.0);
            p.drawImage(bounds, m_image);
            p.setOpacity(1.0);
        }
        if (m_mode == Mode::Original)
            return;
        p.setRenderHint(QPainter::Antialiasing);
        p.setTransform(t);
        for (const trace::Shape& shape : m_result.shapes) {
            const QPainterPath path = CurveObject::toPainterPath(shape.paths);
            if (m_mode == Mode::Result) {
                p.fillPath(path, shape.color);
            } else {
                QPen pen(QColor(0x20, 0x20, 0x20), 1.0);
                pen.setCosmetic(true);
                p.strokePath(path, pen);
            }
        }
    }

    void mousePressEvent(QMouseEvent* e) override
    {
        if (m_picking && e->button() == Qt::LeftButton) {
            const QPoint px = toWidget().inverted().map(e->position()).toPoint();
            if (QRect(QPoint(0, 0), m_image.size()).contains(px) && picked)
                picked(m_image.pixelColor(px));
            return;
        }
        m_drag = e->position();
        setCursor(Qt::ClosedHandCursor);
    }
    void mouseMoveEvent(QMouseEvent* e) override
    {
        if (m_drag.isNull() || !(e->buttons() & Qt::LeftButton))
            return;
        const QTransform inv = toWidget().inverted();
        m_view.center += inv.map(m_drag) - inv.map(e->position());
        m_drag = e->position();
        m_view.changed();
    }
    void mouseReleaseEvent(QMouseEvent*) override
    {
        m_drag = QPointF();
        setCursor(m_picking ? Qt::CrossCursor : Qt::OpenHandCursor);
    }
    void wheelEvent(QWheelEvent* e) override
    {
        m_view.zoom = std::clamp(m_view.zoom * (e->angleDelta().y() > 0 ? 1.25 : 0.8), 0.25, 32.0);
        m_view.changed();
    }

private:
    const QImage& m_image;
    const trace::Result& m_result;
    TraceView& m_view;
    Mode m_mode = Mode::Original;
    int m_transparency = 80;
    bool m_picking = false;
    QPointF m_drag;
};

TraceDialog::TraceDialog(const QImage& image, trace::Preset preset, QWidget* parent)
    : QDialog(parent), m_image(image.convertToFormat(QImage::Format_ARGB32)), m_view(std::make_unique<TraceView>())
{
    setWindowTitle(tr("SuperTrace"));
    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    m_timer->setInterval(200);
    connect(m_timer, &QTimer::timeout, this, &TraceDialog::recompute);

    TraceView& view = *m_view;
    view.center = QPointF(m_image.width() / 2.0, m_image.height() / 2.0);

    // Kopfzeile: Vorschau-Art, Transparenz, Zoom
    auto* head = new QHBoxLayout;
    head->addWidget(new QLabel(tr("Vorschau:"), this));
    m_previewMode = new QComboBox(this);
    m_previewMode->addItems({tr("Vorher und Nachher"), tr("Grosse Vorschau"), tr("Drahtmodell-Überlagerung")});
    head->addWidget(m_previewMode);
    head->addSpacing(12);
    auto* transparencyLabel = new QLabel(tr("Transparenz:"), this);
    head->addWidget(transparencyLabel);
    m_transparency = slider(this);
    m_transparency->setMinimumWidth(100);
    m_transparency->setValue(80);
    head->addWidget(m_transparency);
    auto* transparencyValue = new QLabel(QStringLiteral("80"), this);
    transparencyValue->setMinimumWidth(24);
    head->addWidget(transparencyValue);
    head->addStretch(1);
    auto toolButton = [this](const QString& iconName, const QString& tip) {
        auto* b = new QToolButton(this);
        b->setIcon(icon(iconName));
        b->setToolTip(tip);
        b->setAutoRaise(true);
        return b;
    };
    QToolButton* zoomIn = toolButton(QStringLiteral("zoom-in"), tr("Vergrössern"));
    QToolButton* zoomOut = toolButton(QStringLiteral("zoom-out"), tr("Verkleinern"));
    QToolButton* zoomFit = toolButton(QStringLiteral("zoom-page"), tr("Ganzes Bild einpassen"));
    for (QToolButton* b : {zoomIn, zoomOut, zoomFit})
        head->addWidget(b);

    // Vorschaufenster nebeneinander
    m_before = new TracePreview(m_image, m_result, view, this);
    m_after = new TracePreview(m_image, m_result, view, this);
    m_after->setMode(TracePreview::Mode::Result);
    view.changed = [this] {
        m_before->update();
        m_after->update();
    };
    auto* previews = new QHBoxLayout;
    previews->setSpacing(4);
    previews->addWidget(m_before, 1);
    previews->addWidget(m_after, 1);

    auto* left = new QVBoxLayout;
    left->addLayout(head);
    left->addLayout(previews, 1);

    // Rechts: Art, Bildtyp, Register
    auto* right = new QVBoxLayout;
    auto* top = new QFormLayout;
    auto* type = new QComboBox(this);
    type->addItem(tr("Umriss"));
    type->setToolTip(tr("Mittellinienvektorisierung gibt es in TANGENTA noch nicht"));
    type->setEnabled(false);
    top->addRow(tr("Vektorisierungstyp:"), type);
    m_preset = new QComboBox(this);
    for (trace::Preset p : trace::presets())
        m_preset->addItem(trace::presetName(p), int(p));
    top->addRow(tr("Bildtyp:"), m_preset);
    right->addLayout(top);

    auto* tabs = new QTabWidget(this);
    right->addWidget(tabs, 1);

    // --- Register Einstellungen
    auto* settingsPage = new QWidget(this);
    auto* settingsLayout = new QVBoxLayout(settingsPage);
    auto* controls = new QGroupBox(tr("Vektorisierungs-Steuerelemente"), settingsPage);
    auto* grid = new QGridLayout(controls);
    m_detail = slider(this);
    m_smoothing = slider(this);
    m_corners = slider(this);
    m_smoothingValue = new QSpinBox(this);
    m_cornersValue = new QSpinBox(this);
    for (QSpinBox* sb : {m_smoothingValue, m_cornersValue})
        sb->setRange(0, 100);
    connect(m_smoothing, &QSlider::valueChanged, m_smoothingValue, &QSpinBox::setValue);
    connect(m_smoothingValue, &QSpinBox::valueChanged, m_smoothing, &QSlider::setValue);
    connect(m_corners, &QSlider::valueChanged, m_cornersValue, &QSpinBox::setValue);
    connect(m_cornersValue, &QSpinBox::valueChanged, m_corners, &QSlider::setValue);
    grid->addWidget(new QLabel(tr("Detail:"), this), 0, 0, 1, 3);
    grid->addWidget(new QLabel(QStringLiteral("−"), this), 1, 0);
    grid->addWidget(m_detail, 1, 1);
    grid->addWidget(new QLabel(QStringLiteral("+"), this), 1, 2);
    grid->addWidget(new QLabel(tr("Glättung:"), this), 2, 0, 1, 3);
    grid->addWidget(m_smoothing, 3, 1);
    grid->addWidget(m_smoothingValue, 3, 2);
    grid->addWidget(new QLabel(tr("Kantenglättung:"), this), 4, 0, 1, 3);
    grid->addWidget(m_corners, 5, 1);
    grid->addWidget(m_cornersValue, 5, 2);
    m_detail->setToolTip(tr("Weniger Detail fasst kleine Flecken mit ihrer Umgebung zusammen"));
    m_smoothing->setToolTip(tr("Glättet die Umrisse und spart Knoten"));
    m_corners->setToolTip(tr("Höhere Werte runden Ecken ab"));
    settingsLayout->addWidget(controls);

    auto* options = new QGroupBox(tr("Optionen"), settingsPage);
    auto* optionsLayout = new QGridLayout(options);
    optionsLayout->setColumnMinimumWidth(0, 16);
    m_deleteOriginal = new QCheckBox(tr("Originalbild löschen"), this);
    m_removeBackground = new QCheckBox(tr("Hintergrund entfernen"), this);
    m_backgroundAuto = new QRadioButton(tr("Farbe automatisch wählen"), this);
    m_backgroundGiven = new QRadioButton(tr("Farbe angeben:"), this);
    auto* backgroundGroup = new QButtonGroup(this);
    backgroundGroup->addButton(m_backgroundAuto);
    backgroundGroup->addButton(m_backgroundGiven);
    m_backgroundAuto->setChecked(true);
    m_backgroundSwatch = new QToolButton(this);
    m_backgroundSwatch->setToolTip(tr("Hintergrundfarbe wählen"));
    m_pipette = new QToolButton(this);
    m_pipette->setIcon(icon(QStringLiteral("eyedropper")));
    m_pipette->setToolTip(tr("Hintergrundfarbe in der Vorschau aufnehmen"));
    m_pipette->setCheckable(true);
    m_removeEverywhere = new QCheckBox(tr("Farbe aus ganzem Bild entfernen"), this);
    m_removeEverywhere->setToolTip(tr("Sonst nur die Flächen dieser Farbe, die den Bildrand berühren"));
    m_mergeAdjacent = new QCheckBox(tr("Benachbarte gleichfarbige Objekte verbinden"), this);
    m_removeOverlap = new QCheckBox(tr("Überstand des Objekts entfernen"), this);
    m_removeOverlap->setToolTip(tr("Jede Farbfläche nur in ihrem eigenen Bereich statt übereinander gestapelt"));
    m_groupByColor = new QCheckBox(tr("Objekte farblich gruppieren"), this);
    auto* givenRow = new QHBoxLayout;
    givenRow->addWidget(m_backgroundGiven);
    givenRow->addWidget(m_backgroundSwatch);
    givenRow->addWidget(m_pipette);
    givenRow->addStretch(1);
    optionsLayout->addWidget(m_deleteOriginal, 0, 0, 1, 2);
    optionsLayout->addWidget(m_removeBackground, 1, 0, 1, 2);
    optionsLayout->addWidget(m_backgroundAuto, 2, 1);
    optionsLayout->addLayout(givenRow, 3, 1);
    optionsLayout->addWidget(m_removeEverywhere, 4, 1);
    optionsLayout->addWidget(m_mergeAdjacent, 5, 0, 1, 2);
    optionsLayout->addWidget(m_removeOverlap, 6, 0, 1, 2);
    optionsLayout->addWidget(m_groupByColor, 7, 1);
    settingsLayout->addWidget(options);
    settingsLayout->addStretch(1);
    tabs->addTab(settingsPage, tr("Einstellungen"));

    // --- Register Farben
    auto* colorsPage = new QWidget(this);
    auto* colorsLayout = new QVBoxLayout(colorsPage);
    auto* colorsForm = new QFormLayout;
    auto* colorMode = new QComboBox(this);
    colorMode->addItem(tr("RGB"));
    colorMode->setEnabled(false);
    colorsForm->addRow(tr("Farbmodus:"), colorMode);
    m_colors = new QSpinBox(this);
    m_colors->setRange(2, 64);
    colorsForm->addRow(tr("Anzahl der Farben:"), m_colors);
    m_colorSort = new QComboBox(this);
    m_colorSort->addItems({tr("Häufigkeit"), tr("Ähnlichkeit")});
    colorsForm->addRow(tr("Sortieren nach:"), m_colorSort);
    colorsLayout->addLayout(colorsForm);
    m_colorList = new QListWidget(this);
    m_colorList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    colorsLayout->addWidget(m_colorList, 1);
    auto* colorButtons = new QHBoxLayout;
    m_mergeButton = new QPushButton(tr("Zusammenführen"), this);
    m_mergeButton->setToolTip(tr("Ausgewählte Farben zu einer zusammenfassen"));
    m_editButton = new QPushButton(tr("Bearbeiten …"), this);
    colorButtons->addWidget(m_mergeButton);
    colorButtons->addWidget(m_editButton);
    colorButtons->addStretch(1);
    colorsLayout->addLayout(colorButtons);
    tabs->addTab(colorsPage, tr("Farben"));

    auto* stats = new QGroupBox(tr("Details der Vektorisierungsergebnisse"), this);
    auto* statsForm = new QFormLayout(stats);
    m_curves = new QLabel(this);
    m_nodes = new QLabel(this);
    m_colorCount = new QLabel(this);
    statsForm->addRow(tr("Kurven:"), m_curves);
    statsForm->addRow(tr("Knoten:"), m_nodes);
    statsForm->addRow(tr("Farben:"), m_colorCount);
    right->addWidget(stats);

    auto* rightWidget = new QWidget(this);
    rightWidget->setLayout(right);
    rightWidget->setFixedWidth(330);
    right->setContentsMargins(0, 0, 0, 0);
    auto* columns = new QHBoxLayout;
    columns->addLayout(left, 1);
    columns->addWidget(rightWidget);

    // Fusszeile: Rückgängig/Wiederholen, Zurücksetzen, Optionen | OK, Abbrechen, Hilfe
    auto* foot = new QHBoxLayout;
    m_undoButton = toolButton(QStringLiteral("edit-undo"), tr("Rückgängig"));
    m_redoButton = toolButton(QStringLiteral("edit-redo"), tr("Wiederholen"));
    auto* resetButton = new QPushButton(tr("Zurücksetzen"), this);
    auto* optionsButton = new QPushButton(tr("Optionen …"), this);
    foot->addWidget(m_undoButton);
    foot->addWidget(m_redoButton);
    foot->addWidget(resetButton);
    foot->addWidget(optionsButton);
    foot->addStretch(1);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Help, this);
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Abbrechen"));
    buttons->button(QDialogButtonBox::Help)->setText(tr("Hilfe"));
    foot->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        if (m_timer->isActive())
            recompute();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::helpRequested, this, &TraceDialog::showHelp);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(columns, 1);
    layout->addLayout(foot);
    resize(1080, 660);

    // Verbindungen
    connect(m_previewMode, &QComboBox::currentIndexChanged, this, &TraceDialog::setPreviewMode);
    connect(m_transparency, &QSlider::valueChanged, this, [this, transparencyValue](int v) {
        transparencyValue->setText(QString::number(v));
        m_after->setTransparency(v);
    });
    connect(zoomIn, &QToolButton::clicked, this, [&view] {
        view.zoom = std::min(view.zoom * 1.5, 32.0);
        view.changed();
    });
    connect(zoomOut, &QToolButton::clicked, this, [&view] {
        view.zoom = std::max(view.zoom / 1.5, 0.25);
        view.changed();
    });
    connect(zoomFit, &QToolButton::clicked, this, [this, &view] {
        view.zoom = 1.0;
        view.center = QPointF(m_image.width() / 2.0, m_image.height() / 2.0);
        view.changed();
    });
    connect(m_preset, &QComboBox::activated, this, [this] {
        m_recolor.clear();
        applyPreset(trace::Preset(m_preset->currentData().toInt()));
    });
    for (QSlider* s : {m_detail, m_smoothing, m_corners})
        connect(s, &QSlider::valueChanged, this, &TraceDialog::schedule);
    connect(m_colors, &QSpinBox::valueChanged, this, [this] {
        m_recolor.clear(); // die ermittelten Farben ändern sich
        schedule();
    });
    for (QAbstractButton* b : std::initializer_list<QAbstractButton*>{m_removeBackground, m_backgroundAuto, m_backgroundGiven,
                                                                       m_removeEverywhere, m_mergeAdjacent, m_removeOverlap,
                                                                       m_groupByColor})
        connect(b, &QAbstractButton::toggled, this, [this] {
            updateEnabled();
            schedule();
        });
    connect(m_backgroundSwatch, &QToolButton::clicked, this, [this] {
        const QColor c = QColorDialog::getColor(m_background, this, tr("Hintergrundfarbe"));
        if (c.isValid())
            pickBackground(c);
    });
    connect(m_pipette, &QToolButton::toggled, this, [this](bool on) {
        m_before->setPicking(on);
        m_after->setPicking(on);
    });
    m_before->picked = m_after->picked = [this](const QColor& c) {
        m_pipette->setChecked(false);
        pickBackground(c);
    };
    connect(m_colorSort, &QComboBox::currentIndexChanged, this, &TraceDialog::updateColors);
    connect(m_colorList, &QListWidget::itemSelectionChanged, this, &TraceDialog::updateEnabled);
    connect(m_mergeButton, &QPushButton::clicked, this, [this] {
        QList<QRgb> sources;
        for (QListWidgetItem* item : m_colorList->selectedItems())
            sources << QRgb(item->data(Qt::UserRole).toUInt());
        mergeColors(sources);
    });
    connect(m_editButton, &QPushButton::clicked, this, [this] {
        const QList<QListWidgetItem*> items = m_colorList->selectedItems();
        if (items.size() != 1)
            return;
        const QRgb source = QRgb(items.first()->data(Qt::UserRole).toUInt());
        const QColor c = QColorDialog::getColor(QColor::fromRgb(m_recolor.value(source, source)), this, tr("Farbe bearbeiten"));
        if (c.isValid())
            editColor(source, c);
    });
    connect(m_undoButton, &QToolButton::clicked, this, &TraceDialog::undo);
    connect(m_redoButton, &QToolButton::clicked, this, &TraceDialog::redo);
    connect(resetButton, &QPushButton::clicked, this, &TraceDialog::reset);
    connect(optionsButton, &QPushButton::clicked, this, [this] {
        emit optionsRequested();
        schedule(); // Leistung kann sich geändert haben
    });

    m_preset->setCurrentIndex(m_preset->findData(int(preset)));
    setPreviewMode(0);
    showBackgroundColor();
    applyPreset(preset);
    recompute();
}

TraceDialog::~TraceDialog() = default;

void TraceDialog::setPreviewMode(int mode)
{
    if (m_previewMode->currentIndex() != mode)
        m_previewMode->setCurrentIndex(mode);
    m_before->setVisible(mode == 0);
    m_after->setMode(mode == 2 ? TracePreview::Mode::Wireframe : TracePreview::Mode::Result);
    m_after->setTransparency(m_transparency->value());
    m_transparency->setEnabled(mode == 2);
}

void TraceDialog::applyPreset(trace::Preset preset)
{
    const trace::Settings s = trace::presetSettings(preset);
    const QSignalBlocker b1(m_detail), b2(m_smoothing), b3(m_corners), b4(m_colors), b5(m_removeBackground),
        b6(m_removeOverlap), b7(m_removeEverywhere), b8(m_mergeAdjacent), b9(m_groupByColor), b10(m_backgroundAuto),
        b11(m_backgroundGiven);
    m_detail->setValue(s.detail);
    m_smoothing->setValue(s.smoothing);
    m_smoothingValue->setValue(s.smoothing);
    m_corners->setValue(s.cornerSmoothing);
    m_cornersValue->setValue(s.cornerSmoothing);
    m_colors->setValue(s.colors);
    m_removeBackground->setChecked(s.removeBackground);
    m_removeEverywhere->setChecked(s.removeColorEverywhere);
    m_removeOverlap->setChecked(s.removeOverlap);
    m_mergeAdjacent->setChecked(s.mergeAdjacent);
    m_groupByColor->setChecked(s.groupByColor);
    m_backgroundAuto->setChecked(true);
    updateEnabled();
    schedule();
}

void TraceDialog::setSettings(const trace::Settings& s)
{
    const QSignalBlocker b1(m_detail), b2(m_smoothing), b3(m_corners), b4(m_colors), b5(m_removeBackground),
        b6(m_removeOverlap), b7(m_removeEverywhere), b8(m_mergeAdjacent), b9(m_groupByColor), b10(m_backgroundAuto),
        b11(m_backgroundGiven), b12(m_smoothingValue), b13(m_cornersValue);
    m_detail->setValue(s.detail);
    m_smoothing->setValue(s.smoothing);
    m_smoothingValue->setValue(s.smoothing);
    m_corners->setValue(s.cornerSmoothing);
    m_cornersValue->setValue(s.cornerSmoothing);
    m_colors->setValue(s.colors);
    m_removeBackground->setChecked(s.removeBackground);
    m_removeEverywhere->setChecked(s.removeColorEverywhere);
    m_removeOverlap->setChecked(s.removeOverlap);
    m_mergeAdjacent->setChecked(s.mergeAdjacent);
    m_groupByColor->setChecked(s.groupByColor);
    (s.background ? m_backgroundGiven : m_backgroundAuto)->setChecked(true);
    if (s.background)
        m_background = *s.background;
    m_recolor = s.recolor;
    showBackgroundColor();
    updateEnabled();
    schedule();
}

trace::Settings TraceDialog::settings() const
{
    trace::Settings s;
    s.detail = m_detail->value();
    s.smoothing = m_smoothing->value();
    s.cornerSmoothing = m_corners->value();
    s.colors = m_colors->value();
    s.removeBackground = m_removeBackground->isChecked();
    if (m_backgroundGiven->isChecked())
        s.background = m_background;
    s.removeColorEverywhere = m_removeEverywhere->isChecked();
    s.removeOverlap = m_removeOverlap->isChecked();
    s.mergeAdjacent = m_mergeAdjacent->isChecked();
    s.groupByColor = m_groupByColor->isChecked();
    s.recolor = m_recolor;
    // Optionen > SuperTrace > Leistung: 0 = schnell (kleineres Rechenbild) .. 100 = genau
    s.maxSide = 400 + 12 * std::clamp(Options::instance().integer(QStringLiteral("ws/trace/performance")), 0, 100);
    return s;
}

trace::Preset TraceDialog::preset() const
{
    return trace::Preset(m_preset->currentData().toInt());
}

bool TraceDialog::deleteOriginal() const
{
    return m_deleteOriginal->isChecked();
}

void TraceDialog::schedule()
{
    m_timer->start();
}

void TraceDialog::updateEnabled()
{
    const bool background = m_removeBackground->isChecked();
    m_backgroundAuto->setEnabled(background);
    m_backgroundGiven->setEnabled(background);
    m_removeEverywhere->setEnabled(background);
    m_backgroundSwatch->setEnabled(background && m_backgroundGiven->isChecked());
    m_pipette->setEnabled(background && m_backgroundGiven->isChecked());
    if (!m_pipette->isEnabled())
        m_pipette->setChecked(false);
    const int selected = int(m_colorList->selectedItems().size());
    m_mergeButton->setEnabled(selected >= 2);
    m_editButton->setEnabled(selected == 1);
    m_undoButton->setEnabled(canUndo());
    m_redoButton->setEnabled(canRedo());
}

void TraceDialog::showBackgroundColor()
{
    m_backgroundSwatch->setIcon(swatch(m_background, 18));
}

void TraceDialog::pickBackground(const QColor& color)
{
    m_background = color;
    showBackgroundColor();
    m_removeBackground->setChecked(true);
    m_backgroundGiven->setChecked(true);
    schedule();
}

void TraceDialog::mergeColors(const QList<QRgb>& sources)
{
    if (sources.size() < 2)
        return;
    // Optionen > SuperTrace > Farben mischen: Durchschnitt (nach Pixeln) oder zuerst gewählte
    QColor target;
    if (Options::instance().integer(QStringLiteral("ws/trace/mergeMode")) == 1) {
        target = QColor::fromRgb(m_recolor.value(sources.first(), sources.first()));
    } else {
        double r = 0, g = 0, b = 0, n = 0;
        for (const trace::PaletteEntry& e : std::as_const(m_result.palette)) {
            if (!sources.contains(e.source))
                continue;
            r += e.color.redF() * e.pixels;
            g += e.color.greenF() * e.pixels;
            b += e.color.blueF() * e.pixels;
            n += e.pixels;
        }
        if (n <= 0)
            return;
        target = QColor::fromRgbF(float(r / n), float(g / n), float(b / n));
    }
    // Auch Farben, die schon mit einer der gewählten zusammengeführt waren
    QList<QRgb> outputs;
    for (QRgb s : sources)
        outputs << m_recolor.value(s, s);
    for (const trace::PaletteEntry& e : std::as_const(m_result.palette))
        if (outputs.contains(e.color.rgb()) && !sources.contains(e.source))
            m_recolor.insert(e.source, target.rgb());
    for (auto it = m_recolor.begin(); it != m_recolor.end(); ++it)
        if (outputs.contains(it.value()))
            it.value() = target.rgb();
    for (QRgb s : sources)
        m_recolor.insert(s, target.rgb());
    schedule();
}

void TraceDialog::editColor(QRgb source, const QColor& color)
{
    const QRgb old = m_recolor.value(source, source);
    for (auto it = m_recolor.begin(); it != m_recolor.end(); ++it)
        if (it.value() == old)
            it.value() = color.rgb();
    m_recolor.insert(source, color.rgb());
    schedule();
}

void TraceDialog::undo()
{
    if (!canUndo())
        return;
    m_restoring = true;
    setSettings(m_history[--m_historyPos]);
    recompute();
    m_restoring = false;
    updateEnabled();
}

void TraceDialog::redo()
{
    if (!canRedo())
        return;
    m_restoring = true;
    setSettings(m_history[++m_historyPos]);
    recompute();
    m_restoring = false;
    updateEnabled();
}

void TraceDialog::reset()
{
    m_recolor.clear();
    applyPreset(preset());
}

void TraceDialog::recompute()
{
    m_timer->stop();
    const trace::Settings s = settings();
    if (!m_restoring && (m_history.isEmpty() || !(m_history[m_historyPos] == s))) {
        m_history.resize(m_historyPos + 1);
        m_history.append(s);
        m_historyPos = int(m_history.size()) - 1;
    }
    QApplication::setOverrideCursor(Qt::WaitCursor);
    m_result = trace::run(m_image, s);
    QApplication::restoreOverrideCursor();
    m_curves->setText(QString::number(m_result.shapes.size()));
    m_nodes->setText(QString::number(m_result.nodes));
    m_colorCount->setText(QString::number(m_result.colors));
    m_before->update();
    m_after->update();
    updateColors();
    updateEnabled();
}

void TraceDialog::updateColors()
{
    QList<QRgb> selected;
    for (QListWidgetItem* item : m_colorList->selectedItems())
        selected << QRgb(item->data(Qt::UserRole).toUInt());
    QList<trace::PaletteEntry> entries = m_result.palette;
    if (m_colorSort->currentIndex() == 1) {
        // Ähnlichkeit: nach Farbton, dann Helligkeit
        std::stable_sort(entries.begin(), entries.end(), [](const trace::PaletteEntry& a, const trace::PaletteEntry& b) {
            const int ha = a.color.hslHue(), hb = b.color.hslHue();
            return ha != hb ? ha < hb : a.color.lightness() < b.color.lightness();
        });
    }
    double total = 0;
    for (const trace::PaletteEntry& e : std::as_const(entries))
        total += e.pixels;
    const QSignalBlocker blocker(m_colorList);
    m_colorList->clear();
    for (const trace::PaletteEntry& e : std::as_const(entries)) {
        const QColor c = e.color;
        auto* item = new QListWidgetItem(swatch(c, 20),
                                         tr("R %1  G %2  B %3   (%4 %)")
                                             .arg(c.red())
                                             .arg(c.green())
                                             .arg(c.blue())
                                             .arg(total > 0 ? 100.0 * e.pixels / total : 0.0, 0, 'f', 1),
                                         m_colorList);
        item->setData(Qt::UserRole, uint(e.source));
        item->setSelected(selected.contains(e.source));
    }
}

void TraceDialog::showHelp()
{
    QMessageBox::information(
        this, tr("SuperTrace"),
        tr("<p><b>Bildtyp</b> wählt passende Voreinstellungen: Strichgrafik (schwarz auf weiss), Logo, "
           "Detailliertes Logo, Clipart, Bild niedriger oder hoher Qualität.</p>"
           "<p><b>Detail</b> bestimmt, wie kleine Flecken erhalten bleiben; <b>Glättung</b> glättet die Umrisse und "
           "spart Knoten; <b>Kantenglättung</b> rundet Ecken ab.</p>"
           "<p><b>Hintergrund entfernen</b> lässt die Hintergrundfarbe weg – automatisch die häufigste Randfarbe oder "
           "eine angegebene Farbe (Pipette: in die Vorschau klicken). Ohne <i>Farbe aus ganzem Bild entfernen</i> "
           "verschwinden nur die Flächen, die den Bildrand berühren.</p>"
           "<p>Im Register <b>Farben</b> lassen sich Farben zusammenführen oder ändern.</p>"
           "<p>Die Vorschau zoomt mit dem Mausrad und lässt sich mit der Maus verschieben.</p>"));
}

} // namespace vek
