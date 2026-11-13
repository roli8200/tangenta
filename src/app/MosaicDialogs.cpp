#include "MosaicDialogs.h"

#include "Canvas.h"
#include "ColorDialog.h"
#include "CurveObject.h"
#include "Editor.h"
#include "ExpressionSpinBox.h"
#include "GroupObject.h"
#include "ImageObject.h"
#include "ImagePool.h"
#include "Options.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

#include <cmath>

namespace vek {

namespace {

constexpr QSize kPreview(360, 270);

QDoubleSpinBox* spin(QWidget* parent, double min, double max, double value, const QString& suffix, int decimals = 1)
{
    auto* s = new ExpressionSpinBox(parent);
    s->setRange(min, max);
    s->setDecimals(decimals);
    s->setValue(value);
    s->setSuffix(suffix);
    return s;
}

QSpinBox* intSpin(QWidget* parent, int min, int max, int value)
{
    auto* s = new QSpinBox(parent);
    s->setRange(min, max);
    s->setValue(value);
    return s;
}

// Vorschaubild auf dem Schachbrett (durchsichtige Stellen sichtbar)
QPixmap previewPixmap(const QImage& image)
{
    QImage out(kPreview, QImage::Format_ARGB32_Premultiplied);
    out.fill(QColor(0xf0, 0xf0, 0xf0));
    if (!image.isNull()) {
        const QSize fit = image.size().scaled(kPreview, Qt::KeepAspectRatio);
        QPainter p(&out);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        const QRect r(QPoint((kPreview.width() - fit.width()) / 2, (kPreview.height() - fit.height()) / 2), fit);
        p.fillRect(r, Qt::white);
        p.drawImage(r, image);
    }
    return QPixmap::fromImage(out);
}

} // namespace

std::optional<MosaicSource> mosaicSource(Canvas& canvas, double dpi, bool imageOnly)
{
    Editor& ed = canvas.editor();
    QList<NodeId> ids;
    for (const DrawObject* o : ed.selectedObjects())
        ids << o->id();
    if (ids.isEmpty())
        return std::nullopt;
    const ImageObject* image = nullptr;
    for (NodeId id : ids)
        if (const auto* i = ed.document().findAs<ImageObject>(id); i && !image)
            image = i;
    MosaicSource src;
    if (image && (imageOnly || ids.size() == 1)) {
        src.image = image->displayImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
        // Grösse wie platziert (mm auf der Seite): die Rasterweite gilt in diesen mm, auch wenn die
        // Skalierung des Bilds noch in seiner Matrix steckt
        const QTransform t = image->transform();
        const double sx = std::hypot(t.m11(), t.m12()), sy = std::hypot(t.m21(), t.m22());
        if (sx > 1e-12 && sy > 1e-12) {
            src.size = QSizeF(image->size().width() * sx, image->size().height() * sy);
            src.toPage = QTransform::fromScale(1.0 / sx, 1.0 / sy) * t;
        } else {
            src.size = image->size();
            src.toPage = t;
        }
        src.id = image->id();
        src.objects = {image->id()};
        return src.image.isNull() ? std::nullopt : std::optional<MosaicSource>(src);
    }
    // Auswahl als Bild rechnen (wie In Bitmap konvertieren)
    QRectF bounds;
    for (NodeId id : ids) {
        const auto* o = ed.document().findAs<DrawObject>(id);
        if (!o)
            continue;
        const double pad = o->style().stroke ? o->style().strokeWidth / 2.0 : 0.0;
        const QRectF r = o->boundingRect().adjusted(-pad, -pad, pad, pad);
        bounds = bounds.isNull() ? r : bounds.united(r);
    }
    if (bounds.isEmpty())
        return std::nullopt;
    const double pxPerMm = dpi / 25.4;
    const QSize size(std::clamp(int(std::ceil(bounds.width() * pxPerMm)), 1, 6000), std::clamp(int(std::ceil(bounds.height() * pxPerMm)), 1, 6000));
    QImage pixels(size, QImage::Format_ARGB32_Premultiplied);
    pixels.fill(Qt::transparent);
    {
        QPainter p(&pixels);
        canvas.setRenderFilter(ids);
        canvas.renderPage(p, ed.currentPageIndex(), size.width() / bounds.width(), -bounds.topLeft() * (size.width() / bounds.width()), false);
        canvas.setRenderFilter({});
    }
    src.image = pixels;
    src.size = bounds.size();
    src.toPage = QTransform::fromTranslate(bounds.left(), bounds.top());
    src.id = ids.last();
    src.objects = ids;
    return src;
}

// --- Fotomosaik -----------------------------------------------------------------------------

PhotoMosaicDialog::PhotoMosaicDialog(Canvas& canvas, MosaicSource source, QWidget* parent)
    : QDialog(parent), m_canvas(canvas), m_source(std::move(source))
{
    setWindowTitle(tr("Fotomosaik"));
    const Options& opt = Options::instance();
    auto* layout = new QHBoxLayout(this);
    auto* left = new QVBoxLayout;
    layout->addLayout(left);
    auto* form = new QFormLayout;
    left->addLayout(form);
    // Bildbibliothek
    auto* folderRow = new QHBoxLayout;
    m_folder = new QLineEdit(this);
    m_folder->setPlaceholderText(tr("Ordner mit Bildern"));
    m_folder->setMinimumWidth(220);
    auto* browse = new QPushButton(tr("Durchsuchen …"), this);
    folderRow->addWidget(m_folder, 1);
    folderRow->addWidget(browse);
    form->addRow(tr("Bildbibliothek:"), folderRow);
    m_found = new QLabel(this);
    form->addRow(QString(), m_found);
    m_columns = intSpin(this, 2, 300, opt.integer(QStringLiteral("ws/tools/photoMosaic/columns")));
    form->addRow(tr("Spalten:"), m_columns);
    m_rows = intSpin(this, 0, 300, 0);
    m_rows->setSpecialValueText(tr("Automatisch"));
    m_rows->setToolTip(tr("Automatisch: quadratische Zellen aus dem Seitenverhältnis"));
    form->addRow(tr("Zeilen:"), m_rows);
    m_tint = spin(this, 0, 100, opt.number(QStringLiteral("ws/tools/photoMosaic/tint")), QStringLiteral(" %"), 0);
    m_tint->setToolTip(tr("Kacheln zur Farbe der Vorlage hin verschieben"));
    form->addRow(tr("Farbangleichung:"), m_tint);
    m_repeats = intSpin(this, 0, 10000, opt.integer(QStringLiteral("ws/tools/photoMosaic/maxRepeats")));
    m_repeats->setSpecialValueText(tr("Beliebig"));
    m_repeats->setToolTip(tr("Wie oft dasselbe Bild höchstens vorkommt"));
    form->addRow(tr("Wiederholungen:"), m_repeats);
    m_neighbors = new QCheckBox(tr("Gleiche Bilder nicht nebeneinander"), this);
    m_neighbors->setChecked(true);
    form->addRow(QString(), m_neighbors);
    m_output = new QComboBox(this);
    m_output->addItems({tr("Gruppe aus Bildern"), tr("Ein Bild")});
    m_output->setCurrentIndex(std::clamp(opt.integer(QStringLiteral("ws/tools/photoMosaic/output")), 0, 1));
    form->addRow(tr("Ausgabe:"), m_output);
    m_tilePixels = intSpin(this, 8, 1024, opt.integer(QStringLiteral("ws/tools/photoMosaic/tilePixels")));
    m_tilePixels->setSuffix(tr(" px"));
    m_tilePixels->setToolTip(tr("Auflösung einer Kachel (Breite in Pixeln)"));
    form->addRow(tr("Kachelauflösung:"), m_tilePixels);
    m_replace = new QCheckBox(tr("Original ersetzen"), this);
    form->addRow(QString(), m_replace);
    left->addStretch();
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_ok = buttons->button(QDialogButtonBox::Ok);
    left->addWidget(buttons);
    m_preview = new QLabel(this);
    m_preview->setFixedSize(kPreview);
    m_preview->setFrameShape(QFrame::StyledPanel);
    layout->addWidget(m_preview, 0, Qt::AlignTop);

    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    m_timer->setInterval(120);
    connect(m_timer, &QTimer::timeout, this, &PhotoMosaicDialog::updatePreview);
    connect(browse, &QPushButton::clicked, this, [this] {
        const QString dir = QFileDialog::getExistingDirectory(this, tr("Ordner mit Bildern"), m_folder->text());
        if (!dir.isEmpty())
            setFolder(dir);
    });
    connect(m_folder, &QLineEdit::editingFinished, this, [this] { setFolder(m_folder->text()); });
    for (QSpinBox* s : {m_columns, m_rows, m_repeats})
        connect(s, &QSpinBox::valueChanged, this, &PhotoMosaicDialog::schedulePreview);
    connect(m_tint, &QDoubleSpinBox::valueChanged, this, &PhotoMosaicDialog::schedulePreview);
    connect(m_neighbors, &QCheckBox::toggled, this, &PhotoMosaicDialog::schedulePreview);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    const QString last = opt.text(QStringLiteral("ws/tools/photoMosaic/folder"));
    if (!last.isEmpty())
        setFolder(last);
    else
        updatePreview();
}

PhotoMosaicDialog::~PhotoMosaicDialog() = default;

mosaics::PhotoSettings PhotoMosaicDialog::settings() const
{
    mosaics::PhotoSettings s;
    s.columns = m_columns->value();
    s.rows = m_rows->value();
    s.tint = m_tint->value();
    s.maxRepeats = m_repeats->value();
    s.avoidNeighbors = m_neighbors->isChecked();
    return s;
}

void PhotoMosaicDialog::setFolder(const QString& folder)
{
    if (m_folder->text() != folder)
        m_folder->setText(folder);
    if (folder == m_loaded)
        return;
    m_loaded = folder;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    m_tiles = folder.isEmpty() ? QList<mosaics::Tile>() : mosaics::loadTiles(folder, 96);
    QApplication::restoreOverrideCursor();
    m_found->setText(tr("%n Bild(er) gefunden", nullptr, int(m_tiles.size())));
    updatePreview();
}

QList<mosaics::Cell> PhotoMosaicDialog::cells() const
{
    return mosaics::matchTiles(m_source.image, m_tiles, settings());
}

void PhotoMosaicDialog::schedulePreview()
{
    m_timer->start();
}

void PhotoMosaicDialog::updatePreview()
{
    m_ok->setEnabled(!m_tiles.isEmpty());
    if (m_tiles.isEmpty()) {
        m_preview->setPixmap(previewPixmap(m_source.image));
        return;
    }
    const mosaics::PhotoSettings s = settings();
    const QList<mosaics::Cell> c = mosaics::matchTiles(m_source.image, m_tiles, s);
    const int cellPx = std::max(2, kPreview.width() / std::max(1, s.columns));
    m_preview->setPixmap(previewPixmap(mosaics::renderMosaic(m_source.image, m_tiles, c, s, cellPx)));
}

void PhotoMosaicDialog::saveSettings() const
{
    Options::instance().apply({{QStringLiteral("ws/tools/photoMosaic/folder"), m_folder->text()},
                               {QStringLiteral("ws/tools/photoMosaic/columns"), m_columns->value()},
                               {QStringLiteral("ws/tools/photoMosaic/tint"), m_tint->value()},
                               {QStringLiteral("ws/tools/photoMosaic/maxRepeats"), m_repeats->value()},
                               {QStringLiteral("ws/tools/photoMosaic/output"), m_output->currentIndex()},
                               {QStringLiteral("ws/tools/photoMosaic/tilePixels"), m_tilePixels->value()}});
}

NodeId PhotoMosaicDialog::apply()
{
    if (m_tiles.isEmpty() || m_source.image.isNull())
        return InvalidNodeId;
    saveSettings();
    const mosaics::PhotoSettings s = settings();
    const QList<mosaics::Cell> cells = mosaics::matchTiles(m_source.image, m_tiles, s);
    int cols = 0, rows = 0;
    for (const mosaics::Cell& c : cells) {
        cols = std::max(cols, c.column + 1);
        rows = std::max(rows, c.row + 1);
    }
    if (cols == 0)
        return InvalidNodeId;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    std::unique_ptr<DrawObject> result;
    const int cellPx = m_tilePixels->value();
    if (m_output->currentIndex() == 1) {
        // Ein Bild in der Grösse der Vorlage
        const QImage pixels = mosaics::renderMosaic(m_source.image, m_tiles, cells, s, cellPx);
        auto image = std::make_unique<ImageObject>(imagepool::addImage(pixels), m_source.size);
        image->setTransform(m_source.toPage);
        image->setName(tr("Fotomosaik"));
        result = std::move(image);
    } else {
        // Je Zelle ein Bild, zusammen in einer Gruppe
        auto group = std::make_unique<GroupObject>();
        group->setName(tr("Fotomosaik"));
        const double cellAspect = (m_source.size.height() / rows) / (m_source.size.width() / cols);
        const QSize px(cellPx, std::max(1, int(std::lround(cellPx * cellAspect))));
        for (const mosaics::Cell& c : cells) {
            if (c.tile < 0)
                continue;
            const QImage tile = mosaics::tileImage(m_tiles[c.tile], px, c.target, s.tint);
            const QSizeF sizeMm(c.rect.width() * m_source.size.width(), c.rect.height() * m_source.size.height());
            auto image = std::make_unique<ImageObject>(imagepool::addImage(tile), sizeMm);
            image->setTransform(QTransform::fromTranslate(c.rect.left() * m_source.size.width(), c.rect.top() * m_source.size.height())
                                * m_source.toPage);
            attachDetachedChild(*group, std::move(image));
        }
        if (group->childCount() == 0)
            group.reset();
        result = std::move(group);
    }
    const NodeId id = result ? m_canvas.editor().insertGenerated(std::move(result), m_source.id,
                                                                 m_replace->isChecked() ? m_source.objects : QList<NodeId>{},
                                                                 tr("Fotomosaik"))
                             : InvalidNodeId;
    QApplication::restoreOverrideCursor();
    return id;
}

// --- Punktraster ----------------------------------------------------------------------------

PointillizeDialog::PointillizeDialog(Canvas& canvas, MosaicSource source, const QPainterPath& customShape, QWidget* parent)
    : QDialog(parent), m_canvas(canvas), m_source(std::move(source)), m_custom(customShape)
{
    setWindowTitle(tr("Punktraster"));
    const Options& opt = Options::instance();
    auto* layout = new QHBoxLayout(this);
    auto* left = new QVBoxLayout;
    layout->addLayout(left);
    auto* form = new QFormLayout;
    left->addLayout(form);
    m_shape = new QComboBox(this);
    m_shape->addItem(tr("Kreis"), int(mosaics::PointSettings::Shape::Circle));
    m_shape->addItem(tr("Quadrat"), int(mosaics::PointSettings::Shape::Square));
    m_shape->addItem(tr("Eigene Form"), int(mosaics::PointSettings::Shape::Custom));
    m_shape->setToolTip(tr("Eigene Form: zusammen mit dem Bild ein zweites Objekt auswählen; seine Form wird verwendet"));
    m_shape->setCurrentIndex(m_custom.isEmpty() ? std::clamp(opt.integer(QStringLiteral("ws/tools/pointillize/shape")), 0, 1) : 2);
    if (m_custom.isEmpty())
        m_shape->setItemData(2, 0, Qt::UserRole - 1); // nicht wählbar
    form->addRow(tr("Form:"), m_shape);
    m_mode = new QComboBox(this);
    m_mode->addItems({tr("Farbe"), tr("Grösse nach Helligkeit"), tr("Farbe und Grösse")});
    m_mode->setCurrentIndex(std::clamp(opt.integer(QStringLiteral("ws/tools/pointillize/mode")), 0, 2));
    form->addRow(tr("Verfahren:"), m_mode);
    m_cell = spin(this, 0.1, 500, opt.number(QStringLiteral("ws/tools/pointillize/cell")), QStringLiteral(" mm"), 2);
    m_cell->setToolTip(tr("Abstand der Rasterpunkte (Dichte)"));
    form->addRow(tr("Rasterweite:"), m_cell);
    m_scale = spin(this, 1, 400, opt.number(QStringLiteral("ws/tools/pointillize/scale")), QStringLiteral(" %"), 0);
    m_scale->setToolTip(tr("Grösse der Formen in % der Rasterweite"));
    form->addRow(tr("Skalierung:"), m_scale);
    m_minSize = spin(this, 0, 100, 5, QStringLiteral(" %"), 0);
    m_minSize->setToolTip(tr("Kleinste Form bei Grössenmodulation"));
    form->addRow(tr("Kleinste Grösse:"), m_minSize);
    m_angle = spin(this, -360, 360, opt.number(QStringLiteral("ws/tools/pointillize/angle")), QStringLiteral(" °"), 1);
    form->addRow(tr("Rasterwinkel:"), m_angle);
    m_rotation = spin(this, -360, 360, opt.number(QStringLiteral("ws/tools/pointillize/rotation")), QStringLiteral(" °"), 1);
    form->addRow(tr("Drehung der Formen:"), m_rotation);
    m_jitter = spin(this, 0, 100, opt.number(QStringLiteral("ws/tools/pointillize/jitter")), QStringLiteral(" %"), 0);
    m_jitter->setToolTip(tr("Zufälliger Versatz der Punkte in % der Rasterweite"));
    form->addRow(tr("Zufallsversatz:"), m_jitter);
    m_colors = intSpin(this, 0, 256, opt.integer(QStringLiteral("ws/tools/pointillize/colors")));
    m_colors->setSpecialValueText(tr("Alle"));
    form->addRow(tr("Farben begrenzen:"), m_colors);
    m_colorButton = new QPushButton(this);
    form->addRow(tr("Farbe:"), m_colorButton);
    m_invert = new QCheckBox(tr("Helle Stellen gross"), this);
    form->addRow(QString(), m_invert);
    m_grouping = new QComboBox(this);
    m_grouping->addItems({tr("Einzelne Objekte"), tr("Je Farbe eine Gruppe"), tr("Je Farbe ein Objekt")});
    m_grouping->setCurrentIndex(std::clamp(opt.integer(QStringLiteral("ws/tools/pointillize/grouping")), 0, 2));
    form->addRow(tr("Ergebnis:"), m_grouping);
    m_keep = new QCheckBox(tr("Original behalten"), this);
    m_keep->setChecked(true);
    form->addRow(QString(), m_keep);
    m_count = new QLabel(this);
    form->addRow(QString(), m_count);
    left->addStretch();
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    left->addWidget(buttons);
    m_preview = new QLabel(this);
    m_preview->setFixedSize(kPreview);
    m_preview->setFrameShape(QFrame::StyledPanel);
    layout->addWidget(m_preview, 0, Qt::AlignTop);

    auto showColor = [this] {
        m_colorButton->setText(m_color.toHex());
        m_colorButton->setStyleSheet(QStringLiteral("background:%1;color:%2").arg(m_color.toQColor().name(),
                                                                                  m_color.toQColor().lightness() < 128 ? QStringLiteral("white") : QStringLiteral("black")));
    };
    showColor();
    connect(m_colorButton, &QPushButton::clicked, this, [this, showColor] {
        if (const std::optional<Color> c = ColorDialog::getColor(this, tr("Farbe der Formen"), m_color)) {
            m_color = *c;
            showColor();
            schedulePreview();
        }
    });
    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    m_timer->setInterval(120);
    connect(m_timer, &QTimer::timeout, this, &PointillizeDialog::updatePreview);
    for (QComboBox* c : {m_shape, m_mode})
        connect(c, &QComboBox::currentIndexChanged, this, &PointillizeDialog::schedulePreview);
    for (QDoubleSpinBox* s : {m_cell, m_scale, m_minSize, m_angle, m_rotation, m_jitter})
        connect(s, &QDoubleSpinBox::valueChanged, this, &PointillizeDialog::schedulePreview);
    connect(m_colors, &QSpinBox::valueChanged, this, &PointillizeDialog::schedulePreview);
    connect(m_invert, &QCheckBox::toggled, this, &PointillizeDialog::schedulePreview);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    updatePreview();
}

PointillizeDialog::~PointillizeDialog() = default;

mosaics::PointSettings PointillizeDialog::settings() const
{
    mosaics::PointSettings s;
    s.shape = mosaics::PointSettings::Shape(m_shape->currentData().toInt());
    if (s.shape == mosaics::PointSettings::Shape::Custom && m_custom.isEmpty())
        s.shape = mosaics::PointSettings::Shape::Circle;
    s.mode = mosaics::PointSettings::Mode(m_mode->currentIndex());
    s.cell = m_cell->value();
    s.scale = m_scale->value();
    s.minSize = m_minSize->value();
    s.angle = m_angle->value();
    s.rotation = m_rotation->value();
    s.jitter = m_jitter->value();
    s.limitColors = m_colors->value();
    s.invert = m_invert->isChecked();
    s.color = m_color;
    return s;
}

QList<mosaics::Dot> PointillizeDialog::dots() const
{
    return mosaics::pointillize(m_source.image, m_source.size, settings());
}

void PointillizeDialog::schedulePreview()
{
    m_timer->start();
}

void PointillizeDialog::updatePreview()
{
    const mosaics::PointSettings s = settings();
    const bool sizeOnly = s.mode == mosaics::PointSettings::Mode::Size;
    m_colorButton->setEnabled(sizeOnly);
    m_colors->setEnabled(!sizeOnly);
    m_minSize->setEnabled(s.mode != mosaics::PointSettings::Mode::Color);
    m_invert->setEnabled(s.mode != mosaics::PointSettings::Mode::Color);
    const QList<mosaics::Dot> list = dots();
    m_count->setText(tr("%n Form(en)", nullptr, int(list.size())));
    // Vorschau: Formen auf Weiss, im Seitenverhältnis der Vorlage
    const QSizeF fit = m_source.size.scaled(QSizeF(kPreview), Qt::KeepAspectRatio);
    QImage img(fit.toSize().expandedTo(QSize(1, 1)), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::white);
    {
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        const double k = fit.width() / std::max(1e-9, m_source.size.width());
        p.scale(k, k);
        for (const mosaics::Dot& d : list)
            p.fillPath(mosaics::dotPath(d, s.shape, m_custom), d.color.toQColor());
    }
    m_preview->setPixmap(previewPixmap(img));
}

void PointillizeDialog::saveSettings() const
{
    Options::instance().apply({{QStringLiteral("ws/tools/pointillize/shape"), std::min(m_shape->currentIndex(), 1)},
                               {QStringLiteral("ws/tools/pointillize/mode"), m_mode->currentIndex()},
                               {QStringLiteral("ws/tools/pointillize/cell"), m_cell->value()},
                               {QStringLiteral("ws/tools/pointillize/scale"), m_scale->value()},
                               {QStringLiteral("ws/tools/pointillize/angle"), m_angle->value()},
                               {QStringLiteral("ws/tools/pointillize/rotation"), m_rotation->value()},
                               {QStringLiteral("ws/tools/pointillize/jitter"), m_jitter->value()},
                               {QStringLiteral("ws/tools/pointillize/colors"), m_colors->value()},
                               {QStringLiteral("ws/tools/pointillize/grouping"), m_grouping->currentIndex()}});
}

NodeId PointillizeDialog::apply()
{
    saveSettings();
    const mosaics::PointSettings s = settings();
    const QList<mosaics::Dot> list = dots();
    if (list.isEmpty())
        return InvalidNodeId;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    auto styleFor = [](const Color& c) {
        Style st;
        st.fill = Fill::solid(c);
        st.stroke.reset();
        return st;
    };
    auto curve = [&](const QPainterPath& local, const Color& c) {
        QPainterPath page = m_source.toPage.map(local);
        page.setFillRule(Qt::WindingFill);
        auto o = CurveObject::fromPath(page);
        o->setStyle(styleFor(c));
        return o;
    };
    auto group = std::make_unique<GroupObject>();
    group->setName(tr("Punktraster"));
    const Grouping grouping = Grouping(m_grouping->currentIndex());
    if (grouping == Grouping::Single) {
        for (const mosaics::Dot& d : list)
            attachDetachedChild(*group, curve(mosaics::dotPath(d, s.shape, m_custom), d.color));
    } else {
        // Nach Farbe ordnen, in der Reihenfolge des ersten Auftretens
        QList<Color> order;
        QMap<QString, QList<const mosaics::Dot*>> byColor;
        for (const mosaics::Dot& d : list) {
            const QString hex = d.color.toHex();
            if (!byColor.contains(hex))
                order << d.color;
            byColor[hex] << &d;
        }
        for (const Color& c : std::as_const(order)) {
            const QList<const mosaics::Dot*>& dots = byColor[c.toHex()];
            if (grouping == Grouping::CombinedPerColor) {
                QPainterPath all;
                for (const mosaics::Dot* d : dots)
                    all.addPath(mosaics::dotPath(*d, s.shape, m_custom));
                auto o = curve(all, c);
                o->setName(c.toHex());
                attachDetachedChild(*group, std::move(o));
            } else {
                auto sub = std::make_unique<GroupObject>();
                sub->setName(c.toHex());
                for (const mosaics::Dot* d : dots)
                    attachDetachedChild(*sub, curve(mosaics::dotPath(*d, s.shape, m_custom), c));
                attachDetachedChild(*group, std::move(sub));
            }
        }
    }
    const NodeId id = m_canvas.editor().insertGenerated(std::move(group), m_source.id,
                                                        m_keep->isChecked() ? QList<NodeId>{} : m_source.objects, tr("Punktraster"));
    QApplication::restoreOverrideCursor();
    return id;
}

} // namespace vek
