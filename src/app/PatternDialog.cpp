#include "PatternDialog.h"

#include "ColorDialog.h"
#include "ImagePool.h"

#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QRandomGenerator>
#include <QRadioButton>
#include <QVBoxLayout>

namespace vek {

PatternFillDialog::PatternFillDialog(const PatternFill& pattern, QWidget* parent)
    : QDialog(parent), m_pattern(pattern)
{
    setWindowTitle(tr("Muster- und Texturfüllung"));
    auto* main = new QVBoxLayout(this);
    auto* kinds = new QHBoxLayout;
    m_kinds = new QButtonGroup(this);
    for (const auto& [kind, text] : {std::pair{PatternFill::Kind::TwoColor, tr("Zweifarbig")}, std::pair{PatternFill::Kind::Bitmap, tr("Bitmap")},
                                     std::pair{PatternFill::Kind::Texture, tr("Textur")}}) {
        auto* b = new QRadioButton(text, this);
        m_kinds->addButton(b, int(kind));
        kinds->addWidget(b);
    }
    kinds->addStretch();
    main->addLayout(kinds);

    auto* row = new QHBoxLayout;
    main->addLayout(row);
    m_list = new QListWidget(this);
    m_list->setViewMode(QListView::IconMode);
    m_list->setIconSize(QSize(40, 40));
    m_list->setGridSize(QSize(52, 52));
    m_list->setResizeMode(QListView::Adjust);
    m_list->setMovement(QListView::Static);
    m_list->setMinimumSize(250, 170);
    row->addWidget(m_list, 1);
    auto* side = new QVBoxLayout;
    row->addLayout(side);
    m_preview = new QLabel(this);
    m_preview->setFixedSize(120, 120);
    m_preview->setFrameShape(QFrame::Box);
    side->addWidget(m_preview);
    auto* form = new QFormLayout;
    side->addLayout(form);
    m_fg = new ColorButton(tr("Vordergrund"), this);
    m_bg = new ColorButton(tr("Hintergrund"), this);
    m_fgLabel = new QLabel(this);
    m_bgLabel = new QLabel(this);
    form->addRow(m_fgLabel, m_fg);
    form->addRow(m_bgLabel, m_bg);
    m_tile = new QDoubleSpinBox(this);
    m_tile->setRange(0.5, 1000);
    m_tile->setDecimals(1);
    m_tile->setSuffix(QStringLiteral(" mm"));
    m_tile->setValue(pattern.tile);
    form->addRow(tr("Kachel:"), m_tile);
    m_load = new QPushButton(tr("Bild laden …"), this);
    side->addWidget(m_load);
    m_shuffle = new QPushButton(tr("Neu würfeln"), this);
    side->addWidget(m_shuffle);
    side->addStretch();
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    main->addWidget(buttons);

    m_fg->setColor(pattern.fg);
    m_bg->setColor(pattern.bg);
    connect(m_kinds, &QButtonGroup::idClicked, this, [this](int id) { setKind(PatternFill::Kind(id)); });
    connect(m_list, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0) {
            m_pattern.index = row;
            refresh();
        }
    });
    connect(m_fg, &ColorButton::colorChanged, this, [this](const Color& c) {
        m_pattern.fg = c;
        fillList();
        refresh();
    });
    connect(m_bg, &ColorButton::colorChanged, this, [this](const Color& c) {
        m_pattern.bg = c;
        fillList();
        refresh();
    });
    connect(m_tile, &QDoubleSpinBox::valueChanged, this, [this](double v) {
        m_pattern.tile = v;
        refresh();
    });
    connect(m_shuffle, &QPushButton::clicked, this, [this] {
        m_pattern.seed = QRandomGenerator::global()->bounded(1u, 1000000u);
        fillList();
        refresh();
    });
    connect(m_load, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, tr("Bitmap-Muster laden"), QString(),
                                                          tr("Bilder (*.png *.jpg *.jpeg *.bmp *.gif *.tif *.tiff)"));
        if (path.isEmpty())
            return;
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly))
            return;
        const QString key = imagepool::add(f.readAll());
        if (key.isEmpty())
            return;
        m_pattern.imageKey = key;
        refresh();
    });
    setKind(pattern.kind);
}

void PatternFillDialog::setKind(PatternFill::Kind kind)
{
    if (m_pattern.kind != kind) {
        m_pattern.kind = kind;
        m_pattern.index = 0;
        // Texturen: helle Farbwahl als Vorgabe
        if (kind == PatternFill::Kind::Texture && m_pattern.fg == Color(0, 0, 0) && m_pattern.bg == Color(255, 255, 255)) {
            m_pattern.fg = Color(0x30, 0x50, 0x90);
            m_pattern.bg = Color(0xf0, 0xf4, 0xff);
            m_fg->setColor(m_pattern.fg);
            m_bg->setColor(m_pattern.bg);
        }
    }
    if (QAbstractButton* b = m_kinds->button(int(kind)))
        b->setChecked(true);
    const bool bitmap = kind == PatternFill::Kind::Bitmap;
    m_list->setVisible(!bitmap);
    m_fg->setVisible(!bitmap);
    m_bg->setVisible(!bitmap);
    m_fgLabel->setVisible(!bitmap);
    m_bgLabel->setVisible(!bitmap);
    m_load->setVisible(bitmap);
    m_shuffle->setVisible(kind == PatternFill::Kind::Texture);
    m_fgLabel->setText(kind == PatternFill::Kind::Texture ? tr("Farbe 1:") : tr("Vordergrund:"));
    m_bgLabel->setText(kind == PatternFill::Kind::Texture ? tr("Farbe 2:") : tr("Hintergrund:"));
    fillList();
    refresh();
}

void PatternFillDialog::fillList()
{
    const QSignalBlocker block(m_list);
    m_list->clear();
    if (m_pattern.kind == PatternFill::Kind::Bitmap)
        return;
    const bool texture = m_pattern.kind == PatternFill::Kind::Texture;
    const int n = texture ? patterns::textureCount() : patterns::twoColorCount();
    for (int i = 0; i < n; ++i) {
        PatternFill p = m_pattern;
        p.index = i;
        auto* item = new QListWidgetItem(QIcon(QPixmap::fromImage(p.image().scaled(40, 40, Qt::IgnoreAspectRatio, Qt::SmoothTransformation))),
                                         QString(), m_list);
        item->setToolTip(texture ? patterns::textureName(i) : patterns::twoColorName(i));
    }
    m_list->setCurrentRow(std::clamp(m_pattern.index, 0, n - 1));
}

void PatternFillDialog::refresh()
{
    // Vorschau: drei Kacheln je Seite
    QImage preview(120, 120, QImage::Format_ARGB32_Premultiplied);
    preview.fill(Qt::white);
    const QImage tile = m_pattern.image();
    if (!tile.isNull()) {
        QPainter p(&preview);
        QBrush b(tile);
        b.setTransform(QTransform::fromScale(40.0 / tile.width(), 40.0 / tile.height()));
        p.fillRect(preview.rect(), b);
    }
    m_preview->setPixmap(QPixmap::fromImage(preview));
}

} // namespace vek
