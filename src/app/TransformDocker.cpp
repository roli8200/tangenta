#include "TransformDocker.h"

#include "Editor.h"
#include "Icons.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtMath>

#include <cmath>

namespace vek {

TransformDocker::TransformDocker(Editor& editor, QWidget* parent) : QWidget(parent), m_editor(editor)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);

    // Fünf Knöpfe oben
    auto* bar = new QHBoxLayout;
    m_pages = new QButtonGroup(this);
    const QStringList icons{QStringLiteral("xf-position"), QStringLiteral("xf-rotate"), QStringLiteral("xf-scale"),
                            QStringLiteral("xf-size"), QStringLiteral("xf-skew")};
    const QStringList tips{tr("Position (Alt+F7)"), tr("Drehen (Alt+F8)"), tr("Skalieren und spiegeln (Alt+F9)"),
                           tr("Grösse (Alt+F10)"), tr("Neigung")};
    for (int i = 0; i < icons.size(); ++i) {
        auto* b = new QToolButton(this);
        b->setIcon(icon(icons[i]));
        b->setIconSize(QSize(22, 22));
        b->setToolTip(tips[i]);
        b->setCheckable(true);
        b->setAutoRaise(true);
        m_pages->addButton(b, i);
        bar->addWidget(b);
    }
    bar->addStretch(1);
    layout->addLayout(bar);
    m_title = new QLabel(this);
    QFont bold = m_title->font();
    bold.setBold(true);
    m_title->setFont(bold);
    layout->addWidget(m_title);

    auto* fields = new QGridLayout;
    m_labelA = new QLabel(this);
    m_labelB = new QLabel(this);
    m_a = new QDoubleSpinBox(this);
    m_b = new QDoubleSpinBox(this);
    for (QDoubleSpinBox* s : {m_a, m_b}) {
        s->setRange(-100000, 100000);
        s->setDecimals(3);
        s->setKeyboardTracking(false);
    }
    fields->addWidget(m_labelA, 0, 0);
    fields->addWidget(m_a, 0, 1);
    fields->addWidget(m_labelB, 1, 0);
    fields->addWidget(m_b, 1, 1);
    m_mirrorH = new QToolButton(this);
    m_mirrorH->setIcon(icon(QStringLiteral("mirror-h")));
    m_mirrorH->setToolTip(tr("Waagrecht spiegeln"));
    m_mirrorH->setCheckable(true);
    m_mirrorV = new QToolButton(this);
    m_mirrorV->setIcon(icon(QStringLiteral("mirror-v")));
    m_mirrorV->setToolTip(tr("Senkrecht spiegeln"));
    m_mirrorV->setCheckable(true);
    fields->addWidget(m_mirrorH, 0, 2);
    fields->addWidget(m_mirrorV, 1, 2);
    layout->addLayout(fields);
    m_relative = new QCheckBox(tr("Relative Position"), this);
    m_proportional = new QCheckBox(tr("Proportional"), this);
    layout->addWidget(m_relative);
    layout->addWidget(m_proportional);

    // Bezugspunkt: 3×3 Kästchen
    auto* anchorBox = new QGridLayout;
    anchorBox->setSpacing(2);
    m_anchor = new QButtonGroup(this);
    for (int i = 0; i < 9; ++i) {
        auto* c = new QCheckBox(this);
        c->setToolTip(tr("Bezugspunkt"));
        m_anchor->addButton(c, i);
        anchorBox->addWidget(c, i / 3, i % 3, Qt::AlignCenter);
    }
    m_anchor->setExclusive(true);
    m_anchor->button(4)->setChecked(true);
    auto* anchorRow = new QHBoxLayout;
    anchorRow->addLayout(anchorBox);
    anchorRow->addStretch(1);
    layout->addLayout(anchorRow);

    auto* copyRow = new QHBoxLayout;
    copyRow->addWidget(new QLabel(tr("Kopien:"), this));
    m_copies = new QSpinBox(this);
    m_copies->setRange(0, 999);
    copyRow->addWidget(m_copies);
    copyRow->addStretch(1);
    layout->addLayout(copyRow);
    auto* applyButton = new QPushButton(tr("Zuweisen"), this);
    layout->addWidget(applyButton);
    layout->addStretch(1);

    connect(applyButton, &QPushButton::clicked, this, &TransformDocker::apply);
    connect(m_pages, &QButtonGroup::idClicked, this, [this] { refresh(); });
    connect(m_relative, &QCheckBox::toggled, this, [this] { refresh(); });
    connect(m_anchor, &QButtonGroup::idClicked, this, [this] {
        if (!m_relative->isChecked())
            refresh();
    });
    connect(&m_editor, &Editor::selectionChanged, this, &TransformDocker::refresh);
    connect(&m_editor, &Editor::documentChanged, this, [this] {
        if (page() == Page::Position && !m_relative->isChecked())
            refresh();
    });
    // Proportional: B folgt A
    connect(m_a, &QDoubleSpinBox::valueChanged, this, [this](double v) {
        if (!m_proportional->isChecked() || !m_proportional->isVisible())
            return;
        if (page() == Page::Scale) {
            m_b->setValue(v);
        } else if (page() == Page::Size) {
            const QRectF r = m_editor.selectionBounds();
            if (r.width() > 0)
                m_b->setValue(v * r.height() / r.width());
        }
    });
    m_pages->button(0)->setChecked(true);
    refresh();
}

TransformDocker::Page TransformDocker::page() const
{
    return Page(std::max(0, m_pages->checkedId()));
}

void TransformDocker::showPage(Page p)
{
    m_pages->button(int(p))->setChecked(true);
    refresh();
}

void TransformDocker::refresh()
{
    const Page p = page();
    const QRectF r = m_editor.selectionBounds();
    const QSignalBlocker ba(m_a), bb(m_b);
    m_relative->setVisible(p == Page::Position);
    m_proportional->setVisible(p == Page::Scale || p == Page::Size);
    m_mirrorH->setVisible(p == Page::Scale);
    m_mirrorV->setVisible(p == Page::Scale);
    m_labelB->setVisible(p != Page::Rotate);
    m_b->setVisible(p != Page::Rotate);
    switch (p) {
    case Page::Position: {
        m_title->setText(tr("Position"));
        m_labelA->setText(tr("H:"));
        m_labelB->setText(tr("V:"));
        m_a->setSuffix(QStringLiteral(" mm"));
        m_b->setSuffix(QStringLiteral(" mm"));
        if (m_relative->isChecked()) {
            m_a->setValue(0.0);
            m_b->setValue(0.0);
        } else if (!r.isNull()) {
            const QPointF a = anchorPoint(r);
            m_a->setValue(a.x());
            m_b->setValue(a.y());
        }
        break;
    }
    case Page::Rotate:
        m_title->setText(tr("Drehen"));
        m_labelA->setText(tr("Winkel:"));
        m_a->setSuffix(QStringLiteral(" °"));
        m_a->setValue(0.0);
        break;
    case Page::Scale:
        m_title->setText(tr("Skalieren und spiegeln"));
        m_labelA->setText(tr("H:"));
        m_labelB->setText(tr("V:"));
        m_a->setSuffix(QStringLiteral(" %"));
        m_b->setSuffix(QStringLiteral(" %"));
        m_a->setValue(100.0);
        m_b->setValue(100.0);
        break;
    case Page::Size:
        m_title->setText(tr("Grösse"));
        m_labelA->setText(tr("B:"));
        m_labelB->setText(tr("H:"));
        m_a->setSuffix(QStringLiteral(" mm"));
        m_b->setSuffix(QStringLiteral(" mm"));
        m_a->setValue(r.width());
        m_b->setValue(r.height());
        break;
    case Page::Skew:
        m_title->setText(tr("Neigung"));
        m_labelA->setText(tr("H:"));
        m_labelB->setText(tr("V:"));
        m_a->setSuffix(QStringLiteral(" °"));
        m_b->setSuffix(QStringLiteral(" °"));
        m_a->setValue(0.0);
        m_b->setValue(0.0);
        break;
    }
}

QPointF TransformDocker::anchorPoint(const QRectF& r) const
{
    const int i = std::max(0, m_anchor->checkedId());
    const double fx = (i % 3) / 2.0, fy = (i / 3) / 2.0;
    return QPointF(r.left() + r.width() * fx, r.top() + r.height() * fy);
}

void TransformDocker::setValues(double a, double b)
{
    m_a->setValue(a);
    m_b->setValue(b);
}

void TransformDocker::setAnchor(int index)
{
    if (QAbstractButton* b = m_anchor->button(std::clamp(index, 0, 8)))
        b->setChecked(true);
    if (!m_relative->isChecked())
        refresh();
}

void TransformDocker::setCopies(int copies)
{
    m_copies->setValue(copies);
}

void TransformDocker::setRelative(bool relative)
{
    m_relative->setChecked(relative);
}

void TransformDocker::setMirror(bool horizontal, bool vertical)
{
    m_mirrorH->setChecked(horizontal);
    m_mirrorV->setChecked(vertical);
}

QTransform TransformDocker::transformFor(const QRectF& r) const
{
    const QPointF a = anchorPoint(r);
    auto around = [&a](const QTransform& m) {
        return QTransform::fromTranslate(-a.x(), -a.y()) * m * QTransform::fromTranslate(a.x(), a.y());
    };
    switch (page()) {
    case Page::Position:
        if (m_relative->isChecked())
            return QTransform::fromTranslate(m_a->value(), m_b->value());
        return QTransform::fromTranslate(m_a->value() - a.x(), m_b->value() - a.y());
    case Page::Rotate:
        // Gegen den Uhrzeigersinn (y zeigt nach unten)
        return around(QTransform().rotate(-m_a->value()));
    case Page::Scale: {
        double sx = m_a->value() / 100.0, sy = m_b->value() / 100.0;
        if (m_mirrorH->isChecked())
            sx = -sx;
        if (m_mirrorV->isChecked())
            sy = -sy;
        if (sx == 0.0 || sy == 0.0)
            return QTransform();
        return around(QTransform::fromScale(sx, sy));
    }
    case Page::Size:
        if (r.width() <= 0 || r.height() <= 0 || m_a->value() <= 0 || m_b->value() <= 0)
            return QTransform();
        return around(QTransform::fromScale(m_a->value() / r.width(), m_b->value() / r.height()));
    case Page::Skew:
        return around(QTransform().shear(std::tan(qDegreesToRadians(-m_a->value())), std::tan(qDegreesToRadians(m_b->value()))));
    }
    return QTransform();
}

void TransformDocker::apply()
{
    const QRectF r = m_editor.selectionBounds();
    if (r.isNull())
        return;
    static const QStringList labels{tr("Position"), tr("Drehen"), tr("Skalieren und spiegeln"), tr("Grösse"), tr("Neigung")};
    m_editor.transformSelectionCopies(transformFor(r), m_copies->value(), labels.value(int(page())));
    refresh();
}

} // namespace vek
