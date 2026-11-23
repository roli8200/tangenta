#include "CutoutAssistant.h"

#include "Cutout.h"
#include "Icons.h"
#include "Matting.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QEventLoop>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSettings>
#include <QShortcut>
#include <QSlider>
#include <QSpinBox>
#include <QThread>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <cmath>

namespace vek {

namespace {

QPixmap checkerTile()
{
    QPixmap tile(16, 16);
    tile.fill(QColor(255, 255, 255));
    QPainter p(&tile);
    p.fillRect(0, 0, 8, 8, QColor(204, 204, 204));
    p.fillRect(8, 8, 8, 8, QColor(204, 204, 204));
    return tile;
}

QImage emptyLayer(const QSize& size)
{
    QImage layer(size, QImage::Format_Grayscale8);
    layer.fill(0);
    return layer;
}

bool anySet(const QImage& layer)
{
    for (int y = 0; y < layer.height(); ++y) {
        const uchar* row = layer.constScanLine(y);
        for (int x = 0; x < layer.width(); ++x)
            if (row[x])
                return true;
    }
    return false;
}

} // namespace

// --- Ansicht ---------------------------------------------------------------------------------

CutoutView::CutoutView(CutoutAssistant& owner) : QWidget(&owner), m_owner(owner)
{
    setObjectName(QStringLiteral("cutoutView"));
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(420, 360);
    m_display = owner.m_image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    m_overlay = QImage(owner.m_image.size(), QImage::Format_ARGB32_Premultiplied);
    m_overlay.fill(Qt::transparent);
}

QPointF CutoutView::toImage(const QPointF& widget) const
{
    return widget / m_zoom + m_offset;
}

QPointF CutoutView::toWidget(const QPointF& image) const
{
    return (image - m_offset) * m_zoom;
}

void CutoutView::fitToWindow()
{
    const QSizeF size = m_display.size();
    if (size.isEmpty() || width() < 10)
        return;
    m_zoom = std::min(width() / size.width(), height() / size.height()) * 0.98;
    m_offset = QPointF(size.width() / 2 - width() / 2.0 / m_zoom, size.height() / 2 - height() / 2.0 / m_zoom);
    update();
}

void CutoutView::setZoom(double zoom, const QPointF& anchorWidget)
{
    zoom = std::clamp(zoom, 0.02, 32.0);
    const QPointF anchor = anchorWidget.x() < 0 ? QPointF(width() / 2.0, height() / 2.0) : anchorWidget;
    const QPointF fixed = toImage(anchor);
    m_zoom = zoom;
    m_offset = fixed - anchor / m_zoom;
    update();
}

void CutoutView::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (!m_fitted) {
        fitToWindow();
        m_fitted = true;
    }
}

void CutoutView::updateOverlay(const QRect& imageRect)
{
    const QRect r = imageRect & m_overlay.rect();
    const auto& L = m_owner.m_layers;
    const bool showMarker = !m_owner.m_showMarker || m_owner.m_showMarker->isChecked();
    // In der Vorschau färbte die Füllung das ganze Motiv blau: dort nur Marker und Detailpinsel
    const bool previewing = !m_owner.m_composite.isNull() && !(m_owner.m_showOriginal && m_owner.m_showOriginal->isChecked());
    const bool showFill = (!m_owner.m_showFill || m_owner.m_showFill->isChecked()) && !previewing;
    auto blend = [](QRgb under, const QColor& c, int a) {
        // c mit Deckkraft a über under (vormultipliziert)
        const int ia = 255 - a;
        return qRgba((c.red() * a + qRed(under) * ia) / 255, (c.green() * a + qGreen(under) * ia) / 255,
                     (c.blue() * a + qBlue(under) * ia) / 255, a + qAlpha(under) * ia / 255);
    };
    for (int y = r.top(); y <= r.bottom(); ++y) {
        auto* out = reinterpret_cast<QRgb*>(m_overlay.scanLine(y));
        const uchar* m = L.marker.constScanLine(y);
        const uchar* f = L.fill.constScanLine(y);
        const uchar* k = L.keep.constScanLine(y);
        const uchar* d = L.remove.constScanLine(y);
        for (int x = r.left(); x <= r.right(); ++x) {
            QRgb v = 0;
            if (showFill && f[x])
                v = blend(v, fillColor, 95);
            if (showMarker && m[x])
                v = blend(v, markerColor, 150);
            if (k[x])
                v = blend(v, QColor(250, 220, 40), 140);
            if (d[x])
                v = blend(v, QColor(230, 40, 40), 140);
            out[x] = v;
        }
    }
    update();
}

void CutoutView::rebuildOverlay()
{
    updateOverlay(m_overlay.rect());
}

void CutoutView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(0x5c, 0x60, 0x66));
    const QRectF imageRect(QPointF(0, 0), QSizeF(m_display.size()));
    const QRectF visible = QRectF(toImage(QPointF(0, 0)), toImage(QPointF(width(), height()))) & imageRect;
    if (visible.isEmpty())
        return;
    const QRectF target(toWidget(visible.topLeft()), toWidget(visible.bottomRight()));
    const bool preview = !m_owner.m_composite.isNull() && !(m_owner.m_showOriginal && m_owner.m_showOriginal->isChecked());
    p.setRenderHint(QPainter::SmoothPixmapTransform, m_zoom < 1.0 && m_stroke.isEmpty());
    if (preview) {
        const int bg = m_owner.m_background ? m_owner.m_background->currentIndex() : 0;
        static const QColor colors[] = {QColor(), Qt::white, Qt::black, QColor(128, 128, 128), QColor(0, 200, 0)};
        if (bg == 0)
            p.fillRect(target, QBrush(checkerTile()));
        else
            p.fillRect(target, colors[std::clamp(bg, 1, 4)]);
        p.drawImage(target, m_owner.m_composite, visible);
    } else {
        p.drawImage(target, m_display, visible);
    }
    p.drawImage(target, m_overlay, visible);
    // Pinselspitze
    const auto tool = m_owner.m_tool;
    if (m_mouse.x() >= 0 && !m_panning && !m_space && tool != CutoutAssistant::Tool::Fill && tool != CutoutAssistant::Tool::Pan) {
        const double r = m_owner.brushSize() / 2.0 * m_zoom;
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(0, 0, 0, 160), 1));
        p.drawEllipse(m_mouse, r + 1, r + 1);
        p.setPen(QPen(QColor(255, 255, 255, 220), 1));
        p.drawEllipse(m_mouse, r, r);
    }
}

void CutoutView::mousePressEvent(QMouseEvent* event)
{
    setFocus();
    const QPointF pos = event->position();
    const bool pan = event->button() == Qt::MiddleButton
                     || (event->button() == Qt::LeftButton && (m_space || m_owner.m_tool == CutoutAssistant::Tool::Pan));
    if (pan) {
        m_panning = true;
        m_panStart = pos.toPoint();
        m_panOffset = m_offset;
        setCursor(Qt::ClosedHandCursor);
        return;
    }
    if (event->button() != Qt::LeftButton || m_owner.m_busy)
        return;
    const QPointF image = toImage(pos);
    if (m_owner.m_tool == CutoutAssistant::Tool::Fill) {
        m_owner.fillAt(image.toPoint());
        return;
    }
    m_owner.pushUndo();
    m_stroke = {image};
    m_owner.paintSegment(image, image);
}

void CutoutView::mouseMoveEvent(QMouseEvent* event)
{
    const QPointF pos = event->position();
    m_mouse = pos;
    if (m_panning) {
        m_offset = m_panOffset - (pos - QPointF(m_panStart)) / m_zoom;
        update();
        return;
    }
    if (!m_stroke.isEmpty()) {
        const QPointF image = toImage(pos);
        m_owner.paintSegment(m_stroke.last(), image);
        m_stroke << image;
        return;
    }
    update();
}

void CutoutView::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_panning && (event->button() == Qt::LeftButton || event->button() == Qt::MiddleButton)) {
        m_panning = false;
        setCursor(m_space || m_owner.m_tool == CutoutAssistant::Tool::Pan ? Qt::OpenHandCursor : Qt::CrossCursor);
        return;
    }
    if (!m_stroke.isEmpty() && event->button() == Qt::LeftButton) {
        m_stroke.clear();
        m_owner.strokeFinished();
        update();
    }
}

void CutoutView::wheelEvent(QWheelEvent* event)
{
    const double steps = event->angleDelta().y() / 120.0;
    if (steps != 0)
        setZoom(m_zoom * std::pow(1.25, steps), event->position());
}

void CutoutView::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        m_space = true;
        setCursor(Qt::OpenHandCursor);
        update();
        return;
    }
    if (event->key() == Qt::Key_BracketLeft || event->key() == Qt::Key_BracketRight) {
        const int size = m_owner.brushSize();
        m_owner.setBrushSize(event->key() == Qt::Key_BracketLeft ? std::max(1, size * 4 / 5) : size * 5 / 4 + 1);
        update();
        return;
    }
    QWidget::keyPressEvent(event);
}

void CutoutView::keyReleaseEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        m_space = false;
        setCursor(m_owner.m_tool == CutoutAssistant::Tool::Pan ? Qt::OpenHandCursor : Qt::CrossCursor);
        update();
        return;
    }
    QWidget::keyReleaseEvent(event);
}

void CutoutView::leaveEvent(QEvent* event)
{
    m_mouse = QPointF(-1, -1);
    update();
    QWidget::leaveEvent(event);
}

// --- Dialog ----------------------------------------------------------------------------------

CutoutAssistant::CutoutAssistant(const QImage& image, QWidget* parent) : QDialog(parent), m_image(image)
{
    setWindowTitle(tr("Freistellungs-Assistent"));
    setObjectName(QStringLiteral("cutoutAssistant"));
    const QSize size = m_image.size();
    m_layers = {emptyLayer(size), emptyLayer(size), emptyLayer(size), emptyLayer(size)};
    m_undoLimit = qint64(size.width()) * size.height() > 6'000'000 ? 12 : 30;
    QSettings settings;

    auto* outer = new QHBoxLayout(this);
    // links: Ansicht mit Zoomleiste und Rückgängig
    auto* left = new QVBoxLayout;
    auto* zoomRow = new QHBoxLayout;
    zoomRow->addStretch(1);
    auto zoomButton = [&](const QString& iconName, const QString& tip, std::function<void()> fn) {
        auto* b = new QToolButton(this);
        b->setIcon(icon(iconName));
        b->setIconSize(QSize(20, 20));
        b->setToolTip(tip);
        b->setAutoRaise(true);
        connect(b, &QToolButton::clicked, this, fn);
        zoomRow->addWidget(b);
        return b;
    };
    m_view = new CutoutView(*this);
    zoomButton(QStringLiteral("zoom-in"), tr("Vergrössern"), [this] { m_view->setZoom(m_view->zoom() * 1.25); });
    zoomButton(QStringLiteral("zoom-out"), tr("Verkleinern"), [this] { m_view->setZoom(m_view->zoom() / 1.25); });
    QToolButton* panButton = zoomButton(QStringLiteral("tool-pan"), tr("Hand (Bild verschieben; auch Leertaste oder mittlere Maustaste)"),
                                        [this] { setTool(Tool::Pan); });
    zoomButton(QStringLiteral("zoom-100"), tr("Originalgrösse (1:1)"), [this] { m_view->setZoom(1.0); });
    zoomButton(QStringLiteral("zoom-page"), tr("Ganzes Bild"), [this] { m_view->fitToWindow(); });
    zoomRow->addStretch(1);
    left->addLayout(zoomRow);
    left->addWidget(m_view, 1);
    auto* bottom = new QHBoxLayout;
    m_undoButton = new QToolButton(this);
    m_undoButton->setIcon(icon(QStringLiteral("edit-undo")));
    m_undoButton->setToolTip(tr("Rückgängig (Strg+Z)"));
    m_undoButton->setAutoRaise(true);
    m_redoButton = new QToolButton(this);
    m_redoButton->setIcon(icon(QStringLiteral("edit-redo")));
    m_redoButton->setToolTip(tr("Wiederherstellen (Strg+Y)"));
    m_redoButton->setAutoRaise(true);
    auto* resetButton = new QPushButton(tr("Zurücksetzen"), this);
    bottom->addWidget(m_undoButton);
    bottom->addWidget(m_redoButton);
    bottom->addWidget(resetButton);
    bottom->addStretch(1);
    m_status = new QLabel(this);
    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 100);
    m_progress->setMaximumWidth(160);
    m_progress->hide();
    bottom->addWidget(m_status);
    bottom->addWidget(m_progress);
    left->addLayout(bottom);
    outer->addLayout(left, 1);
    connect(m_undoButton, &QToolButton::clicked, this, &CutoutAssistant::undo);
    connect(m_redoButton, &QToolButton::clicked, this, &CutoutAssistant::redo);
    connect(resetButton, &QPushButton::clicked, this, &CutoutAssistant::reset);
    auto* undoKey = new QShortcut(QKeySequence::Undo, this);
    connect(undoKey, &QShortcut::activated, this, &CutoutAssistant::undo);
    auto* redoKey = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Y), this);
    connect(redoKey, &QShortcut::activated, this, &CutoutAssistant::redo);

    // rechts: Werkzeuge und Einstellungen
    auto* panel = new QWidget(this);
    auto* pl = new QVBoxLayout(panel);
    pl->setContentsMargins(0, 0, 0, 0);
    auto* toolRow = new QHBoxLayout;
    struct ToolDef {
        Tool tool;
        const char* icon;
        QString tip;
    };
    const ToolDef tools[] = {
        {Tool::Marker, "cutout-marker", tr("Marker: Band über die Kante des Objekts ziehen")},
        {Tool::Eraser, "tool-eraser", tr("Radierer: Markierung und Füllung entfernen")},
        {Tool::Fill, "tool-fill", tr("Füllen: innerhalb der Markierung klicken")},
        {Tool::Keep, "cutout-keep", tr("Detail behalten: Bereiche, die zum Objekt gehören")},
        {Tool::Remove, "cutout-remove", tr("Detail entfernen: Bereiche, die weg sollen")},
    };
    auto* group = new QButtonGroup(this);
    for (const ToolDef& def : tools) {
        auto* b = new QToolButton(panel);
        b->setIcon(icon(QString::fromLatin1(def.icon)));
        b->setIconSize(QSize(24, 24));
        b->setToolTip(def.tip);
        b->setCheckable(true);
        b->setAutoRaise(true);
        b->setProperty("tool", int(def.tool));
        b->setObjectName(QStringLiteral("cutoutTool%1").arg(int(def.tool)));
        group->addButton(b);
        toolRow->addWidget(b);
        m_toolButtons << b;
        connect(b, &QToolButton::clicked, this, [this, def] { setTool(def.tool); });
    }
    panButton->setCheckable(true);
    panButton->setProperty("tool", int(Tool::Pan));
    group->addButton(panButton);
    m_toolButtons << panButton;
    toolRow->addStretch(1);
    pl->addLayout(toolRow);

    auto* options = new QGroupBox(tr("Hilfsmitteloptionen"), panel);
    auto* of = new QFormLayout(options);
    auto* sizeRow = new QHBoxLayout;
    m_size = new QSpinBox(options);
    m_size->setRange(1, 999);
    m_size->setSuffix(tr(" px"));
    m_sizeSlider = new QSlider(Qt::Horizontal, options);
    m_sizeSlider->setRange(1, 300);
    sizeRow->addWidget(m_size);
    sizeRow->addWidget(m_sizeSlider, 1);
    of->addRow(tr("Spitzengrösse:"), sizeRow);
    connect(m_size, &QSpinBox::valueChanged, this, [this](int v) {
        const QSignalBlocker block(m_sizeSlider);
        m_sizeSlider->setValue(v);
        QSettings().setValue(QStringLiteral("cutoutAssistant/brush"), v);
        m_view->update();
    });
    connect(m_sizeSlider, &QSlider::valueChanged, m_size, &QSpinBox::setValue);
    const int defaultSize = std::clamp(std::max(size.width(), size.height()) / 60, 6, 120);
    m_size->setValue(settings.value(QStringLiteral("cutoutAssistant/brush"), defaultSize).toInt());
    pl->addWidget(options);

    auto* look = new QGroupBox(tr("Vorschau"), panel);
    auto* ll = new QVBoxLayout(look);
    m_previewButton = new QPushButton(tr("Vorschau"), look);
    m_previewButton->setObjectName(QStringLiteral("cutoutPreview"));
    ll->addWidget(m_previewButton);
    m_showMarker = new QCheckBox(tr("Markierung anzeigen"), look);
    m_showMarker->setChecked(true);
    m_showFill = new QCheckBox(tr("Füllung anzeigen"), look);
    m_showFill->setChecked(true);
    m_showOriginal = new QCheckBox(tr("Originalbild anzeigen"), look);
    ll->addWidget(m_showMarker);
    ll->addWidget(m_showFill);
    ll->addWidget(m_showOriginal);
    auto* bgRow = new QHBoxLayout;
    bgRow->addWidget(new QLabel(tr("Hintergrund:"), look));
    m_background = new QComboBox(look);
    m_background->addItems({tr("Schachbrett"), tr("Weiss"), tr("Schwarz"), tr("Grau"), tr("Grün")});
    m_background->setCurrentIndex(settings.value(QStringLiteral("cutoutAssistant/background"), 0).toInt());
    bgRow->addWidget(m_background, 1);
    ll->addLayout(bgRow);
    pl->addWidget(look);
    connect(m_previewButton, &QPushButton::clicked, this, [this] { preview(); });
    for (QCheckBox* c : {m_showMarker, m_showFill})
        connect(c, &QCheckBox::toggled, this, [this] { m_view->rebuildOverlay(); });
    connect(m_showOriginal, &QCheckBox::toggled, this, [this] { m_view->rebuildOverlay(); });
    connect(m_background, &QComboBox::currentIndexChanged, this, [this](int i) {
        QSettings().setValue(QStringLiteral("cutoutAssistant/background"), i);
        m_view->update();
    });

    auto* edges = new QGroupBox(tr("Kanten"), panel);
    auto* ef = new QFormLayout(edges);
    m_decontaminate = new QCheckBox(tr("Farbsaum entfernen"), edges);
    m_decontaminate->setToolTip(tr("Halbdurchsichtige Kanten (Haare) bekommen die Farbe des Objekts statt des alten Hintergrunds"));
    m_decontaminate->setChecked(settings.value(QStringLiteral("cutoutAssistant/decontaminate"), true).toBool());
    ef->addRow(m_decontaminate);
    auto slider = [&](int min, int max, int value) {
        auto* s = new QSlider(Qt::Horizontal, edges);
        s->setRange(min, max);
        s->setValue(value);
        return s;
    };
    m_shift = slider(-10, 10, 0);
    m_shift->setToolTip(tr("Kante nach innen (−) oder aussen (+) verschieben, in Pixeln"));
    m_feather = slider(0, 20, 0);
    m_hardness = slider(0, 100, 0);
    ef->addRow(tr("Kante verschieben:"), m_shift);
    ef->addRow(tr("Weiche Kante:"), m_feather);
    ef->addRow(tr("Kantenhärte:"), m_hardness);
    pl->addWidget(edges);
    m_recompose = new QTimer(this);
    m_recompose->setSingleShot(true);
    m_recompose->setInterval(150);
    connect(m_recompose, &QTimer::timeout, this, &CutoutAssistant::startCompose);
    for (QSlider* s : {m_shift, m_feather, m_hardness})
        connect(s, &QSlider::valueChanged, m_recompose, qOverload<>(&QTimer::start));
    connect(m_decontaminate, &QCheckBox::toggled, this, [this](bool on) {
        QSettings().setValue(QStringLiteral("cutoutAssistant/decontaminate"), on);
        m_recompose->start();
    });

    auto* result = new QGroupBox(tr("Ergebnis"), panel);
    auto* rl = new QVBoxLayout(result);
    m_resultCutout = new QRadioButton(tr("Ausschnitt"), result);
    m_resultBoth = new QRadioButton(tr("Ausschnitt und Originalbild"), result);
    m_resultCutout->setChecked(true);
    rl->addWidget(m_resultCutout);
    rl->addWidget(m_resultBoth);
    pl->addWidget(result);

    auto* hints = new QGroupBox(tr("Hinweise"), panel);
    auto* hl = new QVBoxLayout(hints);
    m_hint = new QLabel(hints);
    m_hint->setWordWrap(true);
    m_hint->setMinimumHeight(90);
    m_hint->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    hl->addWidget(m_hint);
    pl->addWidget(hints, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, panel);
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Abbrechen"));
    m_ok = buttons->button(QDialogButtonBox::Ok);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    pl->addWidget(buttons);
    panel->setFixedWidth(300);
    outer->addWidget(panel);

    m_autoPreview = new QTimer(this);
    m_autoPreview->setSingleShot(true);
    m_autoPreview->setInterval(300);
    connect(m_autoPreview, &QTimer::timeout, this, [this] { preview(); });

    resize(1180, 780);
    setTool(Tool::Marker);
    updateButtons();
}

CutoutAssistant::~CutoutAssistant()
{
    if (m_cancel)
        m_cancel->store(true);
    for (QThread* t : std::as_const(m_threads)) {
        t->wait();
        delete t;
    }
}

bool CutoutAssistant::keepOriginal() const
{
    return m_resultBoth->isChecked();
}

void CutoutAssistant::setTool(Tool tool)
{
    m_tool = tool;
    for (QToolButton* b : std::as_const(m_toolButtons))
        b->setChecked(b->property("tool").toInt() == int(tool));
    m_view->setCursor(tool == Tool::Pan ? Qt::OpenHandCursor : tool == Tool::Fill ? Qt::PointingHandCursor : Qt::CrossCursor);
    updateHint();
    m_view->update();
}

void CutoutAssistant::setBrushSize(int pixels)
{
    m_size->setValue(pixels);
}

int CutoutAssistant::brushSize() const
{
    return m_size->value();
}

QImage CutoutAssistant::trimapImage() const
{
    return cutout::trimap(m_layers.marker, m_layers.fill, m_layers.keep, m_layers.remove);
}

void CutoutAssistant::pushUndo()
{
    // Ebenen teilen ihre Daten (implizit geteilt): kopiert wird nur, was der Strich ändert
    m_undo << snapshot();
    while (m_undo.size() > m_undoLimit)
        m_undo.removeFirst();
    m_redo.clear();
    updateButtons();
}

void CutoutAssistant::paintSegment(const QPointF& from, const QPointF& to)
{
    const double width = brushSize();
    auto draw = [&](QImage& layer, int value) {
        QPainter p(&layer);
        p.setRenderHint(QPainter::Antialiasing, false);
        p.setPen(QPen(QColor(value, value, value), width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        if (from == to) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(value, value, value));
            p.drawEllipse(from, width / 2, width / 2);
        } else {
            p.drawLine(from, to);
        }
    };
    switch (m_tool) {
    case Tool::Marker:
        draw(m_layers.marker, 255);
        break;
    case Tool::Eraser:
        draw(m_layers.marker, 0);
        draw(m_layers.fill, 0);
        draw(m_layers.keep, 0);
        draw(m_layers.remove, 0);
        break;
    case Tool::Keep:
        draw(m_layers.keep, 255);
        draw(m_layers.remove, 0);
        break;
    case Tool::Remove:
        draw(m_layers.remove, 255);
        draw(m_layers.keep, 0);
        break;
    default:
        return;
    }
    const QRectF dirty = QRectF(from, to).normalized().adjusted(-width, -width, width, width);
    m_view->updateOverlay(dirty.toAlignedRect());
}

void CutoutAssistant::stroke(const QList<QPointF>& points)
{
    if (points.isEmpty())
        return;
    pushUndo();
    paintSegment(points.first(), points.first());
    for (int i = 1; i < points.size(); ++i)
        paintSegment(points[i - 1], points[i]);
    strokeFinished();
}

void CutoutAssistant::strokeFinished()
{
    layersChanged({});
}

void CutoutAssistant::fillAt(const QPoint& imagePoint)
{
    const QImage region = cutout::floodFill(m_layers.marker, imagePoint);
    if (region.isNull()) {
        updateHint(tr("Klicken Sie neben die Markierung, nicht darauf."));
        return;
    }
    pushUndo();
    qint64 count = 0;
    for (int y = 0; y < region.height(); ++y) {
        const uchar* src = region.constScanLine(y);
        uchar* dst = m_layers.fill.scanLine(y);
        for (int x = 0; x < region.width(); ++x) {
            if (src[x]) {
                dst[x] = 255;
                ++count;
            }
        }
    }
    m_view->rebuildOverlay();
    const qint64 total = qint64(region.width()) * region.height();
    layersChanged({});
    // Fast alles gefüllt: Die Markierung ist wohl nicht geschlossen
    if (count > total * 85 / 100)
        updateHint(tr("Fast das ganze Bild ist gefüllt: Die Markierung ist vermutlich nicht geschlossen. "
                      "Rückgängig machen, die Lücke mit dem Marker schliessen und erneut füllen."));
}

void CutoutAssistant::layersChanged(const QRect&)
{
    m_alphaStale = true;
    updateButtons();
    updateHint();
    // Vorschau ist an: gleich nachrechnen
    if (!m_composite.isNull())
        m_autoPreview->start();
}

CutoutAssistant::Layers CutoutAssistant::snapshot() const
{
    Layers state = m_layers;
    if (!m_alphaStale && !m_busy && !m_alpha.isNull()) {
        state.alpha = m_alpha;
        state.composite = m_composite;
    }
    return state;
}

void CutoutAssistant::restore(Layers state)
{
    const QImage alpha = state.alpha, composite = state.composite;
    state.alpha = state.composite = QImage();
    m_layers = state;
    if (alpha.isNull()) {
        m_view->rebuildOverlay();
        layersChanged({});
        return;
    }
    // Ergebnis dieses Stands ist schon bekannt: laufende Rechnung verwerfen, sofort zeigen
    if (m_cancel)
        m_cancel->store(true);
    ++m_generation;
    m_busy = false;
    m_progress->hide();
    m_status->clear();
    m_alpha = alpha;
    m_alphaTrimap = trimapImage();
    m_composite = composite;
    m_alphaStale = false;
    m_view->rebuildOverlay();
    updateButtons();
    updateHint();
    m_recompose->start(); // Kanten-Einstellungen könnten sich seither geändert haben
}

void CutoutAssistant::undo()
{
    if (m_undo.isEmpty())
        return;
    m_redo << snapshot();
    restore(m_undo.takeLast());
}

void CutoutAssistant::redo()
{
    if (m_redo.isEmpty())
        return;
    m_undo << snapshot();
    restore(m_redo.takeLast());
}

void CutoutAssistant::reset()
{
    pushUndo();
    const QSize size = m_image.size();
    m_layers = {emptyLayer(size), emptyLayer(size), emptyLayer(size), emptyLayer(size)};
    m_composite = QImage();
    m_alpha = QImage();
    m_alphaTrimap = QImage();
    m_view->rebuildOverlay();
    layersChanged({});
}

void CutoutAssistant::updateButtons()
{
    m_undoButton->setEnabled(!m_undo.isEmpty());
    m_redoButton->setEnabled(!m_redo.isEmpty());
    const bool hasFill = anySet(m_layers.fill) || anySet(m_layers.keep);
    m_previewButton->setEnabled(hasFill && !m_busy);
    // OK auch während der Berechnung: result() wartet sie ab
    m_ok->setEnabled(hasFill);
}

void CutoutAssistant::updateHint(const QString& extra)
{
    QString text;
    switch (m_tool) {
    case Tool::Marker:
        text = tr("Ziehen Sie mit dem Marker eine geschlossene Linie über die Kante des Objekts: halb auf dem Objekt, "
                  "halb auf dem Hintergrund. Bei Haaren und Fell breiter malen, sodass alle feinen Strähnen im grünen "
                  "Band liegen. Der Bildrand schliesst die Linie ab.");
        break;
    case Tool::Eraser:
        text = tr("Radierer: entfernt Markierung, Füllung und Detailpinsel.");
        break;
    case Tool::Fill:
        text = tr("Klicken Sie innerhalb der Markierung, um das Objekt zu füllen. Danach auf «Vorschau».");
        break;
    case Tool::Keep:
        text = tr("Detail behalten: Malen Sie über Teile des Objekts, die in der Vorschau fehlen.");
        break;
    case Tool::Remove:
        text = tr("Detail entfernen: Malen Sie über Reste des Hintergrunds, die in der Vorschau stehen geblieben sind.");
        break;
    case Tool::Pan:
        text = tr("Ziehen verschiebt das Bild. Mausrad: Zoom.");
        break;
    }
    if (!m_composite.isNull())
        text += QStringLiteral("\n\n") + tr("Prüfen Sie die Kanten mit verschiedenen Hintergründen. Mit «Kante verschieben» "
                                            "lassen sich Säume entfernen.");
    if (!extra.isEmpty())
        text = extra;
    m_hint->setText(text);
}

QImage CutoutAssistant::refined() const
{
    if (m_alpha.isNull())
        return {};
    return cutout::refineAlpha(m_alpha, m_shift->value(), m_feather->value(), m_hardness->value());
}

void CutoutAssistant::preview(bool wait)
{
    if (!anySet(m_layers.fill) && !anySet(m_layers.keep))
        return;
    if (m_alphaStale)
        startSolve();
    else
        startCompose();
    if (wait) {
        QEventLoop loop;
        connect(this, &CutoutAssistant::previewReady, &loop, &QEventLoop::quit);
        if (m_busy)
            loop.exec();
    }
}

void CutoutAssistant::startSolve()
{
    if (m_cancel)
        m_cancel->store(true);
    m_cancel = std::make_shared<std::atomic_bool>(false);
    const int generation = ++m_generation;
    m_busy = true;
    m_alphaStale = false;
    updateButtons();
    m_status->setText(tr("Kanten werden berechnet …"));
    m_progress->setValue(0);
    m_progress->show();
    const QImage image = m_image;
    const QImage tri = trimapImage();
    // Nur ein kleiner Bereich geändert (Pinselstrich): nur dort neu rechnen
    QImage previous;
    QRect region;
    if (!m_alpha.isNull() && m_alphaTrimap.size() == tri.size()) {
        region = cutout::changedBounds(m_alphaTrimap, tri);
        const QRect band = cutout::unknownBounds(tri) | region;
        if (!region.isEmpty() && qint64(region.width()) * region.height() * 4 < qint64(band.width()) * band.height())
            previous = m_alpha;
    }
    const bool useModel = false; // gemessen: das Modell als Vorwissen verschlechtert die Kanten
    const QImage cachedPrior = m_prior;
    const bool decontaminate = m_decontaminate->isChecked();
    const int shift = m_shift->value(), feather = m_feather->value(), hardness = m_hardness->value();
    auto cancel = m_cancel;
    QPointer<CutoutAssistant> self(this);
    QThread* thread = QThread::create([=] {
        QImage prior = cachedPrior;
        if (useModel && prior.isNull()) {
            QMetaObject::invokeMethod(qApp, [self] {
                if (self)
                    self->m_status->setText(tr("KI-Modell schätzt das Objekt …"));
            });
            QString error;
            prior = matting::subjectMask(image, matting::defaultModel(), &error);
        }
        cutout::Progress progress;
        progress.cancel = cancel.get();
        progress.report = [self, generation](int percent) {
            QMetaObject::invokeMethod(qApp, [self, generation, percent] {
                if (self && self->m_generation == generation) {
                    self->m_progress->setValue(percent);
                    self->m_status->setText(tr("Kanten werden berechnet …"));
                }
            });
        };
        cutout::Settings settings;
        if (!useModel || prior.isNull())
            settings.priorWeight = 0;
        const QImage alpha = previous.isNull() ? cutout::solveAlpha(image, tri, useModel ? prior : QImage(), settings, progress)
                                               : cutout::updateAlpha(image, tri, previous, region, settings, progress);
        QImage composite;
        if (!alpha.isNull() && !cancel->load())
            composite = cutout::compose(image, cutout::refineAlpha(alpha, shift, feather, hardness), decontaminate)
                            .convertToFormat(QImage::Format_ARGB32_Premultiplied);
        QMetaObject::invokeMethod(qApp, [self, generation, alpha, composite, prior, useModel, tri] {
            if (!self || self->m_generation != generation)
                return;
            if (useModel && !prior.isNull())
                self->m_prior = prior;
            self->m_busy = false;
            self->m_progress->hide();
            self->m_status->clear();
            if (!alpha.isNull()) {
                const bool first = self->m_composite.isNull();
                self->m_alpha = alpha;
                self->m_alphaTrimap = tri;
                self->m_composite = composite;
                if (first)
                    self->m_view->rebuildOverlay();
            }
            self->updateButtons();
            self->updateHint();
            self->m_view->update();
            emit self->previewReady();
        });
    });
    m_threads << thread;
    thread->start();
}

void CutoutAssistant::startCompose()
{
    if (m_alpha.isNull() || m_busy)
        return;
    const int generation = ++m_generation;
    m_busy = true;
    updateButtons();
    const QImage image = m_image;
    const QImage alpha = refined();
    const bool decontaminate = m_decontaminate->isChecked();
    QPointer<CutoutAssistant> self(this);
    QThread* thread = QThread::create([=] {
        const QImage composite = cutout::compose(image, alpha, decontaminate).convertToFormat(QImage::Format_ARGB32_Premultiplied);
        QMetaObject::invokeMethod(qApp, [self, generation, composite] {
            if (!self || self->m_generation != generation)
                return;
            self->m_busy = false;
            self->m_composite = composite;
            self->updateButtons();
            self->m_view->update();
            emit self->previewReady();
        });
    });
    m_threads << thread;
    thread->start();
}

QImage CutoutAssistant::result()
{
    if (m_alphaStale || m_alpha.isNull() || m_busy) {
        preview(false);
        if (m_busy) {
            QEventLoop loop;
            connect(this, &CutoutAssistant::previewReady, &loop, &QEventLoop::quit);
            loop.exec();
        }
    }
    const QImage alpha = refined();
    if (alpha.isNull())
        return m_image;
    return cutout::compose(m_image, alpha, m_decontaminate->isChecked());
}

} // namespace vek
