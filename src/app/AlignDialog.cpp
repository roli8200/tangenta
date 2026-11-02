#include "AlignDialog.h"

#include "Editor.h"
#include "Icons.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QTabWidget>
#include <QVBoxLayout>

namespace vek {

namespace {

// Kästchen mit Symbol darüber (waagrecht) bzw. davor (senkrecht) wie im klassischen Dialog
QCheckBox* box(QGridLayout* grid, int row, int col, const QString& iconName, const QString& text, bool iconAbove)
{
    auto* parent = grid->parentWidget();
    auto* pic = new QLabel(parent);
    pic->setPixmap(icon(iconName).pixmap(20));
    auto* check = new QCheckBox(text, parent);
    if (iconAbove) {
        grid->addWidget(pic, row, col, Qt::AlignHCenter);
        grid->addWidget(check, row + 1, col);
    } else {
        grid->addWidget(pic, row, col);
        grid->addWidget(check, row, col + 1);
    }
    return check;
}

} // namespace

AlignDialog::AlignDialog(Editor& editor, QWidget* parent) : QDialog(parent), m_editor(editor)
{
    setWindowTitle(tr("Ausrichten und verteilen"));
    setModal(false);
    auto* layout = new QVBoxLayout(this);
    m_tabs = new QTabWidget(this);
    layout->addWidget(m_tabs);

    // --- Ausrichten
    auto* alignPage = new QWidget(m_tabs);
    auto* ag = new QGridLayout(alignPage);
    m_left = box(ag, 0, 2, QStringLiteral("obj-align-left"), tr("Links"), true);
    m_hCenter = box(ag, 0, 3, QStringLiteral("obj-align-hcenter"), tr("Zentriert"), true);
    m_right = box(ag, 0, 4, QStringLiteral("obj-align-right"), tr("Rechts"), true);
    m_top = box(ag, 2, 0, QStringLiteral("obj-align-top"), tr("Oben"), false);
    m_vCenter = box(ag, 3, 0, QStringLiteral("obj-align-vcenter"), tr("Mitte"), false);
    m_bottom = box(ag, 4, 0, QStringLiteral("obj-align-bottom"), tr("Unten"), false);
    auto* targetBox = new QVBoxLayout;
    targetBox->addWidget(new QLabel(tr("Objekte ausrichten an:"), alignPage));
    m_target = new QComboBox(alignPage);
    m_target->addItems({tr("Aktive Objekte"), tr("Seitenrand"), tr("Seitenmitte"), tr("Gitter"), tr("Angegebener Punkt")});
    targetBox->addWidget(m_target);
    auto* point = new QHBoxLayout;
    auto spin = [alignPage](const QString& prefix) {
        auto* s = new QDoubleSpinBox(alignPage);
        s->setRange(-100000, 100000);
        s->setDecimals(3);
        s->setPrefix(prefix);
        s->setSuffix(QStringLiteral(" mm"));
        return s;
    };
    m_pointX = spin(QStringLiteral("x: "));
    m_pointY = spin(QStringLiteral("y: "));
    point->addWidget(m_pointX);
    point->addWidget(m_pointY);
    targetBox->addLayout(point);
    targetBox->addWidget(new QLabel(tr("Für Text:"), alignPage));
    auto* text = new QComboBox(alignPage);
    text->addItem(tr("Begrenzungsrahmen"));
    text->setToolTip(tr("Text wird an seinem Begrenzungsrahmen ausgerichtet"));
    targetBox->addWidget(text);
    targetBox->addStretch(1);
    ag->addLayout(targetBox, 2, 2, 3, 3);
    ag->setColumnStretch(5, 1);
    m_tabs->addTab(alignPage, tr("Ausrichten"));
    auto updatePoint = [this] {
        const bool on = m_target->currentIndex() == 4;
        m_pointX->setEnabled(on);
        m_pointY->setEnabled(on);
    };
    connect(m_target, &QComboBox::currentIndexChanged, this, updatePoint);
    updatePoint();
    for (const QList<QCheckBox*>& group : {QList<QCheckBox*>{m_left, m_hCenter, m_right}, QList<QCheckBox*>{m_top, m_vCenter, m_bottom}})
        for (QCheckBox* b : group)
            connect(b, &QCheckBox::toggled, this, [this, b, group](bool on) {
                if (on)
                    exclusive(b, group);
            });

    // --- Verteilen
    auto* distPage = new QWidget(m_tabs);
    auto* dg = new QGridLayout(distPage);
    const QStringList hIcons{QStringLiteral("obj-dist-left"), QStringLiteral("obj-dist-hcenter"), QStringLiteral("obj-dist-hspace"),
                             QStringLiteral("obj-dist-right")};
    const QStringList hTexts{tr("Links"), tr("Zentriert"), tr("Abstand"), tr("Rechts")};
    const QStringList vIcons{QStringLiteral("obj-dist-top"), QStringLiteral("obj-dist-vcenter"), QStringLiteral("obj-dist-vspace"),
                             QStringLiteral("obj-dist-bottom")};
    const QStringList vTexts{tr("Oben"), tr("Mitte"), tr("Abstand"), tr("Unten")};
    for (int i = 0; i < 4; ++i)
        m_dh << box(dg, 0, 2 + i, hIcons[i], hTexts[i], true);
    for (int i = 0; i < 4; ++i)
        m_dv << box(dg, 2 + i, 0, vIcons[i], vTexts[i], false);
    auto* extent = new QGroupBox(tr("Verteilen auf"), distPage);
    auto* el = new QVBoxLayout(extent);
    m_toSelection = new QRadioButton(tr("Auswahlumfang"), extent);
    m_toPage = new QRadioButton(tr("Seitenumfang"), extent);
    m_toSelection->setChecked(true);
    el->addWidget(m_toSelection);
    el->addWidget(m_toPage);
    dg->addWidget(extent, 2, 2, 3, 4, Qt::AlignTop);
    dg->setColumnStretch(6, 1);
    m_tabs->addTab(distPage, tr("Verteilen"));
    for (const QList<QCheckBox*>& group : {m_dh, m_dv})
        for (QCheckBox* b : group)
            connect(b, &QCheckBox::toggled, this, [this, b, group](bool on) {
                if (on)
                    exclusive(b, group);
            });

    auto* buttons = new QDialogButtonBox(this);
    QPushButton* applyButton = buttons->addButton(tr("Zuweisen"), QDialogButtonBox::ApplyRole);
    QPushButton* close = buttons->addButton(tr("Schliessen"), QDialogButtonBox::RejectRole);
    applyButton->setDefault(true);
    connect(applyButton, &QPushButton::clicked, this, &AlignDialog::apply);
    connect(close, &QPushButton::clicked, this, &QDialog::close);
    layout->addWidget(buttons);
}

void AlignDialog::exclusive(QCheckBox* box, const QList<QCheckBox*>& group)
{
    for (QCheckBox* other : group)
        if (other != box)
            other->setChecked(false);
}

void AlignDialog::showAlignTab()
{
    m_tabs->setCurrentIndex(0);
}

void AlignDialog::showDistributeTab()
{
    m_tabs->setCurrentIndex(1);
}

void AlignDialog::setAlign(align::H h, align::V v, int target)
{
    showAlignTab();
    m_left->setChecked(h == align::H::Left);
    m_hCenter->setChecked(h == align::H::Center);
    m_right->setChecked(h == align::H::Right);
    m_top->setChecked(v == align::V::Top);
    m_vCenter->setChecked(v == align::V::Center);
    m_bottom->setChecked(v == align::V::Bottom);
    m_target->setCurrentIndex(target);
}

void AlignDialog::setDistribute(align::Spread h, align::Spread v, bool toPage)
{
    showDistributeTab();
    for (int i = 0; i < 4; ++i) {
        m_dh[i]->setChecked(int(h) == i + 1);
        m_dv[i]->setChecked(int(v) == i + 1);
    }
    (toPage ? m_toPage : m_toSelection)->setChecked(true);
}

void AlignDialog::apply()
{
    if (m_tabs->currentIndex() == 0) {
        const align::H h = m_left->isChecked() ? align::H::Left
                           : m_hCenter->isChecked() ? align::H::Center
                           : m_right->isChecked()   ? align::H::Right
                                                    : align::H::None;
        const align::V v = m_top->isChecked() ? align::V::Top
                           : m_vCenter->isChecked() ? align::V::Center
                           : m_bottom->isChecked()  ? align::V::Bottom
                                                    : align::V::None;
        m_editor.alignSelection(h, v, Editor::AlignTo(m_target->currentIndex()), QPointF(m_pointX->value(), m_pointY->value()));
        return;
    }
    auto spread = [](const QList<QCheckBox*>& group) {
        for (int i = 0; i < group.size(); ++i)
            if (group[i]->isChecked())
                return align::Spread(i + 1);
        return align::Spread::None;
    };
    m_editor.distributeSelection(spread(m_dh), spread(m_dv), m_toPage->isChecked());
}

} // namespace vek
