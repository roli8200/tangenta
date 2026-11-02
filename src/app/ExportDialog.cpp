#include "ExportDialog.h"

#include "Canvas.h"
#include "Editor.h"
#include "Options.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

#include <cmath>

namespace vek {

ExportDialog::ExportDialog(Kind kind, const QString& suffix, Canvas& canvas, QWidget* parent)
    : QDialog(parent), m_kind(kind), m_suffix(suffix.toLower()), m_canvas(canvas)
{
    setWindowTitle(tr("Exportieren – %1").arg(suffix.toUpper()));
    auto* main = new QVBoxLayout(this);
    // Nur die markierten Objekte ausgeben (Rechteck um sie herum)
    m_selectionOnly = new QCheckBox(tr("Nur markierte Objekte"), this);
    const bool hasSelection = !canvas.editor().selection().isEmpty();
    m_selectionOnly->setEnabled(hasSelection);
    m_selectionOnly->setChecked(hasSelection);
    main->addWidget(m_selectionOnly);
    m_area = new QLabel(this);
    m_area->setEnabled(false);
    main->addWidget(m_area);

    if (kind == Kind::Pdf) {
        auto* pages = new QGroupBox(tr("Seiten"), this);
        auto* col = new QVBoxLayout(pages);
        auto* current = new QRadioButton(tr("Aktuelle Seite"), pages);
        m_allPages = new QRadioButton(tr("Alle Seiten (%1)").arg(canvas.editor().document().pageCount()), pages);
        m_allPages->setChecked(true);
        col->addWidget(m_allPages);
        col->addWidget(current);
        main->addWidget(pages);
        connect(m_selectionOnly, &QCheckBox::toggled, pages, [pages](bool on) { pages->setEnabled(!on); });
        pages->setEnabled(!m_selectionOnly->isChecked());
    }

    if (kind == Kind::Bitmap) {
        auto* form = new QFormLayout;
        main->addLayout(form);
        m_dpi = new QSpinBox(this);
        m_dpi->setRange(36, 2400);
        m_dpi->setSuffix(tr(" dpi"));
        m_dpi->setValue(Options::instance().integer(QStringLiteral("doc/page/resolution")));
        form->addRow(tr("Auflösung:"), m_dpi);
        m_width = new QSpinBox(this);
        m_height = new QSpinBox(this);
        for (QSpinBox* s : {m_width, m_height}) {
            s->setRange(1, 100000);
            s->setSuffix(tr(" Pixel"));
        }
        form->addRow(tr("Breite:"), m_width);
        form->addRow(tr("Höhe:"), m_height);
        m_colorMode = new QComboBox(this);
        m_colorMode->addItems({tr("RGB-Farbe (24 Bit)"), tr("Graustufen (8 Bit)"), tr("Schwarzweiss (1 Bit)")});
        form->addRow(tr("Farbmodus:"), m_colorMode);
        m_transparent = new QCheckBox(tr("Transparenter Hintergrund"), this);
        const bool alpha = m_suffix == QLatin1String("png") || m_suffix.startsWith(QLatin1String("tif")) || m_suffix == QLatin1String("webp")
                           || m_suffix == QLatin1String("gif") || m_suffix == QLatin1String("ico") || m_suffix == QLatin1String("psd");
        m_transparent->setEnabled(alpha);
        m_transparent->setChecked(alpha && hasSelection && m_suffix != QLatin1String("psd"));
        form->addRow(QString(), m_transparent);
        if (m_suffix == QLatin1String("jpg") || m_suffix == QLatin1String("jpeg") || m_suffix == QLatin1String("webp")) {
            m_quality = new QSpinBox(this);
            m_quality->setRange(1, 100);
            m_quality->setSuffix(QStringLiteral(" %"));
            m_quality->setValue(90);
            form->addRow(tr("Qualität:"), m_quality);
        }
        // Breite/Höhe ändern die Auflösung (Seitenverhältnis bleibt)
        connect(m_dpi, &QSpinBox::valueChanged, this, &ExportDialog::updateSize);
        auto fromPixels = [this](bool width) {
            if (m_updating)
                return;
            const QRectF area = output::exportArea(m_canvas, options());
            const double mm = width ? area.width() : area.height();
            const int px = width ? m_width->value() : m_height->value();
            if (mm > 0) {
                const QSignalBlocker block(m_dpi);
                m_dpi->setValue(std::clamp(int(std::lround(px / mm * 25.4)), m_dpi->minimum(), m_dpi->maximum()));
            }
            updateSize();
        };
        connect(m_width, &QSpinBox::editingFinished, this, [fromPixels] { fromPixels(true); });
        connect(m_height, &QSpinBox::editingFinished, this, [fromPixels] { fromPixels(false); });
    }
    if (kind == Kind::Vector && (m_suffix == QLatin1String("eps") || m_suffix == QLatin1String("dxf"))) {
        // Transparenz, Schatten und Unschärfe kann das Format nicht: diese Stellen werden gerastert
        auto* form = new QFormLayout;
        main->addLayout(form);
        m_flattenDpi = new QSpinBox(this);
        m_flattenDpi->setRange(72, 1200);
        m_flattenDpi->setSuffix(tr(" dpi"));
        m_flattenDpi->setValue(300);
        m_flattenDpi->setToolTip(tr("Durchsichtige Stellen (Transparenz, Schatten, Unschärfe, Linsen) werden mit dem Untergrund "
                                    "als Bild in genau ihrer Form eingesetzt; alles Deckende bleibt Vektor."));
        form->addRow(tr("Transparenzen rastern mit:"), m_flattenDpi);
    }
    connect(m_selectionOnly, &QCheckBox::toggled, this, &ExportDialog::updateSize);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Exportieren"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    main->addWidget(buttons);
    updateSize();
}

void ExportDialog::updateSize()
{
    m_updating = true;
    const QRectF area = output::exportArea(m_canvas, options());
    m_area->setText(tr("Bereich: %1 x %2 mm").arg(area.width(), 0, 'f', 1).arg(area.height(), 0, 'f', 1));
    if (m_dpi) {
        const double pxPerMm = m_dpi->value() / 25.4;
        m_width->setValue(std::max(1, int(std::lround(area.width() * pxPerMm))));
        m_height->setValue(std::max(1, int(std::lround(area.height() * pxPerMm))));
    }
    m_updating = false;
}

output::ExportOptions ExportDialog::options() const
{
    output::ExportOptions o;
    const Editor& ed = m_canvas.editor();
    o.page = ed.currentPageIndex();
    if (m_selectionOnly->isChecked())
        o.only = ed.selection();
    if (m_allPages && m_allPages->isChecked() && o.only.isEmpty())
        for (int i = 0; i < ed.document().pageCount(); ++i)
            o.pages << i;
    if (m_dpi)
        o.dpi = m_dpi->value();
    if (m_transparent)
        o.transparent = m_transparent->isEnabled() && m_transparent->isChecked();
    if (m_quality)
        o.quality = m_quality->value();
    if (m_flattenDpi)
        o.flattenDpi = m_flattenDpi->value();
    if (m_colorMode)
        o.colorMode = output::ExportOptions::ColorMode(m_colorMode->currentIndex());
    return o;
}

} // namespace vek
